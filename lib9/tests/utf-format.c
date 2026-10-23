#include "lib9.h"
#include "fmtdef.h"
#include <assert.h>
#include <stdio.h>
#include <limits.h>
#include <stdarg.h>
#include <sys/mman.h>

/* Error text is independent of any running emulator process. */
void
rerrstr(char *p, uint n)
{
	if(n)
		snprintf(p, n, "fixture");
}

/* Decode the fixture protocol without depending on native long width. */
static unsigned
number(FILE *f)
{
	uchar b[4];

	assert(fread(b, 1, 4, f) == 4);
	return (unsigned)b[0] | (unsigned)b[1]<<8 | (unsigned)b[2]<<16 | (unsigned)b[3]<<24;
}

/* Reject impossible metadata and stop when decoded input cannot supply its count. */
static void
spans(void)
{
	Fmt f = {0};
	char output[64], one[1] = {(char)0xe2};
	char two[2] = {(char)0xc2, (char)0xa2};
	Rune r;

	memset(output, 0xa5, sizeof(output));
	f.start = f.to = output;
	f.stop = output+sizeof(output)-1;
	assert(_fmtcpy(&f, one, -1, 1) == -1);
	assert(_fmtcpy(&f, one, 1, -1) == -1);
	assert(_fmtcpy(&f, nil, 1, 1) == -1);
	assert(_fmtcpy(&f, one, 2, 1) == -1);
	assert(_fmtrcpy(&f, nil, 1) == -1);
	assert(_fmtrcpy(&f, nil, -1) == -1);
	assert(f.to == output && (uchar)output[0] == 0xa5);
	assert(_fmtcpy(&f, nil, 0, 0) == 0);
	assert(_fmtrcpy(&f, nil, 0) == 0);
	/* A single unterminated lead byte is a bounded malformed scalar. */
	assert(_fmtcpy(&f, one, 1, 1) == 0);
	assert((char*)f.to-output == 3);
	assert(chartorune(&r, output) == 3 && r == Runeerror);
	/* Two bytes contain one scalar, so a second requested rune must not read past them. */
	f.to = output;
	f.nfmt = 0;
	assert(_fmtcpy(&f, two, 2, 2) == -1 && (char*)f.to-output == 2);
}

typedef struct Sink Sink;
struct Sink
{
	uchar data[256];
	int used;
};

/* Flush into owned storage, allowing a chunk to fill right up to its end. */
static int
collect(Fmt *f)
{
	Sink *sink;
	int size;

	sink = f->farg;
	size = (char*)f->to-(char*)f->start;
	assert(size >= 0 && sink->used+size <= sizeof(sink->data));
	memmove(sink->data+sink->used, f->start, size);
	sink->used += size;
	f->to = f->start;
	return 1;
}

/* Invoke the real engine with a caller-owned streaming output buffer. */
static int
streamformat(Fmt *f, char *format, ...)
{
	int n;

	va_start(f->args, format);
	n = dofmt(f, format);
	va_end(f->args);
	return n;
}

/* Force byte/Rune flush boundaries and check precision does not lose the suffix. */
static void
stream(void)
{
	Rune chunk[3], input[] = {'x',' ',0x1f600,0};
	Rune want[] = {'A','\'',0xa2,0x20ac,0x1f600,'\'','|','\'',
		'x',' ',0x1f600,'\'',' ',' ',' ',' ','|','Z'};
	char *expected = "A'¢€😀'|'x 😀'    |Z";
	Fmt f;
	Sink sink;
	int runes, n;

	for(runes = 0; runes < 2; runes++){
		memset(&f, 0, sizeof(f));
		memset(&sink, 0, sizeof(sink));
		f.runes = runes;
		f.start = f.to = chunk;
		f.stop = runes ? (void*)(chunk+3) : (void*)((char*)chunk+4);
		f.flush = collect;
		f.farg = &sink;
		n = streamformat(&f, "A%#q|%-9Q|%.1sZ", "¢€😀", input, "¢x");
		assert(n == (runes ? nelem(want) : strlen(expected)));
		assert(collect(&f) == 1);
		if(runes)
			assert(sink.used == sizeof(want) && memcmp(sink.data, want, sizeof(want)) == 0);
		else
			assert(sink.used == strlen(expected) && memcmp(sink.data, expected, sink.used) == 0);
	}
}

