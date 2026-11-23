#ifndef NATIVE64_RECORD_RUNTIME_H
#define NATIVE64_RECORD_RUNTIME_H
#include "native64-record-layout.h"
/* The owner is a proved mapped FP/MP slot. Check the actual heap Type before
 * vmdecode forms a field address, including every byte's GC role. */
static void*
native64recordpayload(VmFetch*f,unsigned int mode,Adr*a,unsigned int width,Type **type)
{
 void*value;Type*t;Heap*h;unsigned int off=a->i.s;
 value=native64rawarrayowner(f,mode&~AIND,a->i.f);
 if(value==H||value==nil)error(exNilref);
 if((uintptr)value<=sizeof(Heap)||(uintptr)value%8||(uintptr)value>(uintptr)-1-off-width)error(exBounds);
 h=D2H(value);t=h->t;
 if(h->ref<=0||t==nil||t==H||t->ref<=0||t->size<0||t->size>1024*1024||t->np<0||t->np>(t->size+63)/64||off>(unsigned int)t->size||width>(unsigned int)t->size-off)error(exBounds);
 *type=t;
 return (uchar*)value+off;
}
static void*
native64recordspan(VmFetch*f,unsigned int mode,Adr*a,unsigned int width,unsigned int rootbytes)
{
 Type *t;
 void *payload=native64recordpayload(f,mode,a,width,&t);
 unsigned int i,bit,off=a->i.s;int root;
 for(i=off;i<off+width;i++){bit=i/8;root=i-off<rootbytes;if(!!(bit/8<(unsigned int)t->np&&(t->map[bit/8]&(128>>(bit%8))))!=root)error(exBounds);}
 return payload;
}

static void
native64recordcheckmap(Type *type, unsigned int offset, unsigned int bytes, unsigned int mask)
{
 unsigned int i,bit;
 for(i=0;i<(bytes+7)/8;i++){
  bit=offset/8+i;
  if(!!(bit/8<(unsigned int)type->np&&(type->map[bit/8]&(128>>(bit%8))))!=!!(mask&(1U<<i)))
   error(exBounds);
 }
}

static void
native64recordlistcheck(VmFetch *f, Inst *in, unsigned int bytes, unsigned int mask)
{
 int head=in->op==IHEADMP;
 unsigned int mode=head?USRC(in->add):UDST(in->add);
 Adr *operand=head?&in->s:&in->d;
 List *list;
 Type *item;
 if(mode&AIND)memmove(&list,native64recordspan(f,mode,operand,8,8),sizeof(list));
 else list=(List*)native64rawarrayowner(f,mode,operand->ind);
 if(list==H||list==nil)return;
 if((uintptr)list<=sizeof(Heap)||(uintptr)list%sizeof(void*)||
    (uintptr)list>(uintptr)-1-offsetof(List,data)-bytes||
    D2H(list)->ref<=0||D2H(list)->t!=&Tlist)error(exType);
 item=list->t;
 if(item==nil||item==H||item->ref<=0||item->size!=(int)bytes||
    item->np<0||item->np>(int)((bytes+63)/64))error(exType);
 native64recordcheckmap(item,0,bytes,mask);
}

static void
native64recordobjectcheck(VmFetch *f, Inst *in, unsigned int bytes, unsigned int mask)
{
 int copy=in->op==IMOVMP;
 int list=in->op==ICONSMP||in->op==IHEADMP;
 unsigned int id=copy||list?in->reg:in->s.imm,sm=USRC(in->add),dm=UDST(in->add);
 Type *t;
 if(in->op==IMOVM){
  if(in->reg!=bytes||mask)error(exBounds);
  if(sm&AIND)native64recordspan(f,sm,&in->s,bytes,0);
  if(dm&AIND)native64recordspan(f,dm,&in->d,bytes,0);
  return;
 }
 if(id>=(unsigned int)f->module->ntype||f->module->type==nil||f->module->type==H||
    (t=f->module->type[id])==nil||t==H||t->ref<=0||t->size!=(int)bytes||t->np!=(int)((bytes+63)/64))
  error(exBounds);
 native64recordcheckmap(t,0,bytes,mask);
 if(list){native64recordlistcheck(f,in,bytes,mask);return;}
 if(!copy)return;
 if(sm&AIND){
  native64recordpayload(f,sm,&in->s,bytes,&t);
  native64recordcheckmap(t,in->s.i.s,bytes,mask);
 }
 if(dm&AIND){
  native64recordpayload(f,dm,&in->d,bytes,&t);
  native64recordcheckmap(t,in->d.i.s,bytes,mask);
 }
}
static void*
native64recordfield(VmFetch*f,unsigned int mode,Adr*a,unsigned int width,unsigned int root)
{
 return native64recordspan(f,mode,a,width,root?width:0);
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
