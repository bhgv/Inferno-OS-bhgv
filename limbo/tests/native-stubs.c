#include "limbo.h"
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>

char *emitcode, *emittab;
int emitdyn;
static Type none = {.kind=Tnone};
Type *tnone = &none;
Tattr tattr[Tend];
static Desc descriptor;
static uchar bitmap[18];
static char output[131072];
static size_t used;
int errors;
static int phase, signs, maps;
static void *owned;

/* Track the dynamically sized include-name buffer and every release path. */
void*
allocmem(ulong size)
{
	assert(owned == nil && size < sizeof(output));
	owned = malloc(size);
	assert(owned != nil);
	return owned;
}

/* Static mode and unmatched modules must not leak or free borrowed names. */
static void
releasebuffer(void *p)
{
	if(p == nil)
		return;
	assert(p == owned);
	owned = nil;
	free(p);
}

/* Empty ADT lists avoid requiring the compiler's complete type-resolution pass. */
Type*
usetype(Type *t)
{
	return t;
}

/* Signing must still precede function-layout generation as in the original code. */
ulong
sign(Decl *d)
{
	assert(d->ty->kind == Tfn);
	phase = 1;
	signs++;
	return 0x12345678UL;
}

/* Keep frame layout an explicit adapter; the tests target its emitted map capacity. */
long
idoffsets(Decl *d, long start, int alignment)
{
	(void)d;
	assert(phase == 1 && start == MaxTemp && alignment == MaxAlign);
	return descriptor.size;
}

/* Return an independently selected boundary descriptor, exactly once per row. */
Desc*
mkdesc(long size, Decl *d)
{
	(void)d;
	assert(phase == 1 && size == descriptor.size);
	phase = 2;
	maps++;
	return &descriptor;
}

/* Oversized maps must become source diagnostics before any invalid C row appears. */
void
error(Line line, char *format, ...)
{
	(void)format;
	assert(line.line == 123);
	errors++;
}

/* Render ordinary strings and the two compiler-specific signature/map formats. */
int
print(char *format, ...)
{
	va_list args;
	Desc *d;
	char *name, *module, *function;
	ulong signature;
	long size, count, i;
	int n, total = 0;

	va_start(args, format);
	if(strcmp(format, "%ld,%ld,%M,") == 0){
		size = va_arg(args, long);
		count = va_arg(args, long);
		d = va_arg(args, Desc*);
		n = snprintf(output+used, sizeof(output)-used, "%ld,%ld,{", size, count);
		used += n; total += n;
		for(i = 0; i < count; i++){
			n = snprintf(output+used, sizeof(output)-used, "%s0x%02x", i ? "," : "", d->map[i]);
			used += n; total += n;
		}
		n = snprintf(output+used, sizeof(output)-used, "%s},", count ? "" : "0");
	}else if(strcmp(format, "%s\",0x%lux,%s_%s,") == 0){
		name = va_arg(args, char*);
		signature = va_arg(args, ulong);
		module = va_arg(args, char*);
		function = va_arg(args, char*);
		n = snprintf(output+used, sizeof(output)-used, "%s\",0x%lx,%s_%s,", name, signature, module, function);
	}else
		n = vsnprintf(output+used, sizeof(output)-used, format, args);
	va_end(args);
	assert(n >= 0 && n < sizeof(output)-used);
	used += n;
	assert(used < sizeof(output));
	return total+n;
}

#define free(p) releasebuffer(p)
IMPLEMENTATION
#undef free

/* Keep output and ordering assertions independent across all generator calls. */
static void
reset(void)
{
	used = 0;
	output[0] = 0;
	errors = phase = signs = maps = 0;
	assert(owned == nil);
}

/* Exercise old buffer boundaries, long valid identifiers and both exit paths. */
static void
names(void)
{
	Type interface = {.kind=Tadt}, module = {.kind=Tmodule,.tof=&interface};
	Sym symbol = {0};
	Decl declaration = {.store=Dtype,.ty=&module,.sym=&symbol};
	char name[4097], saved[4097], expected[8192];
	int lengths[] = {0,1,31,32,33,63,64,4096};
	int i, j, mode;

	for(i = 0; i < nelem(lengths); i++){
		for(j = 0; j < lengths[i]; j++)
			name[j] = 'A'+j%26;
		name[lengths[i]] = 0;
		strcpy(saved, name);
		symbol.name = emitcode = name;
		for(mode = 0; mode < 2; mode++){
			emitdyn = mode;
			reset();
			modcode(&declaration);
			assert(owned == nil && strcmp(name, saved) == 0);
			if(mode){
				for(j = 0; saved[j]; j++)
					saved[j] += 'a'-'A';
				snprintf(expected, sizeof(expected), "#include \"%s.h\"", saved);
				assert(strstr(output, expected) != nil);
				snprintf(expected, sizeof(expected), "#include \"%smod.h\"", saved);
				assert(strstr(output, expected) != nil);
			}
		}
		emitdyn = 1;
		reset();
		modcode(nil);
		assert(owned == nil);
	}
}

/* Check exact map limits, varargs exemption and compilable emitted table rows. */
static void
tables(char *directory)
{
	Type fn = {.kind=Tfn,.tof=&none}, interface = {.kind=Tadt};
	Type module = {.kind=Tmodule,.tof=&interface};
	Sym ms = {.name="Native"}, fs = {.name="run"};
	Decl md = {.store=Dtype,.ty=&module,.sym=&ms};
	Decl fd = {.store=Dfn,.ty=&fn,.sym=&fs,.dot=&md};
	long lengths[] = {-1,0,1,15,16,17};
	char path[1024];
	FILE *file;
	int i, varargs;

	interface.ids = &fd;
	fd.src.start.line = 123;
	emittab = "Native";
	memset(bitmap, 0x80, sizeof(bitmap));
	descriptor.map = bitmap;
	descriptor.size = 16*8*sizeof(void*);
	for(varargs = 0; varargs < 2; varargs++)
	for(i = 0; i < nelem(lengths); i++){
		fn.varargs = varargs;
		descriptor.nmap = lengths[i];
		reset();
		modtab(&md);
		assert(signs == 1 && maps == !varargs);
		if(!varargs && (lengths[i] < 0 || lengths[i] > 16)){
			assert(errors == 1 && strstr(output, "Native_run") == nil);
			continue;
		}
		assert(errors == 0 && strstr(output, "map[16]") != nil);
		assert(strstr(output, "#define Nativemodlen\t1") != nil);
		snprintf(path, sizeof(path), "%s/table-%d-%d.c", directory, varargs, i);
		file = fopen(path, "w"); assert(file != nil);
		fputs("typedef unsigned char uchar;\nvoid Native_run(void*);\n", file);
		fputs(output, file);
		fputs("typedef char mapwidth[sizeof(((Runtab*)0)->map)==16 ? 1 : -1];\n", file);
		assert(fclose(file) == 0);
	}
}

/* Run production code generation without invoking the complete Limbo frontend. */
int
main(int argc, char **argv)
{
	assert(argc == 2);
	if(getenv("NATIVE_STUB_MAPS_ONLY") == nil)
		names();
	tables(argv[1]);
	puts("native stubs: long names, owned lowercase paths, map boundaries and sign/layout ordering passed");
	return 0;
}
