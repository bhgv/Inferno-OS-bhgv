/* Translate Dis instructions into C while retaining the Inferno runtime ABI. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <limits.h>
#include "isa.h"
#include "ops.h"

typedef struct Reader Reader;
typedef struct Instruction Instruction;

/* Bounded view of one borrowed Dis wire image. */
struct Reader {
	const unsigned char *data; // Owned input bytes, retained through C emission.
	size_t size, at; // Total image extent and next unread byte offset.
};

/* Decoded wire operands used to specialize the emitted checked decoder. */
struct Instruction {
	int op, add, reg; // Opcode, addressing modes and encoded middle operand.
	int src, dst; // Signed direct offsets or immediate values.
	int sf, ss, df, ds; // First/second offsets for indirect source and destination.
};

static Reader input; // Current bounded input reader.
static Instruction *program; // Owned specialized instruction records.
static unsigned char *type_seen; // Temporary descriptor uniqueness bitmap.
static int nprog; // Validated instruction count for generation and attachment.
static const char *prefix = "dis_module"; // Portable namespace for generated functions.


/* Report byte positions without interpreting input as a formatting string. */
static void
fail(const char *format, ...)
{
	va_list args;

	fprintf(stderr, "dis2c: byte %zu: ", input.at);
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fputc('\n', stderr);
	free(program);
	free(type_seen);
	free((void*)input.data);
	exit(1);
}


/* Consume only spans proven to fit, including empty spans at the input end. */
static const unsigned char*
take(size_t n)
{
	const unsigned char *p;

	if(n > input.size-input.at)
		fail("truncated Dis file");
	p = input.data+input.at;
	input.at += n;
	return p;
}


/* Decode signed 7/14/30-bit operands without signed shift overflow. */
static int
operand(void)
{
	const unsigned char *p;
	uint32_t v;
	unsigned b;

	b = *take(1);
	if(b < 0x80){
		v = b;
		if(b & 0x40)
			v |= UINT32_C(0xffffff80);
	}else if(b < 0xc0){
		v = ((uint32_t)(b & 0x3f)<<8) | *take(1);
		if(b & 0x20)
			v |= UINT32_C(0xffffc000);
	}else{
		p = take(3);
		v = ((uint32_t)(b & 0x3f)<<24) | ((uint32_t)p[0]<<16) | ((uint32_t)p[1]<<8) | p[2];
		if(b & 0x20)
			v |= UINT32_C(0xc0000000);
	}
	/* Avoid implementation-defined unsigned-to-signed conversion. */
	return v <= INT32_MAX ? (int)v : -1-(int)(UINT32_MAX-v);
}


/* Dis words are fixed-width big-endian values, independently of host long. */
static uint32_t
word(void)
{
	const unsigned char *p = take(4);

	return (uint32_t)p[0]<<24 | (uint32_t)p[1]<<16 | (uint32_t)p[2]<<8 | p[3];
}


/* Bound variable record counts by their minimum remaining wire footprint. */
static int
count(int n, size_t minimum)
{
	if(n < 0 || (size_t)n > (input.size-input.at)/minimum)
		fail("invalid record count");
	return n;
}


/* Skip one bounded NUL-terminated wire name without a speculative strlen. */
static void
name(void)
{
	const unsigned char *p, *end;

	p = input.data+input.at;
	end = memchr(p, 0, input.size-input.at);
	if(end == NULL)
		fail("unterminated name");
	input.at += (size_t)(end-p)+1;
}


/* The loader patches these immediate destinations into original Inst pointers. */
static int
branch(int op)
{
	switch(op){
	case ICALL: case IJMP: case ISPAWN:
	case IBEQB: case IBNEB: case IBLTB: case IBLEB: case IBGTB: case IBGEB:
	case IBEQW: case IBNEW: case IBLTW: case IBLEW: case IBGTW: case IBGEW:
	case IBEQF: case IBNEF: case IBLTF: case IBLEF: case IBGTF: case IBGEF:
	case IBEQC: case IBNEC: case IBLTC: case IBLEC: case IBGTC: case IBGEC:
	case IBEQL: case IBNEL: case IBLTL: case IBLEL: case IBGTL: case IBGEL:
		return 1;
	}
	return 0;
}


