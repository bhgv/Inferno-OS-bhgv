/* Actual constructor, String factory and mapped MP cloning; no AWM code body. */
#include "native64-typed-module.h"
typedef struct MPLeaf MPLeaf;
typedef struct MPCell MPCell;
typedef struct MPCase MPCase;
struct MPLeaf {unsigned old,native,oldbytes,bytes;int kind;};
struct MPCell {unsigned old,width;uvlong bits;const uchar *utf;unsigned bytes;const Rune *runes;unsigned nrunes;};
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
struct MPBuild {const MPCase *data;Native64ModuleOwner owner;int jit;};
static int mpbuild(void *arg)
{
    MPBuild *b=arg;const MPCase *c=b->data;unsigned i,at;String *s;
    if(!native64typedmodule(c->spec,b->jit,&b->owner))error(exNomem);
    for(i=0;i<c->ncells;i++)if(!c->cells[i].width){
        at=mpat(c,c->cells+i);
        mpcheck((c->spec->datamap[at/64]&(0x80>>((at/8)%8)))!=0,"string-map");
        s=c2string((char*)c->cells[i].utf,c->cells[i].bytes);
        memmove(b->owner.module->origmp+at,&s,sizeof(s));
    }
    return 1;
}

static void mpverify(const MPCase *c,uchar *data,int refs)
{
    static uchar covered[1024*1024];
    unsigned i,j,at;uvlong bits;String *s;
    mpcheck(c->spec->databytes<=sizeof(covered),"oracle-extent");
    memset(covered,0,c->spec->databytes);
    for(i=0;i<c->ncells;i++){
        const MPCell *v=c->cells+i;at=mpat(c,v);
        mpcheck(at<=c->spec->databytes && (v->width?v->width:8)<=c->spec->databytes-at,"oracle-span");
        memset(covered+at,1,v->width?v->width:8);
        if(v->width){
            if(v->width==1)bits=data[at];
            else if(v->width==4){UWORD word;memmove(&word,data+at,4);bits=word;}
            else memmove(&bits,data+at,8);
            mpcheck(bits==v->bits,"scalar-value");
        }
        else{
            memmove(&s,data+at,sizeof(s));mpcheck(s!=nil && s!=H && D2H(s)->t==&Tstring,"string-owner");
            mpcheck(D2H(s)->ref==refs,"string-refcount");
            mpcheck((s->len<0?-s->len:s->len)==(int)v->nrunes,"string-length");
            for(j=0;j<v->nrunes;j++)mpcheck((s->len<0?s->Srune[j]:(uchar)s->Sascii[j])==v->runes[j],"string-value");
        }
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

int main(void)
{
    unsigned k,jit,i,attempts;int before;MPBuild b;uchar *a,*d;Type *type;
    Tstring.free=freestring;
    for(k=0;k<nelem(mpcases);k++)for(jit=0;jit<2;jit++){
        memset(&b,0,sizeof(b));b.data=mpcases+k;b.jit=jit;before=allocations;
        mpcheck(kprotect(mpbuild,&b),"constructor");attempts=allocations-before;
        mpverify(b.data,b.owner.module->origmp,1);type=b.owner.module->type[0];
        a=H2D(uchar*,heapz(type));newmp(a,b.owner.module->origmp,type);
        d=H2D(uchar*,heapz(type));newmp(d,b.owner.module->origmp,type);
        mpcheck(a!=d && a!=b.owner.module->origmp && d!=b.owner.module->origmp,"independent-clones");
        mpverify(b.data,a,3);mpverify(b.data,d,3);mpverify(b.data,b.owner.module->origmp,3);
        destroy(a);mpverify(b.data,d,2);mpverify(b.data,b.owner.module->origmp,2);
        destroy(d);mpverify(b.data,b.owner.module->origmp,1);
        mpcheck(native64typedmoduleclear(&b.owner),"retirement");mpcheck(!live && !rootcount && Tstring.ref==1,"empty");
        for(i=1;i<=attempts;i++){
            memset(&b.owner,0,sizeof(b.owner));failat=allocations+i;
            mpcheck(!kprotect(mpbuild,&b),"failure-injection");failat=0;
            mpcheck(native64typedmoduleclear(&b.owner),"rollback-retirement");
            mpcheck(!live && !rootcount && !worker.nerr && Tstring.ref==1,"rollback-empty");
        }
        printf("PASS startup MP data: %s jit=%u bytes=%u cells=%u allocation refusals=%u; exact values, roots, two clones and retirement\n",b.data->spec->path,jit,b.data->spec->databytes,b.data->ncells,attempts);
    }
    return 0;
}
