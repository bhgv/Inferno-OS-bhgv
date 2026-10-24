/* Real heap sizing, allocation and VM operation bodies. The pool is bounded. */
#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "pool.h"
#include "raise.h"
#include "kernel.h"
#include <assert.h>
#include <stdio.h>
#include <limits.h>
static Proc worker;
static char *last_error;
Proc *getup(void) {return &worker;}
void nexterror(void) {assert(worker.nerr>0);siglongjmp(worker.estack[--worker.nerr],1);}
void error(char *s) {last_error=s;nexterror();}
int __lsan_is_turned_off(void) {return 1;}
char exModule[]="module";
char exHeap[]="heap",exNegsize[]="negative",exBounds[]="bounds",exNilref[]="nil";
Pool *heapmem;
int mutator,nprop;
void (*heapmonitor)(int,void*,ulong);
Type Tarray={1,0,0,sizeof(Array)},Tmodlink={1},Tlist={1};
REG R;
static ulong requested;
static int poolcalls,live,force_failure;
void *poolalloc(Pool *p,ulong n) {
    void *v;(void)p;requested=n;poolcalls++;
    if(force_failure || n>1024*1024)return nil;
    v=malloc(n);assert(v);memset(v,0xa5,n);live++;return v;
}
void freetype(Type *t) {assert(t->ref>1);t->ref--;}
void destroy(void *p) {
    Heap *h;Array *a;
    if(p==H)return;
    h=D2H(p);assert(h->ref>0);if(--h->ref)return;
    if(h->t==&Tarray) {
        a=p;
        if(a->root==H && a->t->np) {
            WORD j;for(j=0;j<a->len;j++)freeptrs(a->data+j*a->t->size,a->t);
        }
        freetype(a->t);if(a->root!=H)destroy(a->root);Tarray.ref--;
    } else if(h->t==&Tmodlink) Tmodlink.ref--;
    else if(h->t==&Tlist)Tlist.ref--;
    else if(h->t)freetype(h->t);
    free(h);live--;
}
#define OP(n) void n(void)
#define W(r) *((WORD*)(R.r))
#define A(r) *((Array**)(R.r))
#define BIT(bt,nb) ((bt)&(1<<(nb)))
IMPLEMENTATION
static int which,tsz,n,link_count;
static WORD length;
static ulong listsize;
static Type type={1};
static List *list=H;
static Module module;
static Heap *allocated;
static Array *copyfrom;
static int run(void *unused) {
    (void)unused;
    switch(which) {
    case 0:acheck(tsz,length);break;
    case 1:allocated=nheap(n);break;
    case 2:allocated=heap(&type);break;
    case 3:allocated=heapz(&type);break;
    case 4:allocated=heaparray(&type,n);break;
    case 5:allocated=D2H(arraycpy(copyfrom));break;
    case 6:allocated=D2H(mklinkmod(&module,link_count));break;
    case 7:cons(listsize,&list);break;
    case 8:slicea();break;
    case 9:slicela();break;
    case 10:newa();break;
    case 11:newaz();break;
    }
    return 1;
}
static void rejects(char *expected,int noallocation) {
    int before=poolcalls,objects=live;
    last_error=nil;assert(!kprotect(run,nil));
    assert(last_error==expected && worker.nerr==0 && live==objects);
    if(noallocation)assert(before==poolcalls);
}
/* Exhaust overlapping pointer copies against a snapshot, including exact owner counts. */
static void
pointercopies(void)
{
    Type scalar={1};
    void *expected[8], *original[8];
    Array *root, *view;
    WORD offset;
    int first, dest, length, i, j, owners;

    scalar.size=sizeof(WORD);
    type.size=sizeof(void*); type.np=1; type.map[0]=0x80;
    for(first=0;first<=8;first++) for(dest=0;dest<=8;dest++)
    for(length=0;length<=8-first && length<=8-dest;length++) {
        root=H2D(Array*,heaparray(&type,8));
        for(i=0;i<8;i++) {
            original[i]=H2D(void*,heapz(&scalar));
            ((void**)root->data)[i]=expected[i]=original[i];
        }
        for(i=0;i<length;i++)expected[dest+i]=original[first+i];
        view=H2D(Array*,heap(&Tarray));
        view->len=length; view->t=&type; type.ref++;
        view->root=root; D2H(root)->ref++;
        view->data=root->data+first*sizeof(void*);
        offset=dest; R.s=&view; R.d=&root; R.m=&offset; which=9;
        assert(kprotect(run,nil));
        for(i=0;i<8;i++) {
            assert(((void**)root->data)[i]==expected[i]);
            owners=0;
            for(j=0;j<8;j++)if(expected[j]==expected[i])owners++;
            assert(D2H(expected[i])->ref==owners);
        }
        destroy(view); destroy(root);
        assert(scalar.ref==1 && live==0);
    }
    type.np=0;
}

