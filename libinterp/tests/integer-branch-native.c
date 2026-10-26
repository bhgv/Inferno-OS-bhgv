#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <sys/mman.h>
#include "isa.h"

typedef unsigned char uchar;
typedef uint32_t u32int;
typedef uint8_t BYTE;
typedef int64_t LONG;
typedef uintptr_t uintptr;
#if WORD_BITS == 32
typedef int32_t WORD;
#else
typedef int64_t WORD;
#endif
#define nil ((void *)0) // Native null pointer.
#define H ((void *)(uintptr)-1) // Dis sentinel pointer.
#define OP(name) static void name(void) // Extracted production handler signature.
#define JMP(r) memmove(&R.PC, R.r, sizeof(R.PC)) // Byte-safe native target load.
#include "amd64-emit.h"

struct Registers {
    void *PC; // Published Dis instruction pointer.
    void *s; // Borrowed left operand slot.
    void *d; // Borrowed target slot.
    void *m; // Borrowed right operand slot.
};

static struct Registers R; // Synchronous VM invocation state.
static jmp_buf trap; // Active exception boundary.
static char exBounds[] = "bounds"; // Invalid operand span identity.
static char *raised; // Borrowed exception identity.
static unsigned long checks; // Number of executed comparisons.


/* Raise the borrowed VM identity without modifying operand bytes or registers. */
static void
error(char *message)
{
    raised = message;
    longjmp(trap, 1);
}


/* This fixture owns no leaks; keep address/undefined and stack instrumentation active. */
int
__lsan_is_turned_off(void)
{
    return 1;
}

IMPLEMENTATION


/* Evaluate an independent signed/unsigned predicate over captured scalar values. */
static int
predicate(int condition, int64_t left, int64_t right)
{
    switch(condition){
    case 0: return left == right;
    case 1: return left != right;
    case 2: return left < right;
    case 3: return left <= right;
    case 4: return left > right;
    default: return left >= right;
    }
}


/* Capture one scalar in its VM width without imposing native alignment. */
static int64_t
value(void *slot, int width)
{
    BYTE byte;
    int32_t word;
    int64_t big;

    if(width == 1){
        memcpy(&byte, slot, 1);
        return byte;
    }
    if(width == 4){
        memcpy(&word, slot, 4);
        return word;
    }
    memcpy(&big, slot, 8);
    return big;
}


/* Execute C and RX AMD64 against one byte/PC oracle, including C after native decline.
 * memory is a borrowed 512-byte image; bounds errors and decline must be atomic.
 * No VM ownership is transferred and no operation blocks.
 */
static void
verify(void (*handler)(void), int (*leaf)(void *), unsigned char *memory,
       int invalid, void *expectedpc)
{
    struct Registers saved, expected;
    unsigned char before[512];
    int mode, result;

    memcpy(before, memory, sizeof(before));
    memcpy(&saved, &R, sizeof(saved));
    memcpy(&expected, &R, sizeof(expected));
    expected.PC = expectedpc;
    for(mode = 0; mode < 2; mode++){
        memcpy(&R, &saved, sizeof(R));
        raised = nil;
        if(setjmp(trap) == 0){
            result = mode ? leaf(&R) : 0;
            if(mode && result != !invalid){
                fprintf(stderr, "native completion mismatch\n");
                exit(1);
            }
            if(!result)
                handler();
            if(invalid){
                fprintf(stderr, "missing bounds exception\n");
                exit(1);
            }
        }else if(!invalid || raised != exBounds){
            fprintf(stderr, "unexpected bounds exception\n");
            exit(1);
        }
        if(memcmp(before, memory, sizeof(before)) ||
           memcmp(&R, invalid ? &saved : &expected, sizeof(R))){
            fprintf(stderr, "branch image/PC mismatch\n");
            exit(1);
        }
        checks++;
    }
}


