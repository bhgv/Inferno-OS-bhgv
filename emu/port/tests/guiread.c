/* Real GUI entry points, real Tk/Chan types, real lib9 formatting.
 * Tk execution and VM scheduler are boundary stubs; no GUI is started. */
#include "dat.h"
#include "fns.h"
#include "error.h"
#include "interp.h"
#include "draw.h"
#include "tk.h"
#include "canvs.h"
#include "isa.h"
#include "runt.h"
#include <assert.h>
#include <stdio.h>

static Proc worker;
static int lock_depth, missing_process, dir_reads;
static char last_error[128];
static int live_allocations, fail_after=-1, poison_alloc;
static char *getter_value="value", *widget_name=".widget";
static int getter_throw, getter_calls;
static char last_command[12000],last_widget_command[12000];
static int command_error,drop_top,replace_widget;
static int lifecycle, remote_owner, display_calls;
static Tk child;
static Osenv environment;
static char child_path[12000], child_tail[128];
char *eve="eve";
enum {Qdir=0,Qgui,Qguictl};
extern void hostfree(void*) __asm__("free");
extern void *hostrealloc(void*,size_t) __asm__("realloc");
void *guimalloc(size_t n) {
    void *p;
    if(fail_after==0)return nil;
    if(fail_after>0)fail_after--;
    p=calloc(1,n?n:1);if(p){live_allocations++;if(poison_alloc)memset(p,0xa5,n);}return p;
}
void *guirealloc(void *old,size_t n) {
    void *p;
    if(fail_after==0)return nil;
    if(fail_after>0)fail_after--;
    p=hostrealloc(old,n);if(p && !old)live_allocations++;return p;
}
void guifree(void *p) {if(p){live_allocations--;hostfree(p);}}
void *mallocz(ulong n,int zero) {void *p=malloc(n);if(p && zero)memset(p,0,n);return p;}
/* /proc is unavailable in the test sandbox. Leak accounting is explicit above. */
int __lsan_is_turned_off(void) {return 1;}

static Tk widget;
static TkTop top;
static Prog process;
static TkOption *nooptions[] = {nil};
static TkMethod method = {.name="frame", .opts=nooptions};
TkMethod *tkmethod[TKwidgets];
TkCimeth tkcimethod[TkCVarc+1];
char Ebadarg[]="bad argument", Ethread[]="unknown process", Enomem[]="out of memory";
char Eperm[]="permission denied", Efilename[]="bad filename", Eio[]="I/O error";
char Etoobig[]="too big", Eisdir[]="is a directory", Enonexist[]="does not exist";

Proc *getup(void) { return &worker; }
void acquire(void) { assert(lock_depth++ == 0); }
void release(void) { assert(--lock_depth == 0); }
void nexterror(void) { assert(worker.nerr>0); siglongjmp(worker.estack[--worker.nerr],1); }
void error(char *text) { snprintf(last_error,sizeof(last_error),"%s",text); nexterror(); }
Prog *progpid(int pid) { (void)pid;return missing_process ? nil : &process; }
char *tkname(Tk *tk) { if(lifecycle)return tk==&widget ? widget_name : child_path;return tk ? widget_name : "(nil)"; }
Tk *tklook(TkTop *t,char *name,int flag) { (void)t;(void)flag;return strcmp(name,child_path)==0 ? &child : nil; }
Prog *progn(int n) { return !missing_process && n==0 ? &process : nil; }
Prog *currun(void) { return remote_owner ? nil : &process; }
void mkqid(Qid *q,vlong path,ulong vers,int type) {q->path=path;q->vers=vers;q->type=type;}
void devdir(Chan *c,Qid q,char *name,long length,char *user,long perm,Dir *d) {
 (void)c;(void)user;assert(lock_depth==1);memset(d,0,sizeof(*d));d->qid=q;d->name=name;d->length=length;d->mode=perm;
}
int openmode(ulong m) {if(m>3)error(Ebadarg);return m;}
void destroy(void *p) {assert(p==H || p==nil);}
void poolimmutable(void *p) {(void)p;assert(0);}
String *c2string(char *s,int n) {(void)s;(void)n;assert(0);return H;}
char *string2c(String *s) {(void)s;assert(0);return "";}
void Display_allocate(void *v) {F_Display_allocate *f=v;display_calls++;*f->ret=H;}
void Tk_toplevel(void *v) {(void)v;assert(0);}
void Screen_allocate(void *v) {(void)v;assert(0);}
void Screen_newwindow(void *v) {(void)v;assert(0);}
void Image_draw(void *v) {(void)v;assert(0);}
void Tk_putimage(void *v) {(void)v;assert(0);}
char *tkfprint(char *s,int v) { snprintf(s,Tkminitem,"%d",v);return s; }
char *tksinglecmd(TkTop *t,char *s,char **out) {
    (void)t;getter_calls++;
    assert(strlen(s)<sizeof(last_command));strcpy(last_command,s);
    if(out) {
        strcpy(last_widget_command,s);
        *out=malloc(strlen(getter_value)+1);if(!*out)error(Enomem);
        strcpy(*out,getter_value);
    }
    if(getter_throw)error(Ebadarg);
    if(drop_top)process.tktop=nil;
    if(replace_widget)widget.guiqid=0;
    if(lifecycle && !command_error && !drop_top && strncmp(s,"frame ",6)==0) {
        strcpy(child_path,s+6);strcpy(child_tail,strrchr(child_path,'.')+1);
        child.type=TKframe;child.name_tail=child_tail;child.master=&widget;
        widget.slave=&child;widget.siblings=&child;
    }
    return command_error ? Ebadarg : nil;
}
void _fmtlock(void) {}
void _fmtunlock(void) {}
int errfmt(Fmt *f) { return fmtstrcpy(f,"test error"); }
long devdirread(Chan *c,char *s,long n,Dirtab *t,int nt,Devgen *g) {
    (void)c;(void)s;(void)n;(void)t;(void)nt;(void)g;dir_reads++;return 17;
}
static TkEnv default_environment;
TkEnv *tkdefaultenv(TkTop *t) {(void)t;return &default_environment;}
TKNEWOBJ_IMPLEMENTATION
GUIQID_IMPLEMENTATION
GUIRESULT_IMPLEMENTATION
QID_HELPERS_IMPLEMENTATION
GUIGEN_IMPLEMENTATION
GUICHANGE_IMPLEMENTATION
GUISNAPSHOT_IMPLEMENTATION
GUIREAD_IMPLEMENTATION
GUICLOSE_IMPLEMENTATION
GUIWRITE_IMPLEMENTATION

