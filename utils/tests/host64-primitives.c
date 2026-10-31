#include <stdint.h>
#include <stddef.h>
#include "lib9.h"
#include "host64-fpu.h"

extern ulong umult(ulong, ulong, ulong*);
extern int _tas(int*);
extern void FPsave(FPU*);
extern void FPrestore(FPU*);

#define REQUIRE(value, code) if(!(value)) return code // Active outcome checks, including optimized/NDEBUG builds.
static ulong checks; // Actual checked native operations reported by the freestanding binary.


/* Observe raw hardware controls independently of the public control encoding. */
static ulong
rawcontrol(void)
{
#if defined(__x86_64__)
	unsigned short x87;
	uint simd;
	__asm__ volatile("fnstcw %0" : "=m"(x87));
	__asm__ volatile("stmxcsr %0" : "=m"(simd));
	return ((ulong)x87 << 32)|simd;
#elif defined(__aarch64__)
	ulong value;
	__asm__ volatile("mrs %0, fpcr" : "=r"(value));
	return value;
#else
	ulong value;
	__asm__ volatile("frcsr %0" : "=r"(value));
	return value & 0xe0;
#endif
}


/* Observe raw status separately, including fields intentionally absent from Math's API. */
static ulong
rawstatus(void)
{
#if defined(__x86_64__)
	unsigned short x87;
	uint simd;
	__asm__ volatile("fnstsw %0" : "=am"(x87));
	__asm__ volatile("stmxcsr %0" : "=m"(simd));
	return ((ulong)x87 << 32)|(simd & 0x3f);
#elif defined(__aarch64__)
	ulong value;
	__asm__ volatile("mrs %0, fpsr" : "=r"(value));
	return value;
#else
	ulong value;
	__asm__ volatile("frcsr %0" : "=r"(value));
	return value & 0x1f;
#endif
}


/* Keep the deterministic corpus independent of the production multiplication implementation. */
static ulong
randomword(ulong *state)
{
	*state ^= *state << 13;
	*state ^= *state >> 7;
	*state ^= *state << 17;
	return *state;
}


/* Verify complete 128-bit products, native return width and exact high-half destination writes. */
static int
products(void)
{
	struct {
		ulong before; // Guard the high-result destination against underwrites.
		ulong high; // The production assembler must write all eight bytes here.
		ulong after; // Guard against overwriting the next native field.
	} output;
	ulong a, b, low, state;
	unsigned __int128 expected;
	int i;

	state = 0xc0ffee123456789UL;
	for(i = 0; i < 10003; i++){
		a = i == 0 ? ~(ulong)0 : randomword(&state);
		b = i == 0 ? ~(ulong)0 : randomword(&state);
		if(i == 1) a = 0;
		if(i == 2) b = 1;
		expected = (unsigned __int128)a*b;
		output.before = 0x0123456789abcdefUL;
		output.high = 0;
		output.after = 0xfedcba9876543210UL;
		low = umult(a, b, &output.high);
		REQUIRE(low == (ulong)expected && output.high == (ulong)(expected >> 64), 11);
		REQUIRE(output.before == 0x0123456789abcdefUL && output.after == 0xfedcba9876543210UL, 12);
		checks++;
	}
	return 0;
}


/* Test the lock's actual 32-bit ABI instead of accidentally exchanging a whole native ulong. */
static int
locks(void)
{
	struct {
		uint before; // Sentinel immediately before Lock.val.
		int value; // Exact type used by the production Lock declaration.
		uint after; // Sentinel immediately after Lock.val.
	} lock;
	int previous, values[] = {0, 1, -1, 0x12345678};
	int i, j;

	for(i = 0; i < 1024; i++)
	for(j = 0; j < 4; j++){
		lock.before = 0xa5a5a5a5u;
		lock.value = values[j];
		lock.after = 0x5a5a5a5au;
		previous = _tas(&lock.value);
		REQUIRE(previous == values[j] && lock.value == 1, 21);
		REQUIRE(lock.before == 0xa5a5a5a5u && lock.after == 0x5a5a5a5au, 22);
		checks++;
	}
	return 0;
}