/* Run all eighteen predicates and JMP in C and native AMD64, for this WORD model. */
int
main(void)
{
    void (*handlers[])(void) = {beqb,bneb,bltb,bleb,bgtb,bgeb,
        beqw,bnew,bltw,blew,bgtw,bgew,beql,bnel,bltl,blel,bgtl,bgel,jmp};
    int opcodes[] = {IBEQB,IBNEB,IBLTB,IBLEB,IBGTB,IBGEB,
        IBEQW,IBNEW,IBLTW,IBLEW,IBGTW,IBGEW,IBEQL,IBNEL,IBLTL,IBLEL,IBGTL,IBGEL,IJMP};
    uint64_t edges[] = {0,1,2,127,128,255,0x7fffffff,0x80000000,
        0xffffffff,0x7fffffffffffffffULL,0x8000000000000000ULL,0xffffffffffffffffULL};
    LeafLayout abi = {0};
    unsigned char memory[512];
    void *code, *target, *oldpc;
    uintptr bad;
    int op, width, alignment, left, right, take, slot, fault, n, alias;

    abi.wordbytes = sizeof(WORD);
    abi.pc = offsetof(struct Registers, PC);
    abi.s = offsetof(struct Registers, s);
    abi.d = offsetof(struct Registers, d);
    abi.m = offsetof(struct Registers, m);
    code = mmap(nil, 4096, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if(code == MAP_FAILED)
        return 1;
    for(op = 0; op < 19; op++){
        if(getenv("INTEGER_BRANCH_OP") && op != atoi(getenv("INTEGER_BRANCH_OP")))
            continue;
        if(mprotect(code, 4096, PROT_READ|PROT_WRITE))
            return 1;
        n = leafemit(code, 1024, &abi, opcodes[op]);
        if(n <= 0 || mprotect(code, 4096, PROT_READ|PROT_EXEC))
            return 1;
        width = op < 6 ? 1 : op < 12 ? sizeof(WORD) : 8;
        for(alignment = 0; alignment < 8; alignment++){
            for(left = 0; left < 12; left++){
                for(right = 0; right < 12; right++){
                    memset(memory, 0xa5, sizeof(memory));
                    R.s = memory+32+alignment;
                    R.m = memory+128+alignment;
                    R.d = memory+256+alignment;
                    oldpc = R.PC = memory+400;
                    target = memory+448;
                    memcpy(R.s, &edges[left], width);
                    memcpy(R.m, &edges[right], width);
                    memcpy(R.d, &target, sizeof(target));
                    take = op == 18 || predicate(op%6, value(R.s,width), value(R.m,width));
                    verify(handlers[op], code, memory, 0, take ? target : oldpc);
                    /* False branches must tolerate even an unmapped, non-sentinel target. */
                    if(!take){
                        R.PC = oldpc;
                        R.d = (void *)1;
                        verify(handlers[op], code, memory, 0, oldpc);
                    }
                    /* Aliased target bytes must be read after capturing both operands. */
                    for(alias = 1; alias <= 3; alias++){
                        memcpy(R.s, &edges[left], width);
                        memcpy(R.m, &edges[right], width);
                        R.d = alias == 1 ? R.s : alias == 2 ? R.m : (unsigned char *)R.s+1;
                        memcpy(R.d, &target, sizeof(target));
                        R.PC = oldpc;
                        take = op == 18 || predicate(op%6, value(R.s,width), value(R.m,width));
                        verify(handlers[op], code, memory, 0, take ? target : oldpc);
                    }
                }
            }
            for(slot = op == 18 ? 2 : 0; slot < 3; slot++){
                for(fault = 0; fault <= (slot == 2 ? 8 : width); fault++){
                    memset(memory, 0xa5, sizeof(memory));
                    R.s = memory+32+alignment;
                    R.m = memory+128+alignment;
                    R.d = memory+256+alignment;
                    R.PC = memory+400;
                    left = 1;
                    right = op%6 == 1 || op%6 == 2 ? 2 : op%6 == 4 ? 0 : 1;
                    memset(R.s, 0, width);
                    memset(R.m, 0, width);
                    memcpy(R.s, &left, 1);
                    memcpy(R.m, &right, 1);
                    bad = fault == 0 ? 0 : (uintptr)-fault;
                    if(slot == 0){ R.s = (void *)bad; R.m = (void *)1; }
                    if(slot == 1){ R.m = (void *)bad; R.s = (void *)1; }
                    if(slot == 2) R.d = (void *)bad;
                    verify(handlers[op], code, memory, 1, R.PC);
                    if(slot == 2 && op != 18){
                        /* Invert that predicate without changing the malformed target. */
                        right = op%6 == 0 || op%6 == 3 ? 0 : op%6 == 5 ? 2 : 1;
                        memcpy(R.m, &right, 1);
                        verify(handlers[op], code, memory, 0, R.PC);
                    }
                }
            }
        }
    }
    if(munmap(code,4096))
        return 1;
    printf("PASS integer branches C/RX WORD%d: %lu cases\n", WORD_BITS, checks);
    return 0;
}
