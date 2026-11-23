#ifndef NATIVE64_RECORD_RUNTIME_H
#define NATIVE64_RECORD_RUNTIME_H
#include "native64-record-layout.h"
/* The owner is a proved mapped FP/MP slot. Check the actual heap Type before
 * vmdecode forms a field address, including every byte's GC role. */
static void
native64recordfield(VmFetch*f,unsigned int mode,Adr*a,unsigned int width,unsigned int root)
{
 void*value;Type*t;Heap*h;unsigned int i,bit,off=a->i.s;
 value=native64rawarrayowner(f,mode&~AIND,a->i.f);
 if(value==H||value==nil)error(exNilref);
 if((uintptr)value<=sizeof(Heap)||(uintptr)value%8||(uintptr)value>(uintptr)-1-off-width)error(exBounds);
 h=D2H(value);t=h->t;
 if(h->ref<=0||t==nil||t==H||t->ref<=0||t->size<0||t->size>1024*1024||t->np<0||t->np>(t->size+63)/64||off>(unsigned int)t->size||width>(unsigned int)t->size-off)error(exBounds);
 for(i=off;i<off+width;i++){bit=i/8;if(!!(bit/8<(unsigned int)t->np&&(t->map[bit/8]&(128>>(bit%8))))!=root)error(exBounds);}
}
static void
native64recordcheck(VmFetch*f,Inst*p)
{
 unsigned int w[2],r[2],sm=USRC(p->add),dm=UDST(p->add);
 if(!native64recordshape(p->op,w,r))error(exBounds);
 if(sm&AIND)native64recordfield(f,sm,&p->s,w[0],r[0]);
 if((dm&AIND)&&!(dm==(AFP|AIND)&&p->d.i.f==32&&!p->d.i.s))native64recordfield(f,dm,&p->d,w[1],r[1]);
}
#endif
