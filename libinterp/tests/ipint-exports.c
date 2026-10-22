#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "runt.h"
#include "kernel.h"
#include "raise.h"
#include "mp.h"
#include "ipint.h"
#include <assert.h>
#include <stdio.h>
#include <limits.h>

TYPES

extern void *testmalloc(size_t);
extern void testfree(void*);
extern int liveblocks(void);
extern void setfailure(int);
int enc64(char*, int, uchar*, int);
static Proc worker;
static char *last_error;
static int vm_live, vm_failure = -1, checks;
static Type iptype = {1, 0, 0, sizeof(IPint)};
Type *TIPint = &iptype;
Type Tarray = {1}, Tbyte = {1}, Tstring = {1};
char exHeap[] = "heap";
char exNilref[] = "nil";
char exType[] = "type";
char exZdiv[] = "divide by zero";
char exInval[] = "invalid argument";
static char injected[] = "original VM allocation error";
#define MP(x) checkIPint(x)

/* LeakSanitizer cannot inspect processes in this sandbox; track ownership here. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

/* Use real Proc error-frame storage for nested kprotect/krethrow tests. */
Proc*
getup(void)
{
	return &worker;
}

/* Deliver VM failures through the runtime's actual error-frame convention. */
void
nexterror(void)
{
	assert(worker.nerr > 0);
	siglongjmp(worker.estack[--worker.nerr], 1);
}

/* Retain the exact original error across native cleanup and nested rethrows. */
void
error(char *message)
{
	last_error = message;
	nexterror();
}

/* Bound fixture heap storage and fail selected VM allocations deterministically. */
Heap*
nheap(int bytes)
{
	Heap *h;

	if(vm_failure == 0)
		error(injected);
	if(vm_failure > 0)
		vm_failure--;
	assert(bytes >= 0 && bytes < 1024*1024);
	h = calloc(1, sizeof(*h)+bytes);
	assert(h);
	h->ref = 1;
	vm_live++;
	return h;
}

/* Allocate the actual VM IPint layout; native magnitudes remain separately owned. */
Heap*
heap(Type *type)
{
	Heap *h;

	h = nheap(type->size);
	h->t = type;
	type->ref++;
	return h;
}

/* Model ASCII VM strings with the real String and Heap layouts. */
String*
c2string(char *text, int length)
{
	Heap *h;
	String *s;

	h = nheap(sizeof(String)+length+1);
	h->t = &Tstring;
	Tstring.ref++;
	s = H2D(String*, h);
	s->len = length;
	s->max = length+1;
	memmove(s->Sascii, text, length);
	return s;
}

/* Verify reference accounting while releasing VM objects and owned magnitudes. */
void
destroy(void *object)
{
	Heap *h;
	Array *a;

	if(object == H)
		return;
	h = D2H(object);
	assert(h->ref > 0);
	if(--h->ref)
		return;
	if(h->t == TIPint)
		mpfree(((IPint*)object)->b);
	else if(h->t == &Tarray) {
		a = object;
		assert(a->t == &Tbyte && a->root == H);
		Tbyte.ref--;
	}else
		assert(h->t == &Tstring);
	h->t->ref--;
	free(h);
	vm_live--;
}

static int released, random_calls, safe_random;

/* Model the interpreter lock around native random generation. */
void
release(void)
{
	assert(!released);
	released = 1;
}

/* Require the wrapper to restore the interpreter lock before VM allocation. */
void
acquire(void)
{
	assert(released);
	released = 0;
}

/* Fill deterministic bytes while asserting the native call's lock contract. */
void
genrandom(uchar *data, int count)
{
	assert(released && count > 0);
	memset(data, 0xff, count);
	random_calls++;
	if(safe_random) {
		memset(data, 0, count);
		data[count-1] = random_calls <= 2 ? 0 : random_calls-2;
	}
}

/* Supply bounded deterministic witnesses independently of candidate bytes. */
void
prng(uchar *data, int count)
{
	static unsigned int state = 0x7072696d;
	int i;

	assert(released);
	for(i = 0; i < count; i++) {
		state = state*1664525U+1013904223U;
		data[i] = state>>24;
	}
}

int probably_prime(mpint*, int);
void genprime(mpint*, int, int);
void gensafeprime(mpint*, mpint*, int, int);

/* Route native export allocations through the tracked libmp allocator. */
#define malloc testmalloc
#define free testfree
IMPLEMENTATION
#undef malloc
#undef free

