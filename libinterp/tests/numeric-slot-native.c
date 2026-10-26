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
    void *s; // Borrowed source or exponent slot.
    void *d; // Borrowed result slot.
    void *m; // Borrowed scale or base slot.
};

static struct Registers R; // Synchronous VM invocation state.
static jmp_buf trap; // Active exception boundary.
static char exBounds[] = "bounds"; // Invalid operand span identity.
static char exZdiv[] = "zero"; // Negative integer power of zero.
static char exInval[] = "invalid"; // Out-of-range real conversion.
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
                fprintf(stderr, "missing numeric exception\n");
                exit(1);
            }
        }else if(raised != expectederror){
            fprintf(stderr, "numeric error priority mismatch\n");
            exit(1);
        }
        if(memcmp(memory,expected,sizeof(before)) || memcmp(&R,&saved,sizeof(R)) ||
           fetestexcept(FE_ALL_EXCEPT) != flags){
            fprintf(stderr, "numeric memory/REG/FP mismatch\n");
            exit(1);
        }
        checks++;
    }
}


/* Test nine numeric handlers and every finite normal power scale in this WORD model. */
int
main(void)
{
    void (*handlers[])(void) = {addf,subf,mulf,divf,cvtxf,cvtfx,iexpw,iexpl,iexpf};
    int opcodes[] = {IADDF,ISUBF,IMULF,IDIVF,ICVTXF,ICVTFX,IEXPW,IEXPL,IEXPF};
    int rounds[] = {FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};
    LeafLayout abi = {0};
    unsigned char memory[512], expected[512];
    void *code;
    uintptr bad;
    int op, slot, fault, alignment, sw, mw, dw, width, n, exponent, shift, sign, mode, alias, flags;
    WORD number, answer;
    REAL source, scale, product;
    LONG big;
    volatile REAL x, y;
    fenv_t saved;

    if(fegetenv(&saved))
        return 1;
    abi.wordbytes = sizeof(WORD);
    abi.pc = offsetof(struct Registers, PC);
    abi.s = offsetof(struct Registers, s);
    abi.d = offsetof(struct Registers, d);
    abi.m = offsetof(struct Registers, m);
    code = mmap(nil,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(code == MAP_FAILED)
        return 1;
    for(op = 0; op < 9; op++){
        if(getenv("NUMERIC_SLOT_OP") && op != atoi(getenv("NUMERIC_SLOT_OP")))
            continue;
        if(mprotect(code,4096,PROT_READ|PROT_WRITE))
            return 1;
        n = leafemit(code,1024,&abi,opcodes[op]);
        if(n <= 0 || mprotect(code,4096,PROT_READ|PROT_EXEC))
            return 1;
        sw = op == 4 || op >= 6 ? sizeof(WORD) : 8;
        mw = op == 6 ? sizeof(WORD) : 8;
        dw = op == 5 || op == 6 ? sizeof(WORD) : 8;
        for(alignment = 0; alignment < 8; alignment++){
            for(slot = 0; slot < 3; slot++){
                width = slot == 0 ? sw : slot == 1 ? mw : dw;
                for(fault = 0; fault <= width; fault++){
                    memset(memory,0xa5,sizeof(memory));
                    R.s = memory+32+alignment;
                    R.m = memory+128+alignment;
                    R.d = memory+256+alignment;
                    R.PC = memory+400;
                    source = scale = 1.;
                    number = 1;
                    big = 1;
                    memcpy(R.s,sw == 8 && op != 4 && op < 6 ? (void *)&source : (void *)&number,sw);
                    memcpy(R.m,op == 6 ? (void *)&number : op == 7 ? (void *)&big : (void *)&scale,mw);
                    bad = fault == 0 ? 0 : (uintptr)-fault;
                    if(slot == 0){ R.s = (void *)bad; R.m = (void *)1; }
                    if(slot == 1){ R.m = (void *)bad; R.s = (void *)1; }
                    if(slot == 2){
                        R.d = (void *)bad;
                        if(op < 6 || op == 8) R.s = R.m = (void *)1;
                    }
                    memcpy(expected,memory,sizeof(expected));
                    verify(handlers[op],code,memory,exBounds,0,expected,FE_DIVBYZERO);
                }
            }
            if(op == 6 || op == 7){
                memset(memory,0,sizeof(memory));
                number = -1;
                memcpy(memory+32+alignment,&number,sizeof(number));
                R.s = memory+32+alignment;
                R.m = memory+128+alignment;
                R.PC = memory+400;
                memcpy(expected,memory,sizeof(expected));
                for(fault = 0; fault < 10; fault++){
                    R.d = fault == 9 ? (void *)1 : fault == 0 ? nil : (void *)((uintptr)-fault);
                    verify(handlers[op],code,memory,exZdiv,0,expected,FE_DIVBYZERO);
                }
            }
        }
        if(op != 5)
            continue;
        for(exponent = -1022; exponent <= 1023; exponent++){
            shift = -exponent;
            if(shift < -1022) shift = -1022;
            if(shift > 1023) shift = 1023;
            for(sign = 0; sign < 4; sign++){
                source = ldexp(sign&1 ? -1. : 1.,shift);
                scale = ldexp(sign&2 ? -1. : 1.,exponent);
                for(mode = 0; mode < 4; mode++){
                    if(fesetround(rounds[mode]))
                        return 1;
                    for(alias = 0; alias < 4; alias++){
                        memset(memory,0xa5,sizeof(memory));
                        R.s = memory+33;
                        R.m = memory+129;
                        R.d = alias == 0 ? memory+257 : alias == 1 ? R.s : alias == 2 ? R.m : (char *)R.s+1;
                        R.PC = memory+400;
                        memcpy(R.s,&source,8);
                        memcpy(R.m,&scale,8);
                        memcpy(expected,memory,sizeof(expected));
                        feclearexcept(FE_ALL_EXCEPT);
                        feraiseexcept(FE_DIVBYZERO);
                        x = source; y = scale;
                        product = x*y;
                        product = product < 0 ? product-.5 : product+.5;
                        answer = (WORD)product;
                        flags = fetestexcept(FE_ALL_EXCEPT);
                        memcpy(expected+((unsigned char *)R.d-memory),&answer,sizeof(answer));
                        verify(handlers[op],code,memory,nil,1,expected,flags);
                    }
                }
            }
        }
    }
    if(munmap(code,4096) || fesetenv(&saved))
        return 1;
    printf("PASS numeric slots and all normal power scales C/RX WORD%d: %lu cases\n",WORD_BITS,checks);
    return 0;
}
