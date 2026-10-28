#include "lib9.h"
#include "isa.h"
#include "interp.h"
#include "raise.h"
#include <stdio.h>
#include <setjmp.h>
#include <sys/mman.h>
#include <sys/wait.h>

#define check(c) predicate(!!(c), __LINE__) // Preserve predicates in NDEBUG builds.
#define ProgramCount PROGRAM_COUNT // Number of actual translated Dis instructions.

REG R; // Actual native VM register layout.
char exBounds[] = "bounds"; // Scalar/native span failure.
char exNilref[] = "nil"; // Decoder sentinel failure.
char exModule[] = "module"; // Missing or wrapping module metadata.
static Inst original[ProgramCount] = { PROGRAM_INITIALIZERS }; // Authoritative decoded image.
static uchar code[sizeof(original)+8]; // Packed original Dis PCs, including all byte alignments.
static uchar memory[256], saved[256]; // Full operand/owner-cell images and independent snapshots.
static uchar linkbytes[sizeof(Modlink)+8], modulebytes[sizeof(Module)+8]; // Packed module headers.
static Module module; // Stable module identity for ordinary slot tests.
static Modlink instance; // Stable module instance with original Dis PC base.
static DisC *translation; // Actual generated registration, including the callable step.
static jmp_buf boundary; // Local VM exception boundary for native differential cases.
static char *exception; // Exact production exception identity.
static int called; // Complex handler selection observed after generated dispatch.
static ulong checks; // Number of completed explicit regression predicates.
/* Per-route snapshots keep abandoned sanitizer frames out of later decoder calls. */
typedef struct NativeResult NativeResult;

struct NativeResult
{
	REG registers; // VM state immediately after execution or the production error.
	uchar image[sizeof(memory)]; // Complete operand bytes, including untouched neighbors.
	char *failure; // Exact VM error identity in the fork-shared address space.
	int callback; // Complex callback selected by this route.
};


static NativeResult *activeResult; // Shared child result; nil selects ordinary local VM errors.
int native_register(void);


/* Keep memory instrumentation without unavailable process-based leak scanning. */
int
__lsan_is_turned_off(void)
{
	return 1;
}


/* Fail with the exact predicate line without relying on assert. */
static void
predicate(int valid, int line)
{
	if(!valid){
		fprintf(stderr, "dis2c native failure at %d\n", line);
		abort();
	}
	checks++;
}


/* Preserve VM error identities and state at the actual failing operation. */
void
error(char *message)
{
	exception = message;
	if(activeResult != nil){
		activeResult->failure = message;
		memmove(&activeResult->registers, &R, sizeof(R));
		memmove(activeResult->image, memory, sizeof(memory));
		activeResult->callback = called;
		_Exit(0);
	}
	longjmp(boundary, 1);
}


/* Capture the generated registration without replacing any translated instruction. */
int
disc_register(DisC *code)
{
	translation = code;
	return 1;
}


/* Loading/signature validation is covered by the independent integration fixture. */
Module*
parsemod(char *path, uchar *image, ulong length, Dir *dir)
{
	USED(path);
	USED(image);
	USED(length);
	USED(dir);
	return nil;
}

#define OP(name) void name(void) // Borrow genuine simple VM handlers.
OPERATIONS
static void (*operations[])(void) = { HANDLERS }; // Real helpers or observed complex callbacks.


/* Enter generated C or the real decoder plus real VM handler under one error boundary. */
static char*
invoke(int translated, int index)
{
	exception = nil;
	if(setjmp(boundary) == 0){
		if(translated)
			check(translation->step() == 1);
		else{
			dec[original[index].add]();
			R.PC = (Inst*)((uchar*)R.PC+sizeof(Inst));
			operations[original[index].op]();
		}
	}
	return exception;
}