/* A replaced destination element may be the only owner of the source or its view. */
static void
borrowedcopies(void)
{
	Type scalar = {1};
	Array *source, *target, *root;
	void *child;
	WORD offset;
	int length, victim, shared, view, i, before, cases = 0;

	scalar.size = sizeof(WORD);
	type.size = sizeof(void*);
	type.np = 1;
	type.map[0] = 0x80;
	for(length = 1; length <= 16; length++)
	for(offset = 0; offset < 4; offset++)
	for(victim = 0; victim < length; victim++)
	for(shared = 0; shared < 2; shared++)
	for(view = 0; view < 2; view++){
		root = H2D(Array*, heaparray(&type, length));
		child = H2D(void*, heapz(&scalar));
		for(i = 0; i < length; i++){
			((void**)root->data)[i] = child;
			D2H(child)->ref++;
		}
		source = root;
		if(view){
			source = H2D(Array*, heap(&Tarray));
			source->len = length;
			source->t = &type;
			type.ref++;
			source->root = root;
			source->data = root->data;
		}
		target = H2D(Array*, heaparray(&type, length+offset+2));
		((void**)target->data)[offset+victim] = source;
		if(shared)
			D2H(source)->ref++;
		R.s = &((Array**)target->data)[offset+victim];
		R.d = &target;
		R.m = &offset;
		which = 9;
		before = poolcalls;
		assert(kprotect(run, nil));
		assert(poolcalls == before);
		for(i = 0; i < target->len; i++)
			assert(((void**)target->data)[i] ==
				(i >= offset && i < offset+length ? child : H));
		assert(D2H(child)->ref == 1+length*(1+shared));
		if(shared){
			assert(D2H(source)->ref == 1);
			destroy(source);
		}
		destroy(target);
		assert(D2H(child)->ref == 1);
		destroy(child);
		assert(live == 0 && scalar.ref == 1 && type.ref == 1 && Tarray.ref == 1);
		cases++;
	}
	type.np = 0;
	printf("PASS borrowed array copies: %d owner/view/offset cases\n", cases);
}

/* An empty non-nil array can have no data, but invalid destinations still fail. */
static void
emptycopies(void)
{
    Array empty={0}, target={0}, *source=&empty, *destination=&target;
    WORD offset=0;
    int pointers, before;

    for(pointers=0;pointers<2;pointers++) {
        type.np=pointers; type.size=sizeof(void*); type.map[0]=0x80;
        empty.t=target.t=&type; empty.data=target.data=nil;
        empty.len=target.len=0;
        R.s=&source; R.d=&destination; R.m=&offset; which=9;
        before=poolcalls; assert(kprotect(run,nil)); assert(before==poolcalls);
        offset=1; rejects(exBounds,1); offset=0;
        destination=H; rejects(exNilref,1); destination=&target;
    }
    type.np=0;
}

/* Empty or zero-width views retain nil data and their root through repeated slicing. */
static void
emptyviews(int faults)
{
    Array *a, *root, *saved;
    WORD first, last;
    int zeroelement, repeat, before, typerefs;

    for(zeroelement=0; zeroelement<2; zeroelement++) {
        type.size=zeroelement ? 0 : sizeof(WORD);
        root=H2D(Array*,heap(&Tarray));
        root->len=zeroelement ? 3 : 0;
        root->data=nil; root->t=&type; root->root=H; type.ref++;
        a=root; first=zeroelement ? 1 : 0; last=zeroelement ? 2 : 0;
        R.s=&first; R.m=&last; R.d=&a; which=8;
        for(repeat=0; repeat<8; repeat++) {
            saved=a; before=live; typerefs=type.ref;
            if(faults) { force_failure=1; rejects(exHeap,0); force_failure=0; }
            assert(a==saved && live==before && type.ref==typerefs);
            assert(kprotect(run,nil));
            assert(a!=saved && a->data==nil && a->root==root);
            assert(a->len==(zeroelement ? 1 : 0) && D2H(root)->ref==1);
            first=0; last=a->len;
        }
        destroy(a);
        assert(type.ref==1 && Tarray.ref==1);
    }
    type.size=sizeof(WORD);
}

