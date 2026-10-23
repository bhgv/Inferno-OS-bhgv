#include "lib9.h"
#include <assert.h>
#include <stdio.h>
#include <limits.h>
#include <sys/mman.h>

static int checks;

void *native_memmove(void*, void*, ulong);
void *native_memcpy(void*, void*, ulong);
void *native_memset(void*, int, ulong);
char *native_strncpy(char*, char*, long);
char *native_strchr(char*, int);
char *native_strrchr(char*, int);

#define memmove native_memmove
#define memcpy native_memcpy
#define memset native_memset
#define strncpy native_strncpy
#define strchr native_strchr
#define strrchr native_strrchr
IMPLEMENTATION
#undef memmove
#undef memcpy
#undef memset
#undef strncpy
#undef strchr
#undef strrchr

/* Copy through independent scratch storage to model every overlap direction. */
static void
moves(void)
{
	uchar data[96], want[96], temporary[96], source[96];
	int from, to, n, i;

	for(from = 0; from <= 32; from++)
	for(to = 0; to <= 32; to++)
	for(n = 0; n <= 64; n++){
		for(i = 0; i < sizeof(data); i++)
			data[i] = want[i] = source[i] = (uchar)(i*37+5);
		for(i = 0; i < n; i++)
			temporary[i] = want[from+i];
		for(i = 0; i < n; i++)
			want[to+i] = temporary[i];
		assert(native_memmove(data+to, data+from, n) == data+to);
		assert(memcmp(data, want, sizeof(data)) == 0);
		for(i = 0; i < sizeof(data); i++)
			data[i] = want[i] = 0xa5;
		for(i = 0; i < n; i++)
			want[to+i] = source[from+i];
		assert(native_memcpy(data+to, source+from, n) == data+to);
		assert(memcmp(data, want, sizeof(data)) == 0);
		checks++;
	}
	assert(native_memmove(nil, nil, 0) == nil);
}

/* Fill guards, offsets and all byte values, including integer narrowing. */
static void
fills(void)
{
	uchar data[96], want[96];
	int c, to, n, i;

	for(c = -256; c <= 511; c++)
	for(to = 0; to <= 8; to++)
	for(n = 0; n <= 64; n++){
		for(i = 0; i < sizeof(data); i++)
			data[i] = want[i] = (uchar)(i*37+5);
		for(i = 0; i < n; i++)
			want[to+i] = (uchar)c;
		assert(native_memset(data+to, c, n) == data+to);
		assert(memcmp(data, want, sizeof(data)) == 0);
		checks++;
	}
	assert(native_memset(nil, INT_MIN, 0) == nil);
}

/* Check byte truncation, high-bit characters and first/last/NUL positions. */
static void
searches(void)
{
	char input[9];
	char *first, *last;
	int byte, c, i, extras[] = {INT_MIN, INT_MAX, -65536, 65536};

	for(byte = 0; byte < 256; byte++){
		input[0] = byte;
		input[1] = 'a';
		input[2] = byte;
		input[3] = (uchar)(byte+1);
		input[4] = byte;
		input[5] = 0;
		for(c = -1024; c <= 1024; c++){
			first = last = nil;
			for(i = 0;; i++){
				if((uchar)input[i] == (uchar)c){
					if(first == nil)
						first = input+i;
					last = input+i;
				}
				if(input[i] == 0)
					break;
			}
			assert(native_strchr(input, c) == first);
			assert(native_strrchr(input, c) == last);
			checks++;
		}
	}
	for(i = 0; i < nelem(extras); i++){
		input[0] = (uchar)extras[i];
		input[1] = 0;
		assert(native_strchr(input, extras[i]) == input);
		assert(native_strrchr(input, extras[i]) == input);
		checks++;
	}
}

/* Check exact padding/truncation, untouched guards and signed native bounds. */
static void
stringcopies(void)
{
	char input[66], data[96], want[96];
	int length, to, n, i;
	long empty[] = {LONG_MIN, INT_MIN, -1, 0};

	for(length = 0; length <= 64; length++)
	for(to = 0; to <= 8; to++)
	for(n = 0; n <= 64; n++){
		for(i = 0; i < length; i++)
			input[i] = (uchar)(i%255+1);
		input[length] = 0;
		for(i = 0; i < sizeof(data); i++)
			data[i] = want[i] = (char)0xa5;
		for(i = 0; i < n; i++)
			want[to+i] = i < length ? input[i] : 0;
		assert(native_strncpy(data+to, input, n) == data+to);
		assert(memcmp(data, want, sizeof(data)) == 0);
		checks++;
	}
	for(i = 0; i < nelem(empty); i++){
		assert(native_strncpy(nil, nil, empty[i]) == nil);
		checks++;
	}
}