static F_IPint_iptob64 textframe;
static F_IPint_iptob64z zeroframe;
static F_IPint_iptostr formatframe;
static F_IPint_iptobebytes bytesframe;
static F_IPint_iptobytes legacyframe;
static F_IPint_div divideframe;
static F_IPint_mod modframe;
static F_IPint_expmod powerframe;
static F_IPint_shl leftframe;
static F_IPint_shr rightframe;
static F_IPint_add addframe;
static F_IPint_sub subframe;
static F_IPint_mul mulframe;
static F_IPint_neg negframe;
static F_IPint_copy copyframe;
static F_IPint_invert invertframe;
static F_IPint_and andframe;
static F_IPint_ori orframe;
static F_IPint_xor xorframe;
static F_IPint_not notframe;
static F_IPint_random randomframe;
static F_IPints_probably_prime primeframe;
static F_IPints_genprime genframe;
static F_IPints_gensafeprime safeframe;
static mpint *owned_number;
static int operation;

/* Invoke production wrappers inside an outer VM error boundary. */
static int
invoke(void *unused)
{
	(void)unused;
	switch(operation) {
	case 0: IPint_iptob64(&textframe); break;
	case 1: IPint_iptob64z(&zeroframe); break;
	case 2: IPint_iptostr(&formatframe); break;
	case 3: IPint_iptobebytes(&bytesframe); break;
	case 4: IPint_iptobytes(&legacyframe); break;
	case 5: newIPint(owned_number); break;
	case 6: IPint_div(&divideframe); break;
	case 7: IPint_mod(&modframe); break;
	case 8: IPint_expmod(&powerframe); break;
	case 9: IPint_shl(&leftframe); break;
	case 10: IPint_shr(&rightframe); break;
	case 11: IPint_add(&addframe); break;
	case 12: IPint_sub(&subframe); break;
	case 13: IPint_mul(&mulframe); break;
	case 14: IPint_neg(&negframe); break;
	case 15: IPint_copy(&copyframe); break;
	case 16: IPint_invert(&invertframe); break;
	case 17: IPint_and(&andframe); break;
	case 18: IPint_ori(&orframe); break;
	case 19: IPint_xor(&xorframe); break;
	case 20: IPint_not(&notframe); break;
	case 21: IPint_random(&randomframe); break;
	case 22: IPints_probably_prime(&primeframe); break;
	case 23: IPints_genprime(&genframe); break;
	case 24: IPints_gensafeprime(&safeframe); break;
	default: assert(0);
	}
	return 1;
}

