/* Freestanding Linux/RV64 execution of the production encoder and actual cjitexec body. */
#include <stdint.h>
#include <stddef.h>
#include "isa.h"
typedef uint32_t u32int;
typedef unsigned long uintptr;
typedef unsigned char uchar;
#define nil ((void*)0) // Native C boundary used by the actual executor body.
#define H ((void*)-1) // Invalid native owner sentinel used by the extracted executor.
#define O(type, field) offsetof(type, field) // Synthetic fixture layout, not the complete runtime ABI.
#ifdef TEST_WORD64
typedef int64_t TestWord;
#else
typedef int32_t TestWord;
#endif
#include "riscv-emit.h"

typedef struct REG REG;
typedef struct Module Module;
typedef struct Modlink Modlink;

/* Minimal ordinary-owner metadata for this freestanding calling-convention fixture. */
struct Module
{
	int (*cstep)(void); // Nil denotes ordinary decoded LEA execution.
};

struct Modlink
{
	Module *m; // Live synthetic owner; full runtime ownership is tested separately.
	int compiled; // Zero denotes original Dis-PC mode.
};

struct REG
{
	void *PC; // Synthetic decoded leaf layout, independently supplied to the actual encoder.
	void *s, *d, *m; // Native operand addresses, including four-byte-aligned wide values.
	Modlink *M; // Ordinary owner required by the actual decoded LEA guard.
};

static Module ordinarymodule; // This fixture never installs a translated execution profile.
static Modlink ordinarylink = {&ordinarymodule, 0}; // Stable owner for all bridge invocations.
REG R = {nil, nil, nil, nil, &ordinarylink};
static void (*optab[256])(void); // Real executor uses this C handler table.
static char exOp[]="opcode", exCompile[]="compile"; // Observable absent-handler boundary.
static int faults, helpers, fastcalls, acceptfast, expectedop, captured; // Independent callback observations.
static void *mapping; // Owned code page whose lifetime may end inside the C handler.


/* Supply exact byte-copy support without a libc dependency in RV64I builds. */
void*
memmove(void *destination, const void *source, size_t bytes)
{
	unsigned char *out = destination;
	const unsigned char *in = source;
	size_t i;

	if((uintptr)out <= (uintptr)in){
		for(i=0; i<bytes; i++)
			out[i] = in[i];
	}else{
		while(bytes != 0){
			bytes--;
			out[bytes] = in[bytes];
		}
	}
	return destination;
}


/* Provide only freestanding compiler support for zeroed layout storage. */
void*
memset(void *pointer, int value, unsigned long bytes)
{
	volatile unsigned char *output=pointer;
	while(bytes--) *output++=value;
	return pointer;
}


/* RV64I lacks compiler-runtime remainder support; this serves encoder geometry, not emitted Dis arithmetic. */
uint64_t
__umoddi3(uint64_t value, uint64_t divisor)
{
	uint64_t remainder=0, carry;
	int bit;

	if(divisor == 0) return 0;
	for(bit=63; bit>=0; bit--){
		carry=remainder>>63;
		remainder=(remainder<<1)|((value>>bit)&1);
		if(carry || remainder >= divisor) remainder-=divisor;
	}
	return remainder;
}


/* Linux system-call boundary without a libc loader or runtime replacement executor. */
static long
systemcall(long number, long p0, long p1, long p2, long p3, long p4, long p5)
{
	register long a0 __asm__("a0")=p0;
	register long a1 __asm__("a1")=p1;
	register long a2 __asm__("a2")=p2;
	register long a3 __asm__("a3")=p3;
	register long a4 __asm__("a4")=p4;
	register long a5 __asm__("a5")=p5;
	register long a7 __asm__("a7")=number;
	__asm__ volatile("ecall" : "+r"(a0) : "r"(a1),"r"(a2),"r"(a3),"r"(a4),"r"(a5),"r"(a7) : "memory");
	return a0;
}


/* Record only the executor's error boundary; nonlocal VM unwinding is covered by hosted integration gates. */
static void
error(char *message)
{
	if(message == exOp) faults++;
	if(message == exCompile){
		systemcall(93, 21, 0, 0, 0, 0, 0);
		for(;;)
			;
	}
}

EXECUTOR_IMPLEMENTATION


/* Retain ordinary C dispatch for complex or declined native operations. */
static void
handler(void)
{
	helpers++;
}


/* Model only a fast producer result; arguments arrive through the real RV64 calling convention. */
static int
fast(REG *registers)
{
	if(registers != &R) return 0;
	fastcalls++;
	return acceptfast;
}


/* Verify every pointer bit without dereferencing the synthetic high-bit fast identity. */
static int
capture(REG *registers, int opcode, int (*entry)(REG*))
{
	captured = registers == &R && opcode == expectedop && (uintptr)entry == 0xfedcba98f6543210ULL;
	return 37;
}


