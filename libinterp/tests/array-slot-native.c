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
    uchar *FP; // Unused fixture register; array queries never inspect frames.
};

static struct Registers R; // Synchronous VM invocation state.
static jmp_buf trap; // Active exception boundary.
static char exBounds[] = "bounds"; // Invalid operand span identity.
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

typedef struct Type Type;
typedef struct Array Array;

struct Type {
    uchar reserved[24]; // Independent descriptor prefix before its signed byte size.
    int size; // Scalar record width; pointer maps are intentionally unused here.
};

struct Array {
    WORD len; // Signed length in the active VM word width.
    Type *t; // Borrowed descriptor identity for generic indexing only.
    Array *root; // Unused ownership link; address queries never inspect it.
    uchar *data; // Borrowed backing address; zero-sized records preserve any identity.
};

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
                fprintf(stderr, "missing array exception\n");
                exit(1);
            }
        }else if(raised != expectederror){
            fprintf(stderr, "array error priority mismatch\n");
            exit(1);
        }
        if(memcmp(memory,expected,sizeof(before)) || memcmp(&R,&saved,sizeof(R)) ||
           fetestexcept(FE_ALL_EXCEPT) != flags){
            fprintf(stderr, "array memory/REG/FP mismatch\n");
            exit(1);
        }
        checks++;
    }
}