/* Compare every exported byte with an exact expected representation. */
static void
checkexport(const uchar *input, int length, int negative)
{
	IPints_IPint *number;
	String *text, *oldtext;
	Array *bytes, *oldbytes;
	char *expected;
	uchar *prefixed;
	int op, base, baseline, native_before, vm_before, failure, expected_length;

	number = newIPint(betomp((uchar*)input, length, nil));
	if(negative)
		checkIPint(number)->sign = -1;
	baseline = liveblocks();
	textframe.i = number;
	zeroframe.i = number;
	formatframe.i = number;
	bytesframe.i = number;
	legacyframe.i = number;
	for(op = 0; op < 5; op++) {
		operation = op;
		text = c2string("keep", 4);
		bytes = mem2array("keep", 4);
		textframe.ret = &text;
		zeroframe.ret = &text;
		formatframe.ret = &text;
		bytesframe.ret = &bytes;
		legacyframe.ret = &bytes;
		formatframe.base = 16;
		assert(kprotect(invoke, nil));
		if(op >= 3) {
			expected_length = length ? length : 1;
			assert(bytes->len == expected_length);
			if(length)
				assert(memcmp(bytes->data, input, length) == 0);
			else
				assert(bytes->data[0] == 0);
		}else if(op == 1) {
			expected_length = length ? length : 1;
			prefixed = calloc(1, expected_length+1);
			assert(prefixed);
			if(length)
				memmove(prefixed+1, input, length);
			expected = calloc(1, 4*((expected_length+3)/3)+1);
			assert(expected);
			if(length && (input[0]&0x80))
				expected_length = enc64(expected, 4*((expected_length+3)/3)+1,
					prefixed, expected_length+1);
			else
				expected_length = enc64(expected, 4*((expected_length+3)/3)+1,
					prefixed+1, expected_length);
			assert(text->len == expected_length);
			assert(memcmp(text->Sascii, expected, expected_length) == 0);
			free(prefixed);
			free(expected);
		}else {
			expected = mptoa(checkIPint(number), op == 0 ? 64 : 16, nil, 0);
			assert(expected && text->len == strlen(expected));
			assert(memcmp(text->Sascii, expected, text->len) == 0);
			testfree(expected);
		}
		destroy(text);
		destroy(bytes);
		assert(liveblocks() == baseline);
		checks++;
	}

	/* A VM failure after native encoding must retain the previous destination. */
	for(op = 0; op < 5; op++) {
		operation = op;
		oldtext = text = c2string("keep", 4);
		oldbytes = bytes = mem2array("keep", 4);
		native_before = liveblocks();
		vm_before = vm_live;
		vm_failure = 0;
		assert(!kprotect(invoke, nil));
		vm_failure = -1;
		assert(last_error == injected);
		assert(text == oldtext && bytes == oldbytes);
		assert(liveblocks() == native_before && vm_live == vm_before);
		destroy(text);
		destroy(bytes);
		checks++;
	}

	/* Only ordinary text-buffer failures are injected; mpnew failures are fatal. */
	for(op = 0; op < 3; op++)
		for(failure = 0; failure < (op == 2 ? 1 : 2); failure++) {
			operation = op;
			oldtext = text = c2string("keep", 4);
			native_before = liveblocks();
			vm_before = vm_live;
			setfailure(failure);
			assert(!kprotect(invoke, nil));
			setfailure(-1);
			assert(last_error == exHeap && text == oldtext);
			assert(liveblocks() == native_before && vm_live == vm_before);
			destroy(text);
			checks++;
		}

	/* Decimal and base32 string wrappers also accept dynamically sized output. */
	operation = 2;
	for(base = 10; base <= 32; base += 22) {
		text = H;
		formatframe.base = base;
		assert(kprotect(invoke, nil));
		expected = mptoa(checkIPint(number), base, nil, 0);
		assert(expected && text->len == strlen(expected));
		assert(memcmp(text->Sascii, expected, text->len) == 0);
		testfree(expected);
		destroy(text);
		checks++;
	}
	destroy(number);
}

/* Exercise staged pair cleanup, zero divisors and owned constructor failures. */
static void
checkownership(void)
{
	IPints_IPint *a, *b, *zero, *oldq, *oldr, *result;
	struct { IPints_IPint *t0, *t1; } pair;
	int failure, native_before, vm_before;

	a = newIPint(itomp(31, nil));
	b = newIPint(itomp(3, nil));
	zero = newIPint(itomp(0, nil));
	pair.t0 = oldq = newIPint(itomp(7, nil));
	pair.t1 = oldr = newIPint(itomp(8, nil));
	divideframe.ret = (void*)&pair;
	divideframe.i1 = a;
	divideframe.i2 = b;
	operation = 6;
	for(failure = 0; failure < 2; failure++) {
		native_before = liveblocks();
		vm_before = vm_live;
		vm_failure = failure;
		assert(!kprotect(invoke, nil));
		vm_failure = -1;
		assert(last_error == injected && pair.t0 == oldq && pair.t1 == oldr);
		assert(liveblocks() == native_before && vm_live == vm_before);
		checks++;
	}
	assert(kprotect(invoke, nil));
	assert(mptoi(checkIPint(pair.t0)) == 10 && mptoi(checkIPint(pair.t1)) == 1);
	oldq = pair.t0;
	oldr = pair.t1;
	divideframe.i2 = zero;
	assert(!kprotect(invoke, nil));
	assert(last_error == exZdiv && pair.t0 == oldq && pair.t1 == oldr);

	operation = 7;
	result = newIPint(itomp(9, nil));
	modframe.ret = &result;
	modframe.i1 = a;
	modframe.i2 = zero;
	native_before = liveblocks();
	vm_before = vm_live;
	assert(!kprotect(invoke, nil) && last_error == exZdiv);
	assert(mptoi(checkIPint(result)) == 9);
	assert(liveblocks() == native_before && vm_live == vm_before);
	modframe.i2 = b;
	vm_failure = 0;
	assert(!kprotect(invoke, nil));
	vm_failure = -1;
	assert(last_error == injected && mptoi(checkIPint(result)) == 9);
	assert(liveblocks() == native_before && vm_live == vm_before);
	assert(kprotect(invoke, nil) && mptoi(checkIPint(result)) == 1);
	destroy(result);

	operation = 5;
	native_before = liveblocks();
	owned_number = itomp(123, nil);
	vm_failure = 0;
	assert(!kprotect(invoke, nil));
	vm_failure = -1;
	assert(last_error == injected && liveblocks() == native_before);
	destroy(pair.t0);
	destroy(pair.t1);
	destroy(a);
	destroy(b);
	destroy(zero);
	checks += 6;
}