/* Create one shared backing chunk reused by a large valid virtual byte span. */
static FILE*
backing(ulong chunk, int byte)
{
	FILE *file;
	uchar *p;

	file = tmpfile();
	assert(file != nil && ftruncate(fileno(file), chunk) == 0);
	p = mmap(nil, chunk, PROT_READ|PROT_WRITE, MAP_SHARED, fileno(file), 0);
	assert(p != MAP_FAILED);
	memset(p, byte, chunk);
	assert(munmap(p, chunk) == 0);
	return file;
}

/* Map aliases only inside the address reservation owned by this fixture. */
static void
aliases(uchar *base, ulong capacity, ulong chunk, FILE *file)
{
	ulong offset;

	for(offset = 0; offset < capacity; offset += chunk)
		assert(mmap(base+offset, chunk, PROT_READ|PROT_WRITE,
			MAP_SHARED|MAP_FIXED, fileno(file), 0) == base+offset);
}

/* Inspect an entire backing chunk so a narrowed prefix cannot pass the check. */
static void
uniform(uchar *p, ulong n, int byte)
{
	ulong i;

	for(i = 0; i < n; i++)
		assert(p[i] == byte);
}

/* Run >INT_MAX operations with a few MiB of backing, without reducing counts. */
static void
wide(char *only)
{
	ulong n, capacity, chunk;
	uchar *base, *low, *high;
	FILE *source, *destination;
	int operations;

	if(sizeof(ulong) <= sizeof(int)){
		puts("wide spans require a native LP64 host");
		return;
	}
	n = (ulong)INT_MAX+34;
	chunk = 2*1024*1024;
	capacity = (n/chunk+1)*chunk;
	base = mmap(nil, 2*capacity, PROT_NONE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	assert(base != MAP_FAILED);
	low = base;
	high = base+capacity;
	source = backing(chunk, 0x35);
	destination = backing(chunk, 0xa5);
	aliases(low, capacity, chunk, source);
	aliases(high, capacity, chunk, destination);
	operations = 0;
	if(only == nil || strcmp(only, "move-backward") == 0){
		assert(native_memmove(high, low, n) == high);
		uniform(high, chunk, 0x35);
		operations++;
	}
	if(only == nil || strcmp(only, "copy") == 0){
		memset(high, 0xa5, chunk);
		assert(native_memcpy(high, low, n) == high);
		uniform(high, chunk, 0x35);
		operations++;
	}
	if(only == nil || strcmp(only, "move-forward") == 0){
		memset(high, 0xa5, chunk);
		assert(native_memmove(low, high, n) == low);
		uniform(low, chunk, 0xa5);
		operations++;
	}
	if(only == nil || strcmp(only, "fill") == 0){
		assert(native_memset(high, 0x17a, n) == high);
		uniform(high, chunk, 0x7a);
		operations++;
	}
	if(only == nil || strcmp(only, "string-copy") == 0){
		memset(low, 0x35, chunk);
		memset(high, 0xa5, chunk);
		assert(native_strncpy((char*)high, (char*)low, n) == (char*)high);
		uniform(high, chunk, 0x35);
		operations++;
	}
	if(only == nil || strcmp(only, "string-pad") == 0){
		memset(high, 0xa5, chunk);
		assert(native_strncpy((char*)high, "", n) == (char*)high);
		uniform(high, chunk, 0);
		operations++;
	}
	assert(munmap(base, 2*capacity) == 0);
	assert(fclose(source) == 0 && fclose(destination) == 0);
	printf("portable memory: %d operations of %lu bytes passed\n", operations, n);
}

/* Select small exhaustive cases or the optional real native-width spans. */
int
main(int argc, char **argv)
{
	if(argc > 1 && strcmp(argv[1], "wide") == 0){
		wide(argc > 2 ? argv[2] : nil);
		return 0;
	}
	if(argc == 1 || strcmp(argv[1], "moves") == 0)
		moves();
	if(argc == 1 || strcmp(argv[1], "fills") == 0)
		fills();
	if(argc == 1 || strcmp(argv[1], "searches") == 0)
		searches();
	if(argc == 1 || strcmp(argv[1], "strings") == 0)
		stringcopies();
	printf("portable memory/search: %d scenarios passed\n", checks);
	return 0;
}
