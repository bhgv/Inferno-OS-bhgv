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
 if(!p->recordowners){p->recordowners=allocmem(p->count*3*sizeof(*p->recordowners));memset(p->recordowners,0,p->count*3*sizeof(*p->recordowners));}
 p->recordowners[3*pc+side]=i+1;return 1;
}

static int
typedrecordlayoutdescriptor(Type *type, Desc *desc, Dis64Layout *layout, long *shape)
{
 unsigned int i,bits=0,legacy=0,n;
 if(!type||(type->kind!=Tadt&&type->kind!=Ttuple)||!desc||
    !dis64layouttype(type,layout)||layout->legacybytes!=(unsigned int)desc->size||
    !layout->bytes||layout->bytes>Native64RecordBytes||layout->bytes%4||
    layout->legacybytes>Native64RecordBytes||desc->nmap<0||
    (unsigned int)desc->nmap>(layout->legacybytes+31)/32||
    (desc->nmap&&!desc->map))
  return 0;
 for(i=0;i<layout->count;i++){
  Dis64LayoutLeaf *l=layout->leaves+i;
  /* An untyped nil literal still occupies a mapped reference cell. */
  if(!typedmanagedkind(l->kind)&&l->kind!=Tany)continue;
  if(l->legacy%4||l->native%8||l->oldbytes!=4||l->bytes!=8)return 0;
  bits|=1U<<(l->native/8);
 }
 for(i=0;i<(unsigned int)desc->nmap*8;i++){
  if(!(desc->map[i/8]&(128>>(i%8))))continue;
  for(n=0;n<layout->count;n++)if(layout->leaves[n].legacy==i*4)break;
  if(n==layout->count||(!typedmanagedkind(layout->leaves[n].kind)&&layout->leaves[n].kind!=Tany))return 0;
  legacy|=1U<<(layout->leaves[n].native/8);
 }
 if(bits!=legacy)return 0;
 *shape=(layout->bytes/4L<<Native64RecordMapBits)|bits;
 return native64recorddescriptor(*shape);
}

static int
typedrecorddescriptor(Decl *d, Dis64Layout *layout, long *shape)
{
 return d&&typedrecordlayoutdescriptor(d->ty,d->desc,layout,shape);
}

static int
typedrecordnew(Inst *in, Dis64TypedBody *p, unsigned int pc)
{
 Dis64Layout layout={0};
 Decl *d=in->s.decl;
 Type *type=typedrecordvalue(&in->d,in->dm);
 unsigned int dm;
 long dest,shape;
 int actual,ok=0;

 if(in->sm!=Adesc||in->mm!=Anone||in->branch||!d||!d->desc||
    d->desc->id!=in->s.offset||!typedrecorddescriptor(d,&layout,&shape))
  goto done;
 if(in->dm==Afpind&&in->d.reg==16&&!in->d.offset){
  if(p->resultpair||p->resulttriple||p->resultaggregate)goto done;
  type=p->resulttype;dest=32;dm=AFP|AIND;
 }else if((in->dm!=Afp&&in->dm!=Amp)||
          !typedoperand(in->dm,&in->d,TypedRoot,0,0,p,&dest,&dm,&actual))
  goto done;
 if(!type||type->kind!=Tref||!tequal(type->tof,d->ty)||
    !typedrecordregister(p,pc,2,d->ty))
  goto done;
 p->code[pc]=(Dis64Instruction){INEWZ,SRC(AIMM)|DST(dm),shape,0,dest};
 p->returns[pc]=Native64TypedRecordNew;ok=1;
done:
 dis64layoutfree(&layout);
 return ok;
}