/* Invalid references and impossible native lengths must not replace results. */
static void
checkinvalid(void)
{
	IPints_IPint *number, *input;
	String *text, *oldtext;
	Array *bytes, *oldbytes;
	int mode, op, saved_top, native_before, vm_before;

	number = newIPint(itomp(1, nil));
	saved_top = checkIPint(number)->top;
	for(mode = 0; mode < 3; mode++)
		for(op = 0; op < 5; op++) {
			operation = op;
			oldtext = text = c2string("keep", 4);
			oldbytes = bytes = mem2array("keep", 4);
			input = mode == 0 ? H : mode == 1 ? (IPints_IPint*)text : number;
			textframe.i = input;
			zeroframe.i = input;
			formatframe.i = input;
			bytesframe.i = input;
			legacyframe.i = input;
			textframe.ret = &text;
			zeroframe.ret = &text;
			formatframe.ret = &text;
			bytesframe.ret = &bytes;
			legacyframe.ret = &bytes;
			if(mode == 2)
				checkIPint(number)->top = INT_MAX;
			native_before = liveblocks();
			vm_before = vm_live;
			assert(!kprotect(invoke, nil));
			assert(last_error == (mode == 0 ? exNilref : mode == 1 ? exType : exHeap));
			assert(text == oldtext && bytes == oldbytes);
			assert(liveblocks() == native_before && vm_live == vm_before);
			checkIPint(number)->top = saved_top;
			destroy(text);
			destroy(bytes);
			checks++;
		}
	destroy(number);
	operation = 5;
	owned_number = nil;
	assert(!kprotect(invoke, nil) && last_error == exHeap);
	checks++;
}

/* Validate power domains and allocation failure without losing old results. */
static void
checkpowers(void)
{
	IPints_IPint *base, *exp, *mod, *negative, *zero, *result, *old;
	int native_before, vm_before;

	base = newIPint(itomp(17, nil));
	exp = newIPint(itomp(0, nil));
	mod = newIPint(itomp(1, nil));
	negative = newIPint(itomp(-1, nil));
	zero = newIPint(itomp(0, nil));
	result = newIPint(itomp(99, nil));
	powerframe.base = base;
	powerframe.exp = exp;
	powerframe.mod = mod;
	powerframe.ret = &result;
	operation = 8;
	assert(kprotect(invoke, nil) && mptoi(checkIPint(result)) == 0);
	old = result;
	native_before = liveblocks();
	vm_before = vm_live;
	powerframe.exp = negative;
	assert(!kprotect(invoke, nil) && last_error == exInval && result == old);
	powerframe.exp = exp;
	powerframe.mod = zero;
	assert(!kprotect(invoke, nil) && last_error == exZdiv && result == old);
	powerframe.mod = negative;
	assert(!kprotect(invoke, nil) && last_error == exInval && result == old);
	assert(liveblocks() == native_before && vm_live == vm_before);
	powerframe.mod = mod;
	vm_failure = 0;
	assert(!kprotect(invoke, nil) && last_error == injected && result == old);
	vm_failure = -1;
	assert(liveblocks() == native_before && vm_live == vm_before);
	powerframe.mod = H;
	assert(kprotect(invoke, nil) && mptoi(checkIPint(result)) == 1);
	destroy(result);
	destroy(base);
	destroy(exp);
	destroy(mod);
	destroy(negative);
	destroy(zero);
	checks += 6;
}

