#ifndef NATIVE64_RAW_ARRAY_RUNTIME_H
#define NATIVE64_RAW_ARRAY_RUNTIME_H
#include "vmfetch.h"
#include "native64-array-layout.h"
/* Owned index proofs. Neither pointers nor this capability come from Dis files. */
typedef struct Native64RawIndex Native64RawIndex;
typedef struct Native64RawArrays Native64RawArrays;
struct Native64RawIndex {unsigned int pc,width,mapped,record;Type *frame;Inst instruction;};
struct Native64RawArrays {Native64RawArrays *self;Module *owner;unsigned int count;Native64RawIndex index[1];};
static unsigned int native64rawarraywidth(unsigned int c){return native64arraywidth(c);}
static void native64rawarrayfree(Module*m){Native64RawArrays*p=m->native64rawarrays;m->native64rawarrays=nil;m->native64rawarraysfree=nil;if(p!=nil)free(p);}
static void native64rawarraycheck(Array*a,unsigned int width,unsigned int mapped)
{Type*t;unsigned int i;if(a==H||a==nil)return;if((uintptr)a%8||(uintptr)a>(uintptr)-1-sizeof(Array)||D2H(a)->ref<=0||D2H(a)->t!=&Tarray)error(exBounds);t=a->t;if(t==nil||t==H||t->ref<=0||t->size!=(int)width||t->np<0||t->np>(t->size+63)/64)error(exBounds);if(mapped){if(t->np!=1||t->map[0]!=(mapped==1?0xc0:mapped))error(exBounds);return;}for(i=0;i<(unsigned int)t->np;i++)if(t->map[i])error(exBounds);}
/* Read the owner from its proved frame or module-data root before decoding. */
static Array*
native64rawarrayowner(VmFetch*f,unsigned int mode,long offset)
{
 Type*t;uchar*base;Array*a;unsigned int bit;
 if(mode==AFP)base=R.FP;
 else if(mode==AMP){
  if(f->module->mptype<0||f->module->mptype>=f->module->ntype||f->module->type==nil||f->module->type==H)error(exBounds);
  t=f->module->type[f->module->mptype];base=R.MP;
  if(t==nil||t==H||t->ref<=0||t->size<8||t->size>1024*1024||t->np<0||t->np>(t->size+63)/64||base==nil||base==H||(uintptr)base%8||(uintptr)base>(uintptr)-1-t->size||offset<0||offset%8||(unsigned long)offset>(unsigned int)t->size||8>(unsigned int)t->size-offset)error(exBounds);
  bit=offset/8;if(bit/8>=(unsigned int)t->np||!(t->map[bit/8]&(128>>(bit%8))))error(exBounds);
 }else{error(exBounds);return H;}
 memmove(&a,base+offset,sizeof(a));return a;
}
#include "native64-record-runtime.h"
static int
native64rawarraystep(VmFetch*f)
{Native64RawArrays*p=f->module->native64rawarrays;Native64RawIndex*v;unsigned int lo=0,hi,mid;Type*t;Array*a;uintptr base;
 if(p==nil)return 0;if(p==H||p->self!=p||p->owner!=f->module||!p->count||p->count>65536)error(exBounds);hi=p->count;
 while(lo<hi){mid=lo+(hi-lo)/2;if(p->index[mid].pc<(unsigned int)f->index)lo=mid+1;else hi=mid;}
 if(lo==p->count||p->index[lo].pc!=(unsigned int)f->index)return 0;v=p->index+lo;
 if((!v->record&&f->op!=IINDX&&f->op!=ISLICEA&&f->op!=ISLICELA)||memcmp(f->pc,&v->instruction,sizeof(Inst))||R.FP==nil||R.FP==H)error(exBounds);base=(uintptr)R.FP;if(base%8||base>(uintptr)-1-sizeof(Frame))error(exBounds);
 memmove(&t,R.FP+O(Frame,t),sizeof(t));if(t==nil){if(base<OA(Stkext,reg.tos.fu))error(exBounds);memmove(&t,(void*)(base-OA(Stkext,reg.tos.fu)+O(Stkext,reg.TR)),sizeof(t));}
 if(t!=v->frame||t==nil||t==H||t->ref<=0||base>(uintptr)-1-t->size)error(exBounds);
 if(v->record)native64recordcheck(f,&v->instruction);
 else if(f->op==IINDX){a=native64rawarrayowner(f,USRC(v->instruction.add),v->instruction.s.ind);native64rawarraycheck(a,v->width,v->mapped);}
 else if(f->op==ISLICEA){a=native64rawarrayowner(f,UDST(v->instruction.add),v->instruction.d.ind);native64rawarraycheck(a,v->width,v->mapped);}
 else{a=native64rawarrayowner(f,USRC(v->instruction.add),v->instruction.s.ind);native64rawarraycheck(a,v->width,v->mapped);if(a!=H&&a!=nil){a=native64rawarrayowner(f,UDST(v->instruction.add),v->instruction.d.ind);native64rawarraycheck(a,v->width,v->mapped);}}
 vmdecode(f);if(optab[f->op]==nil||optab[f->op]==(void(*)(void))H)error(exOp);optab[f->op]();return 1;
}
#endif
