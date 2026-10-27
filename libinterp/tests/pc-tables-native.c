#include "comp-amd64.c"
#include "tests/leaf-cons-stub.h"
#include "tests/leaf-plain-stubs.h"
/* Supply the builtin identity used by guarded numeric formatting leaves. */
Type Tstring = {1}, Tarray = {1};
#include <fenv.h>
#include <setjmp.h>
#include <stdio.h>

REG R;
/* Supply the live collector color used by guarded owner leaves. */
int mutator, nprop;
char exModule[]="module",exBounds[]="bounds";
static char *raised;
static jmp_buf boundary;
static void (*operation)(void);

/* Compile owned metadata without starting a full emulator process. */
void*
mallocz(ulong size,int clear)
{
	void *p=malloc(size);
	if(p && clear)
		memset(p,0,size);
	return p;
}


/* Valid module compilation uses the real callback without injected failures here. */
int
kprotect(int (*callback)(void*),void *argument)
{
	return callback(argument);
}


/* Observe the exact VM exception while preserving all register and data images. */
void
error(char *message)
{
	raised=message;
	longjmp(boundary,1);
}


/* Retain stack/address instrumentation without unavailable leak-process inspection. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

#define OP(name) void name(void) // Extracted production table and program-address operators.
IMPLEMENTATION


/* Keep VM exceptions outside the test matrix's automatic variables. */
static int
invoke(void)
{
	raised = nil;
	if(setjmp(boundary))
		return 0;
	operation();
	return 1;
}


/* Write signed wire fields without imposing alignment on compiler data. */
static void
field(uchar *base, int offset, vlong value, int width)
{
	int narrow;

	narrow = value;
	memmove(base+offset, width == 4 ? (void*)&narrow : (void*)&value, width);
}