/* Check reverse/minimum/full-word counts and old-result ownership on failure. */
static void
checkshifts(void)
{
	IPints_IPint *number, *result, *old, *zero;
	WORD minimum, wide;
	int native_before, vm_before, op;

	number = newIPint(itomp(8, nil));
	zero = newIPint(itomp(0, nil));
	result = newIPint(itomp(99, nil));
	leftframe.i = rightframe.i = number;
	leftframe.ret = rightframe.ret = &result;
	minimum = (WORD)((UWORD)1 << (sizeof(WORD)*8-1));
	for(op = 9; op <= 10; op++) {
		operation = op;
		leftframe.n = rightframe.n = 1;
		assert(kprotect(invoke, nil));
		assert(mptoi(checkIPint(result)) == (op == 9 ? 16 : 4));
		leftframe.n = rightframe.n = -1;
		assert(kprotect(invoke, nil));
		assert(mptoi(checkIPint(result)) == (op == 9 ? 4 : 16));
		leftframe.n = rightframe.n = minimum;
		old = result;
		native_before = liveblocks();
		vm_before = vm_live;
		if(op == 9) {
			assert(kprotect(invoke, nil));
			assert(mptoi(checkIPint(result)) == 0);
		}else {
			assert(!kprotect(invoke, nil) && last_error == exHeap && result == old);
			assert(liveblocks() == native_before && vm_live == vm_before);
		}
		leftframe.n = rightframe.n = 1;
		old = result;
		native_before = liveblocks();
		vm_before = vm_live;
		vm_failure = 0;
		assert(!kprotect(invoke, nil) && last_error == injected && result == old);
		vm_failure = -1;
		assert(liveblocks() == native_before && vm_live == vm_before);
		leftframe.i = rightframe.i = zero;
		leftframe.n = rightframe.n = minimum;
		assert(kprotect(invoke, nil) && mptoi(checkIPint(result)) == 0);
		leftframe.i = rightframe.i = number;
		checks += 5;
	}
	if(sizeof(WORD) > sizeof(int)) {
		wide = (WORD)((uvlong)1<<32);
		operation = 10;
		rightframe.n = wide;
		assert(kprotect(invoke, nil) && mptoi(checkIPint(result)) == 0);
		operation = 9;
		leftframe.n = -wide;
		assert(kprotect(invoke, nil) && mptoi(checkIPint(result)) == 0);
		leftframe.n = wide;
		old = result;
		assert(!kprotect(invoke, nil) && last_error == exHeap && result == old);
		checks += 3;
	}
	destroy(number);
	destroy(zero);
	destroy(result);
}

/* Exercise all arithmetic wrappers with failures and an output-owned source. */
static void
checkarithmetic(void)
{
	IPints_IPint *result, *other, *old;
	int op, mode, native_before, vm_before, expected[] = {10, 4, 21, -7, 7};

	other = newIPint(itomp(3, nil));
	for(op = 11; op <= 15; op++)
		for(mode = 0; mode < (op <= 13 ? 4 : 3); mode++) {
			result = newIPint(itomp(7, nil));
			old = result;
			addframe.i1 = subframe.i1 = mulframe.i1 = result;
			addframe.i2 = subframe.i2 = mulframe.i2 = other;
			negframe.i = copyframe.i = result;
			addframe.ret = subframe.ret = mulframe.ret = &result;
			negframe.ret = copyframe.ret = &result;
			if(mode == 3) {
				addframe.i1 = subframe.i1 = mulframe.i1 = other;
				addframe.i2 = subframe.i2 = mulframe.i2 = result;
			}
			operation = op;
			native_before = liveblocks();
			vm_before = vm_live;
			if(mode == 1)
				vm_failure = 0;
			if(mode == 2) {
				addframe.i1 = subframe.i1 = mulframe.i1 = H;
				negframe.i = copyframe.i = H;
			}
			if(mode == 0 || mode == 3) {
				assert(kprotect(invoke, nil));
				assert(mptoi(checkIPint(result)) == (mode == 3 && op == 12 ? -4 : expected[op-11]));
			}else {
				assert(!kprotect(invoke, nil));
				assert(last_error == (mode == 1 ? injected : exNilref));
				assert(result == old);
			}
			vm_failure = -1;
			assert(liveblocks() == native_before && vm_live == vm_before);
			destroy(result);
			checks++;
		}
	result = newIPint(itomp(0, nil));
	negframe.i = result;
	negframe.ret = &result;
	operation = 14;
	assert(kprotect(invoke, nil));
	assert(checkIPint(result)->top == 0 && checkIPint(result)->sign == 1);
	destroy(result);
	destroy(other);
	checks++;
}

