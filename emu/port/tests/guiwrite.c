/* Isolate guiwrite's request parsing and cleanup, without starting Tk/Dis. */
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define nil NULL
#define QTDIR 128
#define Eisdir "is a directory"
#define Ethread "unknown process"
#define Ebadarg "bad argument"
#define Enomem "out of memory"
#define PID(qid) 1
#define QID(qid) 1
typedef long long vlong;
typedef unsigned long ulong;
typedef struct Tk Tk;
typedef struct TkTop TkTop;
typedef struct Prog Prog;
typedef struct Chan Chan;
typedef struct rslt_string rslt_string;
struct Tk { int unused; };
struct TkTop { Tk *root; };
struct Prog { TkTop *tktop; };
struct Chan { struct { int type; ulong path; } qid; };
struct rslt_string { rslt_string *next; ulong path; char str[1]; };
static Tk widget;
static TkTop top = {&widget};
static Prog process = {&top};
static rslt_string *root_rslt_strings;
static int lock_depth, error_depth, missing_widget, command_count;
static char last_command[256];
static void error(const char *text) { (void)text; assert(0); }
static void acquire(void) { assert(lock_depth++ == 0); }
static void release(void) { assert(--lock_depth == 0); }
static int waserror(void) { ++error_depth; return 0; }
static void poperror(void) { assert(--error_depth == 0); }
static void nexterror(void) { assert(0); }
static Prog *progpid(int pid) { (void)pid; return &process; }
static Tk *qid2tk(void *qid, TkTop *t)
{ (void)qid; (void)t; return missing_widget ? nil : &widget; }
static char *tkname(Tk *tk) { (void)tk; return ".widget"; }
static int snprint(char *buf, int len, const char *fmt, ...)
{
    int result;
    va_list args;
    va_start(args, fmt);
    result = vsnprintf(buf, (size_t)len, fmt, args);
    va_end(args);
    return result;
}
static char *tksinglecmd(TkTop *t, const char *cmd, char **result)
{
    (void)t;
    if(result != nil){
        ++command_count;
        assert(strlen(cmd) < sizeof(last_command));
        strcpy(last_command, cmd);
        *result = nil;
    }
    return nil;
}

/* The runner inserts the unchanged production guiwrite function here. */
GUIWRITE_IMPLEMENTATION

static void clear_results(void)
{
    while(root_rslt_strings != nil){
        rslt_string *next = root_rslt_strings->next;
        free(root_rslt_strings);
        root_rslt_strings = next;
    }
}

int main(void)
{
    Chan channel = {{0, 1}};
    const char *command = "configure -text hello";
    size_t length = strlen(command);
    char *bytes = malloc(length);
    char many[300];
    int i;
    assert(bytes != nil);
    memcpy(bytes, command, length);
    assert(guiwrite(&channel, bytes, (long)length, 0) == (long)length);
    assert(memcmp(bytes, command, length) == 0);
    assert(strcmp(last_command, ".widget configure -text hello") == 0);
    free(bytes);
    /* Quoted, read-only input must not be overwritten by the tokenizer. */
    command = "\"configure -text hello\"\n";
    assert(guiwrite(&channel, (void *)command, (long)strlen(command), 0) > 0);
    /* Newline at slot 99 used to write args[100] outside the local array. */
    for(i = 0; i < 100; i++){
        many[2*i] = 'x';
        many[2*i+1] = '\n';
    }
    assert(guiwrite(&channel, many, 200, 0) == 200);
    assert(command_count == 101);
    assert(lock_depth == 0 && error_depth == 0);
    missing_widget = 1;
    assert(guiwrite(&channel, (void *)"x", 1, 0) == 0);
    assert(lock_depth == 0 && error_depth == 0);
    clear_results();
    return 0;
}