/* The language bounds invariant permits only [0:0] for a nil array. */
static void
nilslices(void)
{
    Array *a, *old;
    WORD first, last;
    WORD indices[] = {-1, INT_MIN, 1, 2, INT_MAX,
                      -(WORD)(((UWORD)~0)>>1)-1, (WORD)(((UWORD)~0)>>1)};
    unsigned i;
    int nullvalue;

    for(nullvalue=0; nullvalue<2; nullvalue++) {
        a=nullvalue ? nil : H;
        old=a; first=last=0;
        R.s=&first; R.m=&last; R.d=&a; which=8;
        assert(kprotect(run,nil) && a==old);
        for(i=0; i<sizeof(indices)/sizeof(indices[0]); i++) {
            first=last=indices[i];
            rejects(exBounds,1); assert(a==old);
        }
        first=0; last=1; rejects(exNilref,1); assert(a==old);
    }
}

/* Slice pointers and bounds may occupy independently unaligned VM slots. */
static void sliceslots(void)
{
    int widths[]={0,1,3,8,16},w,sa,ma,da,k,offset;
    union {uvlong align;uchar bytes[128];} slots;
    uchar expected[128],bytes[128];
    Array *root,*view,*source,*destination;
    WORD start=1,end=4,where;
    unsigned long cases=0;
    type.np=0;
    for(w=0;w<nelem(widths);w++)for(sa=0;sa<8;sa++)for(ma=0;ma<8;ma++)for(da=0;da<8;da++){
        type.size=widths[w];root=H2D(Array*,heaparray(&type,5));
        memset(slots.bytes,0xa5,sizeof(slots.bytes));R.s=slots.bytes+8+sa;R.m=slots.bytes+40+ma;R.d=slots.bytes+80+da;
        memmove(R.s,&start,sizeof(start));memmove(R.m,&end,sizeof(end));memmove(R.d,&root,sizeof(root));
        memmove(expected,slots.bytes,sizeof(expected));which=8;assert(kprotect(run,nil));
        memmove(&view,slots.bytes+80+da,sizeof(view));assert(view->root==root && view->len==3 && view->data==root->data+type.size);
        memmove(expected+80+da,&view,sizeof(view));assert(!memcmp(slots.bytes,expected,sizeof(expected)));
        destroy(view);assert(type.ref==1 && Tarray.ref==1);cases++;
        for(offset=0;offset<4;offset++){
            source=H2D(Array*,heaparray(&type,2));destination=H2D(Array*,heaparray(&type,5));
            for(k=0;k<2*type.size;k++)source->data[k]=(uchar)(k*17+3);
            memset(destination->data,0x5a,5*type.size);memmove(bytes,destination->data,5*type.size);
            memmove(bytes+offset*type.size,source->data,2*type.size);where=offset;
            memset(slots.bytes,0xa5,sizeof(slots.bytes));R.s=slots.bytes+8+sa;R.m=slots.bytes+40+ma;R.d=slots.bytes+80+da;
            memmove(R.s,&source,sizeof(source));memmove(R.m,&where,sizeof(where));memmove(R.d,&destination,sizeof(destination));
            memmove(expected,slots.bytes,sizeof(expected));which=9;assert(kprotect(run,nil));
            assert(!memcmp(slots.bytes,expected,sizeof(expected)) && !memcmp(bytes,destination->data,5*type.size));
            destroy(source);destroy(destination);assert(type.ref==1 && Tarray.ref==1);cases++;
        }
    }
    assert(!live && !worker.nerr);printf("PASS slice slots: %lu view/copy/alignment cases\n",cases);
}

