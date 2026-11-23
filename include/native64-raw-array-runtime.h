#ifndef NATIVE64_RAW_ARRAY_RUNTIME_H
#define NATIVE64_RAW_ARRAY_RUNTIME_H
#include "vmfetch.h"
#include "native64-array-layout.h"
/* Owned checks for arrays and record fields in a constructed native64 module. */
typedef struct Native64RawIndex Native64RawIndex;
typedef struct Native64RawArrays Native64RawArrays;
enum { Native64ArrayStep, Native64RecordStep, Native64ArrayFieldIndex, Native64ArrayFieldUse,
       Native64RecordArgStep, Native64ArrayRecordStep, Native64ArrayRecordUse,
       Native64ArrayNestedIndex, Native64RecordObjectStep };
struct Native64RawIndex {unsigned int pc,width,mapped,kind;Type *frame;Inst instruction;};
struct Native64RawArrays {Native64RawArrays *self;Module *owner;unsigned int count;Native64RawIndex index[1];};
static unsigned int native64rawarraywidth(unsigned int c){return native64arraywidth(c);}
static void native64rawarrayfree(Module*m){Native64RawArrays*p=m->native64rawarrays;m->native64rawarrays=nil;m->native64rawarraysfree=nil;if(p!=nil)free(p);}
static void native64rawarraycheck(Array*a,unsigned int width,unsigned int mapped)
{Type*t;unsigned int i;if(a==H||a==nil)return;if((uintptr)a<=sizeof(Heap)||(uintptr)a%8||(uintptr)a>(uintptr)-1-sizeof(Array)||D2H(a)->ref<=0||D2H(a)->t!=&Tarray)error(exBounds);t=a->t;if(t==nil||t==H||t->ref<=0||t->size!=(int)width||t->np<0||t->np>(t->size+63)/64)error(exBounds);if(mapped){if(t->np!=1||t->map[0]!=(mapped==1?0xc0:mapped))error(exBounds);return;}for(i=0;i<(unsigned int)t->np;i++)if(t->map[i])error(exBounds);}
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

static Array*
native64arrayvalue(VmFetch *f, unsigned int mode, Adr *operand)
{
 Array *array;
 if(!(mode&AIND))
  return native64rawarrayowner(f,mode,operand->ind);
 memmove(&array,native64recordfield(f,mode,operand,8,1),sizeof(array));
 return array;
}

static void
native64arrayrecordcheck(VmFetch *f, Native64RawIndex *proof)
{
 Inst *in=&proof->instruction;
 unsigned int mode=USRC(in->add);
 int destination=mode==(AFP|AIND)&&(in->s.i.f==48||in->s.i.f==56);
 if(destination)mode=UDST(in->add);
 native64recordspan(f,mode,destination?&in->d:&in->s,proof->width,proof->mapped);
}

static void
native64arraycheckstep(VmFetch *f, Native64RawIndex *proof)
{
 Inst *in=&proof->instruction;
 unsigned int sm=USRC(in->add),dm=UDST(in->add);
 Array *source,*destination;

 switch(f->op){
 case IINDX:
  source=native64arrayvalue(f,sm,&in->s);
  if((dm&AIND)&&proof->kind!=Native64ArrayNestedIndex)native64recordfield(f,dm,&in->d,4,0);
  native64rawarraycheck(source,proof->width,proof->mapped);
  break;
 case INEWAZ:
  if(sm&AIND)native64recordfield(f,sm,&in->s,4,0);
  if(dm&AIND)native64recordfield(f,dm,&in->d,8,1);
  break;
 case ISLICEA:
  if(sm&AIND)native64recordfield(f,sm,&in->s,4,0);
  destination=native64arrayvalue(f,dm,&in->d);
  native64rawarraycheck(destination,proof->width,proof->mapped);
  break;
 case ISLICELA:
  source=native64arrayvalue(f,sm,&in->s);
  native64rawarraycheck(source,proof->width,proof->mapped);
  if(source!=H&&source!=nil){
   destination=native64arrayvalue(f,dm,&in->d);
   native64rawarraycheck(destination,proof->width,proof->mapped);
  }else if(dm&AIND){
   /* Decoding still needs the destination cell for this no-op copy. */
   native64recordfield(f,dm,&in->d,8,1);
  }
  break;
 default:
  error(exBounds);
 }
}

