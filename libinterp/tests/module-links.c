#include "lib9.h"
#include "isa.h"
#include "interp.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static char message[ERRMAX];
static int live, checks, unloads;
Type Tmodlink = {1};
char exHeap[] = "heap";

/* Capture errors without substituting any lookup or signature decision. */
void kwerrstr(char *s, ...) { snprintf(message, sizeof(message), "%s", s); }
/* Keep intentional negative-case diagnostics out of the test log. */
static int quiet(char *fmt, ...) { (void)fmt; return 0; }
/* LeakSanitizer process inspection is unavailable in the sandbox. */
int __lsan_is_turned_off(void) { return 1; }

/* Supply exact Modlink storage; no VM heap failure is injected in this fixture. */
Heap*
nheap(int size)
{
	Heap *h;

	assert(size >= 0 && size < 4096);
	h = calloc(1, sizeof(Heap)+size); assert(h); h->ref = 1; live++;
	return h;
}

/* Verify the module reference transferred to each new Modlink is released once. */
void
destroy(void *p)
{
	Heap *h;
	Modlink *ml;

	if(p == H)return;
	h = D2H(p); assert(h->t == &Tmodlink && h->ref == 1);
	ml = p; assert(ml->MP == H && ml->m->ref > 0);
	ml->m->ref--; unloads++; Tmodlink.ref--; free(h); live--;
}

/* The tests request no MP copy or dynamic data allocation. */
void newdyndata(Modlink *ml) { (void)ml; abort(); }
void newmp(void *d, void *s, Type *t) { (void)d; (void)s; (void)t; abort(); }
void error(char *s) { (void)s; abort(); }
#define snprint snprintf
#define print quiet
IMPLEMENTATION
#undef print
#undef snprint

/* Missing exports must fail even when their signatures match the table sentinel. */
static void
lookup(void)
{
	static int sigs[] = {0, 1, -1, 0x7fffffff, (-2147483647-1), 0x12345678};
	static char *names[] = {"missing", "Alpha", "prefix", "prefix-more"};
	Module m;
	Link exports[4];
	Type frame = {1};
	Inst code[4];
	Import request;
	Modlink *ml;
	Modl saved[4];
	int i, j, slot, ref;

	memset(&m, 0, sizeof(m)); memset(exports, 0, sizeof(exports));
	m.ext = exports; m.name = "sample"; m.prog = code; m.ref = 1;
	for(i = 0; i < 3; i++){
		exports[i].name = i == 0 ? "alpha" : i == 1 ? "prefix-short" : "omega";
		exports[i].frame = &frame; exports[i].u.pc = &code[i];
	}
	/* Non-name sentinel fields deliberately resemble an otherwise valid export. */
	exports[3].frame = &frame; exports[3].u.pc = &code[3];
	ml = mklinkmod(&m, 4);
	for(i = 0; i < 4; i++){ ml->links[i].frame = &frame; ml->links[i].u.pc = &code[3-i]; }
	memmove(saved, ml->links, sizeof(saved)); ref = frame.ref;
	for(i = 0; i < sizeof(sigs)/sizeof(sigs[0]); i++){
		for(j = 0; j < 4; j++)exports[j].sig = sigs[i];
		for(slot = 0; slot < 4; slot++){
			request.sig = sigs[i]; request.name = names[slot]; message[0] = 0;
			assert(linkm(&m, ml, slot, &request) == -1);
			assert(strstr(message, "not implemented") != nil);
			assert(!memcmp(saved, ml->links, sizeof(saved)) && frame.ref == ref); checks++;
		}
		for(j = 0; j < 3; j++){
			request.sig = sigs[i]; request.name = exports[j].name;
			assert(linkm(&m, ml, j, &request) == 0);
			assert(ml->links[j].frame == exports[j].frame && ml->links[j].u.pc == &code[j]);
			memmove(ml->links, saved, sizeof(saved)); checks++;
			request.sig ^= 1; message[0] = 0;
			assert(linkm(&m, ml, j, &request) == -1);
			assert(strstr(message, "typecheck") != nil);
			assert(!memcmp(saved, ml->links, sizeof(saved))); checks++;
		}
	}
	m.ext = nil; request.name = "missing"; request.sig = 0;
	assert(linkm(&m, ml, 0, &request) == -1);
	assert(strstr(message, "not implemented") != nil && !memcmp(saved, ml->links, sizeof(saved)));
	checks++; destroy(ml); assert(live == 0 && m.ref == 0 && frame.ref == ref);
}

/* Public linking must destroy partial results after an unresolved later import. */
static void
publiclinks(void)
{
	Module m;
	Link exports[3];
	Import imports[3];
	Type frame = {1};
	Inst code[2];
	Modlink *ml;
	int missing, sig;

	memset(&m, 0, sizeof(m)); memset(exports, 0, sizeof(exports));
	memset(imports, 0, sizeof(imports));
	m.ext = exports; m.name = "sample"; m.prog = code;
	exports[0].name = "one"; exports[1].name = "two";
	exports[0].frame = exports[1].frame = &frame;
	exports[0].u.pc = &code[0]; exports[1].u.pc = &code[1];
	for(sig = 0; sig < 2; sig++) for(missing = 0; missing < 3; missing++){
		exports[0].sig = exports[1].sig = exports[2].sig = sig;
		imports[0].sig = imports[1].sig = sig;
		imports[0].name = missing == 1 ? "absent" : "one";
		imports[1].name = missing == 2 ? "absent" : "two";
		m.ref = 1; ml = linkmod(&m, imports, 0);
		if(missing)assert(ml == H);
		else{
			assert(ml != H && ml->nlinks == 2 && ml->MP == H);
			assert(ml->links[0].u.pc == &code[0] && ml->links[1].u.pc == &code[1]);
			destroy(ml);
		}
		assert(live == 0 && m.ref == 0 && frame.ref == 1 && Tmodlink.ref == 1); checks++;
	}
}

/* Exercise sentinel collisions, exact names, mismatches and full link cleanup. */
int
main(void)
{
	lookup(); publiclinks();
	assert(live == 0 && Tmodlink.ref == 1 && unloads == 7);
	printf("Module links: %d cases\n", checks);
	return 0;
}
