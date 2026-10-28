#include "limbo.h"
#include <stdio.h>
#include <sys/resource.h>
#include <sys/wait.h>

#define check(c) predicate(!!(c), __LINE__) // Keep all comparisons active under NDEBUG.
#define Deep 30000 // Nesting exceeds the compiler's bounded native stack.

Biobuf *bsym; // Captured symbol-output stream identity.
int nblocks, nfiles; // Real symbol-writer dimension inputs.
File **files; // Borrowed source-file table.
Src nosrc; // Canonical absent source span.
Tattr tattr[Tend]; // Visibility policy shared by production and reference writers.
Type *tstring, *tfnptr; // Canonical string and function-pointer layout identities.
static File filetable[3]; // Deterministic file identities for genuine delta encoding.
static Type scalar, stringtype, pointerlayout; // Stable scalar and callback-layout Types.
static Sym fieldname = { .name = "field" }; // Visible field spelling.
static Sym hiddenname = { .name = ".hidden" }; // Hidden declaration spelling.
static Sym typename = { .name = "Type" }; // Nominal Type and tag spelling.
static Sym modulename = { .name = "Module" }; // Qualified nominal parent spelling.
static Sym implementname = { .name = "Impl" }; // Implemented-module qualifier spelling.
static Decl moduledecl; // Ordinary qualified declaration parent.
static Type types[8192]; // Shallow randomized Type arena.
static Decl declarations[16384]; // Shallow fields, nominal names and tags.
static int ntypes, ndeclarations; // Current randomized arena cursors.
static uint randomstate = 617381; // Repeatable generator state.
static char *output; // Dynamically captured actual formatter bytes.
static ulong length, capacity; // Current output byte length and buffer capacity.
static ulong live, checks, callbacks; // Private ownership, predicates and source callback counts.
static int expectedfatal; // Child is executing an invalid-metadata probe.


/* Diagnose the exact failed invariant without depending on assert configuration. */
static void
predicate(int valid, int line)
{
	if(!valid){
		fprintf(stderr, "symbol output failure at %d\n", line);
		abort();
	}
	checks++;
}


/* Retain memory instrumentation without unavailable process-based leak scanning. */
int
__lsan_is_turned_off(void)
{
	return 1;
}


/* Reject oversized test allocations before materializing unreasonable dimensions. */
void*
allocmem(ulong bytes)
{
	void *p;

	if(bytes > 16*1024*1024)
		_Exit(74);
	p = malloc(bytes == 0 ? 1 : bytes);
	check(p != nil);
	live++;
	return p;
}


/* Track production continuations and file cells independently of output storage. */
static void
workfree(void *p)
{
	if(p != nil){
		check(live != 0);
		live--;
	}
	free(p);
}


/* Expected metadata errors must occur before any header/output is published. */
void
fatal(char *format, ...)
{
	if(expectedfatal)
		_Exit(length == 0 ? 73 : 75);
	fprintf(stderr, "unexpected symbol fatal: %s\n", format);
	abort();
}


/* Provide the production formatter's isolated error-string provider. */
void
rerrstr(char *buf, uint size)
{
	if(size != 0)
		snprintf(buf, size, "fixture");
}


/* Append complete formatter records with a separate ownership domain. */
static void
append(char *text, ulong count)
{
	if(length+count+1 > capacity){
		capacity = 2*(length+count+1)+1024;
		output = realloc(output, capacity);
		check(output != nil);
	}
	memmove(output+length, text, count);
	length += count;
	output[length] = 0;
}


/* Use the real Plan 9 formatter instead of approximating its conversion grammar. */
int
Bprint(Biobuf *stream, char *format, ...)
{
	char text[4096], *end;
	va_list args;

	USED(stream);
	va_start(args, format);
	end = vseprint(text, text+sizeof(text), format, args);
	va_end(args);
	check(end < text+sizeof(text)-1);
	append(text, end-text);
	return end-text;
}


/* Preserve byte-level tag and newline writes in the same captured stream. */
int
Bputc(Biobuf *stream, int value)
{
	char c;

	USED(stream);
	c = value;
	append(&c, 1);
	return value;
}


/* Preserve the real source-delta encoder's ordered file and line lookups. */
Fline
fline(int line)
{
	Fline result;

	check(line >= 0);
	result.file = &filetable[line%3];
	result.line = line/3;
	callbacks++;
	return result;
}


/* Distinguish implemented-module names just as nominal SBL output requires. */
int
isimpmod(Sym *symbol)
{
	return symbol == &implementname;
}


