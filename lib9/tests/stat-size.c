#include "lib9.h"
#include "fcall.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

IMPLEMENTATION

static int checks;
static char *text[4];
static uchar *wire;

/* Disable only unsupported LeakSanitizer process inspection. */
int __lsan_is_turned_off(void) { return 1; }

/* Test exact and short output spans, rejection, wire size and decoded strings. */
static void
check(uint lengths[4], int nullmask)
{
	Dir d, got;
	char *strings;
	uint total, size, caps[8], cap, i, j;

	memset(&d, 0, sizeof(d));
	total = 49;	/* Fixed 9P stat fields, including four string counts. */
	assert(STATFIXLEN == total);
	for(i = 0; i < 4; i++){
		memset(text[i], 'a'+i, lengths[i]); text[i][lengths[i]] = 0;
		if((nullmask & (1<<i)) == 0)total += lengths[i];
	}
	d.name = nullmask&1 ? nil : text[0];
	d.uid = nullmask&2 ? nil : text[1];
	d.gid = nullmask&4 ? nil : text[2];
	d.muid = nullmask&8 ? nil : text[3];
	size = total <= 65537 ? total : 0;
	assert(sizeD2M(&d) == size);
	assert(convD2M(&d, nil, 0) == 0);
	caps[0] = 0; caps[1] = 1; caps[2] = 2; caps[3] = 48;
	caps[4] = 49; caps[5] = total-1; caps[6] = total; caps[7] = total+7;
	for(i = 0; i < 8; i++){
		cap = caps[i];
		memset(wire, 0xa5, total+10);
		j = convD2M(&d, wire+1, cap);
		if(size == 0 || cap < 2){
			assert(j == 0);
			for(j = 0; j < total+10; j++)assert(wire[j] == 0xa5);
		}else if(cap < total){
			assert(j == 2 && GBIT16(wire+1) == total-2);
			assert(wire[0] == 0xa5);
			for(j = 3; j < total+10; j++)assert(wire[j] == 0xa5);
		}else{
			assert(j == total && GBIT16(wire+1) == total-2);
			assert(wire[0] == 0xa5 && wire[total+1] == 0xa5);
			assert(statcheck(wire+1, total) == 0);
			strings = malloc(total+4); assert(strings);
			assert(convM2D(wire+1, total, &got, strings) == total);
			assert(!strcmp(got.name, d.name == nil ? "" : d.name));
			assert(!strcmp(got.uid, d.uid == nil ? "" : d.uid));
			assert(!strcmp(got.gid, d.gid == nil ? "" : d.gid));
			assert(!strcmp(got.muid, d.muid == nil ? "" : d.muid));
			free(strings);
		}
		checks++;
	}
}

/* Reject malformed first records without modifying scalar or string destinations. */
static void
malformed(void)
{
	Dir d = {0}, got, before;
	uchar valid[128], bad[256];
	char output[256], saved[256];
	uint n, cap, field, offset, length, declared;

	d.name = "name"; d.uid = "owner"; d.gid = "group"; d.muid = "last";
	n = convD2M(&d, valid, sizeof(valid));
	assert(n > STATFIXLEN && n < 128);
	for(cap = 0; cap < n; cap++){
		memset(&got, 0xa5, sizeof(got)); before = got;
		memset(output, 0x5a, sizeof(output)); memmove(saved, output, sizeof(saved));
		assert(convM2D(valid, cap, &got, output) == 0);
		assert(!memcmp(&got, &before, sizeof(got)) && !memcmp(output, saved, sizeof(output)));
		checks++;
	}
	for(declared = 0; declared <= n+3; declared++){
		if(declared == n)continue;
		memset(bad, 0, sizeof(bad)); memmove(bad, valid, n);
		PBIT16(bad, declared < 2 ? 0 : declared-2);
		memset(&got, 0xa5, sizeof(got)); before = got;
		memset(output, 0x5a, sizeof(output)); memmove(saved, output, sizeof(saved));
		assert(convM2D(bad, sizeof(bad), &got, output) == 0);
		assert(!memcmp(&got, &before, sizeof(got)) && !memcmp(output, saved, sizeof(output)));
		checks++;
	}
	offset = STATFIXLEN-4*BIT16SZ;
	for(field = 0; field < 4; field++){
		memmove(bad, valid, n); length = GBIT16(bad+offset);
		bad[offset] = bad[offset+1] = 255;
		memset(&got, 0xa5, sizeof(got)); before = got;
		memset(output, 0x5a, sizeof(output)); memmove(saved, output, sizeof(saved));
		assert(statcheck(bad, n) == -1 && convM2D(bad, n, &got, output) == 0);
		assert(!memcmp(&got, &before, sizeof(got)) && !memcmp(output, saved, sizeof(output)));
		offset += BIT16SZ+length;
		checks++;
	}
	/* A caller may provide consecutive records; decode only the first declaration. */
	memmove(bad, valid, n); memmove(bad+n, valid, n);
	assert(convM2D(bad, 2*n, &got, output) == n && !strcmp(got.name, "name"));
	assert(convM2D(valid, n, &got, nil) == n && !strcmp(got.name, ""));
	assert(convM2D(nil, n, &got, output) == 0);
	assert(convM2D(valid, n, nil, output) == 0);
	assert(statcheck(nil, n) == -1 && statcheck(valid, n+1) == -1);
	checks += 5;
}

/* Cover the whole-entry boundary, each string position and aggregate lengths. */
int
main(void)
{
	uint lengths[4], total, seed, left;
	int i, j, delta, mask;

	malformed();
	for(i = 0; i < 4; i++){
		text[i] = malloc(65540); assert(text[i]);
	}
	wire = malloc(270000); assert(wire);
	for(i = 0; i < 4; i++) for(delta = -4; delta <= 4; delta++){
		memset(lengths, 0, sizeof(lengths));
		lengths[i] = 65537-49+delta;
		check(lengths, 0);
	}
	for(i = 0; i < 4; i++){
		memset(lengths, 0, sizeof(lengths)); lengths[i] = 65536;
		check(lengths, 0);
	}
	for(mask = 0; mask < 16; mask++){
		for(i = 0; i < 4; i++)lengths[i] = 16373;
		check(lengths, mask);
	}
	seed = 0x12345678;
	for(j = 0; j < 256; j++){
		total = 65537-49+(j%9)-4; left = total;
		for(i = 0; i < 3; i++){
			seed = seed*1664525U+1013904223U;
			lengths[i] = seed%(left+1); left -= lengths[i];
		}
		lengths[3] = left; check(lengths, 0);
	}
	for(i = 0; i < 4; i++)free(text[i]);
	free(wire);
	printf("stat size limits: %d output-span cases\n", checks);
	return 0;
}