/* Invalid descriptor geometry must fail before allocation, marking or publication. */
static void slicespans(void)
{
    struct {Heap h;Array a;} root,source;
    union {uvlong align;uchar bytes[128];} slots;
    uchar expected[128],data[64];
    Array *owner,*input;
    WORD first=1,last=2,offset=0;
    int operation,mode,sa,ma,da,before;
    unsigned long cases=0;
    for(operation=8;operation<=9;operation++)for(mode=0;mode<8;mode++)for(sa=0;sa<8;sa++)for(ma=0;ma<8;ma++)for(da=0;da<8;da++){
        memset(&root,0,sizeof(root));memset(&source,0,sizeof(source));
        root.h.ref=source.h.ref=10;type.np=0;type.size=8;
        root.a.len=3;root.a.t=&type;root.a.root=H;root.a.data=data;
        source.a.len=1;source.a.t=&type;source.a.root=H;source.a.data=data;
        first=1;last=2;owner=&root.a;input=&source.a;
        if(mode==0)root.a.t=nil;
        if(mode==1)root.a.t=H;
        if(mode==2)type.size=-1;
        if(mode==3)type.size=0x7fffffff;
        if(mode==4)root.a.data=nil;
        if(mode==5)root.a.data=H;
        if(mode==6)root.a.data=(uchar*)(uintptr)-4;
        if(mode==7){if(operation==8)first=-1;else source.a.data=nil;}
        memset(slots.bytes,0xa5,sizeof(slots.bytes));R.s=slots.bytes+8+sa;R.m=slots.bytes+40+ma;R.d=slots.bytes+80+da;
        if(operation==8){memmove(R.s,&first,sizeof(first));memmove(R.m,&last,sizeof(last));}
        else{offset=1;memmove(R.s,&input,sizeof(input));memmove(R.m,&offset,sizeof(offset));}
        memmove(R.d,&owner,sizeof(owner));memmove(expected,slots.bytes,sizeof(expected));which=operation;before=poolcalls;
        assert(!kprotect(run,nil) && last_error==exBounds && poolcalls==before);
        assert(!memcmp(expected,slots.bytes,sizeof(expected)) && root.h.ref==10 && source.h.ref==10);
        cases++;
    }
    type.size=sizeof(WORD);assert(!live && !worker.nerr);printf("PASS slice spans: %lu rejected geometry/alignment cases\n",cases);
}