/* Reject genuine over-wide strings before narrowing native lengths into int. */
static void
wide(void)
{
	ulong length, width, bytes, chunk, capacity, offset, page, lastpage, i;
	char *base, *piece, output[16];
	FILE *file;
	Fmt f;
	int runes;

	if(sizeof(long) <= sizeof(int)){
		puts("wide formatter fixture requires LP64");
		return;
	}
	length = (ulong)INT_MAX+34;
	chunk = 2*1024*1024;
	page = sysconf(_SC_PAGESIZE);
	for(runes = 0; runes < 2; runes++){
		width = runes ? sizeof(Rune) : 1;
		bytes = (length+1)*width;
		capacity = (bytes/chunk+1)*chunk;
		base = mmap(nil, capacity, PROT_NONE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
		assert(base != MAP_FAILED);
		file = tmpfile();
		assert(file != nil && ftruncate(fileno(file), chunk) == 0);
		piece = mmap(nil, chunk, PROT_READ|PROT_WRITE, MAP_SHARED, fileno(file), 0);
		assert(piece != MAP_FAILED);
		if(runes){
			for(i = 0; i < chunk/sizeof(Rune); i++)
				((Rune*)piece)[i] = 0x1f600;
		}else
			memset(piece, 'a', chunk);
		assert(munmap(piece, chunk) == 0);
		for(offset = 0; offset < capacity; offset += chunk)
			assert(mmap(base+offset, chunk, PROT_READ, MAP_SHARED|MAP_FIXED,
				fileno(file), 0) == base+offset);
		/* All fixed replacements stay inside our reservation; only this page has a NUL. */
		lastpage = length*width/page*page;
		assert(mmap(base+lastpage, page, PROT_READ|PROT_WRITE,
			MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED, -1, 0) == base+lastpage);
		if(runes){
			for(i = 0; i < page/sizeof(Rune); i++)
				((Rune*)(base+lastpage))[i] = 0x1f600;
		}else
			memset(base+lastpage, 'a', page);
		memset(base+length*width, 0, width);
		memset(&f, 0, sizeof(f));
		memset(output, 0xa5, sizeof(output));
		f.start = f.to = output;
		f.stop = output+sizeof(output)-1;
		if(runes)
			assert(fmtrunestrcpy(&f, (Rune*)base) == -1);
		else
			assert(fmtstrcpy(&f, base) == -1);
		assert(f.to == output && (uchar)output[0] == 0xa5);
		/* A small explicit precision can still select a prefix of the same input. */
		f.flags = FmtPrec;
		f.prec = 1;
		if(runes)
			assert(fmtrunestrcpy(&f, (Rune*)base) == 0 && (char*)f.to-output == 4);
		else
			assert(fmtstrcpy(&f, base) == 0 && (char*)f.to-output == 1);
		assert(munmap(base, capacity) == 0 && fclose(file) == 0);
	}
	printf("UTF formatting: %lu-byte and Rune input lengths rejected before narrowing\n", length);
}

/* Compare real byte/Rune dispatch against independently generated Unicode text. */
int
main(int argc, char **argv)
{
	FILE *f;
	char format[128], raw[256], expected[1024], output[2048], *p;
	Rune input[256], want[1024], result[2048], scalar, *end;
	unsigned count, mode, length, nformat, ninput, nexpected, nrunewant, i, j;
	int n, cap;

	assert(argc == 2 || argc == 3);
	if(argc == 3){
		wide();
		return 0;
	}
	if(getenv("UTF_FORMAT_SKIP_SPANS") == nil)
		spans();
	quotefmtinstall();
	stream();
	f = fopen(argv[1], "rb");
	assert(f != nil);
	count = number(f);
	for(i = 0; i < count; i++){
		mode = number(f);
		length = number(f);
		nformat = number(f);
		ninput = number(f);
		nexpected = number(f);
		nrunewant = number(f);
		assert(length < sizeof(raw) && nformat < sizeof(format));
		assert(ninput < nelem(input) && nexpected < sizeof(expected) && nrunewant < nelem(want));
		assert(fread(raw, 1, length, f) == length);
		raw[length] = 0;
		assert(fread(format, 1, nformat, f) == nformat);
		format[nformat] = 0;
		for(j = 0; j < ninput; j++)
			input[j] = number(f);
		input[ninput] = 0;
		assert(fread(expected, 1, nexpected, f) == nexpected);
		expected[nexpected] = 0;
		for(j = 0; j < nrunewant; j++)
			want[j] = number(f);
		want[nrunewant] = 0;
		n = mode ? snprint(output, sizeof(output), format, input) :
			snprint(output, sizeof(output), format, raw);
		if(n != nexpected || strcmp(output, expected) != 0){
			fprintf(stderr, "vector %u format=%s got=%s expected=%s n=%d want=%u\n",
				i, format, output, expected, n, nexpected);
			abort();
		}
		end = mode ? runeseprint(result, result+nelem(result), format, input) :
			runeseprint(result, result+nelem(result), format, raw);
		assert(end == result+nrunewant && memcmp(result, want, (nrunewant+1)*sizeof(Rune)) == 0);
		for(cap = 1; cap < nexpected+3; cap++){
			memset(output, 0xa5, sizeof(output));
			if(mode)
				snprint(output, cap, format, input);
			else
				snprint(output, cap, format, raw);
			assert(memchr(output, 0, cap) != nil && (uchar)output[cap] == 0xa5);
			for(p = output; *p; ){
				n = chartorune(&scalar, p);
				assert(n != 1 || (uchar)*p < Runeself);
				p += n;
			}
		}
	}
	assert(fgetc(f) == EOF);
	fclose(f);
	printf("UTF formatting: %u independent byte/Rune and short-buffer vectors passed\n", count);
	return 0;
}
