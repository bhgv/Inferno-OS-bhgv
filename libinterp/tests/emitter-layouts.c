#include "lib9.h"
#include "isa.h"
#include "interp.h"
#include <stddef.h>
#include <limits.h>
#include <stdio.h>
#include "EMITTER_HEADER"
#ifdef TEST_AMD64
#include <sys/mman.h>
#endif

#define layoutcheck(test) layoutpredicate(!!(test), __LINE__) // Retain all predicates under NDEBUG.

static unsigned long checks;
static size_t fields[] = {
LAYOUT_FIELDS
};


/* Diagnose layout and publication invariants before accepting emitted bytes. */
static void
layoutpredicate(int valid, int line)
{
	if(!valid){
		fprintf(stderr, "emitter layout predicate failed at line %d\n", line);
		abort();
	}
}


/* No runtime owners are allocated; avoid unavailable LSan process inspection. */
int
__lsan_is_turned_off(void)
{
	return 1;
}


/* Borrow the actual compiler's complete native ABI initialization and real headers. */
static LeafLayout
baseline(void)
{
	LeafLayout abi;

	memset(&abi, 0, sizeof(abi));
LAYOUT_INITIALIZATION
	return abi;
}


/* Compare array overlap and encoding bounds using independent wide arithmetic. */
static int
arrayoracle(LeafLayout *a, int encoded)
{
	long long offsets[] = {a->arraylen, a->arraytype, a->arrayroot, a->arraydata};
	int widths[] = {a->wordbytes, 8, 8, 8};
	int i, j;

	if(a->wordbytes != 4 && a->wordbytes != 8)
		return 0;
	for(i = 0; i < 4; i++){
		if(offsets[i] < 0 || offsets[i]+widths[i] > INT_MAX ||
		   (encoded && (offsets[i] > 2047 || offsets[i]%widths[i])))
			return 0;
		for(j = 0; j < i; j++)
			if(offsets[i] < offsets[j]+widths[j] && offsets[j] < offsets[i]+widths[i])
				return 0;
	}
	return 1;
}


/* Probe the shared validator separately from per-opcode emitter restrictions. */
static void
arrays(void)
{
	LeafLayout a, saved;
	size_t offsets[] = {offsetof(LeafLayout, arraylen), offsetof(LeafLayout, arraytype),
	                    offsetof(LeafLayout, arrayroot), offsetof(LeafLayout, arraydata)};
	int values[] = {INT_MIN, -8, -1, 0, 1, 4, 8, 16, 24, 32, 2040, 2047,
	                2048, INT_MAX-16, INT_MAX-8, INT_MAX-7, INT_MAX-4, INT_MAX-3, INT_MAX};
	int field, value;

	for(field = 0; field < nelem(offsets); field++)
	for(value = 0; value < nelem(values); value++){
		if(getenv("EMIT_LAYOUT_ARRAY_FIELD") && field != atoi(getenv("EMIT_LAYOUT_ARRAY_FIELD")))
			continue;
		if(getenv("EMIT_LAYOUT_ARRAY_VALUE") && value != atoi(getenv("EMIT_LAYOUT_ARRAY_VALUE")))
			continue;
		a = baseline();
		memmove((uchar*)&a+offsets[field], &values[value], sizeof(int));
		saved = a;
		layoutcheck(leafbytearraylayout(&a) == arrayoracle(&a, 0));
		layoutcheck(leafarraycreatelayout(&a) == arrayoracle(&a, 1));
		layoutcheck(!memcmp(&a, &saved, sizeof(a)));
		checks++;
	}
}