int main(int argc, char **argv) {
    static const int widths[]={0,1,2,4,8,16,4096,0x7fffffff};
    unsigned int i;int checks=0,limit,before;
    Array *a,*b,*old;Heap *h;
    Type *types[]={&type};Modlink ml={0};
    WORD count,first,last,offset;
    List *saved;
    if(argc>1 && !strcmp(argv[1],"slice-slots")){sliceslots();return 0;}
    if(argc>1 && !strcmp(argv[1],"slice-spans")){slicespans();return 0;}
    if(argc>1 && !strcmp(argv[1],"borrowed-copies")) {
        borrowedcopies();
        return 0;
    }
    if(argc>1 && !strcmp(argv[1],"empty-views")) {
        emptyviews(0);
        assert(live==0 && type.ref==1 && Tarray.ref==1);
        puts("PASS empty views: 16 root-preserving slices without error injection");
        return 0;
    }
    type.size=4;module.type=types;module.ntype=1;ml.m=&module;ml.type=types;R.M=&ml;
    which=0;
    for(i=0;i<nelem(widths);i++) {
        tsz=widths[i];
        limit=tsz==0?0x7fffffff:(0x7fffffff-sizeof(Heap)-sizeof(Array))/tsz;
        length=limit;assert(kprotect(run,nil));checks++;
        if(limit<0x7fffffff){length=(WORD)limit+1;rejects(exHeap,1);checks++;}
        length=-1;rejects(exNegsize,1);checks++;
    }
    tsz=-1;length=0;rejects(exHeap,1);
    tsz=16;length=0x11111111;rejects(exHeap,1); /* old multiply-and-wrap heuristic */
    if(sizeof(WORD)>4){tsz=0;length=(WORD)((uvlong)1<<32);rejects(exHeap,1);}
    which=1;n=-1;rejects(exHeap,1);
    n=0x7fffffff;rejects(exHeap,1);
    n=0x7fffffff-sizeof(Heap);force_failure=1;rejects(exHeap,0);
    assert(requested==0x7fffffff);force_failure=0;
    n=0;assert(kprotect(run,nil));assert(allocated->t==nil && allocated->ref==1);
    destroy(H2D(void*,allocated));
    for(which=2;which<=3;which++) {
        type.size=-1;rejects(exHeap,1);type.size=0x7fffffff;rejects(exHeap,1);
        type.size=16;assert(kprotect(run,nil));
        if(which==3)for(i=0;i<16;i++)assert(H2D(uchar*,allocated)[i]==0);
        destroy(H2D(void*,allocated));
    }
    type.size=16;which=4;n=0x11111111;rejects(exHeap,1);
    n=-1;rejects(exNegsize,1);n=3;assert(kprotect(run,nil));
    a=H2D(Array*,allocated);assert(a->len==3 && a->t==&type && a->root==H);
    memset(a->data,42,48);copyfrom=a;which=5;assert(kprotect(run,nil));
    b=H2D(Array*,allocated);assert(b->len==3 && memcmp(a->data,b->data,48)==0);
    old=b;copyfrom=a;a->len=0x11111111;rejects(exHeap,1);a->len=3;
    destroy(a);destroy(old);
    which=6;link_count=-1;rejects(exHeap,1);link_count=0x7fffffff;rejects(exHeap,1);
    link_count=0;assert(kprotect(run,nil));
    assert(requested==sizeof(Heap)+sizeof(Modlink)-sizeof(Modl));
    assert(H2D(Modlink*,allocated)->nlinks==0 && H2D(Modlink*,allocated)->MP==H);
    destroy(H2D(void*,allocated));
    which=7;listsize=~(ulong)0;saved=list;rejects(exHeap,1);assert(list==saved);
    listsize=sizeof(WORD);assert(kprotect(run,nil));destroy(list);list=H;
    /* Generated pointer slots are initialized to H, not zero. */
    type.size=sizeof(void*);type.np=1;type.map[0]=0x80;
    a=H2D(Array*,heaparray(&type,3));
    for(i=0;i<3;i++)assert(((void**)a->data)[i]==H);
    destroy(a);type.np=0;type.size=sizeof(WORD);
    a=H2D(Array*,heaparray(&type,5));
    for(i=0;i<5;i++)((WORD*)a->data)[i]=i+1;
    first=1;last=4;R.s=&first;R.m=&last;R.d=&a;which=8;
    old=a;assert(kprotect(run,nil));assert(a->len==3 && a->root==old && a->data==old->data+sizeof(WORD));
    destroy(a); /* also releases the transferred root */
    a=H2D(Array*,heaparray(&type,2));first=INT_MIN;last=INT_MAX;
    rejects(exBounds,1);destroy(a);
    a=H;first=last=0;assert(kprotect(run,nil));
    /* Check end positions by subtraction, before an overflowing addition. */
    a=H2D(Array*,heaparray(&type,3));b=H2D(Array*,heaparray(&type,5));
    R.s=&a;R.d=&b;R.m=&offset;which=9;
    offset=INT_MAX;rejects(exBounds,1);offset=4;rejects(exBounds,1);
    offset=2;assert(kprotect(run,nil));destroy(a);destroy(b);
    /* Dis sizes keep their full WORD value until the range check. */
    a=H;count=0x11111111;type.size=16;offset=0;R.s=&count;R.m=&offset;R.d=&a;
    for(which=10;which<=11;which++) {
        rejects(exHeap,1);assert(a==H);
        if(sizeof(WORD)>4){count=(WORD)((uvlong)1<<32);rejects(exHeap,1);}
        count=2;assert(kprotect(run,nil));assert(a->len==2);destroy(a);a=H;
        count=0x11111111;
    }
    {
        Type scalar={1};void *value;
        scalar.size=sizeof(WORD);type.size=sizeof(void*);type.np=1;type.map[0]=0x80;
        a=H2D(Array*,heaparray(&type,3));b=H2D(Array*,heaparray(&type,5));
        value=H2D(void*,heapz(&scalar));((void**)a->data)[0]=value;
        ((void**)a->data)[2]=H2D(void*,heapz(&scalar));
        R.s=&a;R.d=&b;R.m=&offset;offset=1;which=9;
        assert(kprotect(run,nil));assert(D2H(value)->ref==2);
        destroy(a);assert(D2H(value)->ref==1);destroy(b);assert(scalar.ref==1);
        type.np=0;
    }
    pointercopies(); borrowedcopies(); emptycopies(); nilslices(); emptyviews(1); sliceslots(); slicespans();
    assert(live==0 && worker.nerr==0 && type.ref==1 && Tarray.ref==1 && Tmodlink.ref==1 && Tlist.ref==1);
    printf("PASS heap bounds: %d array boundaries, overflow counterexample, pool/header limits, copies, links, lists, slices, full-WORD VM sizes, 285 pointer overlaps and empty spans\n",checks);
    return 0;
}