/* Inversion failures must unwind through the VM and leave no native temporaries. */
static void
checkinverses(void)
{
	IPints_IPint *base, *mod, *result, *old;
	int b, m, expected, native_before, vm_before;

	for(m = 1; m <= 31; m++)
		for(b = -32; b <= 32; b++) {
			base = newIPint(itomp(b, nil));
			mod = newIPint(itomp(m, nil));
			result = newIPint(itomp(99, nil));
			old = result;
			invertframe.base = base;
			invertframe.mod = mod;
			invertframe.ret = &result;
			operation = 16;
			for(expected = 0; expected < m; expected++)
				if(((b*expected)%m+m)%m == 1%m)
					break;
			native_before = liveblocks();
			vm_before = vm_live;
			if(expected < m) {
				assert(kprotect(invoke, nil));
				assert(mptoi(checkIPint(result)) == expected);
				old = result;
				vm_failure = 0;
				assert(!kprotect(invoke, nil) && last_error == injected);
				vm_failure = -1;
				assert(result == old);
			}else {
				assert(!kprotect(invoke, nil) && last_error == exInval);
				assert(result == old);
			}
			assert(liveblocks() == native_before && vm_live == vm_before);
			destroy(base);
			destroy(mod);
			destroy(result);
			checks++;
		}
	base = newIPint(itomp(3, nil));
	result = newIPint(itomp(11, nil));
	invertframe.base = base;
	invertframe.mod = result;
	invertframe.ret = &result;
	assert(kprotect(invoke, nil));
	assert(mptoi(checkIPint(result)) == 4);
	destroy(result);
	result = base;
	mod = newIPint(itomp(11, nil));
	invertframe.base = result;
	invertframe.mod = mod;
	assert(kprotect(invoke, nil));
	assert(mptoi(checkIPint(result)) == 4);
	destroy(mod);
	old = result;
	for(m = -1; m <= 0; m++) {
		mod = newIPint(itomp(m, nil));
		invertframe.base = result;
		invertframe.mod = mod;
		native_before = liveblocks();
		vm_before = vm_live;
		assert(!kprotect(invoke, nil));
		assert(last_error == (m == 0 ? exZdiv : exInval));
		assert(result == old);
		assert(liveblocks() == native_before && vm_live == vm_before);
		destroy(mod);
	}
	invertframe.mod = H;
	assert(!kprotect(invoke, nil) && last_error == exNilref && result == old);
	destroy(result);
	checks += 5;

}

/* Cross limb boundaries with exact inverses modulo 2**bits-1. */
static void
checklargeinverses(void)
{
	int widths[] = {31, 32, 33, 64, 127, 256, 521, 1024};
	IPints_IPint *base, *mod, *result;
	mpint *number, *expected;
	unsigned int i;
	int sign;

	for(i = 0; i < sizeof(widths)/sizeof(widths[0]); i++)
		for(sign = -1; sign <= 1; sign += 2) {
			number = mpnew(0);
			mpleft(mpone, widths[i], number);
			mpsub(number, mpone, number);
			mod = newIPint(number);
			base = newIPint(itomp(sign*2, nil));
			result = H;
			expected = mpnew(0);
			mpleft(mpone, widths[i]-1, expected);
			if(sign < 0)
				mpsub(expected, mpone, expected);
			invertframe.base = base;
			invertframe.mod = mod;
			invertframe.ret = &result;
			operation = 16;
			assert(kprotect(invoke, nil));
			assert(mpcmp(checkIPint(result), expected) == 0);
			mpfree(expected);
			destroy(base);
			destroy(mod);
			destroy(result);
			assert(vm_live == 0 && liveblocks() == 0);
			checks++;
		}
}

/* Check bitwise ownership, nil errors, heap failures and canonical zero. */
static void
checkbitwise(void)
{
	IPints_IPint *result, *other, *old;
	ulong expected[] = {3, 7, 4, 0xfffffff8UL};
	int op, mode, native_before, vm_before;

	other = newIPint(itomp(3, nil));
	for(op = 17; op <= 20; op++)
		for(mode = 0; mode < (op == 20 ? 3 : 4); mode++) {
			result = newIPint(itomp(7, nil));
			old = result;
			andframe.i1 = orframe.i1 = xorframe.i1 = result;
			andframe.i2 = orframe.i2 = xorframe.i2 = other;
			andframe.ret = orframe.ret = xorframe.ret = &result;
			notframe.i1 = result;
			notframe.ret = &result;
			if(mode == 3) {
				andframe.i1 = orframe.i1 = xorframe.i1 = other;
				andframe.i2 = orframe.i2 = xorframe.i2 = result;
			}
			if(mode == 2) {
				andframe.i1 = orframe.i1 = xorframe.i1 = H;
				notframe.i1 = H;
			}
			if(mode == 1)
				vm_failure = 0;
			operation = op;
			native_before = liveblocks();
			vm_before = vm_live;
			if(mode == 0 || mode == 3) {
				assert(kprotect(invoke, nil));
				assert(mptoui(checkIPint(result)) == expected[op-17]);
			}else {
				assert(!kprotect(invoke, nil));
				assert(last_error == (mode == 1 ? injected : exNilref));
				assert(result == old);
			}
			vm_failure = -1;
			assert(liveblocks() == native_before && vm_live == vm_before);
			destroy(result);
			checks++;
		}
	result = newIPint(itomp(-7, nil));
	itomp(0, checkIPint(other));
	andframe.i1 = result;
	andframe.i2 = other;
	andframe.ret = &result;
	operation = 17;
	assert(kprotect(invoke, nil));
	assert(checkIPint(result)->top == 0 && checkIPint(result)->sign == 1);
	destroy(result);
	destroy(other);
	checks++;
}

