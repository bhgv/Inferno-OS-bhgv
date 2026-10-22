#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>

typedef unsigned char uchar;
typedef unsigned int uint;
typedef int64_t vlong;
typedef uint64_t uvlong;
typedef uint32_t mpdigit;
#define ulong uint32_t
#define nil NULL
#define USED(x) ((void)(x))
typedef struct Fmt Fmt;
struct Fmt {
	va_list args;
	int prec, flags;
};
#define FmtPrec 1
#include "mp.h"
#define mpdighi ((mpdigit)1<<(Dbits-1))
#define DIGITS(n) (((n)+Dbits-1)/Dbits)
#define MAXUINT UINT_MAX
#define MAXINT (UINT_MAX>>1)
#define MININT (MAXINT+1)
#define MAXUVLONG UINT64_MAX
#define MAXVLONG INT64_MAX
#define MINVLONG ((uvlong)1<<63)

void *testmalloc(size_t);
void *testrealloc(void*, size_t);
void testfree(void*);
void *mallocz(size_t, int);
void sysfatal(char*, ...);
int fmtstrcpy(Fmt*, char*);
int enc32(char*, int, uchar*, int);
int dec32(uchar*, int, char*, int);
int enc64(char*, int, uchar*, int);
int dec64(uchar*, int, char*, int);
#define malloc testmalloc
#define realloc testrealloc
#define free testfree