/* Require deterministic sizing, bounded staging and unchanged rejected output. */
static void
publication(void)
{
	struct {
		u32int before[4]; // Leading caller-output guards.
		u32int code[LeafMax]; // Complete private caller capacity.
		u32int after[4]; // Trailing caller-output guards.
	} output, expected;
	LeafLayout a, saved;
	int values[] = {INT_MIN, -1, 0, 1, 7, 8, 2047, 2048, 4095, 32760,
	                INT_MAX-16, INT_MAX-8, INT_MAX-7, INT_MAX-4, INT_MAX-3, INT_MAX};
	int field, value, op, n, cap, result, i, capacities[4];

	for(field = 0; field < nelem(fields); field++)
	for(value = 0; value < nelem(values); value++)
	for(op = INOP+1; op < MAXDIS; op++){
		if(op == IECLR)
			continue;
		if(getenv("EMIT_LAYOUT_FIELD") && field != atoi(getenv("EMIT_LAYOUT_FIELD")))
			continue;
		if(getenv("EMIT_LAYOUT_OP") && op != atoi(getenv("EMIT_LAYOUT_OP")))
			continue;
		a = baseline();
		memmove((uchar*)&a+fields[field], &values[value], sizeof(int));
		saved = a;
		n = leafemit(nil, LeafMax, &a, op);
		layoutcheck(n >= 0 && n <= LeafMax);
		capacities[0] = 0;
		capacities[1] = n > 0 ? n-1 : 1;
		capacities[2] = n;
		capacities[3] = LeafMax;
		for(i = 0; i < 4; i++){
			cap = capacities[i];
			memset(&output, 0xa5, sizeof(output));
			expected = output;
			result = leafemit(output.code, cap, &a, op);
			layoutcheck(result == (n <= cap ? n : 0));
			layoutcheck(!memcmp(output.before, expected.before, sizeof(output.before)));
			layoutcheck(!memcmp(output.after, expected.after, sizeof(output.after)));
			layoutcheck(!memcmp(output.code+result, expected.code+result,
			                   (LeafMax-result)*sizeof(u32int)));
			layoutcheck(!memcmp(&a, &saved, sizeof(a)));
			checks++;
		}
	}
}


#ifdef TEST_AMD64