/* Keep an aggregate's natural span, including its padding, within its owner. */
static int
typedrecordcopyoperand(Dis64TypedBody *p, Addr *addr, int mode, Type *type,
                       Dis64Layout *part, long *operand, unsigned int *wire)
{
 Dis64Layout record={0},*storage;
 Type *owner;
 Addr pointer;
 unsigned int old,field,base,i;
 long cell;
 int actual,ok=0;

 if(mode==Afpind||mode==Ampind){
  owner=typedfieldowner(addr);
  if(!owner||owner->kind!=Tref||!owner->tof||owner->tof->kind!=Tadt||
     !typedarraysubtype(owner->tof,addr->offset,type)||!dis64layouttype(owner->tof,&record))
   goto done;
  pointer=*addr;pointer.offset=0;pointer.native64type=owner;
  if(!typedoperand(mode==Afpind?Afp:Amp,&pointer,TypedRoot,0,0,p,&cell,wire,&actual)||
     actual!=Tref||cell>65535)
   goto done;
  old=addr->offset;storage=&record;base=cell;
 }else{
  if((mode!=Afp&&mode!=Amp)||addr->offset||!addr->decl||
     !typedarraysubtype(addr->decl->ty,addr->reg-addr->decl->offset,type))
   goto done;
  old=addr->reg;storage=mode==Afp?&p->frame:&p->data;*wire=mode==Afp?AFP:AMP;base=0;
 }
 if(!dis64layoutbindresult(storage,part,old,part->alignment,0,&field))
  goto done;
 for(i=0;i<storage->count;i++)
  if(storage->leaves[i].legacy>=old+part->legacybytes&&storage->leaves[i].native<field+part->bytes)
   goto done;
 if(mode==Afpind||mode==Ampind){
  if(field>4096)goto done;
  *operand=((long)field<<16)|base;*wire|=AIND;
 }else *operand=field;
 ok=1;
done:
 dis64layoutfree(&record);
 return ok;
}

static int
typedrecordwordcopy(Inst *in, Dis64TypedBody *p, unsigned int pc)
{
 Dis64Layout part={0},geometry={0};
 int read=in->sm==Afpind||in->sm==Ampind, ok=0;
 Addr *indirect=read?&in->s:&in->d;
 Type *owner=typedfieldowner(indirect),*t=indirect->native64type;
 unsigned int sm,dm,i;
 long src,dst;

 if((!t||t->kind!=Tadt)&&owner&&owner->kind==Tref&&!indirect->offset)t=owner->tof;
 if(in->mm!=Aimm||in->branch||!t||t->kind!=Tadt||typedwordgeometry(t,&geometry)||!dis64layouttype(t,&part)||
    part.legacybytes!=in->m.offset||!part.bytes||part.bytes>Native64RecordBytes||
    part.alignment!=4||part.count*4!=part.bytes)goto done;
 for(i=0;i<part.count;i++)if(part.leaves[i].kind!=Tint)goto done;
 if(!typedrecordcopyoperand(p,&in->s,in->sm,t,&part,&src,&sm)||
    !typedrecordcopyoperand(p,&in->d,in->dm,t,&part,&dst,&dm)||!((sm|dm)&AIND))goto done;
 if((sm&AIND)&&!typedrecordregister(p,pc,0,typedfieldowner(&in->s)->tof))goto done;
 if((dm&AIND)&&!typedrecordregister(p,pc,1,typedfieldowner(&in->d)->tof))goto done;
 if(!typedrecordregister(p,pc,2,t))goto done;
 p->code[pc]=(Dis64Instruction){IMOVM,SRC(sm)|AXIMM|DST(dm),part.bytes/4L<<Native64RecordMapBits,src,dst};
 p->returns[pc]=Native64TypedRecordCopy;
 ok=1;
done:
 dis64layoutfree(&part);
 dis64layoutfree(&geometry);
 return ok;
}

static int
typedrecordcopy(Inst *in, Dis64TypedBody *p, unsigned int pc)
{
 Dis64Layout layout={0};
 Decl *d=in->m.decl;
 long src,dst,shape;
 unsigned int sm,dm;
 int ok=0;
 if(in->mm!=Adesc||in->branch||!d||!d->desc||d->desc->id!=in->m.offset||
    !typedrecorddescriptor(d,&layout,&shape)||!native64recordmask(shape)||
    !typedrecordcopyoperand(p,&in->s,in->sm,d->ty,&layout,&src,&sm)||
    !typedrecordcopyoperand(p,&in->d,in->dm,d->ty,&layout,&dst,&dm)||
    !typedrecordregister(p,pc,2,d->ty))
  goto done;
 if((sm&AIND)&&!typedrecordregister(p,pc,0,typedfieldowner(&in->s)->tof))goto done;
 if((dm&AIND)&&!typedrecordregister(p,pc,1,typedfieldowner(&in->d)->tof))goto done;
 p->code[pc]=(Dis64Instruction){IMOVMP,SRC(sm)|AXIMM|DST(dm),shape,src,dst};
 p->returns[pc]=Native64TypedRecordCopy;ok=1;
done:
 dis64layoutfree(&layout);
 return ok;
}

