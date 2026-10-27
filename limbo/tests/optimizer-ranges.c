#include "limbo.h"
#include <limits.h>
#include <stdio.h>
#include <sys/wait.h>

static unsigned long checks; // Number of independent range or diagnostic cases.
static int diagnostic; // Instruction dumps observed before missing-declaration errors.


/* This stack-only fixture needs no leak scan or unavailable /proc thread metadata. */
int
__lsan_is_turned_off(void)
{
	return 1;
}


/* Keep regression predicates active under NDEBUG without an assert dependency. */
static void
rangecheck(int condition)
{
	if(!condition)
		abort();
}


/* Fatal compiler errors terminate the child before any invalid use/def publication. */
void
fatal(char *format, ...)
{
	rangecheck(format != nil);
	_Exit(73);
}


/* Observe the diagnostic boundary without depending on instruction rendering. */
void
pins(Inst *instruction)
{
	rangecheck(instruction != nil);
	diagnostic++;
}

IMPLEMENTATION


/* Compare valid declaration ranges with independent bounded endpoint arithmetic. */
static void
ordinary(void)
{
	Decl declarations[8];
	Type types[8];
	Inst instruction = {0};
	Addr address = {0};
	Desc descriptor = {0};
	long ends[8], base, cursor, start, stop;
	int seed, n, j, selected, count, expected, width;

	for(seed = 0; seed < 2048; seed++){
		memset(declarations, 0, sizeof(declarations));
		memset(types, 0, sizeof(types));
		base = seed&1 ? 0x7fffffff-512 : 0;
		cursor = base;
		for(n = 0; n < 8; n++){
			width = (seed*7+n*13)%29;
			declarations[n].offset = cursor;
			declarations[n].ty = &types[n];
			declarations[n].next = n == 7 ? nil : &declarations[n+1];
			types[n].size = width;
			ends[n] = cursor+width;
			cursor += width+(seed+n)%4;
		}
		for(selected = 0; selected < 8; selected++){
			if(types[selected].size == 0)
				continue;
			start = declarations[selected].offset+(seed%types[selected].size);
			for(j = selected; j < 8; j++){
				stop = ends[j];
				expected = 1;
				while(ends[selected+expected-1] < stop)
					expected++;
				count = -991;
				rangecheck(finddec(start, stop-start, declarations, &count, &instruction) == selected && count == expected);
				checks++;
			}
			count = -991;
			rangecheck(finddec(start, 0, declarations, &count, &instruction) == selected && count == 1);
			checks++;
		}
		address.decl = &declarations[0];
		for(n = 0; n < 4; n++){
			address.reg = n == 0 ? -base : n == 1 ? 0 : n == 2 ? 0x7fffffff-base : -base+1;
			rangecheck(fpoff(Afp, &address) == base+address.reg && fpoff(Afpind, &address) == base+address.reg);
			rangecheck(fpoff(Aimm, &address) == -1);
			checks++;
		}
	}
	declarations[0].offset = 0x7fffffff-16;
	declarations[0].next = nil;
	types[0].size = 16;
	count = -991;
	rangecheck(finddec(0x7fffffff-16, 16, declarations, &count, &instruction) == 0 && count == 1);
	rangecheck(finddec(0x7fffffff-1, 1, declarations, &count, &instruction) == 0 && count == 1);
	address.decl = nil;
	address.reg = 0x7fffffff;
	rangecheck(fpoff(Afp, &address) == 0x7fffffff);
	checks += 3;
	for(n = 0; n < 3; n++){
		width = n == 0 ? 0 : n == 1 ? 1 : 0x7fffffff;
		address.offset = width;
		rangecheck(imm(Aimm, &address) == width);
		descriptor.size = width;
		declarations[0].desc = &descriptor;
		address.decl = &declarations[0];
		rangecheck(desc(Adesc, &address) == width);
		instruction.op = ISEND;
		instruction.m.offset = width;
		rangecheck(size(&instruction) == width);
		instruction.op = IMOVM;
		instruction.mm = Aimm;
		rangecheck(size(&instruction) == width);
		instruction.op = IMOVMP;
		instruction.mm = Adesc;
		instruction.m.decl = &declarations[0];
		rangecheck(size(&instruction) == width);
		checks++;
	}
}


