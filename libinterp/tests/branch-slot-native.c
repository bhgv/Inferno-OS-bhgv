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
typedef uint8_t BYTE; /* Byte-valued integer branch operand. */
typedef int64_t LONG; /* Signed big integer branch operand. */
typedef uint64_t ULONG; /* Unsigned big integer branch bits. */
typedef uintptr_t uintptr; /* Native pointer integer branch width. */
#define nil ((void *)0) /* Native null slot. */
#define H ((void *)(uintptr)-1) /* Dis sentinel slot. */
#define JMP(r) memmove(&R.PC, R.r, sizeof(R.PC)) /* Unaligned native target copy. */
#define OP(name) static void name(void) /* Actual handler extraction. */


/* Minimal borrowed register view: integer branch handlers use only these fields. */
typedef struct Registers {
    void *s; /* Left comparison slot; ignored by JMP. */
    void *m; /* Right comparison slot; ignored by JMP. */
    void *d; /* Result or target slot. */
    void *PC; /* Sentinel preserved through exceptions. */
} Registers;

static Registers R; /* One synchronous fixture invocation. */
static jmp_buf trap; /* Active VM exception boundary. */
static const char *raised; /* Last borrowed error identity. */
static char exBounds[] = "bounds"; /* Invalid decoded slot. */

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
        fprintf(stderr, "missing integer branch exception\n");
        exit(1);
    }
    if (raised != expected_error || memcmp(before, memory, sizeof(before)) != 0 ||
        memcmp(&saved, &R, sizeof(saved)) != 0) {
        fprintf(stderr, "integer branch error priority/state mismatch: %s\n", raised);
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


/* Initialize one borrowed comparison cell using its actual BYTE/WORD/LONG type.
 * destination must fit width bytes; value is 1 or 2. No ownership or errors.
 */
static void operand(void *destination, int width, int value)
{
    BYTE byte = value;
    WORD word = value;
    LONG big = value;

    if(width == 1){
        memcpy(destination, &byte, sizeof(byte));
    }else if(width == sizeof(WORD)){
        memcpy(destination, &word, sizeof(word));
    }else{
        memcpy(destination, &big, sizeof(big));
    }
}


/* Run eighteen conditional branches and JMP in this WORD model.
 * No arguments, ownership transfers or worker threads; nonzero exit means failure.
 */
int main(void)
{
    void (*ops[])(void) = {beqb,bneb,bltb,bleb,bgtb,bgeb,
                          beqw,bnew,bltw,blew,bgtw,bgew,
                          beql,bnel,bltl,blel,bgtl,bgel,jmp};
    int truth[][2] = {{1,1},{1,2},{1,2},{1,1},{2,1},{1,1}};
    int falsity[][2] = {{1,2},{1,1},{2,1},{2,1},{1,2},{1,2}};
    unsigned char memory[512];
    int op;
    int alignment;
    int slot;
    int fault;
    int width;
    int extent;
    int take;
    uintptr pointer;
    void *target;

    for(op = 0; op < 19; op++){
        if(getenv("BRANCH_SLOT_OP") != NULL && op != atoi(getenv("BRANCH_SLOT_OP"))){
            continue;
        }
        width = op < 6 ? 1 : op < 12 ? sizeof(WORD) : 8;
        for(alignment = 0; alignment < 8; alignment++){
            for(slot = op == 18 ? 2 : 0; slot < 3; slot++){
                extent = slot == 2 ? sizeof(void *) : width;
                for(fault = 0; fault <= extent; fault++){
                    memset(memory, 0xa5, sizeof(memory));
                    R.s = memory + 32 + alignment;
                    R.m = memory + 128 + alignment;
                    R.d = memory + 256 + alignment;
                    R.PC = memory + 400;
                    if(op != 18){
                        operand(R.s, width, truth[op%6][0]);
                        operand(R.m, width, truth[op%6][1]);
                    }else{
                        R.s = R.m = (void *)1;
                    }
                    pointer = fault == 0 ? 0 : (uintptr)-1 - (fault - 1);
                    if(slot == 0){
                        R.s = (void *)pointer;
                        R.m = (void *)1;
                    }else if(slot == 1){
                        R.m = (void *)pointer;
                        R.s = (void *)1;
                    }else{
                        R.d = (void *)pointer;
                    }
                    failure(ops[op], memory, exBounds);
                    if(op != 18 && slot == 2){
                        operand(R.s, width, falsity[op%6][0]);
                        operand(R.m, width, falsity[op%6][1]);
                        branch_success(ops[op], memory, R.PC);
                    }
                }
            }
            for(take = 0; take < 2; take++){
                memset(memory, 0xa5, sizeof(memory));
                R.s = memory + 32 + alignment;
                R.m = memory + 128 + alignment;
                R.d = memory + 256 + alignment;
                R.PC = memory + 400;
                target = memory + 480;
                if(op != 18){
                    operand(R.s, width, (take ? truth : falsity)[op%6][0]);
                    operand(R.m, width, (take ? truth : falsity)[op%6][1]);
                }else{
                    R.s = R.m = (void *)1;
                }
                memcpy(R.d, &target, sizeof(target));
                if(!take && op != 18){
                    R.d = (void *)1;
                }
                branch_success(ops[op], memory, (take || op == 18) ? target : R.PC);
            }
        }
    }
    printf("PASS C integer branch/target/nonaccess/error: %lu cases WORD%d\n", checks, WORD_BITS);
    return 0;
}
