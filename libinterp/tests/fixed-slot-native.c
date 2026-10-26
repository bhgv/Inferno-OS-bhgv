#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <sys/mman.h>
#include <fenv.h>
#include <math.h>
#include "isa.h"

typedef unsigned char uchar;
typedef uint32_t u32int;
typedef uint8_t BYTE;
typedef double REAL;
typedef uint64_t ULONG;
typedef uint64_t uvlong;
#if WORD_BITS == 32
typedef uint32_t UWORD;
#else
typedef uint64_t UWORD;
#endif
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
#include "amd64-emit.h"

struct Registers {
    void *PC; // Published Dis instruction pointer.
    void *s; // Borrowed source operand slot.
    void *d; // Borrowed result slot.
    void *m; // Borrowed middle operand slot.
    uchar *FP; // Borrowed frame bytes with four-byte field offsets.
};

static struct Registers R; // Synchronous VM invocation state.
static jmp_buf trap; // Active exception boundary.
static char exNilref[] = "nil"; // Missing borrowed frame identity.
static char exBounds[] = "bounds"; // Invalid operand span identity.
static char exZdiv[] = "zero"; // Zero fixed-point divisor or coefficient.
static char exInval[] = "invalid"; // Invalid fixed shift or rounding mask.
static char *raised; // Borrowed exception identity.
static unsigned long checks; // Executed C/native fixture cases.


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


/* Execute C and RX with identical borrowed images and seeded FP state.
 * An invalid native path must decline without stores; C then raises the expected
 * borrowed identity. Success publishes only expected result bytes; no blocking.
 */
static void
verify(void (*handler)(void), int (*leaf)(void *), unsigned char *memory,
       char *expectederror, int native, unsigned char *expected, int flags)
{
    struct Registers saved;
    unsigned char before[512];
    int mode, result;

    memcpy(before, memory, sizeof(before));
    memcpy(&saved, &R, sizeof(saved));
    for(mode = 0; mode < 2; mode++){
        memcpy(memory, before, sizeof(before));
        memcpy(&R, &saved, sizeof(R));
        feclearexcept(FE_ALL_EXCEPT);
        feraiseexcept(FE_DIVBYZERO);
        raised = nil;
        if(setjmp(trap) == 0){
            result = mode ? leaf(&R) : 0;
            if(mode && (result != native || (!result &&
               (memcmp(memory,before,sizeof(before)) || memcmp(&R,&saved,sizeof(R)) ||
                fetestexcept(FE_ALL_EXCEPT) != FE_DIVBYZERO)))){
                fprintf(stderr, "native decline/completion/image/FP mismatch\n");
                exit(1);
            }
            if(!result)
                handler();
            if(expectederror){
                fprintf(stderr, "missing fixed exception\n");
                exit(1);
            }
        }else if(raised != expectederror){
            fprintf(stderr, "fixed error priority mismatch\n");
            exit(1);
        }
        if(memcmp(memory,expected,sizeof(before)) || memcmp(&R,&saved,sizeof(R)) ||
           fetestexcept(FE_ALL_EXCEPT) != flags){
            fprintf(stderr, "fixed memory/REG/FP mismatch\n");
            exit(1);
        }
        checks++;
    }
}


