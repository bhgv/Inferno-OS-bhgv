/* Actual constructor, String factory and mapped MP cloning; no AWM code body. */
#include "native64-typed-module.h"
typedef struct MPLeaf MPLeaf;
typedef struct MPCell MPCell;
typedef struct MPCase MPCase;
typedef struct MPArray MPArray;
struct MPLeaf {unsigned old,native,oldbytes,bytes;int kind;};
struct MPCell {unsigned old,width;uvlong bits;const uchar *utf;unsigned bytes;const Rune *runes;unsigned nrunes;const MPArray *array;};
struct MPArray {unsigned oldwidth,width,length,strings;const MPCell *cells;unsigned ncells;};
struct MPCase {const Native64ModuleSpec *spec;const MPLeaf *leaves;unsigned nleaves;const MPCell *cells;unsigned ncells;};
#include "awm-data.h"

static void mpcheck(int ok,char *predicate){if(!ok){fprintf(stderr,"AWM data predicate: %s\n",predicate);exit(10);}}
static unsigned mpat(const MPCase *c,const MPCell *cell)
{
    unsigned i,width=cell->width?cell->width:4;
    for(i=0;i<c->nleaves;i++){
        const MPLeaf *l=c->leaves+i;
        if(l->old<=cell->old && cell->old+width<=l->old+l->oldbytes)return l->native+cell->old-l->old;
    }
    mpcheck(0,"independent-address");return 0;
}

typedef struct MPBuild MPBuild;
struct MPBuild {const MPCase *data;Native64ModuleOwner owner;int jit;Type *types[256];unsigned ntypes;};
static void mpstore(uchar *data,const MPCell *v)
{
    String *s;UWORD word;uchar byte;
    if(!v->width){s=c2string((char*)v->utf,v->bytes);memmove(data,&s,sizeof(s));}
    else if(v->width==1){byte=v->bits;memmove(data,&byte,1);}
    else if(v->width==4){word=v->bits;memmove(data,&word,4);}
    else memmove(data,&v->bits,8);
}
static void mpcleanup(MPBuild *b)
{
    mpcheck(native64typedmoduleclear(&b->owner),"retirement");
    while(b->ntypes)freetype(b->types[--b->ntypes]);
}
static int mpbuild(void *arg)
{
    MPBuild *b=arg;const MPCase *c=b->data;unsigned i,j,at;Array *a;Type *type;uchar map[]={0x80};
    if(!native64typedmodule(c->spec,b->jit,&b->owner))error(exNomem);
    for(i=0;i<c->ncells;i++)if(!c->cells[i].width){
        at=mpat(c,c->cells+i);
        mpcheck((c->spec->datamap[at/64]&(0x80>>((at/8)%8)))!=0,"string-map");
        if(c->cells[i].array){
            const MPArray *v=c->cells[i].array;
            mpcheck(b->ntypes<nelem(b->types),"array-descriptor-limit");
            type=dtype(freeheap,v->width,v->strings?map:nil,v->strings?1:0);
            if(type==nil)error(exNomem);
            b->types[b->ntypes++]=type;
            a=H2D(Array*,heaparray(type,v->length));
            /* Publish before a String allocation can fail; MP owns rollback. */
            memmove(b->owner.module->origmp+at,&a,sizeof(a));
            memset(a->data,0,v->width*v->length);
            for(j=0;j<v->length;j++)initmem(type,a->data+j*v->width);
            for(j=0;j<v->ncells;j++)mpstore(a->data+(v->cells[j].old/v->oldwidth)*v->width,v->cells+j);
        }else mpstore(b->owner.module->origmp+at,c->cells+i);
    }
    return 1;
}

static void mpvalue(const MPCell *v,uchar *data,int refs)
{
    uvlong bits;String *s;unsigned j;
    if(v->width){
        if(v->width==1)bits=data[0];
        else if(v->width==4){UWORD word;memmove(&word,data,4);bits=word;}
        else memmove(&bits,data,8);
        mpcheck(bits==v->bits,"scalar-value");
    }else{
        memmove(&s,data,sizeof(s));mpcheck(s!=nil && s!=H && D2H(s)->t==&Tstring,"string-owner");
        mpcheck(D2H(s)->ref==refs,"string-refcount");
        mpcheck((s->len<0?-s->len:s->len)==(int)v->nrunes,"string-length");
        for(j=0;j<v->nrunes;j++)mpcheck((s->len<0?s->Srune[j]:(uchar)s->Sascii[j])==v->runes[j],"string-value");
    }
}

static void mparrayverify(const MPArray *v,uchar *slot,int refs)
{
    Array *a;unsigned i,j;void *p;
    memmove(&a,slot,sizeof(a));
    mpcheck(a!=nil && a!=H && D2H(a)->t==&Tarray && D2H(a)->ref==1,"array-owner");
    mpcheck(a->len==(int)v->length && a->t->size==(int)v->width && a->root==H && a->data==(uchar*)a+sizeof(Array),"array-layout");
    mpcheck(a->t->ref==refs+1 && a->t->np==(int)v->strings && (!v->strings || a->t->map[0]==0x80),"array-element-map");
    for(i=0;i<v->length;i++){
        for(j=0;j<v->ncells && v->cells[j].old/v->oldwidth!=i;j++);
        /* Each MP owns a distinct array; contained Strings have shared owners. */
        if(j<v->ncells)mpvalue(v->cells+j,a->data+i*v->width,refs);
        else if(v->strings){memmove(&p,a->data+i*v->width,sizeof(p));mpcheck(p==H,"array-default-root");}
        else for(j=0;j<v->width;j++)mpcheck(a->data[i*v->width+j]==0,"array-default-zero");
    }
}

