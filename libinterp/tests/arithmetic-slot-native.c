#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#if WORD_BITS == 32
typedef int32_t WORD; /* VM integer width under test. */
typedef uint32_t UWORD; /* Unsigned VM word bits. */
#else
typedef int64_t WORD; /* VM integer width under test. */
typedef uint64_t UWORD; /* Unsigned VM word bits. */
#endif
typedef uint8_t BYTE; /* Byte-valued arithmetic operand. */
typedef double REAL; /* Binary64 comparison operands. */
typedef int64_t LONG; /* Signed big arithmetic operand. */
typedef uint64_t ULONG; /* Unsigned big arithmetic bits. */
typedef uintptr_t uintptr; /* Native pointer arithmetic width. */
#define nil ((void *)0) /* Native null slot. */
#define H ((void *)(uintptr)-1) /* Dis sentinel slot. */
#define JMP(r) branchpc(R.r, exBounds) /* Use the captured-target guard from production. */
#define OP(name) static void name(void) /* Actual handler extraction. */


/* Minimal borrowed register view: arithmetic handlers use only these fields. */
typedef struct Registers {
    void *s; /* Divisor or left comparison slot. */
    void *m; /* Dividend or right comparison slot. */
    void *d; /* Result or target slot. */
    void *PC; /* Sentinel preserved through exceptions. */
} Registers;

static Registers R; /* One synchronous fixture invocation. */
static jmp_buf trap; /* Active VM exception boundary. */
static const char *raised; /* Last borrowed error identity. */
static char exBounds[] = "bounds"; /* Invalid decoded slot. */
static char exZdiv[] = "zero"; /* Division by zero. */
static unsigned long checks; /* Successful independent fixture cases. */


/* Raise borrowed message through the active trap; preserve registers and memory. */
static void error(char *message)
{
    raised = message;
    longjmp(trap, 1);
}


/* Return one to disable unsupported leak enumeration in the sandbox.
 * No arguments or ownership are involved. Address/undefined instrumentation
 * remains enabled; this stack-only fixture is not a leak-detection oracle.
 */
int __lsan_is_turned_off(void)
{
    return 1;
}

IMPLEMENTATION


/* Execute a borrowed handler; require the requested error and byte-atomic state.
 * memory is a 512-byte fixture image, expected_error is a stable VM identity.
 * This synchronous check exits on mismatch; it takes no ownership and cannot block.
 */
static void failure(void (*operation)(void), unsigned char *memory, const char *expected_error)
{
    unsigned char before[512];
    Registers saved;

    memcpy(before, memory, sizeof(before));
    memcpy(&saved, &R, sizeof(saved));
    raised = NULL;
    if (setjmp(trap) == 0) {
        operation();
        fprintf(stderr, "missing arithmetic exception\n");
        exit(1);
    }
    if (raised != expected_error || memcmp(before, memory, sizeof(before)) != 0 ||
        memcmp(&saved, &R, sizeof(saved)) != 0) {
        fprintf(stderr, "arithmetic error priority/state mismatch: %s\n", raised);
        exit(1);
    }
    checks++;
}


/* Execute a successful branch with borrowed memory and expected PC identity.
 * No ownership transfer or blocking occurs. Unexpected errors exit the fixture.
 */
static void branch_success(void (*operation)(void), unsigned char *memory, void *pc)
{
    unsigned char before[512];
    Registers saved;

    memcpy(before, memory, sizeof(before));
    memcpy(&saved, &R, sizeof(saved));
    saved.PC = pc;
    if (setjmp(trap) != 0) {
        fprintf(stderr, "unexpected branch bounds exception\n");
        exit(1);
    }
    operation();
    if (memcmp(before, memory, sizeof(before)) != 0 || memcmp(&saved, &R, sizeof(saved)) != 0) {
        fprintf(stderr, "branch target/image mismatch\n");
        exit(1);
    }
    checks++;
}


/* Run twelve actual C handlers in this WORD model; return zero on success.
 * No arguments, worker threads or ownership transfers are involved.
 */
int main(void)
{
    void (*ops[])(void) = {divb, modb, divw, modw, divl, modl,
                         beqf, bnef, bltf, blef, bgtf, bgef};
    double truth[][2] = {{1,1},{1,2},{1,2},{1,1},{2,1},{1,1}};
    double falsity[][2] = {{1,2},{1,1},{2,1},{2,1},{1,2},{1,2}};
    unsigned char memory[512];
    int op;
    int alignment;
    int slot;
    int fault;
    int width;
    uintptr pointer;
    ULONG divisor;
    void *target;
    int take;

    for (op = 0; op < 12; op++) {
        if (getenv("ARITH_SLOT_OP") != NULL && op != atoi(getenv("ARITH_SLOT_OP"))) {
            continue;
        }
        width = op >= 6 ? 8 : op < 2 ? 1 : op < 4 ? sizeof(WORD) : 8;
        for (alignment = 0; alignment < 8; alignment++) {
            for (slot = 0; slot < 3; slot++) {
                for (fault = 0; fault <= width; fault++) {
                    memset(memory, 0xa5, sizeof(memory));
                    R.s = memory + 32 + alignment;
                    R.m = memory + 128 + alignment;
                    R.d = memory + 256 + alignment;
                    R.PC = memory + 400;
                    if (op >= 6) {
                        memcpy(R.s, &truth[op-6][0], 8);
                        memcpy(R.m, &truth[op-6][1], 8);
                    } else {
                        divisor = 1;
                        memcpy(R.s, &divisor, width);
                    }
                    pointer = fault == 0 ? 0 : (uintptr)-1 - (fault - 1);
                    if (slot == 0) {
                        R.s = (void *)pointer;
                        if (op >= 6) {
                            R.m = (void *)1;
                        }
                    } else if (slot == 1) {
                        R.m = (void *)pointer;
                        if (op >= 6) {
                            R.s = (void *)1;
                        }
                    } else {
                        R.d = (void *)pointer;
                        if (op < 6) {
                            R.m = (void *)1;
                        }
                    }
                    failure(ops[op], memory, exBounds);
                    if (op >= 6 && slot == 2) {
                        memcpy(R.s, &falsity[op-6][0], 8);
                        memcpy(R.m, &falsity[op-6][1], 8);
                        branch_success(ops[op], memory, R.PC);
                    }
                }
            }
            if (op < 6) {
                memset(memory, 0xa5, sizeof(memory));
                R.s = memory + 32 + alignment;
                R.m = R.d = (void *)1;
                R.PC = memory + 400;
                divisor = 0;
                memcpy(R.s, &divisor, width);
                failure(ops[op], memory, exZdiv);
            } else {
                for (take = 0; take < 2; take++) {
                    memset(memory, 0xa5, sizeof(memory));
                    R.s = memory + 32 + alignment;
                    R.m = memory + 128 + alignment;
                    R.d = memory + 256 + alignment;
                    R.PC = memory + 400;
                    target = memory + 480;
                    memcpy(R.s, &(take ? truth : falsity)[op-6][0], 8);
                    memcpy(R.m, &(take ? truth : falsity)[op-6][1], 8);
                    memcpy(R.d, &target, sizeof(target));
                    if (!take) {
                        R.d = (void *)1;
                    }
                    branch_success(ops[op], memory, take ? target : R.PC);
                }
            }
        }
    }
    printf("PASS C division/real-branch slot/error/target: %lu cases WORD%d\n", checks, WORD_BITS);
    return 0;
}
