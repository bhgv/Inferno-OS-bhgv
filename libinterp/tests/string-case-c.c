#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "raise.h"
#include <setjmp.h>
#include <stdio.h>

REG R;
char exModule[] = "module", exBounds[] = "bounds", exNilref[] = "nil", exHeap[] = "heap";
static jmp_buf boundary;
static char *raised;
static void (*operation)(void);


/* Capture VM errors before any instruction pointer is published. */
void
error(char *message)
{
	raised = message;
	longjmp(boundary, 1);
}


/* Host sandbox cannot perform LeakSanitizer process inspection. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

#define OP(name) void name(void) // Production operator declaration.
IMPLEMENTATION


/* Store declared wire cells without host alignment assumptions. */
static void
cell(uchar *table, int index, vlong value)
{
	int narrow;

	narrow = value;
	memmove(table+index*IBY2WD, IBY2WD == 4 ? (void*)&narrow : (void*)&value, IBY2WD);
}


/* Keep nonlocal errors outside the matrix's automatic loop variables. */
static int
invoke(void)
{
	raised = nil;
	if(setjmp(boundary))
		return 0;
	operation();
	return 1;
}


/* Compare sorted singleton/range tables with a linear oracle and preserve all other state. */
int
main(void)
{
	uchar tablemem[1024], slots[32], before[1024];
	union {
		uvlong align; // Align every synthetic String header.
		uchar bytes[128]; // Inline ASCII payload and header storage.
	} storage[6];
	String *strings[6], *value, *low, *high;
	Inst code[32], sentinel;
	Module module;
	Modlink link;
	REG expected;
	uchar *table;
	int count, alignment, query, mode, row, target, valid, accepted, checks;
	char *why;
	void (*operators[])(void) = {icase, casel, igoto};
	Rune rune;

	memset(&module, 0, sizeof(module));
	memset(&link, 0, sizeof(link));
	module.prog = code;
	module.nprog = nelem(code);
	link.m = &module;
	link.prog = code+1;
	for(query = 0; query < 6; query++){
		strings[query] = (String*)storage[query].bytes;
		memset(storage[query].bytes, 0, sizeof(storage[query].bytes));
		strings[query]->len = strings[query]->max = 1;
		strings[query]->Sascii[0] = 'a'+query;
	}
	strings[5]->len = -1;
	rune = 0x10000;
	memmove(strings[5]->Srune, &rune, sizeof(rune));
	operation = casec;
	checks = 0;
	for(count = -1; count <= 4; count++)
	for(alignment = 0; alignment < 8; alignment++)
	for(query = 0; query < 7; query++)
	for(mode = 0; mode < 5; mode++){
		memset(tablemem, 0xa5, sizeof(tablemem));
		memset(&R, 0, sizeof(R));
		table = tablemem+alignment;
		cell(table, 0, count);
		target = mode == 1 ? -1 : mode == 2 ? 32 : 31;
		if(count >= 0)cell(table, 1+3*count, target);
		value = query == 6 ? H : strings[query];
		for(row = 0; row < count; row++){
			low = strings[row];
			high = mode == 3 && row == count-1 ? strings[5] : H;
			if(IBY2WD >= sizeof(low)){
				memmove(table+(1+3*row)*IBY2WD, &low, sizeof(low));
				memmove(table+(2+3*row)*IBY2WD, &high, sizeof(high));
			}
			cell(table, 3+3*row, row);
			if(query == row || (mode == 3 && row == count-1 && query < 6 && query >= row))target = row;
		}
		memmove(slots+alignment, &value, sizeof(value));
		R.s = slots+alignment;
		R.d = table;
		R.M = &link;
		R.PC = &sentinel;
		if(mode == 4)module.prog = H;
		else module.prog = code;
		valid = count >= 0 && (count == 0 || IBY2WD >= sizeof(value)) && target >= 0 && target < 32 && mode != 4;
		if(getenv("STRING_CASE_SUCCESS_ONLY") && !valid)
			continue;
		expected = R;
		if(valid)expected.PC = code+target;
		memmove(before, tablemem, sizeof(before));
		accepted = invoke();
		if(accepted != valid || (!accepted && raised != exBounds) ||
		   memcmp(&R, &expected, sizeof(R)) != 0 || memcmp(before, tablemem, sizeof(before)) != 0){
			fprintf(stderr, "case cells=%d count=%d align=%d query=%d mode=%d\n", IBY2WD, count, alignment, query, mode);
			return 1;
		}
		checks++;
	}
	/* Reject hostile metadata before touching table storage or publishing a PC. */
	for(mode = 0; !getenv("STRING_CASE_SUCCESS_ONLY") && mode < 12; mode++){
		memset(&R, 0, sizeof(R));
		memset(tablemem, 0, sizeof(tablemem));
		module.prog = code;
		R.M = &link;
		R.PC = &sentinel;
		R.s = slots;
		R.d = tablemem;
		value = H;
		memmove(slots, &value, sizeof(value));
		why = exBounds;
		switch(mode){
		case 0: R.M = nil; why = exModule; break;
		case 1: R.M = H; why = exModule; break;
		case 2: link.m = nil; why = exModule; break;
		case 3: R.s = nil; break;
		case 4: R.s = H; break;
		case 5: R.s = (void*)((uintptr)-1-2); break;
		case 6: R.d = nil; break;
		case 7: R.d = H; break;
		case 8: R.d = (void*)((uintptr)-1-2); break;
		case 9: cell(tablemem, 0, -1); break;
		case 10: cell(tablemem, 0, 0x7fffffff); break;
		case 11: cell(tablemem, 1, -1); break;
		}
		expected = R;
		if(invoke() || raised != why || memcmp(&R, &expected, sizeof(R)) != 0)
			return 2;
		link.m = &module;
		checks++;
	}
	/* Numeric case/goto fallbacks must reject invalid source slots before a load. */
	for(row = 0; row < nelem(operators); row++)
	for(mode = 0; !getenv("STRING_CASE_SUCCESS_ONLY") && mode < 6; mode++){
		memset(&R, 0, sizeof(R));
		R.M = &link;
		R.PC = &sentinel;
		R.d = (void*)1;
		R.s = mode%3 == 0 ? nil : mode%3 == 1 ? H : (void*)((uintptr)-1-2);
		if(mode >= 3)R.M = nil;
		why = mode >= 3 ? exModule : exBounds;
		expected = R;
		operation = operators[row];
		if(invoke() || raised != why || memcmp(&R, &expected, sizeof(R)) != 0)
			return 4;
		checks++;
	}
	operation = casec;
	/* Legacy compiled empty tables retain full cell-width native destinations. */
	for(alignment = 0; alignment < 8; alignment++){
		memset(&R, 0, sizeof(R));
		R.M = &link;
		R.s = slots;
		R.d = tablemem+alignment;
		link.compiled = 1;
		cell(R.d, 0, 0);
		cell(R.d, 1, IBY2WD == 4 ? 0x80000001LL : 0x1234567880000001LL);
		expected = R;
		expected.PC = (Inst*)(uintptr)(IBY2WD == 4 ? 0x80000001ULL : 0x1234567880000001ULL);
		if(!invoke() || memcmp(&R, &expected, sizeof(R)) != 0)
			return 3;
		checks++;
	}
	printf("string case: %d checks, cell=%d WORD=%zu\n", checks, IBY2WD, sizeof(WORD));
	return 0;
}