/* Invalid VM-word sizes and both allocation failures must retain old output. */
static void
checkrandom(void)
{
	WORD sizes[] = {0, 1, 31, 32, 33, 127, 256};
	IPints_IPint *result, *old;
	mpint *expected;
	int native_before, vm_before, mode;
	unsigned int i;

	result = newIPint(itomp(99, nil));
	randomframe.ret = &result;
	operation = 21;
	for(i = 0; i < sizeof(sizes)/sizeof(sizes[0]); i++) {
		randomframe.nbits = sizes[i];
		random_calls = 0;
		assert(kprotect(invoke, nil));
		assert(!released && random_calls == (sizes[i] != 0));
		expected = mpnew(0);
		mpleft(mpone, sizes[i], expected);
		mpsub(expected, mpone, expected);
		assert(mpcmp(checkIPint(result), expected) == 0);
		mpfree(expected);
		checks++;
	}
	for(mode = 0; mode < 6; mode++) {
		old = result;
		native_before = liveblocks();
		vm_before = vm_live;
		random_calls = 0;
		randomframe.nbits = 128;
		if(mode == 0)
			randomframe.nbits = -1;
		if(mode == 1)
			randomframe.nbits = (WORD)((UWORD)1 << (sizeof(WORD)*8-1));
		if(mode == 2)
			randomframe.nbits = 0x7fffffff;
		if(mode == 3)
			randomframe.nbits = sizeof(WORD) > sizeof(int) ? (WORD)((uvlong)1<<32) : -2;
		if(mode == 4)
			setfailure(0);
		if(mode == 5)
			vm_failure = 0;
		assert(!kprotect(invoke, nil));
		assert(last_error == (mode < 4 ? exInval : mode == 4 ? exHeap : injected));
		setfailure(-1);
		vm_failure = -1;
		assert(result == old && !released && random_calls == (mode == 5));
		assert(liveblocks() == native_before && vm_live == vm_before);
		checks++;
	}
	destroy(result);
}

/* Validate prime arguments while locked and publish generated output atomically. */
static void
checkprimes(void)
{
	IPints_IPint *number, *result, *old;
	WORD answer;
	int values[] = {-3, 0, 1, 2, 3, 4, 17};
	int expected[] = {0, 0, 0, 1, 1, 0, 1};
	int native_before, vm_before, mode;
	unsigned int i;

	operation = 22;
	primeframe.ret = &answer;
	for(i = 0; i < sizeof(values)/sizeof(values[0]); i++) {
		number = newIPint(itomp(values[i], nil));
		primeframe.n = number;
		primeframe.nrep = 2;
		answer = 99;
		assert(kprotect(invoke, nil) && answer == expected[i] && !released);
		destroy(number);
		checks++;
	}
	answer = 99;
	primeframe.n = H;
	assert(!kprotect(invoke, nil) && last_error == exNilref && !released && answer == 99);
	checks++;
	number = newIPint(itomp(3, nil));
	primeframe.n = number;
	primeframe.nrep = (WORD)((UWORD)1 << (sizeof(WORD)*8-1));
	assert(kprotect(invoke, nil) && answer == 1 && !released);
	checks++;
	if(sizeof(WORD) > sizeof(int)) {
		primeframe.nrep = (WORD)((uvlong)1<<32);
		answer = 99;
		assert(!kprotect(invoke, nil) && last_error == exInval && !released && answer == 99);
		checks++;
	}
	destroy(number);

	result = newIPint(itomp(99, nil));
	genframe.ret = &result;
	genframe.nbits = 4;
	genframe.nrep = 2;
	operation = 23;
	assert(kprotect(invoke, nil) && !released && mptoi(checkIPint(result)) == 11);
	checks++;
	for(mode = 0; mode < 7; mode++) {
		old = result;
		native_before = liveblocks();
		vm_before = vm_live;
		random_calls = 0;
		genframe.nbits = mode == 0 ? -1 : mode == 1 ? 0 : mode == 2 ? 1 :
			mode == 3 ? 0x7fffffff : mode == 4 ?
			(sizeof(WORD) > sizeof(int) ? (WORD)((uvlong)1<<32) : -2) : 4;
		genframe.nrep = mode == 5 ? (sizeof(WORD) > sizeof(int) ? (WORD)((uvlong)1<<32) : 2) : 2;
		if(mode == 6 || (mode == 5 && sizeof(WORD) == sizeof(int)))
			vm_failure = 0;
		assert(!kprotect(invoke, nil));
		assert(last_error == (vm_failure == 0 ? injected : exInval));
		assert(result == old && !released);
		assert(random_calls == (vm_failure == 0));
		vm_failure = -1;
		assert(liveblocks() == native_before && vm_live == vm_before);
		checks++;
	}
	destroy(result);
}