/* Test six array commands with independent address, span and alias oracles. */
int
main(void)
{
    void (*handlers[])(void) = {indb,indw,indl,indf,indx,lena};
    int opcodes[] = {IINDB,IINDW,IINDL,IINDF,IINDX,ILENA};
    int strides[] = {0,1,3,7,8,16,-1};
    WORD lengths[] = {-1,0,1,2,7,8};
    WORD indices[] = {-1,0,1,2,6,7,8,(WORD)((UWORD)1<<(WORD_BITS-1)),(WORD)(((UWORD)1<<(WORD_BITS-1))-1)};
    LeafLayout abi = {0};
    unsigned char memory[512], expected[512];
    void *code, *output;
    uintptr owner, descriptor, backing, bad;
    WORD length, index, result;
    int op, n, alignment, slot, fault, width, li, ii, si, dataid, alias, valid;
    char *selection = getenv("ARRAY_SLOT_CASE");

    if(selection && strcmp(selection,"slots") && strcmp(selection,"values") &&
       strcmp(selection,"negative") && strcmp(selection,"headers")){
        fprintf(stderr,"unknown ARRAY_SLOT_CASE\n");
        return 1;
    }
    abi.wordbytes = sizeof(WORD);
    abi.pc = offsetof(struct Registers, PC);
    abi.s = offsetof(struct Registers, s);
    abi.d = offsetof(struct Registers, d);
    abi.m = offsetof(struct Registers, m);
    abi.arraylen = offsetof(Array, len);
    abi.arraytype = offsetof(Array, t);
    abi.arraydata = offsetof(Array, data);
    abi.typesize = offsetof(Type, size);
    code = mmap(nil,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(code == MAP_FAILED)
        return 1;
    for(op = 0; op < 6; op++){
        if(getenv("ARRAY_SLOT_OP") && op != atoi(getenv("ARRAY_SLOT_OP")))
            continue;
        if(mprotect(code,4096,PROT_READ|PROT_WRITE))
            return 1;
        n = leafemit(code,128,&abi,opcodes[op]);
        if(n <= 0 || mprotect(code,4096,PROT_READ|PROT_EXEC))
            return 1;
        if(!selection || strcmp(selection,"slots")==0){
            for(alignment = 0; alignment < 8; alignment++){
                for(slot = 0; slot < (op == 5 ? 2 : 3); slot++){
                    width = slot == 0 ? sizeof(owner) : sizeof(WORD);
                    for(fault = 0; fault <= width; fault++){
                        memset(memory,0xa5,sizeof(memory));
                        R.s = memory+32+alignment;
                        R.d = memory+128+alignment;
                        R.m = memory+256+alignment;
                        R.PC = memory+480;
                        bad = fault == 0 ? 0 : (uintptr)-fault;
                        if(slot == 0){ R.s = (void *)bad; R.d = (void *)1; }
                        if(slot == 1){ R.d = (void *)bad; R.s = (void *)1; }
                        if(slot == 2){ R.m = (void *)bad; R.s = R.d = (void *)1; }
                        memcpy(expected,memory,sizeof(expected));
                        verify(handlers[op],code,memory,exBounds,0,expected,FE_DIVBYZERO);
                    }
                }
                for(fault = 0; fault <= (int)sizeof(WORD); fault++){
                    memset(memory,0xa5,sizeof(memory));
                    R.s = memory+32+alignment;
                    R.d = memory+128+alignment;
                    R.m = op == 5 ? (void *)1 : memory+256+alignment;
                    R.PC = memory+480;
                    owner = fault == 0 ? 0 : (uintptr)-fault;
                    index = 0;
                    memcpy(R.s,&owner,sizeof(owner));
                    if(op != 5) memcpy(R.d,&index,sizeof(index));
                    memcpy(expected,memory,sizeof(expected));
                    valid = op == 5 && fault < 2;
                    result = 0;
                    if(valid) memcpy(expected+128+alignment,&result,sizeof(result));
                    verify(handlers[op],code,memory,valid ? nil : exBounds,valid,expected,FE_DIVBYZERO);
                }
            }
        }
        if(!selection || strcmp(selection,"values")==0){
            for(li = 0; li < 6; li++){
                for(ii = 0; ii < 9; ii++){
                    for(si = 0; si < 7; si++){
                        for(dataid = 0; dataid < 3; dataid++){
                            for(alignment = 0; alignment < 8; alignment++){
                                for(alias = 0; alias < 5; alias++){
                                    memset(memory,0xa5,sizeof(memory));
                                    owner = (uintptr)(memory+320+alignment);
                                    descriptor = (uintptr)(memory+400+alignment);
                                    backing = dataid == 0 ? 0 : dataid == 1 ? (uintptr)-1 : (uintptr)0x100002000ULL;
                                    length = lengths[li]; index = indices[ii];
                                    width = op == 0 ? 1 : op == 1 ? sizeof(WORD) : op == 4 ? strides[si] : 8;
                                    memcpy(memory+320+alignment+abi.arraylen,&length,sizeof(length));
                                    memcpy(memory+320+alignment+abi.arraytype,&descriptor,sizeof(descriptor));
                                    memcpy(memory+320+alignment+abi.arraydata,&backing,sizeof(backing));
                                    memcpy(memory+400+alignment+abi.typesize,&strides[si],sizeof(int));
                                    R.s = memory+32+alignment;
                                    R.d = memory+128+alignment;
                                    R.m = op == 5 ? (void *)1 : memory+256+alignment;
                                    R.PC = memory+480;
                                    memcpy(R.s,&owner,sizeof(owner));
                                    if(op != 5) memcpy(R.d,&index,sizeof(index));
                                    output = alias == 0 ? memory+256+alignment : alias == 1 ? R.s : alias == 2 ? (void *)(memory+128+alignment) : alias == 3 ? (uchar *)R.s+1 : memory+320+alignment+abi.arraylen;
                                    if(op == 5) R.d = output; else R.m = output;
                                    valid = op == 5 || (index >= 0 && index < length && width >= 0 &&
                                        (width == 0 || (dataid == 2 && (uvlong)index <= 0x7fffffff/(unsigned)width)));
                                    result = op == 5 ? length : (WORD)(backing+(width == 0 ? 0 : (uintptr)index*(unsigned)width));
                                    memcpy(expected,memory,sizeof(expected));
                                    if(valid) memcpy(expected+((uchar *)output-memory),&result,sizeof(result));
                                    verify(handlers[op],code,memory,valid ? nil : exBounds,valid,expected,FE_DIVBYZERO);
                                }
                            }
                        }
                    }
                }
            }
        }
        if(!selection || strcmp(selection,"negative")==0){
            if(op != 5){
                /* A negative index preserves bounds without probing an inaccessible header. */
                memset(memory,0xa5,sizeof(memory));
                owner = 1; index = -1;
                R.s = memory+32; R.d = memory+128; R.m = memory+256; R.PC = memory+480;
                memcpy(R.s,&owner,sizeof(owner));
                memcpy(R.d,&index,sizeof(index));
                memcpy(expected,memory,sizeof(expected));
                verify(handlers[op],code,memory,exBounds,0,expected,FE_DIVBYZERO);
            }
        }
        if(!selection || strcmp(selection,"headers")==0){
            /* Generic descriptor-prefix wrap is rejected before a size-field load. */
            for(fault = 0; fault < 30; fault++){
                memset(memory,0xa5,sizeof(memory));
                owner = (uintptr)(memory+320);
                descriptor = fault == 0 ? 0 : (uintptr)-fault;
                backing = (uintptr)0x100002000ULL;
                length = 1; index = 0;
                memcpy(memory+320+abi.arraylen,&length,sizeof(length));
                memcpy(memory+320+abi.arraytype,&descriptor,sizeof(descriptor));
                memcpy(memory+320+abi.arraydata,&backing,sizeof(backing));
                R.s = memory+32; R.d = memory+128; R.m = op == 5 ? (void *)1 : memory+256; R.PC = memory+480;
                memcpy(R.s,&owner,sizeof(owner));
                if(op != 5) memcpy(R.d,&index,sizeof(index));
                valid = op != 4;
                /* Only prefixes that actually wrap are used for the generic probe. */
                if(!valid && fault > abi.typesize+4) continue;
                result = op == 5 ? length : (WORD)backing;
                memcpy(expected,memory,sizeof(expected));
                if(valid) memcpy(expected+(op == 5 ? 128 : 256),&result,sizeof(result));
                verify(handlers[op],code,memory,valid ? nil : exBounds,valid,expected,FE_DIVBYZERO);
            }
        }
    }
    if(munmap(code,4096))
        return 1;
    printf("PASS array slot/header/address/alias C/RX WORD%d: %lu cases\n",WORD_BITS,checks);
    return 0;
}