/* Nonpolymorphic fixture functions retain their actual argument lists. */
int
ispoly(Decl *d)
{
	USED(d);
	return 0;
}


/* Unexpected polymorphic rewriting is a fixture contract failure. */
void
rmfnptrs(Decl *d)
{
	USED(d);
	abort();
}


/* Format source spans with the actual formatter and caller-supplied boundaries. */
char*
seprint(char *buf, char *end, char *format, ...)
{
	va_list args;

	va_start(args, format);
	buf = vseprint(buf, end, format, args);
	va_end(args);
	return buf;
}

#define free workfree // Observe production-only ownership.
#include "borrowed.c"

REFERENCE
#undef free


/* Reset only stream state, keeping all borrowed compiler graph identities live. */
static void
reset(void)
{
	length = callbacks = 0;
	lastf = lastline = 0;
	if(output != nil)
		output[0] = 0;
}


/* Generate a deterministic unsigned value without signed arithmetic overflow. */
static uint
randomword(void)
{
	randomstate = randomstate*1664525U+1013904223U;
	return randomstate;
}


/* Allocate and initialize a visible borrowed declaration in the shallow arena. */
static Decl*
newdecl(Type *t, int store)
{
	Decl *d;

	check(ndeclarations < nelem(declarations));
	d = &declarations[ndeclarations++];
	memset(d, 0, sizeof(*d));
	d->ty = t;
	d->sym = randomword()%7 == 0 ? &hiddenname : &fieldname;
	d->store = store;
	d->offset = randomword()%256;
	d->src.start.line = randomword()%99;
	d->src.stop.line = randomword()%99;
	d->src.start.pos = randomword()%40;
	d->src.stop.pos = randomword()%40;
	return d;
}


/* Build all supported shallow Type families, including shared adjacent tag layouts. */
static Type*
make(int depth)
{
	Type *t, *tagtype;
	Decl *first, *second, *tag;
	int choice;

	check(ntypes < nelem(types));
	t = &types[ntypes++];
	memset(t, 0, sizeof(*t));
	t->sbl = -1;
	t->size = IBY2WD;
	choice = depth == 0 ? randomword()%9 : randomword()%18;
	if(choice < 9){
		static int kinds[] = {Tnone, Tany, Tint, Tbig, Tbyte, Treal, Tstring, Tfix, Tpoly}; // Scalar grammar coverage.
		t->kind = kinds[choice];
		return t;
	}
	if(choice < 13){
		static int kinds[] = {Tref, Tarray, Tlist, Tchan}; // Unary wrapper grammar coverage.
		t->kind = kinds[choice-9];
		t->tof = make(depth-1);
		return t;
	}
	t->kind = choice == 13 ? Tfn : choice == 14 ? Ttuple : choice == 15 ? Texception : choice == 16 ? Tadt : Tmodule;
	first = newdecl(make(depth-1), t->kind == Tfn ? Darg : t->kind == Tmodule ? Dglobal : Dfield);
	second = newdecl(make(depth-1), first->store);
	first->next = second;
	t->ids = first;
	if(t->kind == Tfn)
		t->tof = make(depth-1);
	if(t->kind == Tadt || t->kind == Tmodule){
		t->decl = newdecl(t, Dtype);
		t->decl->sym = &typename;
		t->decl->dot = &moduledecl;
		t->decl->tag = 3;
		if(t->kind == Tadt && randomword()%2){
			tagtype = make(0);
			tagtype->ids = newdecl(make(depth-1), Dfield);
			tag = newdecl(tagtype, Dtag);
			t->tags = tag;
			tag->next = newdecl(tagtype, Dtag);
			tag->next->next = newdecl(make(0), Dtag);
		}
	}
	return t;
}


/* Compare complete bytes, delta state, callback counts and continuation ownership. */
static void
shallow(void)
{
	Type *t;
	char *expected;
	ulong bytes, calls;
	int i, file, line;

	for(i = 0; i < 10000; i++){
		ntypes = ndeclarations = 0;
		t = make(4);
		if(i%7 == 0){
			if(t->decl == nil)
				t->decl = newdecl(t, Dtype);
			t->sbl = i%31;
		}
		tattr[Tany].vis = 0;
		reset();
		reference_sbltype(t, i&1);
		bytes = length;
		calls = callbacks;
		file = lastf;
		line = lastline;
		expected = malloc(bytes+1);
		check(expected != nil);
		memmove(expected, output, bytes+1);
		reset();
		tattr[Tany].vis = 0;
		sbltype(t, i&1);
		check(length == bytes && memcmp(output, expected, bytes+1) == 0);
		check(callbacks == calls && lastf == file && lastline == line);
		check(live == 0 && tattr[Tany].vis == 0);
		free(expected);
	}
}


