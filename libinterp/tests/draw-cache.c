#include "lib9.h"
#include "draw.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>

#define BIHASH BIHASH_VALUE
static void *cacheqlock;
static int live, calls, failat, locked, checks;

/* Keep allocator instrumentation without unsupported process enumeration. */
int __lsan_is_turned_off(void) { return 1; }

/* Count successful cache allocations and fail a selected allocation. */
static void*
cachemalloc(ulong n)
{
	void *p;
	if(++calls == failat)
		return nil;
	p = malloc(n);
	assert(p != nil);
	live++;
	return p;
}

/* Release records and copied names through the same ownership counter. */
static void
cachefree(void *p)
{
	if(p != nil)
		live--;
	free(p);
}

/* Name duplication must fail independently from cache-record allocation. */
static char*
cachedup(char *s)
{
	char *p = cachemalloc(strlen(s)+1);
	if(p != nil)
		strcpy(p, s);
	return p;
}

/* Enforce balanced cache locking and leave logging outside the fixture. */
void libqlock(void *p) { assert(p == cacheqlock && locked++ == 0); }
void libqunlock(void *p) { assert(p == cacheqlock && --locked == 0); }
int print(char *fmt, ...) { USED(fmt); return 0; }

#define malloc cachemalloc
#define free cachefree
#define strdup cachedup
IMPLEMENTATION
#undef malloc
#undef free
#undef strdup

/* Compute only the relevant low sixteen bits using wider unsigned arithmetic. */
static int
oracle(char *s)
{
	uint64_t h = 0;
	while(*s){
		h = ((h+(unsigned char)*s++)*2)&0xffff;
		if(h & 256)
			h++;
	}
	return h%BIHASH;
}

/* Check long/high-byte names, record publication and display-specific collisions. */
int
main(void)
{
	Cache *table[BIHASH], *c, *other;
	Display displays[2];
	char name[4097];
	int n, failure, j;

	memset(name, 0xff, sizeof name-1);
	name[sizeof name-1] = 0;
	assert(drawhash(name) == oracle(name));
	checks++;
	memset(table, 0, sizeof table);
	memset(displays, 0, sizeof displays);
	for(n = 0; n <= 4096; n += n < 70 ? 1 : 137){
		for(j = 0; j < n; j++)name[j] = 1+(j*71+n)%255;
		name[n] = 0;
		assert(drawhash(name) == oracle(name));
		for(failure = 1; failure <= 2; failure++){
			calls = 0; failat = failure;
			assert(cacheinstall(table, &displays[0], name, (void*)17, "font") == nil);
			assert(cachelookup(table, &displays[0], name) == nil);
			assert(live == 0 && locked == 0);
			checks++;
		}
		failat = 0;
		c = cacheinstall(table, &displays[0], name, (void*)17, "font");
		assert(c != nil && c->ref == 0 && c->u.ptr == (void*)17);
		assert(c->name != name && strcmp(c->name, name) == 0);
		assert(cachelookup(table, &displays[0], name) == c);
		assert(cacheinstall(table, &displays[0], name, (void*)18, "font") == nil);
		assert(live == 2 && locked == 0);
		other = cacheinstall(table, &displays[1], name, (void*)19, "font");
		assert(other != nil && other != c && live == 4);
		cacheuninstall(table, &displays[0], name, "font");
		assert(cachelookup(table, &displays[0], name) == nil);
		assert(cachelookup(table, &displays[1], name) == other && live == 2);
		cacheuninstall(table, &displays[1], name, "font");
		cacheuninstall(table, &displays[1], name, "font");
		assert(live == 0 && locked == 0);
		checks++;
	}
	printf("draw cache: %d checks passed\n", checks);
	return 0;
}
