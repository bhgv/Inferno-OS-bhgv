#include "lib9.h"
#include "isa.h"
#include "interp.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>

static ulong *patch, *tinit, *base;

IMPLEMENTATION

/* Reject bad public input without modifying module storage or compiler globals. */
int
main(void)
{
	Module m, before, *input;
	Modlink ml, *instance;
	Inst instructions[2] = {{0}};
	Link exports[1] = {{0}};
	Type descriptor = {0}, *table[] = {&descriptor};
	int mode, size, checks = 0;

	for(mode = 0; mode < 19; mode++){
		memset(&m, 0, sizeof(m)); memset(&ml, 0, sizeof(ml));
		m.prog = instructions; m.nprog = 2; m.entry = instructions;
		m.ext = exports; m.ntype = 1; m.type = table;
		input = &m; instance = &ml; size = 2;
		switch(mode){
		case 0: input = nil; break;
		case 1: input = H; break;
		case 2: size = INT_MIN; break;
		case 3: size = 0; break;
		case 4: size = 1; break;
		case 5: m.compiled = 1; break;
		case 6: m.prog = nil; break;
		case 7: m.prog = H; break;
		case 8: m.entry = nil; break;
		case 9: m.entry = H; break;
		case 10: m.ext = nil; break;
		case 11: m.ext = H; break;
		case 12: m.ntype = -1; break;
		case 13: m.type = nil; break;
		case 14: m.type = H; break;
		case 15: size = m.nprog = INT_MAX; break;
		case 16: m.ntype = INT_MAX; break;
		case 17: instance = H; break;
		case 18: ml.nlinks = -1; break;
		}
		before = m;
		patch = (ulong*)1; tinit = (ulong*)2; base = (ulong*)3;
		assert(compile(input, size, instance) == 0);
		assert(!memcmp(&before, &m, sizeof(m)));
		assert(patch == (ulong*)1 && tinit == (ulong*)2 && base == (ulong*)3);
		checks++;
	}
	memset(&m, 0, sizeof(m));
	m.prog = instructions; m.nprog = 2; m.entry = instructions;
	m.ext = exports; m.ntype = 1; m.type = table;
	assert(compile(&m, 2, nil) == 1); checks++;
	m.ntype = 0; m.type = nil;
	assert(compile(&m, 2, nil) == 1); checks++;
	printf("legacy JIT input: %d cases, WORD=%zu\n", checks, sizeof(WORD));
	return 0;
}