/* Exercise each nesting family with a small fixed C stack and large private graph. */
static void
deep(char *family)
{
	struct rlimit limit = {256*1024, 256*1024};
	Type *chain, *tagtypes;
	char prefix[128], *cursor;
	ulong prefixbytes;
	Decl *members, *names, *tags;
	int i, kind;

	chain = calloc(Deep+1, sizeof(*chain));
	tagtypes = calloc(Deep, sizeof(*tagtypes));
	members = calloc(Deep, sizeof(*members));
	names = calloc(Deep, sizeof(*names));
	tags = calloc(Deep, sizeof(*tags));
	check(chain && tagtypes && members && names && tags);
	chain[Deep] = scalar;
	kind = !strcmp(family, "wrappers") ? Tarray : !strcmp(family, "functions") ? Tfn : !strcmp(family, "tuples") ? Ttuple : Tadt;
	for(i = 0; i < Deep; i++){
		chain[i].kind = kind;
		chain[i].size = IBY2WD;
		chain[i].sbl = -1;
		chain[i].tof = kind == Tfn ? &scalar : &chain[i+1];
		members[i].ty = &chain[i+1];
		members[i].store = kind == Tfn ? Darg : Dfield;
		members[i].sym = &fieldname;
		chain[i].ids = kind == Tarray ? nil : &members[i];
		if(kind == Tadt){
			names[i].ty = &chain[i];
			names[i].sym = &typename;
			names[i].tag = 1;
			chain[i].decl = &names[i];
			chain[i].ids = nil;
			tags[i].sym = &fieldname;
			tags[i].ty = &tagtypes[i];
			tagtypes[i].size = IBY2WD;
			tagtypes[i].ids = &members[i];
			chain[i].tags = &tags[i];
		}
	}
	reset();
	check(setrlimit(RLIMIT_STACK, &limit) == 0);
	sbltype(chain, 0);
	check(live == 0 && length > Deep && output[length-1] == (kind == Tarray || kind == Tfn ? 'i' : '\n'));
	if(kind == Tarray)
		snprintf(prefix, sizeof(prefix), "A");
	else if(kind == Tfn)
		snprintf(prefix, sizeof(prefix), "F1\n0:field:0,0 ");
	else if(kind == Ttuple)
		snprintf(prefix, sizeof(prefix), "t%d.1\n0:field:0,0 ", IBY2WD);
	else
		snprintf(prefix, sizeof(prefix), "pType 0,0 %d\n0\n1\nfield:0,0 %d\n1\n0:field:0,0 ", IBY2WD, IBY2WD);
	prefixbytes = strlen(prefix);
	check(length == Deep*prefixbytes+1+(kind == Tarray ? 0 : kind == Tfn ? 2*Deep : Deep));
	cursor = output;
	for(i = 0; i < Deep; i++){
		check(memcmp(cursor, prefix, prefixbytes) == 0);
		cursor += prefixbytes;
	}
	check(*cursor++ == 'i');
	if(kind != Tarray)
		for(i = 0; i < Deep; i++){
			check(*cursor++ == '\n');
			if(kind == Tfn)
				check(*cursor++ == 'i');
		}
	check(cursor == output+length);
	free(chain);
	free(tagtypes);
	free(members);
	free(names);
	free(tags);
}


/* Keep a caller's pre-existing visibility after nested function-pointer rendering. */
static void
visibility(void)
{
	Type function = { .kind = Tfn, .tof = &scalar, .sbl = -1 };
	Type any = { .kind = Tany, .sbl = -1 };
	char expected[128];
	Type reference = { .kind = Tref, .tof = &function, .sbl = -1 };
	Decl member = { .ty = &scalar, .store = Dfield, .sym = &fieldname };
	int i;

	pointerlayout = scalar;
	for(i = 0; i < 2; i++){
		tattr[Tany].vis = i;
		reset();
		sbltype(&reference, 0);
		check(tattr[Tany].vis == i && !strcmp(output, "Ri") && live == 0);
	}
	pointerlayout.kind = Ttuple;
	pointerlayout.ids = &member;
	member.ty = &any;
	snprintf(expected, sizeof(expected), "Rt%d.1\n0:field:0,0 N\n", IBY2WD);
	for(i = 0; i < 2; i++){
		tattr[Tany].vis = i;
		reset();
		sbltype(&reference, 0);
		check(!strcmp(output, expected) && tattr[Tany].vis == i && live == 0);
	}
}