/* Test all nine fixed commands against explicit slot/frame and scalar oracles. */
int
main(void)
{
    void (*handlers[])(void) = {mulx,divx,cvtxx,mulx0,divx0,cvtxx0,mulx1,divx1,cvtxx1};
    int opcodes[] = {IMULX,IDIVX,ICVTXX,IMULX0,IDIVX0,ICVTXX0,IMULX1,IDIVX1,ICVTXX1};
    LeafLayout abi = {0};
    unsigned char memory[512], expected[512];
    void *code;
    uintptr bad;
    WORD one = 1, zero = 0, x, y, coefficient, result, scale;
    WORD invalidscales[] = {-64,64,(WORD)((UWORD)1<<(WORD_BITS-1)),(WORD)(((UWORD)1<<(WORD_BITS-1))-1)};
    LONG numerator, divisor;
    int op, n, alignment, slot, fault, extent, alias, i, j, c;
    char *failure;

    abi.wordbytes = sizeof(WORD);
    abi.pc = offsetof(struct Registers, PC);
    abi.s = offsetof(struct Registers, s);
    abi.d = offsetof(struct Registers, d);
    abi.m = offsetof(struct Registers, m);
    abi.fp = offsetof(struct Registers, FP);
    abi.fixedcoef = NREG*IBY2WD;
    abi.fixedtmp = (NREG+2)*IBY2WD;
    code = mmap(nil,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(code == MAP_FAILED)
        return 1;
    for(op = 0; op < 9; op++){
        if(getenv("FIXED_SLOT_OP") && op != atoi(getenv("FIXED_SLOT_OP")))
            continue;
        if(mprotect(code,4096,PROT_READ|PROT_WRITE))
            return 1;
        n = leafemit(code,1024,&abi,opcodes[op]);
        if(n <= 0 || mprotect(code,4096,PROT_READ|PROT_EXEC))
            return 1;
        for(alignment = 0; alignment < 8; alignment++){
            for(slot = 0; slot < 3; slot++){
                for(fault = 0; fault <= (int)sizeof(WORD); fault++){
                    memset(memory,0xa5,sizeof(memory));
                    R.s = memory+32+alignment;
                    R.m = memory+128+alignment;
                    R.d = memory+256+alignment;
                    R.FP = memory+320+alignment;
                    R.PC = memory+480;
                    memcpy(R.s,&one,sizeof(one));
                    memcpy(R.m,op == 2 || op == 5 || op == 8 ? &zero : &one,sizeof(one));
                    memcpy(R.FP+abi.fixedcoef,&one,sizeof(one));
                    memcpy(R.FP+abi.fixedtmp,&zero,sizeof(zero));
                    bad = fault == 0 ? 0 : (uintptr)-fault;
                    if(slot == 0){
                        R.s = (void *)bad;
                        if(op != 2) R.m = (void *)1;
                    }
                    if(slot == 1){
                        R.m = (void *)bad;
                        if(op != 1) R.s = (void *)1;
                    }
                    if(slot == 2){
                        R.d = (void *)bad;
                        if(op == 0) R.s = R.m = (void *)1;
                        if(op == 1) R.m = (void *)1;
                        if(op == 2) R.s = (void *)1;
                    }
                    memcpy(expected,memory,sizeof(expected));
                    verify(handlers[op],code,memory,exBounds,0,expected,FE_DIVBYZERO);
                }
            }
            /* A missing/wrapped frame must fail without changing any borrowed bytes. */
            if(op != 2){
                extent = (op == 5 || op == 8 ? abi.fixedcoef : abi.fixedtmp)+sizeof(WORD);
                for(fault = 0; fault <= extent; fault++){
                    memset(memory,0xa5,sizeof(memory));
                    R.s = memory+32+alignment;
                    R.m = memory+128+alignment;
                    R.d = memory+256+alignment;
                    R.PC = memory+480;
                    memcpy(R.s,&one,sizeof(one));
                    memcpy(R.m,op == 5 || op == 8 ? &zero : &one,sizeof(one));
                    R.FP = fault == 0 ? nil : (uchar *)((uintptr)-fault);
                    memcpy(expected,memory,sizeof(expected));
                    verify(handlers[op],code,memory,fault < 2 ? exNilref : exBounds,0,expected,FE_DIVBYZERO);
                }
            }
            /* Zero divisors retain their priority over malformed result slots. */
            if(op != 0 && op != 2){
                memset(memory,0xa5,sizeof(memory));
                R.s = memory+32+alignment;
                R.m = memory+128+alignment;
                R.FP = memory+320+alignment;
                R.PC = memory+480;
                memcpy(R.s,op == 1 || op == 4 || op == 7 ? &zero : &one,sizeof(one));
                memcpy(R.m,op == 5 || op == 8 ? &zero : &one,sizeof(one));
                memcpy(R.FP+abi.fixedcoef,&zero,sizeof(zero));
                memcpy(R.FP+abi.fixedtmp,&zero,sizeof(zero));
                for(fault = 0; fault < 3; fault++){
                    R.d = fault == 0 ? nil : fault == 1 ? H : (void *)1;
                    memcpy(expected,memory,sizeof(expected));
                    verify(handlers[op],code,memory,exZdiv,0,expected,FE_DIVBYZERO);
                }
            }
            if(op == 0 || op == 2){
                /* Scale errors precede inaccessible scalar inputs and results. */
                for(fault = 0; fault < 4; fault++){
                    memset(memory,0xa5,sizeof(memory));
                    R.s = R.d = (void *)1;
                    R.m = op == 2 ? memory+128+alignment : (void *)1;
                    R.FP = memory+320+alignment;
                    R.PC = memory+480;
                    memcpy(R.FP+abi.fixedtmp,&invalidscales[fault],sizeof(WORD));
                    if(op == 2)
                        memcpy(R.m,&invalidscales[fault],sizeof(WORD));
                    memcpy(expected,memory,sizeof(expected));
                    verify(handlers[op],code,memory,exInval,0,expected,FE_DIVBYZERO);
                }
            }
            if(op >= 3){
                /* Coefficient zero shortcuts retain C handling of unused scales. */
                memset(memory,0xa5,sizeof(memory));
                R.s = memory+32+alignment;
                R.m = memory+128+alignment;
                R.d = memory+256+alignment;
                R.FP = memory+320+alignment;
                R.PC = memory+480;
                scale = op >= 6 ? 256 : 64;
                memcpy(R.s,op == 5 || op == 8 ? &zero : &one,sizeof(WORD));
                memcpy(R.m,op == 5 || op == 8 ? &scale : &zero,sizeof(WORD));
                memcpy(R.FP+abi.fixedcoef,&one,sizeof(WORD));
                memcpy(R.FP+abi.fixedtmp,&scale,sizeof(WORD));
                memcpy(expected,memory,sizeof(expected));
                memcpy(expected+256+alignment,&zero,sizeof(WORD));
                verify(handlers[op],code,memory,nil,0,expected,FE_DIVBYZERO);
            }
        }
        /* Small independent arithmetic covers every result/source/frame alias. */
        for(i = -3; i <= 3; i++){
            for(j = -3; j <= 3; j++){
                for(c = -2; c <= 2; c++){
                    if(c == 0) continue;
                    for(alias = 0; alias < 5; alias++){
                        memset(memory,0xa5,sizeof(memory));
                        R.s = memory+33;
                        R.m = memory+129;
                        R.d = alias == 0 ? memory+257 : alias == 1 ? R.s : alias == 2 ? R.m : alias == 3 ? (uchar *)R.s+1 : memory+341;
                        R.FP = op == 2 ? nil : memory+321;
                        R.PC = memory+480;
                        x = i; y = j; coefficient = c; scale = 0;
                        memcpy(R.s,&y,sizeof(y));
                        memcpy(R.m,op == 2 || op == 5 || op == 8 ? &scale : &x,sizeof(x));
                        memcpy(memory+321+abi.fixedcoef,&coefficient,sizeof(coefficient));
                        memcpy(memory+321+abi.fixedtmp,&scale,sizeof(scale));
                        failure = (op == 1 || op == 4 || op == 7) && y == 0 ? exZdiv : nil;
                        numerator = op == 0 || op == 3 || op == 6 ? (LONG)x*y : op == 1 ? x : op == 4 || op == 7 ? (LONG)coefficient*x : y;
                        divisor = op == 1 || op == 4 || op == 7 ? y : op == 0 || op == 2 ? 1 : coefficient;
                        result = failure ? 0 : numerator/divisor;
                        memcpy(expected,memory,sizeof(expected));
                        if(!failure)
                            memcpy(expected+((uchar *)R.d-memory),&result,sizeof(result));
                        verify(handlers[op],code,memory,failure,!failure,expected,FE_DIVBYZERO);
                    }
                }
            }
        }
    }
    if(munmap(code,4096))
        return 1;
    printf("PASS fixed slot/frame/error/alias C/RX WORD%d: %lu cases\n",WORD_BITS,checks);
    return 0;
}
