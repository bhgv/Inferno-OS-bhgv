#include "scalar-native64.h"
#include "native64-frame-cell.h"

static int celltransactions; // Independent completed stack/collector cases.


/* Generated frame metadata names an ordinary VM function; this test never calls its body. */
void
Scalar_caller(void *frame)
{
	USED(frame);
}


/* Resolve real generated scalar storage while proving adjacent managed roots survive actual collection. */
static void
celltransaction(int phase, int state, int move)
{
	Type *type, *other;
	F_Scalar_caller *first, *second;
	F_Math_bits64real callee;
	REAL value;
	uvlong bits, oldbits, observed;
	void *slot;
	uchar *savedfp, *savedsp, *savedts;
	int before;

	startcase(phase, state);
	type = native64frametype(&limbo_native64_Scalar_caller_entry);
	other = native64frametype(&limbo_native64_Scalar_caller_entry);
	check(type != nil && other != nil && type != other);
	check(type->size == 88 && type->np == 2 && type->map[1] == 0x20);
	first = (F_Scalar_caller*)stackframes(type, 2);
	second = (F_Scalar_caller*)((uchar*)first+type->size);
	process.R.FP = (uchar*)first;
	process.R.TS = stackspace.bytes+sizeof(stackspace.bytes);
	bits = 0x3ff0000000000000ULL+(unsigned int)celltransactions;
	oldbits = 0x1122334455667788ULL;
	memmove(&first->result, &oldbits, 8);
	memmove(&second->result, &oldbits, 8);
	first->keep = H2D(Array*, heaparray(&Tbyte, 4));
	second->keep = H2D(Array*, heaparray(&Tbyte, 4));
	memset(first->keep->data, 0x5a, 4);
	memset(second->keep->data, 0x6b, 4);
	before = descriptors;
	check(native64framecell(&process, type, 72) == &first->result);
	check(native64framecell(&process, type, 80) == nil);
	check(native64framecell(&process, type, 64) == &first->padding);
	check(native64framecell(&process, type, 32) == nil);
	check(native64framecell(&process, type, 73) == nil);
	check(native64framecell(&process, type, 88) == nil);
	check(native64framecell(&process, other, 72) == nil);
	savedfp = process.R.FP; savedsp = process.R.SP; savedts = process.R.TS;
	process.R.FP += 8;
	check(native64framecell(&process, type, 72) == nil);
	process.R.FP = savedfp;
	process.R.TS = savedsp-8;
	check(native64framecell(&process, type, 72) == nil);
	process.R.TS = savedts;
	process.R.SP--;
	check(native64framecell(&process, type, 72) == nil);
	process.R.SP = savedsp;
	check(descriptors == before);
	first->regs.t = nil; // Match the collector's real extent-descriptor fallback.
	check(native64framecell(&process, type, 72) == &first->result);
#ifdef FRAMECELL_ROOT_CONTROL
	type->map[1] = 0; // Remove the only independent GC roots; scalar bytes still look valid.
#endif
	epochs(6);
	check(!freed[2] && !freed[3]);
	memset(&callee, 0, sizeof(callee));
	callee.ret = &value;
	memmove(&callee.b, &bits, 8);
	Math_bits64real(&callee);
	/* Force a safepoint between native production and late logical resolution. */
	epochs(6);
	check(!freed[2] && !freed[3]);
	if(move)
		process.R.FP = (uchar*)second;
	slot = native64framecell(&process, type, 72);
	check(slot == (move ? &second->result : &first->result));
	memmove(slot, &value, 8);
	epochs(6);
	check(!freed[2] && !freed[3] && first->keep->data[0] == 0x5a && second->keep->data[0] == 0x6b);
	memmove(&observed, &first->result, 8);
	check(observed == (move ? oldbits : bits));
	memmove(&observed, &second->result, 8);
	check(observed == (move ? bits : oldbits));
	freeptrs(first, type); freeptrs(second, type);
	initmem(type, first); initmem(type, second);
	check(freed[2] == 1 && freed[3] == 1);
	process.R.EX = process.R.SP = process.R.FP = process.R.TS = nil;
	epochs(6);
	check(Tarray.ref == 1 && Tbyte.ref == 1);
	freetype(type); freetype(other);
	celltransactions++;
}


/* Cover every collector color/process state with stable and relocated current frames. */
int
main(void)
{
	int phase, state, move;

	check(limbo_native64_Scalar_layout() && offsetof(F_Scalar_caller, result) == 72);
	for(phase=0; phase<3; phase++)
	for(state=Palt; state<=Pbroken; state++)
	for(move=0; move<2; move++)
		celltransaction(phase, state, move);
	printf("PASS typed scalar VM cells: %d transactions, actual heap/Math/collector, mapped neighbors and late current-frame publication\n", celltransactions);
	return 0;
}