static int
typedrecordresult(Inst *in, Dis64TypedBody *p, Type *result, unsigned int pc,
                  unsigned int *base, int local)
{
 Type *field;
 long source;
 unsigned int mode;
 if(in->sm!=Afpind||(!local&&!typedrecordowner(&in->s,in->sm))||
    !(field=typedrecordvalue(&in->s,in->sm))||!tequal(result,field)||
    !typedrecordlayoutoperand(in->sm,&in->s,1,p,&source,&mode)||
    !typedrecordregister(p,pc,0,typedfieldowner(&in->s)->tof))
  return 0;
 *base=source&65535;
 p->code[pc]=(Dis64Instruction){ILEA,SRC(mode)|DST(AFP|AIND),0,source,32};
 p->returns[pc]=Native64TypedRecordResult;
 return 1;
}

static int
typedrecordpath(Inst *in, Dis64TypedBody *p, unsigned int pc)
{
 long source,dest;
 unsigned int sm,dm;
 int actual;
 Type *field=typedrecordvalue(&in->s,in->sm),*target=typedrecordvalue(&in->d,in->dm);
 if(!field||field->kind!=Tref||!target||!tequal(field,target)||
    !typedrecordlayoutoperand(in->sm,&in->s,1,p,&source,&sm)||
    !typedoperand(Afp,&in->d,TypedRoot,0,0,p,&dest,&dm,&actual)||actual!=Tref||
    !typedrecordregister(p,pc,0,typedfieldowner(&in->s)->tof))
  return 0;
 p->code[pc]=(Dis64Instruction){IMOVP,SRC(sm)|DST(dm),
  (Native64RecordPath<<16)|((unsigned long)source>>16),source&65535,dest};
 return 1;
}