/* Compare every register, callback and operand byte with actual interpreter execution. */
static void
compare(int index)
{
	REG before, after;
	char *actual, *expected;
	int observed;

	before = R;
	memmove(saved, memory, sizeof(saved));
	called = -1;
	actual = invoke(1, index);
	after = R;
	observed = called;
	{
		uchar result[sizeof(memory)];
		memmove(result, memory, sizeof(result));
		R = before;
		memmove(memory, saved, sizeof(memory));
		called = -1;
		expected = invoke(0, index);
		check(actual == expected && called == observed);
		check(!memcmp(&R, &after, sizeof(R)));
		check(!memcmp(memory, result, sizeof(memory)));
	}
}


/* Restore packed code, stable high native pointers and deterministic raw operand bits. */
static void
prepare(int index, int alignment)
{
	uint state;
	int i;

	memmove(code+alignment, original, sizeof(original));
	module.nprog = ProgramCount;
	module.prog = (Inst*)(code+alignment);
	instance.m = &module;
	instance.prog = module.prog;
	memset(&R, 0, sizeof(R));
	R.M = &instance;
	R.PC = (Inst*)(code+alignment+(uintptr)index*sizeof(Inst));
	R.s = memory+64;
	R.m = memory+96;
	R.d = memory+128;
	R.t = -819;
	state = 1913+index*17+alignment;
	for(i = 0; i < sizeof(memory); i++){
		state = state*1664525U+1013904223U;
		memory[i] = state>>24;
	}
}


/* Exercise 16 inlined commands, every slot alignment and overlapping result placements. */
static void
slots(void)
{
	static uvlong realbits[] = { // Raw REAL payloads, copied without floating evaluation.
		0x7ff8000012345678ULL, 0x7ff0000012345678ULL,
		0xfff8000012345678ULL, 0x8000000000000000ULL,
		0x7ff0000000000000ULL, 0x0000000000000001ULL,
	};
	int index, a, b, alias;

	for(index = 0; index < 16; index++)
	for(a = 0; a < 8; a++)
	for(b = 0; b < 8; b++)
	for(alias = 0; alias < 9; alias++){
		prepare(index, b);
		R.s = memory+64+a;
		R.m = memory+(alias == 8 ? 65 : 96)+b;
		R.d = alias == 0 ? memory+128+b : alias < 5 ? (uchar*)R.s+alias-2 : (uchar*)R.m+alias-6;
		if(index == 3)
			memmove(R.s, &realbits[(a+b+alias)%nelem(realbits)], sizeof(realbits[0]));
		compare(index);
	}
}


/* Match all 144 specialized decoders with packed pointer cells and original PCs. */
static void
addresses(void)
{
	uchar *target;
	int index, a, b;

	for(index = 16; index < 160; index++)
	for(a = 0; a < 8; a++)
	for(b = 0; b < 8; b++){
		prepare(index, a);
		R.FP = memory+16+b;
		R.MP = memory+64+b;
		target = memory+192;
		memmove(R.FP+1, &target, sizeof(target));
		memmove(R.MP+1, &target, sizeof(target));
		compare(index);
	}
}


/* Keep actual opcode-to-C selection for every active command, alongside inline commands. */
static void
fallback(void)
{
	int index;

	for(index = 160; index < ProgramCount; index++){
		prepare(index, index%8);
		compare(index);
	}
}


/* Reject every malformed slot before loading any of its otherwise poisoned peers. */
static void
invalid(void)
{
	void *bad[3];
	int index, role, which;

	bad[0] = nil;
	bad[1] = H;
	bad[2] = (void*)((uintptr)-1-1);
	for(index = 0; index < 16; index++)
	for(role = 0; role < (index < 4 ? 2 : 3); role++)
	for(which = 0; which < 3; which++){
		if(index == 0 && which == 2)
			continue; /* BYTE at max-2 is numerically valid, so it must not be dereferenced. */
		prepare(index, 3);
		if(role == 0)
			R.s = bad[which];
		else if(role == 1)
			R.d = bad[which];
		else
			R.m = bad[which];
		if(role != 0)
			R.s = (void*)1; /* A numerically valid peer must remain unread after rejection. */
		compare(index);
		check(exception == exBounds);
	}
}


/* Compare decoder errors in child processes without unwinding instrumented frames.
 * Error callbacks publish directly before exit; no stack instrumentation is disabled. */