/* Read exactly the fields selected by one valid Dis addressing mode. */
static void
address(int mode, int *value, int *first, int *second)
{
	if(mode == AXXX)
		return;
	if(mode == AMP || mode == AFP || mode == AIMM){
		*value = operand();
		return;
	}
	if(mode != (AIND|AMP) && mode != (AIND|AFP))
		fail("invalid addressing mode");
	*first = operand();
	*second = operand();
	if(*first < 0 || *first > 65535 || *second < 0 || *second > 65535)
		fail("indirect offset does not fit Inst");
}


/* Validate the wire format; native pointer maps and allocation remain loader-owned. */
static void
parse(void)
{
	int magic, flags, ntype, nexport, n, i, j, id, size, np, depth, tag, offset;
	int entry, entrytype, packed, total, typed, pc1, pc2;
	Instruction *ins;
	uint32_t type, length;

	magic = operand();
	if(magic == SMAGIC){
		n = operand();
		if(n < 0)
			fail("negative signature length");
		take((size_t)n);
	}else if(magic != XMAGIC)
		fail("bad magic");
	flags = operand();
	if(flags < 0 || (flags & (HASLDT0|DYNMOD)) || (flags & ~127))
		fail("unsupported runtime flags");
	if(operand() < 0)
		fail("negative stack size");
	nprog = count(operand(), 2);
	if(operand() < 0)
		fail("negative data size");
	ntype = count(operand(), 3);
	nexport = count(operand(), 7);
	entry = operand(); entrytype = operand();
	if(entry < -1 || entry >= nprog || entrytype < -1 || entrytype >= ntype)
		fail("invalid entry index");
	program = calloc(nprog ? (size_t)nprog : 1, sizeof(*program));
	type_seen = calloc(ntype ? (size_t)ntype : 1, 1);
	if(program == NULL || type_seen == NULL)
		fail("out of memory");
	for(i = 0; i < nprog; i++){
		ins = &program[i];
		ins->op = *take(1); ins->add = *take(1);
		if(ins->op >= MAXDIS || handlers[ins->op] == NULL)
			fail("unknown opcode");
		if((ins->add & ARM) != AXNON){
			ins->reg = operand();
			if(ins->reg < -32768 || ins->reg > 65535 ||
			   ((ins->add & ARM) != AXIMM && ins->reg < 0))
				fail("middle operand does not fit Inst");
		}
		address(USRC(ins->add), &ins->src, &ins->sf, &ins->ss);
		address(UDST(ins->add), &ins->dst, &ins->df, &ins->ds);
		if(branch(ins->op) && UDST(ins->add) == AIMM && (ins->dst < 0 || ins->dst >= nprog))
			fail("branch target outside program");
	}
	for(i = 0; i < ntype; i++){
		id = operand(); size = operand(); np = operand();
		if(id < 0 || id >= ntype || type_seen[id] || size < 0 || np < 0 || np > 128*1024)
			fail("invalid type descriptor");
		type_seen[id] = 1;
		take((size_t)np);
	}
	free(type_seen);
	type_seen = NULL;
	depth = 0;
	for(;;){
		tag = *take(1);
		if(tag == 0)
			break;
		n = DLEN(tag);
		if(n == 0)
			n = operand();
		offset = operand();
		if(n < 0 || offset < 0)
			fail("negative initializer span");
		switch(DTYPE(tag)){
		case DEFB: case DEFS:
			take((size_t)n); break;
		case DEFW:
			take((size_t)count(n, 4)*4); break;
		case DEFL: case DEFF:
			take((size_t)count(n, 8)*8); break;
		case DEFA:
			type = word(); length = word();
			if(type >= (uint32_t)ntype || length > INT32_MAX)
				fail("invalid array initializer");
			break;
		case DIND:
			if(word() > INT32_MAX || depth == DADEPTH)
				fail("invalid initializer nesting");
			depth++; break;
		case DAPOP:
			if(depth == 0)
				fail("unbalanced initializer pop");
			depth--; break;
		default: fail("unsupported initializer kind");
		}
	}
	if(depth != 0)
		fail("unclosed array initializer");
	name();
	for(i = 0; i < nexport; i++){
		id = operand(); type = (uint32_t)operand();
		if(id < -1 || id >= nprog || (type != UINT32_MAX && type >= (uint32_t)ntype))
			fail("invalid export index");
		word();
		if(id == -1 && (type != UINT32_MAX || input.size-input.at < 4 || memcmp(input.data+input.at, ".mp\0", 4) != 0))
			fail("invalid global-data export");
		name();
	}
	if(flags & HASLDT){
		n = count(operand(), 1);
		for(i = 0; i < n; i++){
			size = count(operand(), 5);
			for(j = 0; j < size; j++){
				word(); name();
			}
		}
		take(1);
	}
	if(flags & HASEXCEPT){
		n = count(operand(), 6);
		for(i = 0; i < n; i++){
			if(operand() < 0)
				fail("negative exception offset");
			pc1 = operand(); pc2 = operand(); id = operand(); packed = operand();
			if(pc1 < 0 || pc2 < pc1 || pc2 > nprog || id < -1 || id >= ntype || packed < 0)
				fail("invalid exception handler");
			typed = packed>>16; total = packed & 65535;
			if(typed > total)
				fail("invalid exception counts");
			count(total, 2);
			for(j = 0; j < total; j++){
				name(); id = operand();
				if(id < 0 || id >= nprog)
					fail("invalid exception target");
			}
			id = operand();
			if(id < -1 || id >= nprog)
				fail("invalid default exception target");
		}
		take(1);
	}
	/* Both Limbo compilers append an optional source-path string. */
	if(input.at != input.size)
		name();
	if(input.at != input.size)
		fail("trailing bytes after source path");
}


