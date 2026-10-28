/* Execute the real globalBconst with small symbol/format adapters on the native host. */
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define nil NULL                   /* Retained compiler null value. */
#define Dglobal 1                  /* Fixture's declaration-store identity. */
typedef unsigned long ulong;       /* Actual native host formatting width. */
typedef long long vlong; // Checked compiler arithmetic uses native signed 64-bit intermediates.
typedef unsigned long long uvlong; /* Full unsigned constant width. */

typedef struct Node Node;
typedef struct Decl Decl;
typedef struct Sym Sym;


/* Only fields read by the retained function are supplied; no compiler algorithm is modeled. */
struct Node
{
    long long val; /* Exact signed constant supplied by the independent fixture. */
    int src;       /* Borrowed source identity passed through to mkids. */
};


/* Owned declaration associated with one interned symbol. */
struct Decl
{
    Node *init; /* Original node captured by globalBconst. */
    int refs;   /* Reference accounting performed by the retained function. */
};


/* Interned symbol owns its name and optionally one declaration. */
struct Sym
{
    char name[64]; /* Stable copied symbol identity, including the terminator. */
    Decl *decl;    /* Owned declaration, released by the fixture. */
};

static Sym symbols[32]; /* Bounded fixture symbol table. */
static int used;        /* Initialized symbol-table extent. */
static void *tbig;      /* Opaque built-in type identity borrowed by mkids. */


/* Preserve the relevant Plan9 lux formatting through native unsigned-long printf.
 * buffer/end borrow bounded storage; format/arguments follow seprint's contract.
 * Returns the written end pointer; malformed fixture formats terminate visibly. */
static char *seprint(char *buffer, char *end, const char *format, ...)
{
    char translated[64];
    size_t input = 0;
    size_t output = 0;
    va_list arguments;
    while (format[input] && output + 1 < sizeof(translated))
    {
        if (strncmp(format + input, "lux", 3) == 0)
        {
            translated[output++] = 'l';
            translated[output++] = 'x';
            input += 3;
        }
        else
        {
            translated[output++] = format[input++];
        }
    }
    if (format[input])
    {
        fprintf(stderr, "fixture format exceeds its translation buffer\n");
        exit(2);
    }
    translated[output] = 0;
    va_start(arguments, format);
    vsnprintf(buffer, (size_t)(end - buffer), translated, arguments);
    va_end(arguments);
    return buffer + strlen(buffer);
}


/* Intern borrowed name text, returning the same symbol only for an identical full name.
 * flags is unused. A full table terminates rather than silently merging identities. */
static Sym *enter(char *name, int flags)
{
    int index;
    (void)flags;
    for (index = 0; index < used; index++)
    {
        if (strcmp(symbols[index].name, name) == 0)
        {
            return &symbols[index];
        }
    }
    if (used >= 32 || strlen(name) >= sizeof(symbols[0].name))
    {
        fprintf(stderr, "fixture symbol capacity exceeded\n");
        exit(2);
    }
    strcpy(symbols[used].name, name);
    return &symbols[used++];
}


/* Allocate one declaration; source/symbol/type/tail are borrowed and do not escape.
 * Returns an owned zeroed declaration or terminates on actual allocation failure. */
static Decl *mkids(int *source, Sym *symbol, void *type, void *tail)
{
    Decl *declaration;
    (void)source;
    (void)symbol;
    (void)type;
    (void)tail;
    declaration = calloc(1, sizeof(*declaration));
    if (!declaration)
    {
        fprintf(stderr, "fixture declaration allocation failed\n");
        exit(2);
    }
    return declaration;
}


/* Installation has no additional effect in the fixture; both inputs remain borrowed. */
static void installids(int store, Decl *declaration)
{
    (void)store;
    (void)declaration;
}


/* Unexpected arithmetic rejection is a fixture failure for these small use counts. */
static void
fatal(char *message, ...)
{
    fprintf(stderr, "%s\n", message);
    abort();
}


/* RETAINED_GLOBAL_BCONST */


/* Distinct signed/high-bit values must retain distinct real declarations and twenty-byte names.
 * No arguments/results escape; every allocated declaration is released on success or failure. */
int main(void)
{
    const long long values[] = {-2, -5, -65536, -65537, LLONG_MIN, LLONG_MAX, 4294967294LL, 8589934590LL};
    Node nodes[sizeof(values) / sizeof(values[0])];
    size_t index;
    int status = 0;
    for (index = 0; index < sizeof(values) / sizeof(values[0]); index++)
    {
        Decl *declaration;
        nodes[index].val = values[index];
        nodes[index].src = (int)index;
        declaration = globalBconst(&nodes[index]);
        if (declaration->init != &nodes[index] || declaration->refs != 1 || strlen(symbols[used - 1].name) != 20)
        {
            fprintf(stderr, "big constant identity merged/truncated at %lld\n", values[index]);
            status = 1;
            break;
        }
    }
    for (index = 0; index < (size_t)used; index++)
    {
        free(symbols[index].decl);
    }
    if (!status)
    {
        puts("PASS actual compiler big-constant identity on the native host");
    }
    return status;
}