static int
native64rawarraystep(VmFetch *f)
{
 Native64RawArrays *proofs=f->module->native64rawarrays;
 Native64RawIndex *proof,*use=nil;
 VmFetch next;
 unsigned int lo=0,hi,mid;
 Type *frame;
 uintptr base=(uintptr)R.FP;

 if(proofs==nil)return 0;
 if(proofs==H||proofs->self!=proofs||proofs->owner!=f->module||!proofs->count||proofs->count>65536)
  error(exBounds);
 hi=proofs->count;
 while(lo<hi){
  mid=lo+(hi-lo)/2;
  if(proofs->index[mid].pc<(unsigned int)f->index)lo=mid+1;else hi=mid;
 }
 if(lo==proofs->count||proofs->index[lo].pc!=(unsigned int)f->index)return 0;
 proof=proofs->index+lo;
 if(proof->kind==Native64ArrayFieldUse||proof->kind==Native64ArrayRecordUse||
    memcmp(f->pc,&proof->instruction,sizeof(Inst))||
    R.FP==nil||R.FP==H||base%8||base>(uintptr)-1-sizeof(Frame))
  error(exBounds);
 memmove(&frame,R.FP+O(Frame,t),sizeof(frame));
 if(frame==nil){
  if(base<OA(Stkext,reg.tos.fu))error(exBounds);
  memmove(&frame,(void*)(base-OA(Stkext,reg.tos.fu)+O(Stkext,reg.TR)),sizeof(frame));
 }
 if(frame!=proof->frame||frame==nil||frame==H||frame->ref<=0||base>(uintptr)-1-frame->size)
  error(exBounds);
 if(proof->kind==Native64ArrayFieldIndex){
  if(lo+1>=proofs->count||f->index+1>=f->module->nprog)error(exBounds);
  use=proof+1;
  if((use->kind!=Native64ArrayFieldUse&&use->kind!=Native64ArrayRecordUse)||
     use->pc!=proof->pc+1||use->frame!=frame||
     memcmp(f->pc+1,&use->instruction,sizeof(Inst)))error(exBounds);
  next=(VmFetch){f->module,f->pc+1,f->index+1,use->instruction.op,use->instruction.add};
 }
 if(proof->kind==Native64RecordStep)native64recordcheck(f,&proof->instruction);
 else if(proof->kind==Native64RecordObjectStep)
  native64recordobjectcheck(f,&proof->instruction,proof->width,proof->mapped);
 else if(proof->kind==Native64ArrayRecordStep)native64arrayrecordcheck(f,proof);
 else if(proof->kind==Native64RecordArgStep){
  Inst *in=&proof->instruction;
  int root=in->op==IMOVP||in->op==ILENA;
  void *field=native64recordfield(f,USRC(in->add),&in->s,root?8:4,root);
  if(in->op==ILENA){
   Array *array;
   memmove(&array,field,sizeof(array));
   if(array!=H&&array!=nil&&((uintptr)array<=sizeof(Heap)||(uintptr)array%8||
      (uintptr)array>(uintptr)-1-sizeof(Array)||D2H(array)->ref<=0||D2H(array)->t!=&Tarray))
    error(exBounds);
  }
 }
 else native64arraycheckstep(f,proof);
 vmdecode(f);
 vmrun(f);
 if(use!=nil){
  /* Only the index opcode has executed. Both instructions share this quantum. */
  if(use->kind==Native64ArrayRecordUse)native64arrayrecordcheck(&next,use);
  vmdecode(&next);
  vmrun(&next);
 }
 return 1;
}
#endif