/* Specialize checked native address decoding while preserving unused registers. */
static void
emitaddress(FILE *out, const char *reg, int mode, int value, int first, int second)
{
	if(mode == AMP || mode == AFP)
		fprintf(out, "\t\tR.%s = decaddress(R.%s, %d);\n", reg, mode == AMP ? "MP" : "FP", value);
	else if(mode == AIMM)
		fprintf(out, "\t\tR.%s = (uchar*)ip+O(Inst, %s);\n", reg, reg);
	else if(mode == (AIND|AMP) || mode == (AIND|AFP))
		fprintf(out, "\t\tR.%s = decindirect(R.%s, %d, %d);\n", reg, mode == (AIND|AMP) ? "MP" : "FP", first, second);
}


/* Keep modular arithmetic inline after complete slot checks and unaligned captures.
 * Moves preserve raw bits and overlap; complex operations retain VM handlers. */
static void
emitoperation(FILE *out, int op)
{
	const char *type = NULL, *symbol = NULL;

	switch(op){
	case IMOVB: type = "BYTE"; break;
	case IMOVW: type = "WORD"; break;
	case IMOVL: type = "LONG"; break;
	case IMOVF: type = "REAL"; break;
	case IADDW: type = "UWORD"; symbol = "+"; break;
	case ISUBW: type = "UWORD"; symbol = "-"; break;
	case IMULW: type = "UWORD"; symbol = "*"; break;
	case IANDW: type = "UWORD"; symbol = "&"; break;
	case IORW: type = "UWORD"; symbol = "|"; break;
	case IXORW: type = "UWORD"; symbol = "^"; break;
	case IADDL: type = "ULONG"; symbol = "+"; break;
	case ISUBL: type = "ULONG"; symbol = "-"; break;
	case IMULL: type = "ULONG"; symbol = "*"; break;
	case IANDL: type = "ULONG"; symbol = "&"; break;
	case IORL: type = "ULONG"; symbol = "|"; break;
	case IXORL: type = "ULONG"; symbol = "^"; break;
	}
	if(type != NULL){
		fprintf(out, "\t\t{\n");
		if(symbol != NULL){
			fprintf(out, "\t\t\t%s source, middle, result;\n", type);
			fprintf(out,
				"\t\t\tif(!%s_slot(R.d, sizeof(result)) || !%s_slot(R.s, sizeof(source)) || !%s_slot(R.m, sizeof(middle))){\n"
				"\t\t\t\terror(exBounds);\n"
				"\t\t\t\treturn 0;\n"
				"\t\t\t}\n", prefix, prefix, prefix);
			fprintf(out, "\t\t\tmemmove(&source, R.s, sizeof(source));\n\t\t\tmemmove(&middle, R.m, sizeof(middle));\n");
			fprintf(out, "\t\t\tresult = middle %s source;\n", symbol);
			fprintf(out, "\t\t\tmemmove(R.d, &result, sizeof(result));\n");
		}else{
			fprintf(out,
				"\t\t\tif(!%s_slot(R.d, sizeof(%s)) || !%s_slot(R.s, sizeof(%s))){\n"
				"\t\t\t\terror(exBounds);\n"
				"\t\t\t\treturn 0;\n"
				"\t\t\t}\n", prefix, type, prefix, type);
			fprintf(out, "\t\t\tmemmove(R.d, R.s, sizeof(%s));\n", type);
		}
		fprintf(out, "\t\t}\n");
	}else
		fprintf(out, "\t\t%s();\n", handlers[op]);
}