/* Execute span comparisons as real host code without accessing the numeric owner. */
static void
spans(void)
{
	X64Buf b;
	uchar *code;
	unsigned long long limit, owners[9];
	int extents[] = {1, 4, 8, 125, 126, 127, 128, 32768, INT_MAX-2, INT_MAX-1};
	int i, j, expected, actual;
	int (*entry)(unsigned long long);

	for(i = 0; i < nelem(extents); i++){
		b.n = 0;
		x64byte(&b, 0xf3); x64byte(&b, 0x0f); x64byte(&b, 0x1e); x64byte(&b, 0xfa);
		x64pcspan(&b, 7, extents[i]);
		x64byte(&b, 0xb8); x64word(&b, 1); x64byte(&b, 0xc3);
		layoutcheck(b.n > 0 && b.n <= sizeof(b.bytes));
		code = mmap(nil, sizeof(b.bytes), PROT_READ|PROT_WRITE,
		            MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
		layoutcheck(code != MAP_FAILED);
		memmove(code, b.bytes, b.n);
		__builtin___clear_cache((char*)code, (char*)code+b.n);
		layoutcheck(mprotect(code, sizeof(b.bytes), PROT_READ|PROT_EXEC) == 0);
		entry = (int(*)(unsigned long long))code;
		limit = ~0ULL-(unsigned int)extents[i];
		owners[0] = 0; owners[1] = 1; owners[2] = ~0ULL; owners[3] = ~0ULL-1;
		owners[4] = limit; owners[5] = limit+1; owners[6] = limit-1;
		owners[7] = 16; owners[8] = 0x100000000ULL;
		for(j = 0; j < nelem(owners); j++){
			expected = owners[j] != 0 && owners[j] <= limit;
			actual = entry(owners[j]);
			layoutcheck(actual == expected);
			checks++;
		}
		layoutcheck(munmap(code, sizeof(b.bytes)) == 0);
	}
}
#endif


/* Describe only helpers called by a given opcode, leaving unused identities alone. */
static int
usescallback(int field, int op)
{
	switch(field){
	case 0:
		return op == ICONSB || op == ICONSW || op == ICONSL || op == ICONSF ||
		       op == ICONSM || op == ICONSMP || op == ICONSP;
	case 1: return op == ICONSMP;
	case 2:
		return op == INEWCB || op == INEWCW || op == INEWCL || op == INEWCF || op == INEWCP;
	case 3: return op == INEWCM;
	case 4: return op == INEWCMP;
	default: return op == ISEND || op == IRECV;
	}
}


/* Reject bad called targets atomically, preserve unused fields and AMD64 byte targets. */
static void
callbacks(void)
{
	size_t offsets[] = {offsetof(LeafLayout, consentry), offsetof(LeafLayout, constypeentry),
	                    offsetof(LeafLayout, channelentry), offsetof(LeafLayout, rawchannelentry),
	                    offsetof(LeafLayout, mappedchannelentry), offsetof(LeafLayout, channeltransferentry)};
	unsigned long long values[] = {0, ~0ULL, 0x123456780000ULL, 0x123456780001ULL,
	                              0x123456780002ULL, 0x123456780003ULL, ~3ULL};
	LeafLayout a, saved;
	u32int output[LeafMax+2], original[LeafMax+2];
	void *target;
	int field, value, op, baselinecount, expected, result, called;

	for(field = 0; field < nelem(offsets); field++)
	for(op = INOP+1; op < MAXDIS; op++){
		if(op == IECLR)
			continue;
		if(getenv("EMIT_LAYOUT_CALLBACK_FIELD") && field != atoi(getenv("EMIT_LAYOUT_CALLBACK_FIELD")))
			continue;
		if(getenv("EMIT_LAYOUT_CALLBACK_OP") && op != atoi(getenv("EMIT_LAYOUT_CALLBACK_OP")))
			continue;
		a = baseline();
		baselinecount = leafemit(nil, LeafMax, &a, op);
		called = usescallback(field, op);
		if(called)
			layoutcheck(baselinecount > 0);
		for(value = 0; value < nelem(values); value++){
			a = baseline();
			target = (void*)values[value];
			memmove((uchar*)&a+offsets[field], &target, sizeof(target));
			saved = a;
			expected = baselinecount;
			if(called && (values[value] == 0 || values[value] == ~0ULL))
				expected = 0;
#ifdef EMITTER_ARM64
			if(called && values[value]%4 != 0)
				expected = 0;
#endif
			memset(output, 0xa5, sizeof(output));
			memmove(original, output, sizeof(output));
			layoutcheck(leafemit(nil, LeafMax, &a, op) == expected);
			result = leafemit(output+1, LeafMax, &a, op);
			layoutcheck(result == expected && !memcmp(&a, &saved, sizeof(a)));
			layoutcheck(output[0] == original[0]);
			layoutcheck(!memcmp(output+result+1, original+result+1,
			                   (LeafMax+1-result)*sizeof(*output)));
			checks++;
		}
	}
}


/* Exercise shared Array layout rules and the complete priority leaf opcode set. */
int
main(void)
{
	if(!getenv("EMIT_LAYOUT_CASE") || !strcmp(getenv("EMIT_LAYOUT_CASE"), "callbacks"))
		callbacks();
	if(!getenv("EMIT_LAYOUT_CASE") || !strcmp(getenv("EMIT_LAYOUT_CASE"), "array"))
		arrays();
	if(!getenv("EMIT_LAYOUT_CASE") || !strcmp(getenv("EMIT_LAYOUT_CASE"), "publication"))
		publication();
#ifdef TEST_AMD64
	if(!getenv("EMIT_LAYOUT_CASE") || !strcmp(getenv("EMIT_LAYOUT_CASE"), "span"))
		spans();
#endif
	layoutcheck(checks > 0);
	printf("PASS emitter layouts: %lu array/publication/callback cases\n", checks);
	return 0;
}