/* Repeated compiler outputs must release only their own file cells, never names. */
static void
lifetime(void)
{
	Decl module = { .sym = &modulename };
	int i;

	for(i = 0; i < 100; i++){
		sblmod(&module);
		check(live == 0 && nsfiles == 0);
		check(sblfile("first") == 0 && sblfile("second") == 1);
		check(sblfile("first") == 0 && live == 2);
	}
	sblmod(&module);
	check(live == 0 && sfiles == nil && ftail == nil);
}


/* Run invalid dimensions in a child so production fatal errors preserve semantics. */
static void
invalid(char *family, int which)
{
	Inst instruction = { .op = IMOVW };
	Decl declaration = { .ty = &scalar };
	Decl *vector[1] = { &declaration };

	reset();
	expectedfatal = 1;
	nblocks = 1;
	if(!strcmp(family, "blocks")){
		instruction.block = which == 0 ? -1 : which == 1 ? 1 : 0x7fffffff;
		sblinst(&instruction, 1);
	}else if(!strcmp(family, "source")){
		instruction.src.start.line = 1;
		instruction.src.start.pos = 0x7fffffff;
		sblinst(&instruction, 1);
	}else if(!strcmp(family, "blockids")){
		blockid = which == 0 ? -1 : 0x7fffffff;
		sblinst(&instruction, 1);
	}else if(!strcmp(family, "cardinality")){
		sblinst(which == 0 ? nil : &instruction, which == 1 ? 0 : which == 0 ? 1 : 2);
	}else if(!strcmp(family, "vectors")){
		if(which == 0)
			sblty(nil, 1);
		else if(which == 1)
			sblfn(vector, -1);
		else if(which == 2){
			vector[0] = nil;
			sblty(vector, 1);
		}else if(which == 3){
			declaration.ty = nil;
			sblty(vector, 1);
		}else if(which == 4)
			sblfn(vector, 0x7fffffff);
		else
			sblty((Decl**)((uintptr)-sizeof(void*)+1), 1);
	}else{
		nblocks = which == 0 ? -1 : which == 1 ? 0x7fffffff : 1;
		sblinst(which == 2 ? nil : &instruction, which == 2 ? -1 : 1);
	}
	_Exit(76);
}


/* Require invalid metadata to reject before formatting or oversized allocation. */
static void
metadata(char *family)
{
	int i, status;
	pid_t child;

	for(i = 0; i < (!strcmp(family, "vectors") ? 6 : 3); i++){
		child = fork();
		check(child >= 0);
		if(child == 0)
			invalid(family, i);
		check(waitpid(child, &status, 0) == child);
		check(WIFEXITED(status) && WEXITSTATUS(status) == 73);
	}
}


/* Compare nominal references, ADT picks and complete function sections byte for byte. */
static void
sections(void)
{
	Type adts[2] = {0}, refs[2] = {0}, functions[2] = {0};
	Decl names[2] = {0}, members[2] = {0}, fns[2] = {0};
	Decl *typevector[2] = { &names[0], &names[1] };
	Decl *fnvector[2] = { &fns[0], &fns[1] };
	Inst pcs[2] = {0};
	Type pick = { .kind = Tadtpick, .sbl = -1 };
	Decl pickname = {0};
	char *expected;
	ulong bytes, calls;
	int i, forced, file, line;

	for(i = 0; i < 2; i++){
		adts[i].kind = Tadt;
		adts[i].size = IBY2WD;
		adts[i].sbl = -1;
		adts[i].decl = &names[i];
		adts[i].ids = &members[i];
		names[i].sym = &typename;
		names[i].ty = &adts[i];
		names[i].dot = &moduledecl;
		names[i].src.start.line = 3*i;
		names[i].src.stop.line = 3*i+1;
		members[i].sym = &fieldname;
		members[i].ty = &refs[i];
		members[i].store = Dfield;
		refs[i].kind = Tref;
		refs[i].sbl = -1;
		refs[i].tof = &adts[1-i];
		functions[i].kind = Tfn;
		functions[i].sbl = -1;
		functions[i].tof = &adts[i];
		functions[i].ids = &members[i];
		fns[i].ty = &functions[i];
		fns[i].sym = &fieldname;
		fns[i].locals = &members[1-i];
		fns[i].pc = &pcs[i];
		fns[i].dot = i == 0 ? &names[i] : nil;
		pcs[i].pc = 100+i;
	}
	reset();
	reference_sblty(typevector, 2);
	reference_sblfn(fnvector, 2);
	bytes = length;
	calls = callbacks;
	file = lastf;
	line = lastline;
	expected = malloc(bytes+1);
	check(expected != nil);
	memmove(expected, output, bytes+1);
	adts[0].sbl = adts[1].sbl = -1;
	reset();
	sblty(typevector, 2);
	sblfn(fnvector, 2);
	check(length == bytes && !memcmp(output, expected, bytes+1));
	check(callbacks == calls && lastf == file && lastline == line && live == 0);
	free(expected);
	pick.decl = &pickname;
	pickname.dot = &names[0];
	for(forced = 0; forced < 2; forced++){
		reset();
		reference_sbltype(&pick, forced);
		bytes = length;
		expected = malloc(bytes+1);
		check(expected != nil);
		memmove(expected, output, bytes+1);
		reset();
		sbltype(&pick, forced);
		check(length == bytes && !memcmp(output, expected, bytes+1) && live == 0);
		free(expected);
	}
}


