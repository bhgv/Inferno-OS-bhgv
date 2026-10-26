#include "comp-amd64.c"
#include "tests/leaf-plain-stubs.h"
/* Supply the builtin identity used by guarded numeric formatting leaves. */
Type Tstring = {1}, Tarray = {1};
#include <stdio.h>
#include <setjmp.h>
#include <fenv.h>
#include <errno.h>

REG R;
int mutator, nprop;
char exNilref[] = "nil", exNomem[] = "memory", exHeap[] = "heap";
static jmp_buf boundary;
static char *raised;


/* Exercise the actual compiler and its executable mapping ownership. */
void*
mallocz(ulong n, int clear)
{
	void *p;

	p = malloc(n);
	if(p != nil && clear)
		memset(p, 0, n);
	return p;
}


/* Descriptor publication does not inject exceptions in this matrix. */
int
kprotect(int (*fn)(void*), void *arg)
{
	return fn(arg);
}


/* Observe the exact production C error identity at a fixture boundary. */
void
error(char *text)
{
	raised = text;
	longjmp(boundary, 1);
}


/* Rune conversion belongs to C fallback; this ASCII matrix must not reach it. */
int
runetochar(char *p, Rune *r)
{
	(void)p;
	(void)r;
	abort();
	return 0;
}


/* This matrix must neither inspect nor allocate a Rune cache. */
ulong
msize(void *p)
{
	(void)p;
	abort();
	return 0;
}