/* Emit ordinary C code plus the exact image needed by the authoritative module loader. */
static void
emit(FILE *out)
{
	size_t at;
	int i, mode;
	Instruction *ins;

	fprintf(out, "/* Generated by dis2c. Link with the matching Inferno runtime. */\n");
	fprintf(out, "#include \"lib9.h\"\n#include \"isa.h\"\n#include \"interp.h\"\n#include \"raise.h\"\n#include \"dec-addr.h\"\n\n");
	for(i = 0; i < MAXDIS; i++)
		fprintf(out, "extern void %s(void);\n", handlers[i]);
	fprintf(out,
		"\n"
		"\n"
		"/* Bound full decoded operand cells independently of VM WORD width. */\n"
		"static int\n"
		"%s_slot(void *slot, uintptr width)\n"
		"{\n"
		"\treturn slot != nil && slot != H && (uintptr)slot <= (uintptr)-1-width;\n"
		"}\n", prefix);
	fprintf(out, "\n\n/* One translated Dis instruction; scheduler and exception PCs remain Dis PCs. */\nstatic int\n%s_step(void)\n{\n", prefix);
	fprintf(out, "\tInst *ip, *program;\n\tModule *module;\n\tint count;\n\tuintptr pc, base;\n\n\tip = R.PC;\n\tpc = (uintptr)ip;\n");
	fprintf(out, "\tif(!%s_slot(R.M, O(Modlink, prog)+sizeof(program))){\n\t\terror(exModule);\n\t\treturn 0;\n\t}\n", prefix);
	fprintf(out,
		"\tmemmove(&module, (uchar*)R.M+O(Modlink, m), sizeof(module));\n"
		"\tmemmove(&program, (uchar*)R.M+O(Modlink, prog), sizeof(program));\n");
	fprintf(out, "\tif(!%s_slot(module, O(Module, nprog)+sizeof(count))){\n\t\terror(exModule);\n\t\treturn 0;\n\t}\n", prefix);
	fprintf(out, "\tmemmove(&count, (uchar*)module+O(Module, nprog), sizeof(count));\n\tbase = (uintptr)program;\n");
	fprintf(out,
		"\tif(count != %d || count <= 0 || program == nil || program == H ||\n"
		"\t   (uintptr)count > (uintptr)-1/sizeof(Inst) ||\n"
		"\t   base > (uintptr)-1-(uintptr)count*sizeof(Inst) ||\n"
		"\t   pc < base || (pc-base)%%sizeof(Inst) != 0 || (pc-base)/sizeof(Inst) >= (uintptr)count){\n"
		"\t\terror(exBounds);\n"
		"\t\treturn 0;\n"
		"\t}\n", nprog);
	fprintf(out, "\tswitch((pc-base)/sizeof(Inst)){\n");
	for(i = 0; i < nprog; i++){
		ins = &program[i];
		fprintf(out, "\tcase %d: /* %s */\n", i, handlers[ins->op]);
		emitaddress(out, "s", USRC(ins->add), ins->src, ins->sf, ins->ss);
		emitaddress(out, "d", UDST(ins->add), ins->dst, ins->df, ins->ds);
		if(UDST(ins->add) != AXXX){
			mode = ins->add & ARM;
			if(mode == AXNON)
				fprintf(out, "\t\tR.m = R.d;\n");
			else if(mode == AXIMM)
				fprintf(out, "\t\tR.t = (short)%d;\n\t\tR.m = &R.t;\n", ins->reg);
			else
				fprintf(out, "\t\tR.m = decaddress(R.%s, %d);\n", mode == AXINF ? "FP" : "MP", ins->reg);
		}
		fprintf(out, "\t\tR.PC = (Inst*)(pc+sizeof(Inst));\n");
		emitoperation(out, ins->op);
		fprintf(out, "\t\treturn 1;\n");
	}
	fprintf(out, "\t}\n\treturn 0;\n}\n\n/* Preserve data initializers, imports, signatures and exception metadata verbatim. */\n");
	fprintf(out, "static uchar %s_image[] = {\n", prefix);
	for(at = 0; at < input.size; at++){
		if(at%12 == 0) fputc('\t', out);
		fprintf(out, "0x%02x,%s", input.data[at], at%12 == 11 ? "\n" : " ");
	}
	fprintf(out, "\n};\nstatic DisC %s_code = { %s_image, sizeof(%s_image), %d, %s_step, nil };\n", prefix, prefix, prefix, nprog, prefix);
	fprintf(out,
		"\n"
		"/* Register before loading this exact image; repeated registration is harmless. */\n"
		"int\n"
		"%s_register(void)\n"
		"{\n"
		"\treturn disc_register(&%s_code);\n"
		"}\n", prefix, prefix);
	fprintf(out,
		"\n"
		"/* Load embedded metadata through the normal checked and signed-module loader. */\n"
		"Module*\n"
		"%s_load(char *path, Dir *dir)\n"
		"{\n"
		"\tif(path == nil || dir == nil || !%s_register())\n"
		"\t\treturn nil;\n"
		"\treturn parsemod(path, %s_image, sizeof(%s_image), dir);\n"
		"}\n", prefix, prefix, prefix, prefix);
}


