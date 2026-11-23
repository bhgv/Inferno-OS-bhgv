#include "../include/native64-record-layout.h"
/* Full declared layouts prove the leaf; field names and module names do not
 * select generic records. Existing closed protocols retain their encodings. */
static int
typedrecordowner(Addr*a,int mode)
{
 Type*t;
 if(mode!=Afpind&&mode!=Ampind)return 0;
 t=typedfieldowner(a);
 return t&&t->kind==Tref&&t->tof&&t->tof->kind==Tadt&&
  !typedlibraryrecord(t)&&!typedwmref(t,"Draw","Context")&&!typeddrawowner(a)&&!typedwindowowner(a);
}
static Type*
typedrecordvalue(Addr*a,int mode)
{
 Type*t=typedfieldowner(a);
 if(mode==Afpind||mode==Ampind)return t&&t->kind==Tref?typedarrayleaf(t->tof,a->offset):nil;
 return a->native64type?a->native64type:typedarrayleaf(a->decl?a->decl->ty:nil,a->decl?a->reg-a->decl->offset:0);
}
static int
typedrecordlayoutoperand(int mode,Addr*a,int root,Dis64TypedBody*p,long*out,unsigned int*wire)
{
 Type*t;Dis64Layout l={0};Addr owner;unsigned int i;long base;int actual,ok=0;
 if((mode!=Afpind&&mode!=Ampind)||(t=typedfieldowner(a))==nil||
    t->kind!=Tref||!t->tof||t->tof->kind!=Tadt||!dis64layouttype(t->tof,&l))goto done;
 for(i=0;i<l.count;i++)if(l.leaves[i].legacy==(unsigned int)a->offset)break;
 if(i==l.count||l.leaves[i].oldbytes!=4||l.leaves[i].bytes!=(root?8:4)||
    (root?!typedmanagedkind(l.leaves[i].kind):l.leaves[i].kind!=Tint)||l.leaves[i].native>4096)goto done;
 owner=*a;owner.offset=0;owner.native64type=t;
 if(!typedoperand(mode==Afpind?Afp:Amp,&owner,TypedRoot,0,0,p,&base,wire,&actual)||actual!=Tref||base>65535)goto done;
 *out=((long)l.leaves[i].native<<16)|base;*wire|=AIND;ok=1;
 done:dis64layoutfree(&l);return ok;
}
static int
typedrecordoperand(int mode,Addr*a,int root,Dis64TypedBody*p,long*out,unsigned int*wire)
{
 int actual;
 if(mode!=Afpind&&mode!=Ampind)return typedoperand(mode,a,root?TypedRoot:Tint,!root,0,p,out,wire,&actual);
 return typedrecordowner(a,mode)&&typedrecordlayoutoperand(mode,a,root,p,out,wire);
}
static int
typedrecordregister(Dis64TypedBody*p,unsigned int pc,unsigned int side,Type*t)
{
 unsigned int i;
 for(i=0;i<p->nrecordtypes;i++)if(p->recordtypes[i]==t)break;
 if(i==64)return 0;
 if(i==p->nrecordtypes)p->recordtypes[p->nrecordtypes++]=t;
 if(!p->recordowners){p->recordowners=allocmem(p->count*2*sizeof(*p->recordowners));memset(p->recordowners,0,p->count*2*sizeof(*p->recordowners));}
 p->recordowners[2*pc+side]=i+1;return 1;
}

static int
typedrecordargument(Inst *in, Inst *frame, Dis64TypedBody *p, Type *type,
                    unsigned int pc, unsigned int old, unsigned int offset, int variadic)
{
 Type *value;
 long source;
 unsigned int mode,op;
 int root=typedmanagedkind(type->kind);

 if((!root&&type->kind!=Tint)||in->op!=(root?IMOVP:IMOVW)||in->branch||
    in->mm!=Anone||in->dm!=Afpind||in->d.reg!=frame->d.reg||in->d.offset!=old||
    offset>65535||!typedrecordowner(&in->s,in->sm))
  return 0;
 value=typedrecordvalue(&in->s,in->sm);
 if(!value||!tequal(value,type)||!typedrecordoperand(in->sm,&in->s,root,p,&source,&mode)||
    !typedrecordregister(p,pc,0,typedfieldowner(&in->s)->tof))
  return 0;
 op=variadic&&!root?ICVTWL:in->op;
 p->code[pc]=(Dis64Instruction){op,SRC(mode)|DST(AFP|AIND),0,source,offset};
 p->returns[pc]=variadic?Native64TypedRecordVararg:Native64TypedRecordArg;
 return 1;
}