static void
compareisolated(int index)
{
	NativeResult *results;
	pid_t child;
	int route, status;

	results = mmap(nil, 2*sizeof(*results), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
	check(results != MAP_FAILED);
	for(route = 0; route < 2; route++){
		child = fork();
		check(child >= 0);
		if(child == 0){
			called = -1;
			activeResult = &results[route];
			results[route].failure = invoke(route, index);
			memmove(&results[route].registers, &R, sizeof(R));
			memmove(results[route].image, memory, sizeof(memory));
			results[route].callback = called;
			_Exit(0);
		}
		check(waitpid(child, &status, 0) == child);
		check(WIFEXITED(status) && WEXITSTATUS(status) == 0);
	}
	check(results[0].failure == results[1].failure && results[0].callback == results[1].callback);
	check(!memcmp(&results[0].registers, &results[1].registers, sizeof(R)));
	check(!memcmp(results[0].image, results[1].image, sizeof(memory)));
	check(munmap(results, 2*sizeof(*results)) == 0);
}


/* Match decoder error priority for nil bases, wrapped offsets and nil indirect targets. */
static void
decode(void)
{
	uchar *target;
	int index, which;

	for(index = 16; index < 160; index++)
	for(which = 0; which < 4; which++){
		prepare(index, 1);
		R.FP = memory+16;
		R.MP = memory+64;
		target = which == 2 ? nil : which == 3 ? H : memory+192;
		memmove(R.FP+1, &target, sizeof(target));
		memmove(R.MP+1, &target, sizeof(target));
		if(which < 2)
			R.FP = R.MP = which == 0 ? nil : (uchar*)((uintptr)-1-4);
		compareisolated(index);
	}
}


/* Reject invalid metadata before decoding, operand reads or Dis-PC publication. */
static void
headers(void)
{
	REG before;
	Module *mp;
	Modlink *ml;
	char *failure;
	int which, a;

	for(a = 0; a < 8; a++){
		prepare(0, a);
		memmove(modulebytes+a, &module, sizeof(module));
		mp = (Module*)(modulebytes+a);
		memmove(linkbytes+a, &instance, sizeof(instance));
		ml = (Modlink*)(linkbytes+a);
		memmove((uchar*)ml+O(Modlink, m), &mp, sizeof(mp));
		R.M = ml;
		check(invoke(1, 0) == nil);
	}
	for(which = 0; which < 15; which++){
		prepare(0, 0);
		if(which < 3)
			R.M = which == 0 ? nil : which == 1 ? H : (Modlink*)((uintptr)-1-1);
		else if(which < 6)
			instance.m = which == 3 ? nil : which == 4 ? H : (Module*)((uintptr)-1-1);
		else if(which < 9)
			instance.prog = which == 6 ? nil : which == 7 ? H : (Inst*)((uintptr)-1-sizeof(Inst));
		else if(which < 12)
			R.PC = (Inst*)(which == 9 ? (uchar*)((uintptr)instance.prog-1) : which == 10 ? (uchar*)instance.prog+1 : (uchar*)instance.prog+sizeof(original));
		else
			module.nprog = which == 12 ? -1 : which == 13 ? 0 : ProgramCount-1;
		before = R;
		failure = invoke(1, 0);
		check(failure == (which < 6 ? exModule : exBounds));
		check(!memcmp(&before, &R, sizeof(R)));
	}
}


/* Select independent alignment, decoder, fallback and rejection regression families. */
int
main(int argc, char **argv)
{
	check(argc == 2 && native_register() && translation != nil);
	if(!strcmp(argv[1], "slots"))
		slots();
	else if(!strcmp(argv[1], "addresses"))
		addresses();
	else if(!strcmp(argv[1], "fallback"))
		fallback();
	else if(!strcmp(argv[1], "invalid"))
		invalid();
	else if(!strcmp(argv[1], "decode"))
		decode();
	else
		headers();
	printf("PASS dis2c native %s WORD%zu: %lu predicates\n", argv[1], sizeof(WORD)*8, checks);
	return 0;
}
