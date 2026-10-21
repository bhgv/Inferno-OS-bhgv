/* Real GUI entry points, real Tk/Chan types, real lib9 formatting.
 * Tk execution and VM scheduler are boundary stubs; no GUI is started. */
#include "dat.h"
#include "fns.h"
#include "error.h"
#include "interp.h"
#include "draw.h"
#include "tk.h"
#include "canvs.h"
#include <assert.h>
#include <stdio.h>

static Proc worker;
static int lock_depth, missing_process, dir_reads;
static char last_error[128];
static Tk widget;
static TkTop top;
static Prog process;
static TkOption *nooptions[] = {nil};
static TkMethod method = {.name="frame", .opts=nooptions};
TkMethod *tkmethod[TKwidgets];
TkCimeth tkcimethod[TkCVarc+1];
char Ebadarg[]="bad argument", Ethread[]="unknown process", Enomem[]="out of memory";

Proc *getup(void) { return &worker; }
void acquire(void) { assert(lock_depth++ == 0); }
void release(void) { assert(--lock_depth == 0); }
void nexterror(void) { assert(worker.nerr>0); siglongjmp(worker.estack[--worker.nerr],1); }
void error(char *text) { snprintf(last_error,sizeof(last_error),"%s",text); nexterror(); }
Prog *progpid(int pid) { (void)pid;return missing_process ? nil : &process; }
char *tkname(Tk *tk) { return tk ? ".widget" : "(nil)"; }
char *tkfprint(char *s,int v) { snprintf(s,Tkminitem,"%d",v);return s; }
char *tksinglecmd(TkTop *t,char *s,char **out) { (void)t;(void)s;if(out)*out=nil;return nil; }
void _fmtlock(void) {}
void _fmtunlock(void) {}
int errfmt(Fmt *f) { return fmtstrcpy(f,"test error"); }
static int guigen(Chan *c,char *s,Dirtab *t,int nt,int n,Dir *d) {
    (void)c;(void)s;(void)t;(void)nt;(void)n;(void)d;return -1;
}
long devdirread(Chan *c,char *s,long n,Dirtab *t,int nt,Devgen *g) {
    (void)c;(void)s;(void)n;(void)t;(void)nt;(void)g;dir_reads++;return 17;
}
GUIQID_IMPLEMENTATION
GUIRESULT_IMPLEMENTATION
QID_HELPERS_IMPLEMENTATION
GUIREAD_IMPLEMENTATION
GUICLOSE_IMPLEMENTATION

static void enqueue(Qid q,char *text) {
    long n=strlen(text);
    rslt_string *r=malloc(sizeof(*r)+n);
    assert(r);r->qid=q;r->len=n;r->pos=0;memcpy(r->str,text,n+1);
    acquire();guiresultqueue(r);release();
}
static void balanced(void) { assert(lock_depth==0 && worker.nerr==0); }
static void badread(Chan *c,long count,vlong offset,char *expected) {
    char buffer[8];
    if(waserror()) {
        assert(strcmp(last_error,expected)==0);balanced();return;
    }
    guiread(c,buffer,count,offset);assert(0);
}
int main(void) {
    Chan c={0},other={0},high={0};
    unsigned char out[16];
    int i;
    for(i=0;i<TKwidgets;i++)tkmethod[i]=&method;
    widget.type=TKframe; top.root=&widget;process.tktop=&top;
    c.qid.path=((uvlong)42<<16)|1;c.qid.vers=42;c.flag=COPEN;
    other=c;other.qid.vers=43;
    high=c;high.qid.path+=((uvlong)1<<40);
    enqueue(c.qid,"abcdef");
    memset(out,0xa5,sizeof(out));
    assert(guiread(&c,out,0,0)==0 && root_rslt_strings && root_rslt_strings->pos==0);
    assert(out[0]==0xa5);balanced();
    assert(guiread(&c,out,2,0)==2 && memcmp(out,"ab",2)==0 && out[2]==0xa5);
    assert(guiread(&c,out,2,2)==2 && memcmp(out,"cd",2)==0);
    assert(guiread(&c,out,10,4)==2 && memcmp(out,"ef",2)==0 && !root_rslt_strings);
    balanced();
    enqueue(c.qid,"one");enqueue(c.qid,"two");
    assert(guiread(&c,out,16,0)==3 && memcmp(out,"one",3)==0);
    assert(guiread(&c,out,16,3)==3 && memcmp(out,"two",3)==0);
    enqueue(c.qid,"");assert(guiread(&c,out,1,0)==0 && !root_rslt_strings);balanced();
    enqueue(c.qid,"low");enqueue(other.qid,"vers");enqueue(high.qid,"high");
    assert(guiread(&high,out,16,0)==4 && memcmp(out,"high",4)==0);
    assert(guiread(&other,out,16,0)==4 && memcmp(out,"vers",4)==0);
    assert(guiread(&c,out,16,0)==3 && memcmp(out,"low",3)==0);
    enqueue(c.qid,"a");enqueue(other.qid,"keep");enqueue(c.qid,"b");
    guiclose(&c);balanced();
    assert(root_rslt_strings && guisameresult(root_rslt_strings,&other.qid) && !root_rslt_strings->next);
    guiclose(&other);assert(!root_rslt_strings);balanced();
    top.root=nil;
    for(i=0;i<100;i++) { assert(guiread(&c,out,1,0)==0);balanced(); }
    badread(&c,-1,0,Ebadarg);badread(&c,1,-1,Ebadarg);
    missing_process=1;badread(&c,1,0,Ethread);missing_process=0;
    c.qid.type=QTDIR;assert(guiread(&c,out,1,0)==17 && dir_reads==1);balanced();
    puts("PASS GUI read/close: partial and zero reads, FIFO replies, full Qid isolation, complete close cleanup and error-stack balance");
    return 0;
}