/* Either heap failure must reclaim both native values and preserve the old pair. */
static void
checksafeprime(void)
{
	IPints_IPint *oldp, *olda;
	int mode, native_before, vm_before;

	safeframe.ret = malloc(sizeof(*safeframe.ret));
	assert(safeframe.ret != nil);
	safeframe.ret->t0 = newIPint(itomp(99, nil));
	safeframe.ret->t1 = newIPint(itomp(101, nil));
	operation = 24;
	safe_random = 1;
	for(mode = 0; mode < 6; mode++) {
		random_calls = 0;
		safeframe.nbits = mode == 3 ? 2 : mode == 4 ? -1 : mode == 5 ? 0x7fffffff : 3;
		safeframe.nrep = 2;
		oldp = safeframe.ret->t0;
		olda = safeframe.ret->t1;
		native_before = liveblocks();
		vm_before = vm_live;
		if(mode == 1 || mode == 2)
			vm_failure = mode-1;
		if(mode == 0) {
			assert(kprotect(invoke, nil));
			assert(mptoi(checkIPint(safeframe.ret->t0)) == 7);
			assert(mptoi(checkIPint(safeframe.ret->t1)) == 3);
		}else {
			assert(!kprotect(invoke, nil));
			assert(last_error == (mode <= 2 ? injected : exInval));
			assert(safeframe.ret->t0 == oldp && safeframe.ret->t1 == olda);
		}
		vm_failure = -1;
		assert(!released && random_calls == (mode <= 2 ? 5 : 0));
		assert(liveblocks() == native_before && vm_live == vm_before);
		checks++;
	}
	safe_random = 0;
	destroy(safeframe.ret->t0);
	destroy(safeframe.ret->t1);
	free(safeframe.ret);
}

/* Cover the old fixed-buffer limits and verify all VM/native ownership balances. */
int
main(void)
{
	uchar data[4096];
	uchar plain[] = {0x01, 0x23};
	int sizes[] = {0, 1, 767, 768, 1023, 1024, 1025, 4096};
	unsigned int i;

	mpsetminbits(1);
	for(i = 0; i < sizeof(data); i++)
		data[i] = 0x80 | (i*17&0x7f);
	for(i = 0; i < sizeof(sizes)/sizeof(sizes[0]); i++) {
		checkexport(data, sizes[i], 0);
		assert(vm_live == 0 && liveblocks() == 0 && worker.nerr == 0);
	}
	checkexport(plain, sizeof(plain), 0);
	checkexport(plain, sizeof(plain), 1);
	checkexport(data, 1025, 1);
	checkownership();
	checkinvalid();
	checkpowers();
	checkshifts();
	checkarithmetic();
	checkinverses();
	checklargeinverses();
	checkbitwise();
	checkrandom();
	checkprimes();
	checksafeprime();
	assert(vm_live == 0 && liveblocks() == 0 && worker.nerr == 0);
	assert(TIPint->ref == 1 && Tarray.ref == 1 && Tbyte.ref == 1 && Tstring.ref == 1);
	printf("PASS IPints: %d large export/VM unwind/native failure/ownership cases\n", checks);
	return 0;
}
