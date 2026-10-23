#include "lib9.h"
#include <bio.h>
#include <assert.h>
#include <stdio.h>

static uchar sink[1024];
static int used, writes, failat, checks, maxwrite, zerowrite;
/* Inject a single interrupt without accepting bytes from the failed write. */
static long
outputwrite(int fd, void *data, long n)
{
	assert(fd == 17 && n > 0 && used+n <= sizeof(sink));
	writes++;
	if(writes == failat)return zerowrite ? 0 : -1;
	if(maxwrite > 0 && n > maxwrite)n = maxwrite;
	memmove(sink+used, data, n); used += n;
	return n;
}
/* Model exchange of an interruption error with the buffered writer. */
int errstr(char *p, uint n) { snprintf(p, n, "interrupted"); return 0; }
/* LeakSanitizer process inspection is unavailable in the sandbox. */
int __lsan_is_turned_off(void) { return 1; }
#define write outputwrite
IMPLEMENTATION
#undef write

/* Build exact-capacity output buffers with guard bytes. */
static void
setup(Biobuf *b, uchar *storage, int capacity)
{
	memset(b, 0, sizeof(*b)); memset(storage, 0xa5, capacity+2);
	b->bbuf = storage+1; b->ebuf = b->bbuf+capacity;
	b->bsize = capacity; b->ocount = -capacity;
	b->state = Bwactive; b->fid = 17;
	used = writes = maxwrite = zerowrite = 0; failat = -1;
}

/* Flushing an interrupted full buffer must retain every previously accepted byte. */
static void
interrupts(void)
{
	Biobuf b;
	uchar storage[34], data[128];
	int cap, fault, i, accepted;

	for(i = 0; i < sizeof(data); i++)data[i] = (uchar)(i*17+3);
	for(cap = 1; cap <= 32; cap++) for(fault = 1; fault <= 3; fault++){
		setup(&b, storage, cap); failat = fault;
		assert(Bwrite(&b, data, cap*4) == Beof);
		assert(b.state == Bwactive && b.ocount == 0);
		assert(used == (fault-1)*cap && b.offset == used);
		assert(Bflush(&b) == 0);
		accepted = fault*cap;
		assert(used == accepted && !memcmp(sink, data, accepted));
		assert(Bwrite(&b, data+accepted, cap*4-accepted) == cap*4-accepted);
		assert(Bflush(&b) == 0);
		assert(used == cap*4 && !memcmp(sink, data, used) && b.offset == used);
		assert(storage[0] == 0xa5 && storage[cap+1] == 0xa5); checks++;
	}
}

/* Read and inactive states never accept bytes, mutate guards or attempt flushes. */
static void
states(void)
{
	Biobuf b;
	uchar storage[18], saved[18], byte = 42;
	int state, pending;

	for(state = Binactive; state <= Bracteof; state++){
		if(state == Bwactive)continue;
		for(pending = -16; pending <= 0; pending++){
			setup(&b, storage, 16); b.state = state; b.ocount = pending;
			memmove(saved, storage, sizeof(storage));
			assert(Bputc(&b, byte) == Beof);
			assert(Bwrite(&b, &byte, 1) == Beof);
			assert(b.state == state && b.ocount == pending && writes == 0);
			assert(!memcmp(saved, storage, sizeof(storage))); checks++;
		}
	}
}

/* Ordinary byte writes retain the existing Bputc success result and payload order. */
static void
bytes(void)
{
	Biobuf b;
	uchar storage[10];
	int i;

	setup(&b, storage, 8);
	for(i = 0; i < 100; i++)assert(Bputc(&b, i) == 0);
	assert(Bflush(&b) == 0 && used == 100);
	for(i = 0; i < 100; i++)assert(sink[i] == i);
	checks++;
}

/* Short native writes must preserve byte order through each buffered API. */
static void
shortwrites(void)
{
	Biobuf b;
	uchar storage[34], data[128];
	int cap, chunk, i, api;

	for(i = 0; i < sizeof(data); i++)
		data[i] = i;
	for(cap = 2; cap <= 32; cap++)
	for(chunk = 1; chunk < cap; chunk++)
	for(api = 0; api < 2; api++){
		setup(&b, storage, cap);
		maxwrite = chunk;
		if(api == 0)
			assert(Bwrite(&b, data, sizeof(data)) == sizeof(data));
		else
			for(i = 0; i < sizeof(data); i++)
				assert(Bputc(&b, data[i]) == 0);
		assert(Bflush(&b) == 0 && b.state == Bwactive);
		assert(used == sizeof(data) && b.offset == used);
		assert(!memcmp(sink, data, sizeof(data)));
		assert(storage[0] == 0xa5 && storage[cap+1] == 0xa5);
		checks++;
	}
}

/* An interrupted partial write retains only its suffix, including zero progress. */
static void
shortinterrupts(void)
{
	Biobuf b;
	uchar storage[34], data[64];
	int cap, prefix, zero;

	memset(data, 0x63, sizeof(data));
	for(cap = 2; cap <= 32; cap++)
	for(prefix = 1; prefix < cap; prefix++)
	for(zero = 0; zero < 2; zero++){
		setup(&b, storage, cap);
		maxwrite = prefix;
		failat = 2;
		zerowrite = zero;
		assert(Bwrite(&b, data, cap+1) == Beof);
		assert(used == prefix && b.offset == prefix && b.ocount == -prefix);
		assert(b.state == Bwactive && !memcmp(b.bbuf, data+prefix, cap-prefix));
		assert(Bflush(&b) == 0 && used == cap && b.offset == cap);
		assert(Bwrite(&b, data+cap, 1) == 1 && Bflush(&b) == 0);
		assert(used == cap+1 && !memcmp(sink, data, cap+1));
		assert(storage[0] == 0xa5 && storage[cap+1] == 0xa5);
		checks++;
	}
}

/* A flush failure accounts for its accepted prefix and keeps the remaining bytes. */
static void
shortfailures(void)
{
	Biobuf b;
	uchar storage[18], data[16];
	int prefix, zero, i;

	for(i = 0; i < sizeof(data); i++)
		data[i] = i;
	for(prefix = 1; prefix < 16; prefix++)
	for(zero = 0; zero < 2; zero++){
		setup(&b, storage, 16);
		assert(Bwrite(&b, data, 16) == 16);
		maxwrite = prefix;
		failat = 2;
		zerowrite = zero;
		assert(Bflush(&b) == Beof && b.state == Binactive);
		assert(used == prefix && b.offset == prefix && b.ocount == -prefix);
		assert(!memcmp(b.bbuf, data+prefix, 16-prefix));
		assert(Bputc(&b, 0) == Beof && Bwrite(&b, data, 1) == Beof);
		assert(writes == 2);
		checks++;
	}
}

/* Exercise production buffer transitions with deterministic write/error boundaries. */
int
main(void)
{
	states(); interrupts(); bytes(); shortwrites(); shortinterrupts(); shortfailures();
	printf("PASS Bio output: %d state/interrupt/byte cases\n", checks);
	return 0;
}