/* Seed real non-API fields so control/status setters cannot pass by erasing them. */
static void
seednativefields(void)
{
#if defined(__x86_64__)
	uint environment[7], simd;

	__asm__ volatile("fnstenv %0" : "=m"(environment));
	environment[1] |= 2u;
	__asm__ volatile("fldenv %0" : : "m"(environment) : "memory");
	__asm__ volatile("stmxcsr %0" : "=m"(simd));
	simd |= 0x8002u;
	__asm__ volatile("ldmxcsr %0" : : "m"(simd) : "memory");
#elif defined(__aarch64__)
	ulong value;

	__asm__ volatile("mrs %0, fpcr" : "=r"(value));
	value |= 1ul << 25;
	__asm__ volatile("msr fpcr, %0" : : "r"(value) : "memory");
	__asm__ volatile("mrs %0, fpsr" : "=r"(value));
	value |= (1ul << 27)|0x80ul;
	__asm__ volatile("msr fpsr, %0" : : "r"(value) : "memory");
#endif
}


/* Check represented masks and exact saved environments with sentinels at both context boundaries. */
static int
environments(void)
{
	struct {
		uint before[2]; // Keep FPsave's first byte inside its declared object.
		FPU value; // Use the exact production layout, not a fixture substitute.
		uint after[2]; // Detect both old ARM-context assumptions and oversized writes.
	} saved;
	FPU original;
	ulong modes[] = {FPRNR, FPRZ, FPRPINF, FPRNINF};
	ulong control, status, mask;
	int i, flags;

	FPsave(&original);
	seednativefields();
	REQUIRE(sizeof(ulong) == 8 && sizeof(uint) == 4, 31);
#if defined(__x86_64__)
	REQUIRE(sizeof(FPU) == 32 && offsetof(FPU, mxcsr) == 28, 32);
	mask = 0x3d;
#elif defined(__aarch64__)
	REQUIRE(sizeof(FPU) == 8 && offsetof(FPU, fpsr) == 4, 33);
	mask = 0x1f;
#else
	REQUIRE(sizeof(FPU) == 4, 34);
	mask = 0x1f;
#endif
	for(i = 0; i < 4; i++)
	for(flags = 0; flags < 64; flags++){
		setfcr(modes[i]|FPPDBL);
		setfsr(flags);
		REQUIRE((getfcr() & FPRMASK) == modes[i], 35);
		REQUIRE(getfsr() == ((ulong)flags & mask), 36);
		control = rawcontrol();
		status = rawstatus();
		saved.before[0] = saved.before[1] = 0x12345678;
		saved.after[0] = saved.after[1] = 0x87654321;
		FPsave(&saved.value);
		REQUIRE(rawcontrol() == control && rawstatus() == status, 37);
		REQUIRE(saved.before[0] == 0x12345678 && saved.before[1] == 0x12345678, 38);
		REQUIRE(saved.after[0] == 0x87654321 && saved.after[1] == 0x87654321, 39);
		setfcr(modes[(i+1)%4]|FPPDBL);
		setfsr(flags ^ 31);
		FPrestore(&saved.value);
		REQUIRE(rawcontrol() == control && rawstatus() == status, 40);
		checks++;
	}
	FPrestore(&original);
	return 0;
}


/* Report the executed corpus without requiring a target libc or a copied interpreter model. */
static void
report(void)
{
	char buffer[64], digits[24];
	ulong count;
	int used, n;
	const char *prefix = "PASS host64 primitives: ";

	used = 0;
	while(*prefix != 0) buffer[used++] = *prefix++;
	count = checks;
	n = 0;
	do{
		digits[n++] = '0'+count%10;
		count /= 10;
	}while(count != 0);
	while(n != 0) buffer[used++] = digits[--n];
	buffer[used++] = '\n';
#if defined(__x86_64__)
	ulong result = 1;
	__asm__ volatile("syscall" : "+a"(result) : "D"((ulong)1), "S"(buffer), "d"((ulong)used) : "rcx", "r11", "memory");
#elif defined(__aarch64__)
	register ulong number __asm__("x8") = 64;
	register ulong fd __asm__("x0") = 1;
	register char *data __asm__("x1") = buffer;
	register ulong length __asm__("x2") = used;
	__asm__ volatile("svc 0" : "+r"(fd) : "r"(number), "r"(data), "r"(length) : "memory");
#else
	register ulong number __asm__("a7") = 64;
	register ulong fd __asm__("a0") = 1;
	register char *data __asm__("a1") = buffer;
	register ulong length __asm__("a2") = used;
	__asm__ volatile("ecall" : "+r"(fd) : "r"(number), "r"(data), "r"(length) : "memory");
#endif
}


/* The assembly test entry supplies a proper native C ABI stack on each target. */
int
run(void)
{
	int result;

	result = products();
	if(result != 0) return result;
	result = locks();
	if(result != 0) return result;
	result = environments();
	if(result != 0) return result;
	report();
	return 0;
}