/* Compare repaired/absent spans, skipped noops and stable per-module block IDs. */
static void
instructions(void)
{
	Inst code[4] = {0}, copy[4];
	char *expected;
	ulong bytes;
	int i;

	nblocks = 3;
	blockid = 0;
	for(i = 0; i < 4; i++){
		code[i].op = i == 2 ? INOOP : IMOVW;
		code[i].block = i == 2 ? -1 : i%3;
		code[i].next = i == 3 ? nil : &code[i+1];
	}
	code[0].src.start.line = 1;
	code[0].src.start.pos = 8;
	code[1].src.start.line = 2;
	code[1].src.stop.line = 2;
	code[1].src.stop.pos = 19;
	memmove(copy, code, sizeof(code));
	for(i = 0; i < 4; i++)
		copy[i].next = i == 3 ? nil : &copy[i+1];
	reset();
	reference_sblinst(copy, 3);
	bytes = length;
	expected = malloc(bytes+1);
	check(expected != nil);
	memmove(expected, output, bytes+1);
	reset();
	blockid = 0;
	sblinst(code, 3);
	check(length == bytes && !memcmp(output, expected, bytes+1));
	free(expected);
	check(blockid == 2 && live == 0 && callbacks == 6);
	check(code[0].src.stop.line == 1 && code[0].src.stop.pos == 9);
	check(memcmp(&code[3].src, &code[1].src, sizeof(Src)) == 0);
	reset();
	nblocks = 0;
	sblinst(nil, 0);
	check(!strcmp(output, "0\n") && live == 0);
	reset();
	nblocks = 1;
	blockid = 0x7fffffff-1;
	code[0].next = nil;
	sblinst(code, 1);
	check(blockid == 0x7fffffff && live == 0);
	check(strstr(output, "2147483646\n") != nil);
}


/* Select independent regression families using real compiler WORD configurations. */
int
main(int argc, char **argv)
{
	int i;

	check(argc == 2);
	for(i = 0; i < Tend; i++)
		tattr[i].vis = 1;
	for(i = 0; i < 3; i++)
		filetable[i].sbl = i;
	scalar.kind = Tint;
	scalar.size = IBY2WD;
	scalar.sbl = -1;
	stringtype.kind = Tstring;
	stringtype.sbl = -1;
	tstring = &stringtype;
	pointerlayout = scalar;
	tfnptr = &pointerlayout;
	moduledecl.sym = &modulename;
	if(!strcmp(argv[1], "shallow"))
		shallow();
	else if(!strcmp(argv[1], "visibility"))
		visibility();
	else if(!strcmp(argv[1], "lifetime"))
		lifetime();
	else if(!strcmp(argv[1], "sections"))
		sections();
	else if(!strcmp(argv[1], "instructions"))
		instructions();
	else if(!strcmp(argv[1], "blocks") || !strcmp(argv[1], "source") || !strcmp(argv[1], "vectors") || !strcmp(argv[1], "counts") || !strcmp(argv[1], "cardinality") || !strcmp(argv[1], "blockids"))
		metadata(argv[1]);
	else
		deep(argv[1]);
	printf("PASS symbol output %s WORD%d: %lu predicates\n", argv[1], IBY2WD*8, checks);
	free(output);
	return 0;
}