/* Reject overflow, missing tails, absent descriptors and host-long narrowing. */
static void
invalid(int which)
{
	Decl declarations[2] = {{0}};
	Type types[2] = {{0}};
	Inst instruction = {0};
	Desc descriptor = {0};
	Addr address = {0};
	int n, count = -991;

	for(n = 0; n < 2; n++){
		declarations[n].ty = &types[n];
		declarations[n].offset = n*16;
		types[n].size = 16;
	}
	declarations[0].next = &declarations[1];
	declarations[0].desc = &descriptor;
	address.decl = &declarations[0];
	switch(which){
	case 0: finddec(8, 25, declarations, &count, &instruction); break;
	case 1: finddec(8, 0x7fffffff, declarations, &count, &instruction); break;
	case 2: finddec(8, -1, declarations, &count, &instruction); break;
	case 3: finddec(-1, 1, declarations, &count, &instruction); break;
	case 4: declarations[0].ty = nil; finddec(0, 1, declarations, &count, &instruction); break;
	case 5: declarations[0].offset = 1; types[0].size = LONG_MAX; finddec(1, 1, declarations, &count, &instruction); break;
	case 6: declarations[0].offset = LONG_MAX; types[0].size = 16; finddec(1, 1, declarations, &count, &instruction); break;
	case 7: types[1].size = -1; finddec(8, 24, declarations, &count, &instruction); break;
	case 8: declarations[1].ty = nil; finddec(8, 24, declarations, &count, &instruction); break;
	case 9: address.reg = LONG_MAX; declarations[0].offset = 1; fpoff(Afp, &address); break;
	case 10: address.reg = LONG_MIN; declarations[0].offset = 1; fpoff(Afpind, &address); break;
	case 11: address.reg = 1; declarations[0].offset = LONG_MAX; fpoff(Afp, &address); break;
	case 12: declarations[0].offset = -1; fpoff(Afp, &address); break;
	case 13: address.reg = -1; fpoff(Afp, &address); break;
	case 14: address.offset = LONG_MAX; imm(Aimm, &address); break;
	case 15: address.offset = -1; imm(Aimm, &address); break;
	case 16: descriptor.size = LONG_MAX; desc(Adesc, &address); break;
	case 17: descriptor.size = -1; desc(Adesc, &address); break;
	case 18: address.decl = nil; desc(Adesc, &address); break;
	case 19: declarations[0].desc = nil; desc(Adesc, &address); break;
	case 20: instruction.op = ILEA; instruction.m.offset = LONG_MAX; size(&instruction); break;
	case 21: instruction.op = IALT; instruction.m.offset = -1; size(&instruction); break;
	case 22: finddec(0, 1, nil, &count, &instruction); break;
	default: abort();
	}
	_Exit(17);
}


/* Isolate fatal diagnostics so ASan retains normal stack instrumentation. */
int
main(void)
{
	int which, status, selected;
	pid_t child;

	selected = getenv("OPTIM_RANGE_CASE") ? atoi(getenv("OPTIM_RANGE_CASE")) : -1;
	if(selected < 0)
		ordinary();
	for(which = 0; which < 23; which++){
		if(LONG_MAX == 0x7fffffff && (which == 14 || which == 16 || which == 20))
			continue;
		if(selected >= 0 && which != selected)
			continue;
		child = fork();
		rangecheck(child >= 0);
		if(child == 0)
			invalid(which);
		rangecheck(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 73);
		checks++;
	}
	printf("PASS optimizer byte ranges WORD%d: %lu cases\n", IBY2WD*8, checks);
	return 0;
}
