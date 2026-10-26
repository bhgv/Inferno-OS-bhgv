#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#if WORD_BITS == 32
typedef int32_t WORD; /* VM count width under test. */
typedef uint32_t UWORD; /* Unsigned VM word bits. */
#else
typedef int64_t WORD; /* VM count width under test. */
typedef uint64_t UWORD; /* Unsigned VM word bits. */
#endif
typedef uint8_t BYTE; /* Byte-valued shift operand. */
typedef int64_t LONG; /* Signed big shift operand. */
typedef uint64_t ULONG; /* Unsigned big shift bits. */
typedef uintptr_t uintptr; /* Native pointer arithmetic width. */
#define nil ((void *)0) /* Native null slot. */
#define H ((void *)(uintptr)-1) /* Dis sentinel slot. */
#define OP(name) static void name(void) /* Actual handler extraction. */


/* Minimal borrowed register view: shift handlers use only these fields. */
typedef struct Registers {
    void *s; /* Count slot. */
    void *m; /* Value slot. */
    void *d; /* Result slot. */
    void *PC; /* Sentinel preserved through exceptions. */
} Registers;

static Registers R; /* One synchronous fixture invocation. */
static jmp_buf trap; /* Active VM exception boundary. */
static const char *raised; /* Last borrowed error identity. */
static char exBounds[] = "bounds"; /* Invalid decoded slot. */
static char exInval[] = "invalid"; /* Invalid shift count. */
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
        fprintf(stderr, "missing shift exception\n");
        exit(1);
    }
    if (raised != expected_error || memcmp(before, memory, sizeof(before)) != 0 ||
        memcmp(&saved, &R, sizeof(saved)) != 0) {
        fprintf(stderr, "shift error priority/state mismatch: %s\n", raised);
        exit(1);
    }
    checks++;
}


/* Check all eight production handlers on this WORD model; return nonzero on failure.
 * No arguments, ownership transfer or worker threads are involved.
 */
int main(void)
{
    void (*ops[])(void) = {shlb, shrb, shlw, shrw, lsrw, shll, shrl, lsrl};
    unsigned char memory[512];
    int op;
    int alignment;
    int slot;
    int fault;
    int width;
    int extent;
    WORD count;
    uintptr pointer;

    for (op = 0; op < 8; op++) {
        if (getenv("SHIFT_SLOT_OP") != NULL && op != atoi(getenv("SHIFT_SLOT_OP"))) {
            continue;
        }
        width = op < 2 ? 1 : op < 5 ? sizeof(WORD) : 8;
        for (alignment = 0; alignment < 8; alignment++) {
            for (slot = 0; slot < 3; slot++) {
                extent = slot == 0 ? sizeof(WORD) : width;
                for (fault = 0; fault <= extent; fault++) {
                    memset(memory, 0xa5, sizeof(memory));
                    R.s = memory + 32 + alignment;
                    R.m = memory + 128 + alignment;
                    R.d = memory + 256 + alignment;
                    R.PC = memory + 400;
                    count = 1;
                    memcpy(R.s, &count, sizeof(count));
                    pointer = fault == 0 ? 0 : (uintptr)-1 - (fault - 1);
                    if (slot == 0) {
                        R.s = (void *)pointer;
                    } else if (slot == 1) {
                        R.m = (void *)pointer;
                    } else {
                        R.d = (void *)pointer;
                        R.m = (void *)1; /* Destination bounds must win before value read. */
                    }
                    failure(ops[op], memory, exBounds);
                }
            }
            for (fault = 0; fault < 3; fault++) {
                memset(memory, 0xa5, sizeof(memory));
                R.s = memory + 32 + alignment;
                R.m = (void *)1;
                R.d = (void *)1;
                R.PC = memory + 400;
                count = fault == 0 ? -1 : width * 8 + fault - 1;
                memcpy(R.s, &count, sizeof(count));
                failure(ops[op], memory, exInval);
            }
        }
    }
    printf("PASS C shift slot/count/error-atomicity: %lu cases WORD%d\n", checks, WORD_BITS);
    return 0;
}