static void mpverify(const MPCase *c,uchar *data,int refs)
{
    static uchar covered[1024*1024];
    unsigned i,j,at;
    mpcheck(c->spec->databytes<=sizeof(covered),"oracle-extent");
    memset(covered,0,c->spec->databytes);
    for(i=0;i<c->ncells;i++){
        const MPCell *v=c->cells+i;at=mpat(c,v);
        mpcheck(at<=c->spec->databytes && (v->width?v->width:8)<=c->spec->databytes-at,"oracle-span");
        memset(covered+at,1,v->width?v->width:8);
        if(v->array)mparrayverify(v->array,data+at,refs);
        else mpvalue(v,data+at,refs);
    }
    for(i=0;i<c->spec->databytes;i++)
        if(!covered[i] && !(c->spec->datamap[i/64]&(0x80>>((i/8)%8))))
            mpcheck(data[i]==0,"default-zero");
    for(i=0;i<c->nleaves;i++){
        const MPLeaf *l=c->leaves+i;
        if(l->kind<0 || l->oldbytes!=4 || l->bytes!=8)continue;
        for(j=0;j<c->ncells && c->cells[j].old!=l->old;j++);
        if(j==c->ncells){void *p;memmove(&p,data+l->native,8);mpcheck(p==H,"default-root");}
    }
}

typedef struct MPClone MPClone;
struct MPClone {Module *module;uchar *data;int rooted;};
static int mpclone(void *arg)
{
    MPClone *c=arg;
    c->data=H2D(uchar*,heapz(c->module->type[0]));
    ptradd(D2H(c->data));c->rooted=1;
    newmp(c->data,c->module->origmp,c->module->type[0]);return 1;
}
static void mpcloneclear(MPClone *c)
{
    if(c->rooted)ptrdel(D2H(c->data));
    destroy(c->data);c->data=H;c->rooted=0;
}
static void mpdistinct(const MPCase *c,uchar *a,uchar *b)
{
    unsigned i,at;Array *x,*y;
    for(i=0;i<c->ncells;i++)if(c->cells[i].array){
        at=mpat(c,c->cells+i);memmove(&x,a+at,sizeof(x));memmove(&y,b+at,sizeof(y));
        mpcheck(x!=y && x->data!=y->data,"independent-array-clones");
    }
}

int main(void)
{
    unsigned k,jit,i,attempts,cloneattempts;int before;MPBuild b;MPClone a,d;
    Tstring.free=freestring;Tarray.free=freearray;Tarray.size=sizeof(Array);
    for(k=0;k<nelem(mpcases);k++)for(jit=0;jit<2;jit++){
        memset(&b,0,sizeof(b));b.data=mpcases+k;b.jit=jit;before=allocations;
        mpcheck(kprotect(mpbuild,&b),"constructor");attempts=allocations-before;
        mpverify(b.data,b.owner.module->origmp,1);
        a=(MPClone){b.owner.module,H,0};d=a;before=allocations;
        mpcheck(kprotect(mpclone,&a),"first-clone");cloneattempts=allocations-before;
        mpcheck(kprotect(mpclone,&d),"second-clone");
        mpcheck(a.data!=d.data && a.data!=b.owner.module->origmp && d.data!=b.owner.module->origmp,"independent-clones");
        mpdistinct(b.data,a.data,d.data);mpdistinct(b.data,a.data,b.owner.module->origmp);
        mpverify(b.data,a.data,3);mpverify(b.data,d.data,3);mpverify(b.data,b.owner.module->origmp,3);
        mpcloneclear(&a);mpverify(b.data,d.data,2);mpverify(b.data,b.owner.module->origmp,2);
        mpcloneclear(&d);mpverify(b.data,b.owner.module->origmp,1);
        for(i=1;i<=cloneattempts;i++){
            a=(MPClone){b.owner.module,H,0};failat=allocations+i;
            mpcheck(!kprotect(mpclone,&a),"clone-failure-injection");failat=0;
            mpcloneclear(&a);mpverify(b.data,b.owner.module->origmp,1);
            mpcheck(rootcount==1 && !worker.nerr,"clone-rollback-roots");
        }
        mpcleanup(&b);mpcheck(!live && !rootcount && Tstring.ref==1 && Tarray.ref==1,"empty");
        for(i=1;i<=attempts;i++){
            memset(&b.owner,0,sizeof(b.owner));failat=allocations+i;
            mpcheck(!kprotect(mpbuild,&b),"failure-injection");failat=0;
            mpcleanup(&b);
            mpcheck(!live && !rootcount && !worker.nerr && Tstring.ref==1 && Tarray.ref==1,"rollback-empty");
        }
        printf("PASS startup MP data: %s jit=%u bytes=%u cells=%u allocation refusals=%u clone refusals=%u; exact values, roots, two independent clones and retirement\n",b.data->spec->path,jit,b.data->spec->databytes,b.data->ncells,attempts,cloneattempts);
    }
    return 0;
}