/* Restrict generated symbol prefixes to portable, non-reserved C identifiers. */
static int
identifier(const char *s)
{
	if(!isalpha((unsigned char)*s))
		return 0;
	for(s++; *s; s++)
		if(!isalnum((unsigned char)*s) && *s != '_')
			return 0;
	return 1;
}


/* Keep the host tool buildable as C99 with no Inferno libraries or Python at run time. */
int
main(int argc, char **argv)
{
	const char *file = NULL, *output = NULL;
	unsigned char *data;
	long length;
	FILE *in, *out;
	int i, check = 0;

	for(i = 1; i < argc; i++){
		if(strcmp(argv[i], "-n") == 0 && i+1 < argc)
			prefix = argv[++i];
		else if(strcmp(argv[i], "-o") == 0 && i+1 < argc)
			output = argv[++i];
		else if(strcmp(argv[i], "--check") == 0)
			check = 1;
		else if(strcmp(argv[i], "--help") == 0){
			puts("usage: dis2c [-n symbol_prefix] [-o output.c] [--check] input.dis");
			return 0;
		}else if(argv[i][0] == '-' || file != NULL)
			fail("invalid arguments (use --help)");
		else
			file = argv[i];
	}
	if(file == NULL || !identifier(prefix) || (output != NULL && strcmp(file, output) == 0))
		fail("missing input, invalid prefix, or output equals input");
	in = fopen(file, "rb");
	if(in == NULL || fseek(in, 0, SEEK_END) != 0 || (length = ftell(in)) < 0 || length >= 8*1024*1024 || fseek(in, 0, SEEK_SET) != 0)
		fail("cannot read input or input exceeds the Dis file-size limit");
	data = malloc(length ? (size_t)length : 1);
	if(data == NULL)
		fail("out of memory");
	input.data = data; input.size = (size_t)length;
	if(fread(data, 1, (size_t)length, in) != (size_t)length || fclose(in) != 0)
		fail("cannot read input");
	parse();
	if(!check){
		out = output == NULL ? stdout : fopen(output, "wb");
		if(out == NULL)
			fail("cannot open output");
		emit(out);
		if(ferror(out) || (out == stdout ? fflush(out) : fclose(out)) != 0)
			fail("cannot write output");
	}
	free(program); free(data);
	return 0;
}
