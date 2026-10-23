#include "lib9.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>

/* gettokens is exported by tokenize.c but absent from the public headers. */
int gettokens(char*, char**, int, char*);

/* Read bounded, little-endian fixture integers independently of host long size. */
static unsigned
number(FILE *f)
{
	uchar b[4];

	assert(fread(b, 1, 4, f) == 4);
	return (unsigned)b[0] | (unsigned)b[1]<<8 | (unsigned)b[2]<<16 | (unsigned)b[3]<<24;
}

/* Replay oracle vectors and check token values, offsets and untouched guards. */
int
main(int argc, char **argv)
{
	FILE *f;
	uchar buffer[512];
	char sep[128], expected[256], *args[10];
	unsigned count, mode, max, length, nsep, narg, offset, n, i, j;
	int actual;

	assert(argc == 2);
	f = fopen(argv[1], "rb");
	assert(f != nil);
	count = number(f);
	for(i = 0; i < count; i++){
		mode = number(f);
		max = number(f);
		length = number(f);
		nsep = number(f);
		assert(length < 256 && nsep < sizeof(sep) && max <= 6);
		memset(buffer, 0xa5, sizeof(buffer));
		assert(fread(buffer+16, 1, length, f) == length);
		buffer[16+length] = 0;
		assert(fread(sep, 1, nsep, f) == nsep);
		sep[nsep] = 0;
		for(j = 0; j < nelem(args); j++)
			args[j] = (char*)buffer;
		if(mode)
			actual = tokenize((char*)buffer+16, args+1, max);
		else
			actual = gettokens((char*)buffer+16, args+1, max, sep);
		narg = number(f);
		assert(actual == narg);
		for(j = 0; j < narg; j++){
			offset = number(f);
			n = number(f);
			assert(n < sizeof(expected));
			assert(fread(expected, 1, n, f) == n);
			expected[n] = 0;
			assert(args[j+1] >= (char*)buffer+16 && args[j+1] <= (char*)buffer+16+length);
			if(!mode)
				assert(args[j+1] == (char*)buffer+16+offset);
			assert(strcmp(args[j+1], expected) == 0);
		}
		for(j = narg+1; j < nelem(args); j++)
			assert(args[j] == (char*)buffer);
		assert(args[0] == (char*)buffer);
		for(j = 0; j < 16; j++)
			assert(buffer[j] == 0xa5);
		for(j = 17+length; j < sizeof(buffer); j++)
			assert(buffer[j] == 0xa5);
	}
	assert(fgetc(f) == EOF);
	fclose(f);
	printf("tokenization: %u oracle vectors passed\n", count);
	return 0;
}