static int
typedrecordargument(Inst *in, Inst *frame, Dis64TypedBody *p, Type *type,
                    unsigned int pc, unsigned int old, unsigned int offset, int variadic)
{
 Type *value;
 long source;
 unsigned int mode,op;
 int root=typedmanagedkind(type->kind),length=in->op==ILENA&&type->kind==Tint&&!variadic;

 if((!root&&type->kind!=Tint)||(!length&&in->op!=(root?IMOVP:IMOVW))||in->branch||
    in->mm!=Anone||in->dm!=Afpind||in->d.reg!=frame->d.reg||in->d.offset!=old||
    offset>65535||(!length&&!(type->kind==Tstring&&!variadic)&&!typedrecordowner(&in->s,in->sm)))
  return 0;
 value=typedrecordvalue(&in->s,in->sm);
 if(!value||(length?value->kind!=Tarray:!tequal(value,type))||
    !typedrecordlayoutoperand(in->sm,&in->s,root||length,p,&source,&mode)||
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
 if(op==ICASE||op==IGOTO)return 0;
 if(op!=IADDC&&!typedrecordowner(&in->s,in->sm)&&!typedrecordowner(&in->d,in->dm)&&!typedrecordowner(&in->m,in->mm))return 0;
 branch=op==IBEQW||op==IBNEW||op==IBLTW||op==IBLEW||op==IBGTW||op==IBGEW||op==IBEQC||op==IBNEC||op==IBEQL||op==IBNEL;
 if(branch&&typedrecordowner(&in->m,in->mm)){
  Addr swap=in->s;int mode=in->sm;in->s=in->m;in->sm=in->mm;in->m=swap;in->mm=mode;
  op=op==IBLTW?IBGTW:op==IBLEW?IBGEW:op==IBGTW?IBLTW:op==IBGEW?IBLEW:op;
 }
 a=typedrecordvalue(&in->s,in->sm);
 if(branch&&(op==IBEQW||op==IBNEW)&&a&&typedmanagedkind(a->kind))op=a->kind==Tstring?(op==IBEQW?IBEQC:IBNEC):(op==IBEQW?IBEQL:IBNEL);
 if(!native64recordshape(op,w,r)||
    !(typedrecordoperand(in->sm,&in->s,r[0],p,&src,&sm)||
      (op==IADDC&&typedrecordlayoutoperand(in->sm,&in->s,1,p,&src,&sm))))return 0;
 if(op==IADDC&&(!a||a->kind!=Tstring))return 0;
 if(op==IINDC||op==ILENC||op==ICVTCW){if(!a||a->kind!=Tstring)return 0;}
 if(op==ILENA&&(!a||a->kind!=Tarray))return 0;
 if(op==ILENL&&(!a||a->kind!=Tlist))return 0;
 if(op==ICONSW&&(!a||a->kind!=Tint||!(sm&AIND)||in->mm!=Anone||
    (in->dm!=Afp&&in->dm!=Amp)||!typedintlisttype(&in->d)))return 0;
 if(branch){
  if(in->dm!=Apc||!in->branch||in->d.offset!=in->branch->pc||in->d.offset<(int)p->first||in->d.offset>=(int)stop)return 0;
  dst=in->d.offset-p->first;dm=AIMM;
 }else if(in->dm==Afpind&&in->d.reg==16&&!in->d.offset){
  if(op==IMOVP&&(p->resultaggregate==48||(p->resultpair&&p->resultpair<3))&&
     (sm&AIND)&&in->mm==Anone&&!in->branch&&p->resulttype->ids&&
     a&&tequal(a,p->resulttype->ids->ty)&&
     typedrecordregister(p,pc,0,typedfieldowner(&in->s)->tof)){
   p->code[pc]=(Dis64Instruction){IMOVP,SRC(sm)|DST(AFP|AIND),0,src,32};
   p->returns[pc]=Native64TypedRecordPair;
   return 1;
  }
  if(p->resultaggregate||p->resultpair||p->resulttriple||(r[1]?!a||!tequal(a,p->resulttype):p->resultkind!=Tint))return 0;
  dst=32;dm=AFP|AIND;
 }else if(in->branch||!(typedrecordoperand(in->dm,&in->d,r[1],p,&dst,&dm)||
                       (op==IADDC&&typedrecordlayoutoperand(in->dm,&in->d,1,p,&dst,&dm))))return 0;
 if(op==IMOVP){b=typedrecordvalue(&in->d,in->dm);if(dst==32&&dm==(AFP|AIND))b=p->resulttype;if(!a||!b||(!tequal(a,b)&&a->kind!=Tany))return 0;}
 if(op==IADDC){int result=dst==32&&dm==(AFP|AIND);b=result?p->resulttype:typedrecordvalue(&in->d,in->dm);if(!b||b->kind!=Tstring||(!(sm&AIND)&&(!(dm&AIND)||result)))return 0;}
 if(in->mm!=Anone){int actual;
  if(op==IMOVP||op==IMOVW||op==ILENC||op==ILENA||op==ILENL||op==ICVTCW||op==ICVTWC)return 0;
  if(!typedoperand(in->mm,&in->m,r[0]&&op!=IINDC?TypedRoot:Tint,!r[0]||op==IINDC,1,p,&mid,&wire,&actual))return 0;
  if(r[0]&&op!=IINDC){b=typedrecordvalue(&in->m,in->mm);if(!a||!b||(b->kind!=Tany&&!tequal(a,b))||((op==IBEQC||op==IBNEC)&&a->kind!=Tstring))return 0;}
  mm=wire==AIMM?AXIMM:wire==AFP?AXINF:AXINM;
 }else if(branch||op==IINDC)return 0;
 if((sm&AIND)&&!typedrecordregister(p,pc,0,typedfieldowner(&in->s)->tof))return 0;
 if(!branch&&(dm&AIND)&&!(dm==(AFP|AIND)&&dst==32)&&!typedrecordregister(p,pc,1,typedfieldowner(&in->d)->tof))return 0;
 p->code[pc]=(Dis64Instruction){op,SRC(sm)|mm|DST(dm),mid,src,dst};p->returns[pc]=Native64TypedRecord;return 1;
}