/* Preserve ASan stack checking without process-based leak inspection. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

#define OP(name) void name(void) // Names for extracted production operators.
IMPLEMENTATION


/* Compare actual C and RX conversions, full signed ranges, FP state and errno. */
int
main(void)
{
	static char *texts[] = {
		"", "+", "-", "0", "-0", "+0", "1", "-1", "999999999",
		"-999999999", "+999999999", "000000000", "1000000000", "2147483647",
		"-2147483648", "9223372036854775807", "-9223372036854775808", " 12",
		"12 ", "12x", "++1", "--1", "\xff", "\t1", "0x1", "123456789",
		"4294967295", "-4294967296", "999999999999999", "-999999999999999",
		"1000000000000000", "999999999999999999", "-999999999999999999",
		"1000000000000000000", "-1000000000000000000", "123456789012345678",
		"1.5", "-0.0", "1e2", "1e-300", "1e-999", "1e999", "Inf", "nan",
		"2147483646", "2147483648", "-2147483647", "-2147483649",
		"9223372036854775806", "-9223372036854775807",
		"9223372036854775808", "-9223372036854775809",
		"9999999999", "-9999999999", "9999999999999999999", "-9999999999999999999",
		"0000000000", "+0000000000", "-0000000000",
		"0000000000000000000", "+0000000000000000000", "-0000000000000000000",
		"00000000000000000000", "+00000000000000000000", "-00000000000000000000",
		"18446744073709551615", "-18446744073709551615",
		"18446744073709551616", "-18446744073709551616"
	};
	Module module = {0};
	Inst instructions[3] = {{0}, {0}, {0}}, sentinel;

	union {
		uvlong align; // Native base alignment for the independent byte images.
		uchar bytes[128]; // Object or operand slots plus surrounding canaries.
	} object, slots;
	uchar expectedobject[128], expectedslots[128], savedobject[128], savedslots[128];
	String *s;
	REG saved;
	LONG value, big;
	uvlong magnitude, limit;
	WORD word;
	REAL real;
	int i, op, sa, da, alias, kind, pass, valid, length, j, round, flags, seed;
	int negative, digits, want_errno, actual_errno;
	int rounds[] = {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO};
	unsigned long checks = 0;

	instructions[0].op = ICVTCW;
	instructions[1].op = ICVTCL;
	instructions[2].op = ICVTCF;
	module.prog = instructions;
	module.nprog = 3;
	if(compile(&module, 3, nil) != 1 || !module.jit || module.compiled)
		abort();
	if(!module.jit->entry[ICVTCW] || !module.jit->entry[ICVTCL] || !module.jit->entry[ICVTCF])
		abort();
	for(seed = 0; seed < 2; seed++)
	for(round = 0; round < 4; round++)
	for(i = 0; i < sizeof(texts)/sizeof(texts[0]); i++)
	for(kind = 0; kind < 4; kind++)
	for(op = 0; op < 3; op++)
	for(sa = 0; sa < 8; sa++)
	for(da = 0; da < 8; da++)
	for(alias = 0; alias < 3; alias++){
		memset(object.bytes, 0xa5, sizeof(object.bytes));
		s = (String*)object.bytes;
		length = strlen(texts[i]);
		s->len = s->max = length;
		s->tmp = nil;
		memmove(s->Sascii, texts[i], length+1);
		negative = texts[i][0] == '-';
		digits = length-(negative || texts[i][0] == '+');
		valid = digits <= (op == 2 ? 15 : op == 0 && sizeof(WORD) == 4 ? 10 : 19);
		magnitude = 0;
		for(j = length-digits; j < length; j++){
			if(texts[i][j] < '0' || texts[i][j] > '9')
				valid = 0;
			else if(digits <= 19)
				magnitude = magnitude*10+(texts[i][j]-'0');
		}
		if(op != 2){
			limit = (op == 0 && sizeof(WORD) == 4 ? 0x7fffffffULL : 0x7fffffffffffffffULL)+negative;
			if(magnitude > limit)
				valid = 0;
		}
		value = valid && length ? (LONG)(negative ? 0-magnitude : magnitude) : 0;
		real = strtod(texts[i], nil);
		if(op == 2 && (strcmp(texts[i], "+") == 0 || strcmp(texts[i], "-") == 0))
			valid = 0;
		if(kind == 1){
			s = H;
			valid = 1;
			value = 0;
			real = 0;
		}
		if(kind == 2){
			s = nil;
			valid = 0;
		}
		if(kind == 3){
			((String*)object.bytes)->Sascii[length] = 'x';
			valid = 0;
		}
		if(getenv("DECIMAL_ACCEPTED_ONLY") && op == 2 && !valid && kind != 2)
			continue;
		memset(slots.bytes, 0xa5, sizeof(slots.bytes));
		memset(&R, 0, sizeof(R));
		R.s = slots.bytes+8+sa;
		R.d = alias == 1 ? R.s : alias == 2 ? object.bytes+64+da : slots.bytes+80+da;
		R.PC = &sentinel;
		memmove(R.s, &s, sizeof(s));
		saved = R;
		memmove(savedobject, object.bytes, sizeof(object.bytes));
		memmove(savedslots, slots.bytes, sizeof(slots.bytes));
		for(pass = 0; pass < 2; pass++){
			memmove(object.bytes, savedobject, sizeof(object.bytes));
			memmove(slots.bytes, savedslots, sizeof(slots.bytes));
			memmove(expectedobject, object.bytes, sizeof(object.bytes));
			memmove(expectedslots, slots.bytes, sizeof(slots.bytes));
			R = saved;
			raised = nil;
			if(fesetround(rounds[round]))
				abort();
			feclearexcept(FE_ALL_EXCEPT);
			if(seed)
				feraiseexcept(FE_INVALID|FE_INEXACT);
			flags = fetestexcept(FE_ALL_EXCEPT);
			want_errno = EDOM;
			if(valid || (pass && kind != 2)){
				if(pass && kind != 1){
					((String*)expectedobject)->Sascii[length] = 0;
					if(op != 2){
#ifdef DECIMAL_VM_BIG
						if(op)
							value = (LONG)fixture_big_values[i];
						else
#endif
							value = op ? strtoll(texts[i], nil, 10) : strtol(texts[i], nil, 10);
					}
				}
				big = value;
				word = (WORD)value;
				memmove(alias == 2 ? expectedobject+64+da : expectedslots+(alias == 1 ? 8+sa : 80+da),
					op == 2 ? (void*)&real : op ? (void*)&big : (void*)&word,
					op ? sizeof(big) : sizeof(word));
			}
			/* Reference parsing must not seed state for the tested operation. */
			feclearexcept(FE_ALL_EXCEPT);
			if(seed)
				feraiseexcept(FE_INVALID|FE_INEXACT);
			if(pass && kind != 2){
				errno = EDOM;
				if(op == 2){
					volatile REAL expected = strtod(kind == 1 ? "" : texts[i], nil);

					(void)expected;
					flags = fetestexcept(FE_ALL_EXCEPT);
				}else if(op){
#ifndef DECIMAL_VM_BIG
					volatile LONG expected = strtoll(kind == 1 ? "" : texts[i], nil, 10);

					(void)expected;
#endif
					/* The production modular lib9 parser does not modify errno. */
				}else{
					volatile long expected = strtol(kind == 1 ? "" : texts[i], nil, 10);

					(void)expected;
				}
				want_errno = errno;
				feclearexcept(FE_ALL_EXCEPT);
				if(seed)
					feraiseexcept(FE_INVALID|FE_INEXACT);
			}
			errno = EDOM;
			if(pass == 0){
				if(module.jit->entry[instructions[op].op](&R) != valid)
					abort();
			}else if(setjmp(boundary) == 0){
				if(op == 2)
					cvtcf();
				else if(op)
					cvtcl();
				else
					cvtcw();
				if(kind == 2)
					abort();
			}else if(kind != 2 || raised != exNilref)
				abort();
			actual_errno = errno;
			if(memcmp(object.bytes, expectedobject, sizeof(object.bytes)) ||
			   memcmp(slots.bytes, expectedslots, sizeof(slots.bytes)) ||
			   memcmp(&saved, &R, sizeof(R)))
				abort();
			if(fegetround() != rounds[round] || fetestexcept(FE_ALL_EXCEPT) != flags ||
			   actual_errno != want_errno)
				abort();
		}
		checks++;
	}
	if(fesetround(FE_TONEAREST))
		abort();
	feclearexcept(FE_ALL_EXCEPT);
	module.jit->freecode(module.jit->code, module.jit->size);
	free(module.jit);
	printf("PASS decimal native: %lu actual C/JIT/alias/error/FP/errno cases WORD%zu\n", checks, sizeof(WORD)*8);
	return 0;
}