/* Compare independent table oracles with C and real RX leaves while retaining complete images. */
int
main(void)
{
	void (*operators[])(void) = {icase, casel, igoto, movpc};
	int opcodes[] = {ICASE, ICASEL, IGOTO, IMOVPC};
	uchar memory[2048], before[2048], expectedmemory[2048];
	Inst code[32], sentinel;
	Module module;
	Modlink link;
	REG saved, expected;
	WORD word;
	LONG big;
	Inst *targetpc;
	int family, count, query, defaultpc, alignment, row, header, stride;
	int target, valid, accepted, checks, mode, fpflags;
	char *why;

	memset(code, 0, sizeof(code));
	memset(&module, 0, sizeof(module));
	memset(&link, 0, sizeof(link));
	for(family = 0; family < nelem(code); family++)
		code[family].op = IRUNT;
	for(family = 0; family < nelem(opcodes); family++)
		code[family].op = opcodes[family];
	module.prog = code;
	module.nprog = nelem(code);
	if(!compile(&module, nelem(code), nil))
		return 1;
	for(family = 0; family < nelem(opcodes); family++)
		if(module.jit->entry[opcodes[family]] == nil)
			return 2;
	link.m = &module;
	link.prog = code+1;
	checks = 0;
	feclearexcept(FE_ALL_EXCEPT);
	feraiseexcept(FE_DIVBYZERO);
	fpflags = fetestexcept(FE_ALL_EXCEPT);
	for(family = 0; family < 4; family++)
	for(count = 0; count <= 5; count++)
	for(query = -2; query <= 12; query++)
	for(defaultpc = -1; defaultpc <= 32; defaultpc += defaultpc == 0 ? 31 : 1)
	for(alignment = 0; alignment < 8; alignment++){
		memset(memory, 0xa5, sizeof(memory));
		memset(&R, 0, sizeof(R));
		R.M = &link;
		R.PC = &sentinel;
		R.s = memory+64+alignment;
		R.d = memory+512+alignment;
		big = word = query;
		memmove(R.s, family == 1 ? (void*)&big : (void*)&word, family == 1 ? sizeof(big) : sizeof(word));
		target = defaultpc;
		if(family < 2){
			header = family == 1 ? 2*IBY2WD : IBY2WD;
			stride = family == 1 ? 2*IBY2LG+2*IBY2WD : 3*IBY2WD;
			field(R.d, 0, count, 4);
			for(row = 0; row < count; row++){
				field(R.d, header+row*stride, row*2, family == 1 ? 8 : 4);
				field(R.d, header+row*stride+(family == 1 ? IBY2LG : IBY2WD), row*2+1, family == 1 ? 8 : 4);
				field(R.d, header+row*stride+(family == 1 ? 2*IBY2LG : 2*IBY2WD), row+1, 4);
				if(query == row*2)target = row+1;
			}
			field(R.d, header+count*stride, defaultpc, 4);
			valid = target >= 0 && target < nelem(code);
		}else if(family == 2){
			field(R.d, -IBY2WD, count, 4);
			for(row = 0; row < count; row++)
				field(R.d, row*IBY2WD, row == 0 ? defaultpc : row+1, 4);
			target = query == 0 ? defaultpc : query+1;
			valid = query >= 0 && query < count && target >= 0 && target < nelem(code);
		}else{
			target = query;
			valid = query >= 0 && query < nelem(code);
		}
		if(getenv("PC_TABLE_SUCCESS_ONLY") && !valid)
			continue;
		saved = expected = R;
		memmove(before, memory, sizeof(memory));
		memmove(expectedmemory, memory, sizeof(memory));
		if(valid){
			targetpc = code+target;
			if(family == 3)memmove(expectedmemory+512+alignment, &targetpc, sizeof(targetpc));
			else expected.PC = targetpc;
		}
		operation = operators[family];
		accepted = invoke();
		if(accepted != valid || (!accepted && raised != exBounds) ||
		   memcmp(&R, &expected, sizeof(R)) || memcmp(memory, expectedmemory, sizeof(memory)))
			return 3;
		R = saved;
		memmove(memory, before, sizeof(memory));
		accepted = module.jit->entry[opcodes[family]](&R);
		if(accepted != valid || memcmp(&R, &expected, sizeof(R)) ||
		   memcmp(memory, expectedmemory, sizeof(memory)) || fetestexcept(FE_ALL_EXCEPT) != fpflags){
			fprintf(stderr, "native PC mismatch op=%d cell=%d WORD=%zu count=%d query=%d default=%d alignment=%d\n",
				family, IBY2WD, sizeof(WORD), count, query, defaultpc, alignment);
			return 4;
		}
		checks++;
	}
	/* Malformed source/destination/module spans must reject without a host load or write. */
	for(family = 0; family < 4; family++)
	for(mode = 0; !getenv("PC_TABLE_SUCCESS_ONLY") && mode < 12; mode++){
		memset(&R, 0, sizeof(R));
		R.M = &link;
		R.PC = &sentinel;
		R.s = memory;
		R.d = (void*)1;
		word = 0;
		memmove(memory, &word, sizeof(word));
		why = exBounds;
		if(mode < 3)R.s = mode == 0 ? nil : mode == 1 ? H : (void*)((uintptr)-1-2);
		else if(mode < 6){
			R.M = mode == 3 ? nil : mode == 4 ? H : (void*)((uintptr)-1-2);
			why = exModule;
		}else if(mode < 9){
			link.m = mode == 6 ? nil : mode == 7 ? H : (void*)((uintptr)-1-2);
			why = exModule;
		}else if(family == 3)
			R.d = mode == 9 ? nil : mode == 10 ? H : (void*)((uintptr)-1-2);
		else continue;
		saved = R;
		operation = operators[family];
		if(invoke() || raised != why || memcmp(&R, &saved, sizeof(R)))
			return 5;
		if(module.jit->entry[opcodes[family]](&R) || memcmp(&R, &saved, sizeof(R)))
			return 6;
		link.m = &module;
		checks++;
	}
	module.jit->freecode(module.jit->code, module.jit->size);
	free(module.jit);
	printf("PASS PC tables C/RX: %d cases cell=%d WORD=%zu\n", checks, IBY2WD, sizeof(WORD));
	return 0;
}