/* Unmap the actual trampoline while the C executor is active; its return must go directly to the C caller. */
static void
retirecode(void)
{
	if(systemcall(215, (long)mapping, 4096, 0, 0, 0, 0) == 0){
		mapping=nil;
		helpers++;
	}
}


/* Publish RW-created instructions as RX, synchronizing this hart before calling the actual generated code. */
static int
publish(void)
{
	if(systemcall(226, (long)mapping, 4096, 5, 0, 0, 0) != 0) return 0;
	/* Preserve the same cache instruction with assemblers that require explicit Zifencei naming. */
	__asm__ volatile(".word 0x0000100f" ::: "memory");
	return 1;
}


/* Exercise all active opcodes, both code alignments, C fallback, native leaves and destructive C ownership. */
int
run(void)
{
	u32int guard[64], *code, *leaf;
	int op, alignment, mode, before, nativebefore, active, i, cap;
	int (*entry)(REG*);
	RvLayout abi={0};
	TestWord source=-7, middle=31, destination=0;

	if(sizeof(uintptr) != 8 || sizeof(void*) != 8 || (uintptr)cjitexec <= 0xffffffffULL) return 1;
	for(i=0; i<64; i++) guard[i]=0xdeadbeef;
	for(cap=0; cap<15; cap++){
		if(rvbridgeemit(guard, cap, (uintptr)cjitexec, IADDW, 0) != -1) return 2;
		for(i=0; i<64; i++) if(guard[i] != 0xdeadbeef) return 3;
	}
	if(rvbridgeemit(nil, 64, 0, IADDW, 0) != 0 || rvbridgeemit(nil, 64, 1, IADDW, 0) != 0 ||
	   rvbridgeemit(nil, 64, (uintptr)cjitexec, IADDW, ~0ULL) != 0) return 4;
	mapping=(void*)systemcall(222, 0, 4096, 3, 0x22, -1, 0);
	if((long)mapping < 0) return 5;
	for(alignment=0; alignment<=4; alignment+=4){
		code=(u32int*)((char*)mapping+alignment);
		if(systemcall(226, (long)mapping, 4096, 3, 0, 0, 0) != 0) return 6;
		expectedop=IADDW; captured=0;
		if(rvbridgeemit(code, 64, (uintptr)capture, expectedop, 0xfedcba98f6543210ULL) != 15 || !publish()) return 7;
		entry=(int(*)(REG*))code;
		if(entry(&R) != 37 || !captured) return 8;
		active=0;
		for(op=0; op<256; op++){
			if(op <= INOP || op >= MAXDIS || op == IECLR){
				if(rvbridgeemit(nil, 64, (uintptr)cjitexec, op, 0) != 0) return 9;
				continue;
			}
			active++; optab[op]=handler;
			for(mode=0; mode<3; mode++){
				if(systemcall(226, (long)mapping, 4096, 3, 0, 0, 0) != 0) return 10;
				if(rvbridgeemit(code, 64, (uintptr)cjitexec, op, mode ? (uintptr)fast : 0) != 15 || !publish()) return 11;
				before=helpers; nativebefore=fastcalls; acceptfast=mode == 2;
				if(entry(&R) != 1 || helpers != before+(mode != 2) || fastcalls != nativebefore+(mode != 0)) return 12;
			}
		}
		if(active != 173) return 13;
	}
	if(systemcall(226, (long)mapping, 4096, 3, 0, 0, 0) != 0) return 14;
	code=mapping; leaf=(u32int*)((char*)mapping+256);
	abi.xlen=64; abi.wordbytes=sizeof(TestWord); abi.pc=offsetof(REG, PC);
	abi.s=offsetof(REG, s); abi.d=offsetof(REG, d); abi.m=offsetof(REG, m);
	R.s=&source; R.m=&middle; R.d=&destination;
	if(rvemit(leaf, RvMax, &abi, IADDW) <= 0 || rvbridgeemit(code, 64, (uintptr)cjitexec, IADDW, (uintptr)leaf) != 15 || !publish()) return 15;
	before=helpers; entry=(int(*)(REG*))code;
	if(entry(&R) != 1 || destination != 24 || helpers != before) return 16;
	optab[IALT]=nil;
	if(cjitexec(&R, IALT, nil) != 0 || faults != 1 || cjitexec(nil, IADDW, nil) != 0) return 17;
	if(systemcall(226, (long)mapping, 4096, 3, 0, 0, 0) != 0) return 18;
	if(rvbridgeemit(code, 64, (uintptr)cjitexec, IALT, 0) != 15) return 19;
	optab[IALT]=retirecode;
	if(!publish() || entry(&R) != 1 || mapping != nil) return 20;
	return 0;
}
