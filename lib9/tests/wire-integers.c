#include "lib9.h"
#include "fcall.h"
#include <assert.h>
#include <stdio.h>

IMPLEMENTATION

/* Disable only unavailable LeakSanitizer process inspection. */
int __lsan_is_turned_off(void) { return 1; }

/* Compare little-endian decoding against multiplication of unsigned values. */
static void
checkbytes(uchar *p)
{
	uvlong expected, scale;
	int i;

	expected = 0;
	scale = 1;
	for(i = 0; i < 8; i++){
		expected += p[i]*scale;
		if(i == 3)
			assert(GBIT32(p) == expected);
		scale *= 256;
	}
	assert(GBIT64(p) == expected);
}

/* Exercise unaligned high bytes and real stat encode/decode bodies. */
int
main(void)
{
	uchar storage[16], buf[256], *p;
	char strings[256];
	Dir d, result;
	u32int random;
	uint n;
	int i, j, k, cases;

	cases = 0;
	p = storage+1;
	for(i = 0; i < 8; i++) for(j = 0; j < 256; j++){
		memset(storage, 0, sizeof(storage));
		p[i] = j;
		checkbytes(p);
		cases++;
	}
	random = 0x12345678;
	for(i = 0; i < 65536; i++){
		for(k = 0; k < 8; k++){
			random = random*1664525U + 1013904223U;
			p[k] = random>>24;
		}
		checkbytes(p);
		cases++;
		memset(&d, 0, sizeof(d));
		d.type = 0xffff; d.dev = GBIT32(p);
		d.qid.type = 0xff; d.qid.vers = GBIT32(p+4);
		d.qid.path = GBIT64(p);
		d.mode = d.atime = d.mtime = GBIT32(p);
		d.length = GBIT64(p) & 0x7fffffffffffffffULL;
		if(i & 1)
			d.length = -1-d.length;
		d.name = "name"; d.uid = "user"; d.gid = "group"; d.muid = "modifier";
		n = sizeD2M(&d);
		assert(n <= sizeof(buf) && convD2M(&d, buf, n) == n);
		assert(statcheck(buf, n) == 0);
		memset(&result, 0, sizeof(result));
		assert(convM2D(buf, n, &result, strings) == n);
		assert(result.type == d.type && result.dev == d.dev);
		assert(result.qid.type == d.qid.type && result.qid.vers == d.qid.vers);
		assert(result.qid.path == d.qid.path && result.length == d.length);
		assert(result.mode == d.mode && result.atime == d.atime && result.mtime == d.mtime);
		assert(!strcmp(result.name, d.name) && !strcmp(result.uid, d.uid));
		assert(!strcmp(result.gid, d.gid) && !strcmp(result.muid, d.muid));
	}
	printf("wire integer decoding: %d vectors, 65536 stat round trips\n", cases);
	return 0;
}