static void enqueue(Chan *c,char *text) {
    long n=strlen(text);
    rslt_string *r=malloc(sizeof(*r)+n);
    assert(r);r->qid=c->qid;r->len=n;r->pos=0;memcpy(r->str,text,n+1);
    acquire();guiresultqueue(c,r);release();
}
static void balanced(void) { assert(lock_depth==0 && worker.nerr==0); }
static void badread(Chan *c,long count,vlong offset,char *expected) {
    char buffer[8];
    if(waserror()) {
        assert(strcmp(last_error,expected)==0);balanced();return;
    }
    guiread(c,buffer,count,offset);assert(0);
}
GUIWRITE_TESTS
GUILIFECYCLE_TESTS

int main(void) {
    Chan c={0},other={0},high={0};
    unsigned char out[16];
    int i;
    {
        Fmt f;
        assert(fmtstrinit(&f)==0);
        f.farg=(void*)(uintptr)0x7fffff80UL;
        assert(fmtprint(&f,"%s","0123456789012345678901234567890123456789")<0);
        free(f.start);assert(live_allocations==0);
    }
    quotefmtinstall();
    for(i=0;i<TKwidgets;i++)tkmethod[i]=&method;
    widget.type=TKframe;widget.guiqid=1;guinextid=1; top.root=&widget;process.tktop=&top;
    c.qid.path=QPID(42)|1;c.qid.vers=42;c.flag=COPEN;
    other=c;other.qid.vers=43;
    high=c;high.qid.path+=((uvlong)1<<40);
    enqueue(&c,"abcdef");
    memset(out,0xa5,sizeof(out));
    assert(guiread(&c,out,0,0)==0 && root_rslt_strings && root_rslt_strings->pos==0);
    assert(out[0]==0xa5);balanced();
    assert(guiread(&c,out,2,0)==2 && memcmp(out,"ab",2)==0 && out[2]==0xa5);
    assert(guiread(&c,out,2,2)==2 && memcmp(out,"cd",2)==0);
    assert(guiread(&c,out,10,4)==2 && memcmp(out,"ef",2)==0 && !root_rslt_strings);
    balanced();
    enqueue(&c,"one");enqueue(&c,"two");
    assert(guiread(&c,out,16,0)==3 && memcmp(out,"one",3)==0);
    assert(guiread(&c,out,16,3)==3 && memcmp(out,"two",3)==0);
    enqueue(&c,"");assert(guiread(&c,out,1,0)==0 && !root_rslt_strings);balanced();
    enqueue(&c,"low");enqueue(&other,"vers");enqueue(&high,"high");
    assert(guiread(&high,out,16,0)==4 && memcmp(out,"high",4)==0);
    assert(guiread(&other,out,16,0)==4 && memcmp(out,"vers",4)==0);
    assert(guiread(&c,out,16,0)==3 && memcmp(out,"low",3)==0);
    enqueue(&c,"a");enqueue(&other,"keep");enqueue(&c,"b");
    guiclose(&c);balanced();
    assert(root_rslt_strings && guisameresult(root_rslt_strings,&other) && !root_rslt_strings->next);
    guiclose(&other);assert(!root_rslt_strings);balanced();
    top.root=nil;
    for(i=0;i<100;i++) { assert(guiread(&c,out,1,0)==0);balanced(); }
    badread(&c,-1,0,Ebadarg);badread(&c,1,-1,Ebadarg);
    missing_process=1;badread(&c,1,0,Ethread);missing_process=0;
    c.qid.type=QTDIR;assert(guiread(&c,out,1,0)==17 && dir_reads==1);balanced();
    c.qid.type=0;top.root=&widget;
    {
        static TkOption option[]={{.o="text"},{.o=nil}};
        static TkOption *options[]={option,nil};
        static char payload[4096],large_name[2048];
        static char complete[16000],chunks[16000];
        long full,n,at;
        int calls, failures=0, successes=0;
        memset(payload,'T',sizeof(payload)-1);payload[sizeof(payload)-1]=0;
        memset(large_name,'N',sizeof(large_name)-1);large_name[0]='.';large_name[sizeof(large_name)-1]=0;
        widget.type=TKtext;method.name="text";method.opts=options;
        getter_value=payload;widget_name=large_name;
        full=guiread(&c,complete,sizeof(complete),0);
        assert(full>4096 && full<(long)sizeof(complete));
        assert(strlen(last_command)>2000); /* No old 256-byte command truncation. */
        at=guiread(&c,chunks,7,0);calls=getter_calls;
        getter_value="changed after the first chunk";
        while((n=guiread(&c,chunks+at,7,at))>0)at+=n;
        assert(at==full && memcmp(complete,chunks,full)==0 && getter_calls==calls);
        assert(guiread(&c,out,1,((vlong)1)<<40)==0);
        assert(guiread(&c,out,0,0)==0 && getter_calls==calls);balanced();
        getter_value=payload;
        guiclose(&c);assert(c.aux==nil && live_allocations==0);
        /* Every allocation/formatter growth boundary must unwind without leaks. */
        for(i=0;i<128;i++) {
            fail_after=i;
            if(waserror()) {
                assert(strcmp(last_error,Enomem)==0);balanced();failures++;
            }else{
                assert(guiread(&c,complete,sizeof(complete),0)>0);poperror();successes++;
            }
            fail_after=-1;guiclose(&c);balanced();assert(live_allocations==0);
        }
        assert(failures>0 && successes>0);
        getter_throw=1;badread(&c,1,0,Ebadarg);getter_throw=0;
        guiclose(&c);assert(live_allocations==0);
        widget.type=TKwidgets;badread(&c,1,0,Ebadarg);widget.type=TKtext;
        guiclose(&c);assert(live_allocations==0);
        /* Canvas snapshot, including a window item with no focus or subwindow. */
        {
            Tk *canvas_widget=malloc(sizeof(Tk)+sizeof(TkCanvas));
            TkCitem *item=malloc(sizeof(TkCitem)+sizeof(TkCwind));
            TkCanvas *canvas=TKobj(TkCanvas,canvas_widget);
            assert(canvas_widget && item);
            canvas_widget->guiqid=widget.guiqid;canvas_widget->type=TKcanvas;canvas->width=20;canvas->height=30;
            item->type=TkCVwindow;item->id=3;canvas->head=item;
            tkcimethod[TkCVwindow].name="window";top.root=canvas_widget;
            method.opts=nil;method.cmd=nil;method.name="canvas";
            n=guiread(&c,complete,sizeof(complete)-1,0);assert(n>0);complete[n]=0;
            assert(strstr(complete,"create window") && strstr(complete,"(nil)"));
            guiclose(&c);
            item->type=TkCVarc+1;badread(&c,1,0,Ebadarg);guiclose(&c);
            free(item);free(canvas_widget);top.root=&widget;
        }
        assert(live_allocations==0);balanced();
    }
    puts("PASS GUI read/close: snapshot chunks, exact read boundaries, long getters, allocation failures and Tk error cleanup");
    test_guiwrite(&c);
    test_guilifecycle();
    puts("PASS GUI read/close: partial and zero reads, FIFO replies, full Qid isolation, complete close cleanup and error-stack balance");
    return 0;
}
