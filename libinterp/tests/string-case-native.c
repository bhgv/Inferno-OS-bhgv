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

char exNilref[] = "nil", exHeap[] = "heap";

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


/* Compare real C and RX-plus-C case paths over signed32 Rune endpoints. */
static int
runecases(Module *module, Modlink *link, Inst *code)
{
	union {
		uvlong align; // Natural alignment of every synthetic String header.
		uchar bytes[128]; // Rune or byte payload and independent header.
	} storage[9];
	Rune keys[] = {0x80000000U, 0xffffffffU, 0, 0xff, 0xd800, 0x10ffff, 0x110000, 0x7fffffff};
	String *strings[9], *low, *high, *value;
	uchar table[128], before[128], source[32];
	REG saved, expected;
	vlong lo, hi, key;
	int i, a, b, q, shift, route, target, fast, checks;

	if(IBY2WD != sizeof(void*))
		return 0;
	memset(storage, 0, sizeof(storage));
	for(i=0; i<9; i++){
		strings[i] = (String*)storage[i].bytes;
		strings[i]->len = i == 8 ? 1 : -1;
		strings[i]->max = 1;
		if(i == 8)
			strings[i]->Sascii[0] = 0xff;
		else
			memmove(strings[i]->Srune, &keys[i], sizeof(Rune));
	}
	module->prog = code;
	link->compiled = 0;
	operation = casec;
	checks = 0;
	for(a=0; a<8; a++)
	for(b=a; b<8; b++)
	for(q=0; q<10; q++)
	for(shift=0; shift<8; shift++){
		low = strings[a];
		high = strings[b];
		value = q == 9 ? H : strings[q];
		lo = keys[a];
		hi = keys[b];
		key = q == 8 ? 0xff : q == 9 ? 0 : keys[q];
		if(lo > 0x7fffffffLL)
			lo -= 0x100000000LL;
		if(hi > 0x7fffffffLL)
			hi -= 0x100000000LL;
		if(key > 0x7fffffffLL)
			key -= 0x100000000LL;
		target = q != 9 && key >= lo && key <= hi ? 5 : 31;
		memset(table, 0, sizeof(table));
		cell(table+shift, 0, 1);
		memmove(table+shift+IBY2WD, &low, sizeof(low));
		memmove(table+shift+2*IBY2WD, &high, sizeof(high));
		cell(table+shift, 3, 5);
		cell(table+shift, 4, 31);
		memcpy(before, table, sizeof(table));
		memmove(source+shift, &value, sizeof(value));
		memset(&saved, 0, sizeof(saved));
		saved.M = link;
		saved.s = source+shift;
		saved.d = table+shift;
		saved.PC = code+30;
		expected = saved;
		expected.PC = code+target;
		for(route=0; route<2; route++){
			R = saved;
			fast = route == 0 ? 0 : module->jit->entry[ICASEC](&R);
			if(!fast){
				if(memcmp(&R, &saved, sizeof(R)) != 0 || !invoke())
					return -1;
			}
			if(memcmp(&R, &expected, sizeof(R)) != 0 || memcmp(table, before, sizeof(table)) != 0)
				return -1;
			checks++;
		}
	}
	return checks;
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
	REG expected, saved;
	int nativevalid;
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
	memset(code, 0, sizeof(code));
	for(row=0; row<nelem(code); row++)
		code[row].op=IRUNT;
	code[0].op = ICASEC;
	if(!compile(&module, nelem(code), nil) || module.jit->entry[ICASEC] == nil)
		return 5;
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
		saved = expected = R;
		if(valid)expected.PC = code+target;
		memmove(before, tablemem, sizeof(before));
		accepted = invoke();
		if(accepted != valid || (!accepted && raised != exBounds) ||
		   memcmp(&R, &expected, sizeof(R)) != 0 || memcmp(before, tablemem, sizeof(before)) != 0){
			fprintf(stderr, "case cells=%d count=%d align=%d query=%d mode=%d\n", IBY2WD, count, alignment, query, mode);
			return 1;
		}
		R = saved;
		nativevalid = valid && (count == 0 || (count > 0 && IBY2WD == 8 && query == count/2));
		expected = saved;
		if(nativevalid)expected.PC = code+target;
		if(module.jit->entry[ICASEC](&R) != nativevalid ||
		   memcmp(&R, &expected, sizeof(R)) || memcmp(before, tablemem, sizeof(before)))
			return 6;
		checks++;
	}
	/* Identity comparisons validate headers even though no character payload is read. */
	for(mode = 0; mode < 13; mode++)
	for(target = -1; target <= 32; target += target == 0 ? 31 : 1)
	for(alignment = 0; alignment < 8; alignment++){
		memset(&R, 0, sizeof(R));
		memset(tablemem, 0, sizeof(tablemem));
		memset(storage, 0, sizeof(storage));
		strings[0]->len = strings[0]->max = 1;
		strings[1]->len = strings[1]->max = 1;
		strings[0]->Sascii[0] = strings[1]->Sascii[0] = 'a';
		value = low = strings[0];
		if(mode == 1)strings[0]->len = -1;
		if(mode == 2 || mode == 9 || mode == 10)strings[0]->len = strings[0]->max = 0;
		if(mode == 3)value = low = H;
		if(mode == 4)value = low = nil;
		if(mode == 5)strings[0]->len = (-0x7fffffff-1);
		if(mode == 6 || mode == 7)strings[0]->max = 0;
		if(mode == 7 || mode == 11)strings[0]->len = -1;
		if(mode == 11)strings[0]->max = -1;
		if(mode == 8 || mode == 12)low = strings[1];
		if(mode == 12)strings[0]->len = strings[0]->max = strings[1]->len = strings[1]->max = 0;
		if(mode == 9)low = H;
		if(mode == 10)value = H;
		valid = IBY2WD == 8 && (mode <= 3 || (mode >= 8 && mode <= 10) || mode == 12) && target >= 0 && target < 32;
		if(getenv("STRING_CASE_SUCCESS_ONLY") && !valid)continue;
		module.prog = code;
		table = tablemem+alignment;
		cell(table, 0, 1);
		if(IBY2WD == 8)memmove(table+IBY2WD, &low, sizeof(low));
		cell(table, 2, 1); /* A poisoned upper bound must remain unread after an equal lower bound. */
		cell(table, 3, target);
		cell(table, 4, target);
		memmove(slots+alignment, &value, sizeof(value));
		R.M = &link;
		R.s = slots+alignment;
		R.d = table;
		R.PC = &sentinel;
		saved = expected = R;
		if(valid)expected.PC = code+target;
		operation = casec;
		if(invoke() != valid || memcmp(&R, &expected, sizeof(R)))return 7;
		R = saved;
		nativevalid = valid && (mode <= 3 || mode == 9 || mode == 10 || mode == 12);
		expected = saved;
		if(nativevalid)expected.PC = code+target;
		if(module.jit->entry[ICASEC](&R) != nativevalid || memcmp(&R, &expected, sizeof(R)))return 8;
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
	/* An eight-byte count must decline before a read crossing the native address boundary. */
	for(mode = 4; !getenv("STRING_CASE_SUCCESS_ONLY") && mode <= 7; mode++){
		memset(&R, 0, sizeof(R));
		R.M = &link;
		R.PC = &sentinel;
		value = H;
		memmove(slots, &value, sizeof(value));
		R.s = slots;
		R.d = (void*)((uintptr)-1-(IBY2WD == 8 ? mode : 2));
		operation = casec;
		saved = R;
		if(invoke() || raised != exBounds || memcmp(&R, &saved, sizeof(R)))return 9;
		if(module.jit->entry[ICASEC](&R) || memcmp(&R, &saved, sizeof(R)))return 10;
		checks++;
	}
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
	mode = runecases(&module, &link, code);
	if(mode < 0)
		return 11;
	checks += mode;
	module.jit->freecode(module.jit->code, module.jit->size);
	free(module.jit);
	printf("string case C/RX: %d checks, cell=%d WORD=%zu\n", checks, IBY2WD, sizeof(WORD));
	return 0;
}