static void
typedrecordtree(Type*t)
{
 Decl*d;unsigned int n=0;
 fprint(2,"[%d,%d,[",t->kind,t->size);
 if(t->kind==Tadt||t->kind==Ttuple)for(d=t->ids;d;d=d->next)if(d->store==Dfield){fprint(2,"%s[%d,",n++?",":"",d->offset);typedrecordtree(d->ty);fprint(2,"]");}
 fprint(2,"]]");
}
static int
typedrecordinst(Inst*original,Dis64TypedBody*p,unsigned int pc,unsigned int stop)
{
 Inst copy=*original,*in=&copy;unsigned int w[2],r[2],op=in->op,sm,dm,mm=AXNON,wire;long src,dst,mid=0;int branch;Type*a,*b;
 if(!typedrecordowner(&in->s,in->sm)&&!typedrecordowner(&in->d,in->dm)&&!typedrecordowner(&in->m,in->mm))return 0;
 branch=op==IBEQW||op==IBNEW||op==IBLTW||op==IBLEW||op==IBGTW||op==IBGEW||op==IBEQC||op==IBNEC||op==IBEQL||op==IBNEL;
 if(branch&&typedrecordowner(&in->m,in->mm)){
  Addr swap=in->s;int mode=in->sm;in->s=in->m;in->sm=in->mm;in->m=swap;in->mm=mode;
  op=op==IBLTW?IBGTW:op==IBLEW?IBGEW:op==IBGTW?IBLTW:op==IBGEW?IBLEW:op;
 }
 a=typedrecordvalue(&in->s,in->sm);
 if(branch&&(op==IBEQW||op==IBNEW)&&a&&typedmanagedkind(a->kind))op=a->kind==Tstring?(op==IBEQW?IBEQC:IBNEC):(op==IBEQW?IBEQL:IBNEL);
 if(!native64recordshape(op,w,r)||!typedrecordoperand(in->sm,&in->s,r[0],p,&src,&sm))return 0;
 if(op==IINDC||op==ILENC||op==ICVTCW){if(!a||a->kind!=Tstring)return 0;}
 if(op==ILENA&&(!a||a->kind!=Tarray))return 0;
 if(op==ILENL&&(!a||a->kind!=Tlist))return 0;
 if(branch){
  if(in->dm!=Apc||!in->branch||in->d.offset!=in->branch->pc||in->d.offset<(int)p->first||in->d.offset>=(int)stop)return 0;
  dst=in->d.offset-p->first;dm=AIMM;
 }else if(in->dm==Afpind&&in->d.reg==16&&!in->d.offset){
  if(p->resultaggregate||p->resultpair||p->resulttriple||(r[1]?!a||!tequal(a,p->resulttype):p->resultkind!=Tint))return 0;
  dst=32;dm=AFP|AIND;
 }else if(in->branch||!typedrecordoperand(in->dm,&in->d,r[1],p,&dst,&dm))return 0;
 if(op==IMOVP){b=typedrecordvalue(&in->d,in->dm);if(dst==32&&dm==(AFP|AIND))b=p->resulttype;if(!a||!b||(!tequal(a,b)&&a->kind!=Tany))return 0;}
 if(in->mm!=Anone){int actual;
  if(op==IMOVP||op==IMOVW||op==ILENC||op==ILENA||op==ILENL||op==ICVTCW||op==ICVTWC)return 0;
  if(!typedoperand(in->mm,&in->m,r[0]&&op!=IINDC?TypedRoot:Tint,!r[0]||op==IINDC,1,p,&mid,&wire,&actual))return 0;
  if(r[0]&&op!=IINDC){b=typedrecordvalue(&in->m,in->mm);if(!a||!b||(b->kind!=Tany&&!tequal(a,b))||((op==IBEQC||op==IBNEC)&&a->kind!=Tstring))return 0;}
  mm=wire==AIMM?AXIMM:wire==AFP?AXINF:AXINM;
 }else if(branch||op==IINDC)return 0;
 if(typedrecordowner(&in->s,in->sm)&&!typedrecordregister(p,pc,0,typedfieldowner(&in->s)->tof))return 0;
 if(!branch&&typedrecordowner(&in->d,in->dm)&&!typedrecordregister(p,pc,1,typedfieldowner(&in->d)->tof))return 0;
 p->code[pc]=(Dis64Instruction){op,SRC(sm)|mm|DST(dm),mid,src,dst};p->returns[pc]=Native64TypedRecord;return 1;
}
