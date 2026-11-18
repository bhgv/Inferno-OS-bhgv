/* Trusted compiler IR only. Included after the shared typed operand/CFG checks. */
typedef struct TypedMulti TypedMulti;
struct TypedMulti {
    Decl *globals,**functions;
    Inst **instructions;
    int *entries,nf,external,loads;
    Dis64TypedBody *bodies;
    uchar *state;
    unsigned int *height,count,cost;
};

static int
typedbytearrayaddr(Addr *a)
{
 Type*t;
 return a->decl && (t=a->decl->ty) && t->kind==Tarray && t->tof && t->tof->kind==Tbyte && a->reg==a->decl->offset && !a->offset;
}

#include "dis64-external.h"
#include "dis64-variadic.h"

/* List primitives belong only to the selected importing-client lowering. */
static int
typedclientlist(Inst *in,Dis64TypedBody *p,Dis64Instruction *out)
{
 unsigned int sm,dm;long source,destination;int actual;
 int sk=in->op==ICONSP?Tstring:Tlist,dk=in->op==IHEADP?Tstring:Tlist;
 if(in->mm!=Anone || in->branch!=nil ||
    !typedoperand(in->sm,&in->s,sk,0,0,p,&source,&sm,&actual))return typedfail(p,"client-list-roles");
 if(in->dm==Afpind){
  if(in->op==ICONSP || in->d.reg!=16 || in->d.offset || p->resultkind!=dk)return typedfail(p,"client-list-result");
  destination=32;dm=AFP|AIND;
 }else if(!typedoperand(in->dm,&in->d,dk,0,0,p,&destination,&dm,&actual))return typedfail(p,"client-list-roles");
 *out=(Dis64Instruction){in->op,SRC(sm)|DST(dm),0,source,destination};return 1;
}

/* Native checked-LOAD diagnostics admit only list of string here. A map bit
 * alone cannot distinguish List/String or prove the element class. */
static int
typedloadlisttype(Addr *a)
{
 Type *t;
 if(!a->decl)return 0;t=a->decl->ty;
 if(a->native64type && a->decl->store==Dlocal && a->decl->sym && a->decl->sym->name[0]=='.' && a->decl->sym->name[1]=='t')t=a->native64type;
 return t && t->kind==Tlist && t->tof && t->tof->kind==Tstring && a->reg==a->decl->offset && !a->offset;
}
static int
typedloadlist(Inst *in,Decl *fn,Dis64TypedBody *p,Dis64Instruction *out)
{
 Type *result=fn->ty->tof;
 if((in->op!=ICONSP && !typedloadlisttype(&in->s)) ||
    (in->op!=IHEADP && (in->dm==Afpind?
      (result->kind!=Tlist || !result->tof || result->tof->kind!=Tstring):!typedloadlisttype(&in->d))))
  return typedfail(p,"load-list-element-class");
 return typedclientlist(in,p,out);
}

/* One bounded nil -> String cons chain in a local root, then its argument
 * store. Keep the pending frame rooted through all intervening allocations. */
static int
typedexternalstringlist(TypedMulti*c,Dis64TypedBody*p,Inst*frame,unsigned int end,unsigned int*at)
{
 Inst*in=c->instructions[*at];Addr temp;unsigned int start=*at,n=0;
 if(in->sm!=Amp || in->mm!=Anone || in->branch || in->dm!=Afp || !in->s.decl || in->s.decl->store!=Dglobal ||
    in->s.reg!=in->s.decl->offset || !in->s.decl->ty || in->s.decl->ty->kind!=Tany ||
    !in->s.decl->sym || strcmp(in->s.decl->sym->name,"nil") || in->s.offset || in->d.offset || !typedloadlisttype(&in->d) ||
    !typedinst(in,c->count,p,p->code+*at-p->first,p->returns+*at-p->first))return typedfail(p,"external-list-init");
 temp=in->d;p->returns[(*at)++-p->first]=112;
 while(*at<end && (in=c->instructions[*at]) && in->op==ICONSP){
  if(n++==256 || in->dm!=Afp || in->d.reg!=temp.reg || in->d.decl!=temp.decl || in->d.offset ||
     !typedloadlisttype(&in->d) || !typedclientlist(in,p,p->code+*at-p->first))return typedfail(p,"external-list-cons");
  p->returns[(*at)++-p->first]=112;
 }
 if(!n || *at>=end || !(in=c->instructions[*at]) || in->op!=IMOVP || in->sm!=Afp || in->s.reg!=temp.reg || in->s.decl!=temp.decl ||
    in->s.offset || in->dm!=Afpind || in->d.reg!=frame->d.reg)return typedfail(p,"external-list-store");
 USED(start);return 1;
}

static int
typedrootargclear(TypedMulti*c,Dis64TypedBody*p,Inst*store,unsigned int end,unsigned int*at)
{
 Inst*in;
 if(*at>=end || store->sm!=Afp || !(in=c->instructions[*at]) || in->op!=IMOVP || in->dm!=Afp || in->d.reg!=store->s.reg)return 1;
 if(!store->s.decl || store->s.decl->store!=Dlocal || !store->s.decl->sym || store->s.decl->sym->name[0]!='.' || store->s.decl->sym->name[1]!='t' ||
    in->sm!=Amp || in->mm!=Anone || in->branch || in->d.decl!=store->s.decl || in->d.offset || !in->s.decl || in->s.decl->store!=Dglobal ||
    !in->s.decl->ty || in->s.decl->ty->kind!=Tany || !in->s.decl->sym || strcmp(in->s.decl->sym->name,"nil") || in->s.reg!=in->s.decl->offset || in->s.offset ||
    !typedinst(in,c->count,p,p->code+*at-p->first,p->returns+*at-p->first))return typedfail(p,"external-root-clear");
 p->returns[(*at)++-p->first]=126;return 1;
}

/* Fixed Draw.Context geometry from trusted .m declarations, not a guessed C size. */
static int
typedclientcontext(Inst *in,Dis64TypedBody *p,Dis64Instruction *out)
{
 Dis64Layout layout={0};Decl *d=in->s.decl;unsigned int i,dm;long destination;int actual,ok=0;
 if(in->sm!=Adesc || in->mm!=Anone || in->branch || !d || !d->ty || d->ty->kind!=Tadt || !d->desc || d->desc->id!=in->s.offset || d->desc->size!=12 || d->desc->nmap!=1 || !d->desc->map || d->desc->map[0]!=0xe0 ||
    !d->sym || strcmp(d->sym->name,"Context") || !d->dot || !d->dot->sym || strcmp(d->dot->sym->name,"Draw") ||
    !dis64layouttype(d->ty,&layout) || layout.legacybytes!=12 || layout.bytes!=24 || layout.alignment!=8 || layout.count!=3)goto done;
 for(i=0;i<3;i++)if(layout.leaves[i].legacy!=4*i || layout.leaves[i].native!=8*i || layout.leaves[i].oldbytes!=4 || layout.leaves[i].bytes!=8 || layout.leaves[i].kind!=(i==2?Tchan:Tref))goto done;
 if(p->sourceroles && in->dm==Afpind){if(in->d.reg!=16 || in->d.offset || p->resultkind!=Tref)goto done;destination=32;dm=AFP|AIND;}
 else if(!typedoperand(in->dm,&in->d,TypedRoot,0,0,p,&destination,&dm,&actual) || actual!=Tref || dm!=AFP)goto done;
 *out=(Dis64Instruction){in->op,SRC(AIMM)|DST(dm),0,1,destination};ok=1;
 done:dis64layoutfree(&layout);return ok?1:typedfail(p,"client-context-geometry");
}

/* Primitive channel payloads retain their existing builtin Type/mover identity. */
static int
typedclientchannel(Inst*in,Decl*fn,Dis64TypedBody*p,Dis64Instruction*out,uchar*returned)
{
 Type*t;unsigned int dm,mm=AXNON,wire;long destination,capacity=0;int actual,kind;
 if(in->sm!=Anone || in->branch)return typedfail(p,"channel-operands");
 if(in->dm==Afpind){if(in->d.reg!=16 || in->d.offset)return typedfail(p,"channel-result");t=fn->ty->tof;destination=32;dm=AFP|AIND;*returned=1;}
 else{
  t=in->d.native64type?in->d.native64type:in->d.decl?in->d.decl->ty:nil;
  if(!typedoperand(in->dm,&in->d,TypedRoot,0,0,p,&destination,&dm,&actual) || actual!=Tchan)return typedfail(p,"channel-destination");
 }
 if(!t || t->kind!=Tchan || !t->tof)return typedfail(p,"channel-payload");kind=t->tof->kind;
 if(!((in->op==INEWCB && kind==Tbyte) || (in->op==INEWCW && (kind==Tint || kind==Tfix)) || (in->op==INEWCL && kind==Tbig) || (in->op==INEWCF && kind==Treal) ||
      (in->op==INEWCP && (kind==Tstring || kind==Tref || kind==Tchan || kind==Tarray || kind==Tlist || kind==Tmodule))))return typedfail(p,"channel-payload");
 if(in->mm!=Anone){if(!typedoperand(in->mm,&in->m,Tint,1,1,p,&capacity,&wire,&actual))return typedfail(p,"channel-capacity");mm=wire==AIMM?AXIMM:wire==AFP?AXINF:AXINM;}
 *out=(Dis64Instruction){in->op,SRC(AXXX)|mm|DST(dm),capacity,0,destination};return 1;
}

/* A scratch Decl retains its storage type after reuse. Indirect access needs
 * the original base owner, not the field value's type or the scratch's first type. */
static Type*
typedfieldowner(Addr*a)
{
 if(a->native64base)return a->native64base;
 if(!a->decl || (a->decl->store==Dlocal && a->decl->sym && a->decl->sym->name[0]=='.' && a->decl->sym->name[1]=='t'))return nil;
 return a->decl->ty;
}

/* One exact Context field and one direct mapped root. No nested dereference. */
static int
typedclientfield(Inst *in,Dis64TypedBody *p,unsigned int pc,int probe)
{
 Addr *a,owner;Type *t;Dis64Layout layout={0};unsigned int i,bm,vm;long base,value=0;int actual,read,kind,ok=0;vm=AFP;
 read=in->sm==Afpind || in->sm==Ampind;a=read?&in->s:&in->d;
 if(in->op!=IMOVP || in->mm!=Anone || in->branch || (read?(in->dm!=Afp && in->dm!=Amp):(in->dm!=Afpind && in->dm!=Ampind)) ||
    !a->decl || !(t=typedfieldowner(a)) || t->kind!=Tref || !(t=t->tof) || t->kind!=Tadt ||
    !t->decl || !t->decl->sym || strcmp(t->decl->sym->name,"Context") || !t->decl->dot ||
    !t->decl->dot->sym || strcmp(t->decl->dot->sym->name,"Draw") ||
    a->offset<0 || a->offset>8 || a->offset%4 || !dis64layouttype(t,&layout) || layout.bytes!=24 || layout.count!=3)goto done;
 for(i=0;i<3;i++)if(layout.leaves[i].legacy!=4*i || layout.leaves[i].native!=8*i || layout.leaves[i].oldbytes!=4 || layout.leaves[i].bytes!=8 || layout.leaves[i].kind!=(i==2?Tchan:Tref))goto done;
 owner=*a;owner.offset=0;owner.native64type=typedfieldowner(a);
 if(!typedoperand((read?in->sm:in->dm)==Afpind?Afp:Amp,&owner,TypedRoot,0,0,p,&base,&bm,&actual) || actual!=Tref)goto done;
 kind=a->offset==8?Tchan:Tref;
 if(!probe && (!typedoperand(read?in->dm:in->sm,read?&in->d:&in->s,TypedRoot,0,0,p,&value,&vm,&actual) ||
    (actual!=kind && actual!=Tany) || (vm!=AFP && vm!=AMP)))goto done;
 if(!p->contextfields){p->contextfields=allocmem(p->count*sizeof(long));memset(p->contextfields,0,p->count*sizeof(long));}
 p->contextfields[pc]=2*a->offset;
 p->code[pc]=(Dis64Instruction){IMOVP,read?SRC(bm|AIND)|DST(vm):SRC(vm)|DST(bm|AIND),0,read?base:value,read?value:base};ok=1;
 done:dis64layoutfree(&layout);return ok?1:typedfail(p,"context-field-roles");
}

/* One Context field copied to the same field in another Context. */
static int
typedcontextcopy(Inst*in,Dis64TypedBody*p,unsigned int pc)
{
 Inst probe=*in;Dis64Instruction read;long displacement;
 if(in->op!=IMOVP || in->mm!=Anone || in->branch || (in->sm!=Afpind && in->sm!=Ampind) || (in->dm!=Afpind && in->dm!=Ampind) || in->s.offset!=in->d.offset)return typedfail(p,"context-copy-roles");
 probe.dm=Afp;probe.d=in->s;if(!typedclientfield(&probe,p,pc,1))return 0;read=p->code[pc];displacement=p->contextfields[pc];
 probe=*in;probe.sm=Afp;probe.s=in->d;if(!typedclientfield(&probe,p,pc,1))return 0;
 p->code[pc]=(Dis64Instruction){IMOVP,SRC(USRC(read.mode))|DST(UDST(p->code[pc].mode)),displacement,read.source,p->code[pc].destination};p->returns[pc]=173;return 1;
}

#include "dis64-window.h"
#include "dis64-draw-fields.h"

/* Immutable byte constants are immediate operands, never mutable MP storage. */
static int
typedclientbyteoperand(int mode,Addr *a,int kind,Dis64TypedBody *p,long *value,unsigned int *wire)
{
 int actual;Decl *d=a->decl;
 if(kind==Tbyte && mode==Amp && d && d->ty && d->ty->kind==Tbyte && d->init && d->init->op==Oconst &&
    a->reg==d->offset && a->offset==0 && d->init->val>=0 && d->init->val<=255){*value=d->init->val;*wire=AIMM;return 1;}
 return typedoperand(mode,a,kind,kind==Tint || kind==Tbyte,0,p,value,wire,&actual);
}
static int
typedclientbyte(Inst *in,Dis64TypedBody *p,unsigned int pc)
{
 unsigned int sm,dm;long source,destination;int sk=in->op==ICVTWB?Tint:Tbyte,dk=in->op==ICVTBW?Tint:Tbyte,actual;
 if(in->mm!=Anone || in->branch || !typedclientbyteoperand(in->sm,&in->s,sk,p,&source,&sm))return typedfail(p,"client-byte-roles");
 if(in->dm==Afpind){
  if(p->resultkind!=dk || in->d.reg!=16 || in->d.offset)return typedfail(p,"client-byte-result");
  destination=32;dm=AFP|AIND;
 }else if(!typedoperand(in->dm,&in->d,dk,0,0,p,&destination,&dm,&actual) || dm!=AFP)return typedfail(p,"client-byte-roles");
 p->code[pc]=(Dis64Instruction){in->op,SRC(sm)|DST(dm),0,source,destination};if(dm==(AFP|AIND))p->returns[pc]=1;return 1;
}
/* Reuse the existing reserved first tail cell; the scalar result uses the second. */
static int
typedclientindex(Inst *in,Inst *use,Dis64TypedBody *p,unsigned int pc)
{
 unsigned int sm,dm,um;long source,index,value;int actual,read;Addr *a;
 if((in->op!=IINDB && in->op!=IINDW) || in->branch || in->mm!=Afp || !in->m.decl || in->m.offset ||
    !typedoperand(in->sm,&in->s,Tarray,0,0,p,&source,&sm,&actual) || sm!=AFP ||
    !typedoperand(in->dm,&in->d,Tint,1,0,p,&index,&dm,&actual) || (dm!=AFP && dm!=AIMM))return typedfail(p,"client-index-roles");
 if(!use || use->branch || use->mm!=Anone)return typedfail(p,"client-index-consumer");
 read=use->sm==Afpind;a=read?&use->s:&use->d;
 if(a->decl!=in->m.decl || a->reg!=in->m.reg || a->offset ||
    (in->op==IINDW?use->op!=IMOVW:(read?(use->op!=IMOVB && use->op!=ICVTBW):(use->op!=IMOVB && use->op!=ICVTWB))) || (!read && use->dm!=Afpind))return typedfail(p,"client-index-address");
 if(read){if(!typedoperand(use->dm,&use->d,use->op==IMOVB?Tbyte:Tint,0,0,p,&value,&um,&actual) || um!=AFP)return 0;}
 else if(!typedclientbyteoperand(use->sm,&use->s,use->op==IMOVB?Tbyte:Tint,p,&value,&um) || (um!=AFP && um!=AIMM))return 0;
 p->code[pc]=(Dis64Instruction){in->op,SRC(AFP)|AXINF|DST(dm),p->frame.bytes,source,index};
 p->code[pc+1]=(Dis64Instruction){use->op,read?SRC(AFP|AIND)|DST(AFP):SRC(um)|DST(AFP|AIND),0,read?p->frame.bytes:value,read?value:p->frame.bytes};
 if(!p->contextfields){p->contextfields=allocmem(p->count*sizeof(long));memset(p->contextfields,0,p->count*sizeof(long));}
 return 1;
}

static int
typedloadbytes(Inst *in,Dis64TypedBody *p,unsigned int pc)
{
 Addr *a=in->op==ICVTAC?&in->s:&in->d;
 if(!typedbytearrayaddr(a))return typedfail(p,"load-byte-array-class");
 if(in->op==INEWA || in->op==INEWAZ){
  Decl*d=in->m.decl;
  if(!d || !d->ty || d->ty->kind!=Tbyte)return typedfail(p,"load-byte-array-class");
 }
 return 1;
}

static int
typedclientarray(Inst *in,Dis64TypedBody *p,unsigned int pc)
{
 unsigned int sm,mm,dm;long source,middle,destination;int actual;Decl *d=in->m.decl;
 if(in->branch || !typedoperand(in->sm,&in->s,Tint,1,0,p,&source,&sm,&actual) ||
    !typedoperand(in->dm,&in->d,Tarray,0,0,p,&destination,&dm,&actual) || dm!=AFP)return typedfail(p,"byte-array-roles");
 if(in->op==INEWA || in->op==INEWAZ){
  if(in->mm!=Adesc || !d || !d->ty || (d->ty->kind!=Tbyte && d->ty->kind!=Tint) || !d->desc || d->desc->id!=in->m.offset)return typedfail(p,"byte-array-descriptor");
  middle=d->ty->kind==Tint?1:0;mm=AIMM;
 }else if(!typedoperand(in->mm,&in->m,Tint,1,1,p,&middle,&mm,&actual) || (mm!=AIMM && mm!=AFP))return typedfail(p,"byte-array-slice");
 p->code[pc]=(Dis64Instruction){in->op==INEWA?INEWAZ:in->op,SRC(sm)|(mm==AIMM?AXIMM:AXINF)|DST(dm),middle,source,destination};return 1;
}

static int
typedclientcodec(Inst *in,Dis64TypedBody *p,unsigned int pc)
{
 unsigned int sk,dk,sm,dm;long source,destination;int actual;
 switch(in->op){
 case ICVTCW:sk=Tstring;dk=Tint;break;case ICVTCL:sk=Tstring;dk=Tbig;break;case ICVTCF:sk=Tstring;dk=Treal;break;
 case ICVTWC:sk=Tint;dk=Tstring;break;case ICVTLC:sk=Tbig;dk=Tstring;break;case ICVTFC:sk=Treal;dk=Tstring;break;
 case ICVTCA:sk=Tstring;dk=Tarray;break;case ICVTAC:sk=Tarray;dk=Tstring;break;
 default:return 0;
 }
 if(in->mm!=Anone || in->branch || !typedoperand(in->sm,&in->s,sk,sk==Tint,0,p,&source,&sm,&actual) ||
    !typedoperand(in->dm,&in->d,dk,0,0,p,&destination,&dm,&actual) || dm!=AFP)return typedfail(p,"client-codec-roles");
 p->code[pc]=(Dis64Instruction){in->op,SRC(sm)|DST(dm),0,source,destination};return 1;
}

static int
typedclientstring(Inst *in,Dis64TypedBody *p,unsigned int pc,unsigned int stop)
{
 unsigned int sm,mm,dm;long source,middle,destination;int actual,index=in->op==IINDC;
 if(in->op==IADDC || in->op==IINSC || in->op==ISLICEC){
  int numeric=in->op!=IADDC;
  if(in->branch || !typedoperand(in->sm,&in->s,numeric?Tint:Tstring,numeric,0,p,&source,&sm,&actual) ||
     !typedoperand(in->mm==Anone?in->dm:in->mm,in->mm==Anone?&in->d:&in->m,numeric?Tint:Tstring,numeric,1,p,&middle,&mm,&actual) ||
     !typedoperand(in->dm,&in->d,Tstring,0,0,p,&destination,&dm,&actual) || (dm!=AFP && !(p->sourceroles && in->op==ISLICEC && dm==AMP)) ||
     (numeric?(mm!=AIMM && mm!=AFP):(mm!=AFP && mm!=AMP)))return typedfail(p,"string-write-roles");
  p->code[pc]=(Dis64Instruction){in->op,SRC(sm)|(mm==AIMM?AXIMM:mm==AFP?AXINF:AXINM)|DST(dm),middle,source,destination};return 1;
 }

 if(!typedoperand(in->sm,&in->s,Tstring,0,0,p,&source,&sm,&actual) ||
    !typedoperand(in->mm,&in->m,index?Tint:Tstring,index,1,p,&middle,&mm,&actual))return 0;
 if(index){if(in->branch || !typedoperand(in->dm,&in->d,Tint,0,0,p,&destination,&dm,&actual) || dm!=AFP || (mm!=AIMM && mm!=AFP))return typedfail(p,"string-index-roles");}
 else{
  if(in->dm!=Apc || !in->branch || in->branch->pc<(int)p->first || in->branch->pc>=(int)stop || in->d.offset!=in->branch->pc || (mm!=AFP && mm!=AMP))return typedfail(p,"string-branch-roles");
  destination=in->branch->pc-p->first;dm=AIMM;
 }
 p->code[pc]=(Dis64Instruction){in->op,SRC(sm)|(mm==AIMM?AXIMM:mm==AFP?AXINF:AXINM)|DST(dm),middle,source,destination};return 1;
}

/* One numeric String temporary, bounded in-place concatenation, argument, clear. */
static int
typedexternalstringexpr(TypedMulti*c,Dis64TypedBody*p,Inst*frame,Decl*arg,unsigned int end,unsigned int*at,unsigned int offset)
{
 Inst*in=c->instructions[*at],*first=in;unsigned int n=0,sm,mm;long source,middle;int actual;
 if(in->op!=ICVTWC || in->dm!=Afp || !in->d.decl || in->d.decl->store!=Dlocal || !in->d.decl->sym || in->d.decl->sym->name[0]!='.' || in->d.decl->sym->name[1]!='t' || in->d.offset || !typedclientcodec(in,p,*at-p->first))return typedfail(p,"external-string-expression");
 p->returns[(*at)++-p->first]=180;
 while(*at<end && (in=c->instructions[*at]) && in->op==IADDC && in->dm==Afp){
  if(n++>=8 || in->d.decl!=first->d.decl || in->d.reg!=first->d.reg || in->d.offset ||
     !((in->sm==Afp && in->s.decl==first->d.decl && in->s.reg==first->d.reg && !in->s.offset) || in->mm==Anone || (in->mm==Afp && in->m.decl==first->d.decl && in->m.reg==first->d.reg && !in->m.offset)) || !typedclientstring(in,p,*at-p->first,end))return typedfail(p,"external-string-expression");
  p->returns[(*at)++-p->first]=180;
 }
 if(*at>=end || !(in=c->instructions[*at]) || in->op!=IADDC || in->dm!=Afpind || in->d.reg!=frame->d.reg || in->d.offset!=arg->offset || in->branch ||
    !((in->sm==Afp && in->s.decl==first->d.decl && in->s.reg==first->d.reg && !in->s.offset) || (in->mm==Afp && in->m.decl==first->d.decl && in->m.reg==first->d.reg && !in->m.offset)) ||
    !typedoperand(in->sm,&in->s,Tstring,0,0,p,&source,&sm,&actual) || actual!=Tstring ||
    !typedoperand(in->mm,&in->m,Tstring,0,1,p,&middle,&mm,&actual) || actual!=Tstring || (mm!=AFP && mm!=AMP))return typedfail(p,"external-string-argument");
 p->code[*at-p->first]=(Dis64Instruction){IADDC,SRC(sm)|(mm==AFP?AXINF:AXINM)|DST(AFP|AIND),middle,source,offset};p->returns[(*at)++-p->first]=64;
 if(*at>=end || !(in=c->instructions[*at]) || in->op!=IMOVP || in->sm!=Amp || in->mm!=Anone || in->dm!=Afp || in->branch ||
    !in->s.decl || in->s.decl->store!=Dglobal || !in->s.decl->ty || in->s.decl->ty->kind!=Tany || !in->s.decl->sym || strcmp(in->s.decl->sym->name,"nil") || in->s.reg!=in->s.decl->offset || in->s.offset ||
    in->d.decl!=first->d.decl || in->d.reg!=first->d.reg || in->d.offset || !typedinst(in,end,p,p->code+*at-p->first,p->returns+*at-p->first))return typedfail(p,"external-string-clear");
 p->returns[(*at)++-p->first]=181;return 1;
}

/* Pointer comparisons use the complete native reference, including fused
 * Context field reads. The nil constant remains a readonly mapped MP root. */
static int
typedclientreferencekind(int kind)
{
 return kind==Tref || kind==Tchan || kind==Tarray || kind==Tlist || kind==Tmodule || kind==Tany;
}
static int
typedclientcompare(Inst *in,Dis64TypedBody *p,unsigned int pc,unsigned int stop)
{
 Addr *left=&in->s,*right=&in->m;int lm=in->sm,rm=in->mm,sk,mk;unsigned int sm,mm;long source,middle;
 if(in->dm!=Apc || !in->branch || in->branch->pc<(int)p->first || in->branch->pc>=(int)stop || in->d.offset!=in->branch->pc)return typedfail(p,"reference-branch");
 if(lm==Amp && rm==Afpind){Addr *a=left;int m=lm;left=right;right=a;lm=rm;rm=m;}
 if(lm==Afpind){
  Inst field=*in;Dis64Instruction *out=p->code+pc;long base;
  /* Reuse the same exact Context owner/field geometry, with a temporary
   * direct destination solely for deriving its checked base/displacement. */
  field.op=IMOVP;field.s=*left;field.sm=Afpind;field.mm=Anone;field.dm=Afp;field.d=*left;field.branch=nil;
  if(!typedclientfield(&field,p,pc,1) || !typedoperand(rm,right,TypedRoot,0,1,p,&middle,&mm,&mk) || mk!=Tany || mm!=AMP)return typedfail(p,"field-nil-roles");
  base=out->source;*out=(Dis64Instruction){in->op==IBEQW?IBEQL:in->op==IBNEW?IBNEL:in->op,SRC(AFP|AIND)|AXINM|DST(AIMM),middle,base,in->branch->pc-p->first};return 1;
 }
 if(!typedoperand(lm,left,TypedRoot,0,0,p,&source,&sm,&sk) || !typedoperand(rm,right,TypedRoot,0,1,p,&middle,&mm,&mk))return 0;
 if(sk==Tany && mk!=Tany){long v=source;unsigned int mode=sm;int kind=sk;source=middle;sm=mm;sk=mk;middle=v;mm=mode;mk=kind;}
 if(!typedclientreferencekind(sk) || !typedclientreferencekind(mk) || sm!=AFP ||
    (mk==Tany?mm!=AMP:mm!=AFP || sk!=mk || sk==Tmodule))return typedfail(p,"reference-compare-roles");
 p->code[pc]=(Dis64Instruction){in->op==IBEQW?IBEQL:in->op==IBNEW?IBNEL:in->op,SRC(sm)|(mm==AMP?AXINM:AXINF)|DST(AIMM),middle,source,in->branch->pc-p->first};return 1;
}

/* Exact Context field/nil tests in the checked LOAD path. */
static int
typedloadcontextcompare(Inst*in,Dis64TypedBody*p,unsigned int pc,unsigned int stop)
{
 Addr *left=&in->s,*right=&in->m;int lm=in->sm,rm=in->mm,actual;unsigned int mm,bm,index=0,window,draw;long middle,base;Inst field=*in;
 if(in->dm!=Apc || !in->branch || in->branch->pc<(int)p->first || in->branch->pc>=(int)stop || in->d.offset!=in->branch->pc)return typedfail(p,"context-nil-branch");
 if(lm==Amp && (rm==Afpind || rm==Ampind)){left=&in->m;right=&in->s;lm=in->mm;rm=in->sm;}
 if((lm!=Afpind && lm!=Ampind) || rm!=Amp)return typedfail(p,"context-nil-roles");
 field.op=IMOVP;field.s=*left;field.sm=lm;field.mm=Anone;field.dm=Afp;field.branch=nil;
 window=typedwindowowner(left);draw=typeddrawowner(left);
 if(!(window?typedwindowfield(&field,p,pc,1,&index):draw?typeddrawfield(&field,p,pc,1,&index):typedclientfield(&field,p,pc,1)) || !typedoperand(rm,right,TypedRoot,0,1,p,&middle,&mm,&actual) || actual!=Tany || mm!=AMP)return typedfail(p,"context-nil-roles");
 base=p->code[pc].source;bm=lm==Afpind?AFP:AMP;
 p->code[pc]=(Dis64Instruction){in->op==IBEQW?IBEQL:IBNEL,SRC(bm|AIND)|AXINM|DST(AIMM),middle,base,in->branch->pc-p->first};
 if(draw && index==2)return typedfail(p,"draw-string-nil-roles");
 p->returns[pc]=window?101+index:draw?114+index:97+left->offset/4;return 1;
}

/* Comparing a direct ref ADT with nil only observes the root pointer.
 * It neither admits the ADT's fields nor assumes a C payload geometry. */
static int
typedloadreftype(Addr *a)
{
 Type *t;
 return a->decl && (t=a->decl->ty) && t->kind==Tref && t->tof && t->tof->kind==Tadt &&
  a->reg==a->decl->offset && a->offset==0;
}

/* Checked native startup needs direct ref ADT, list-of-String and Module nil tests.
 * Nil is the compiler's readonly Tany MP root; no aggregate dereference or
 * comparison of unrelated reference classes is admitted by this route. */
static int
typedloadcompare(Inst *in,Dis64TypedBody *p,unsigned int pc,unsigned int stop)
{
 Addr *value=&in->s,*empty=&in->m;int vm=in->sm,nm=in->mm,kind,actual;
 unsigned int sm,mm;long source,middle;
 if(value->decl && value->decl->ty && value->decl->ty->kind==Tany){
  value=&in->m;empty=&in->s;vm=in->mm;nm=in->sm;
 }
 if((in->op!=IBEQW && in->op!=IBNEW && in->op!=IBEQL && in->op!=IBNEL) ||
    !value->decl || !value->decl->ty ||
    (!(value->decl->ty->kind==Tmodule && value->reg==value->decl->offset) && !typedloadlisttype(value) && !typedloadreftype(value)) ||
    !empty->decl || !empty->decl->ty || empty->decl->ty->kind!=Tany || nm!=Amp ||
    !typedoperand(vm,value,TypedRoot,0,0,p,&source,&sm,&kind) || kind!=value->decl->ty->kind || (typedloadreftype(value) && sm!=AFP) ||
    !typedoperand(nm,empty,TypedRoot,0,1,p,&middle,&mm,&actual) || actual!=Tany ||
    in->dm!=Apc || !in->branch || in->branch->pc<(int)p->first || in->branch->pc>=(int)stop ||
    in->d.offset!=in->branch->pc)return typedfail(p,"load-nil-roles");
 p->code[pc]=(Dis64Instruction){in->op==IBEQW?IBEQL:in->op==IBNEW?IBNEL:in->op,
  SRC(sm)|AXINM|DST(AIMM),middle,source,in->branch->pc-p->first};return 1;
}

static int typedmultibody(TypedMulti*,unsigned int,unsigned int);

/* A destructured tuple may bind to two separate local declarations. Both
 * compiler declarations and both relocated root cells must agree. */
static int
typedpairslot(TypedMulti *c,Dis64TypedBody *p,Addr *a)
{
 unsigned int i;Decl *fn,*d;int pass;
 if(!a->decl || !a->decl->ty || a->reg!=a->decl->offset)return 0;
 if(typedpairtype(a->decl->ty))return 1;
 if(a->decl->ty->kind!=Tmodule)return 0;
 for(i=0;i<(unsigned int)c->nf;i++)if(c->functions[i]->pc->pc==(int)p->first){
  fn=c->functions[i];
  for(pass=0;pass<2;pass++)for(d=pass?fn->locals:fn->ty->ids;d;d=d->next)
   if(d->offset==a->reg+4 && d->ty && d->ty->kind==Tlist && d->ty->tof && d->ty->tof->kind==Tstring)return 1;
 }
 return 0;
}

static int
typedchannelslot(TypedMulti*c,Dis64TypedBody*p,Addr*a)
{
 unsigned int i,j,found=0;Decl*fn,*d;int pass;
 if(!a->decl || !a->decl->ty || a->reg!=a->decl->offset || a->offset)return 0;
 if(typedchanneltriple(a->decl->ty))return 1;
 if(a->decl->ty->kind!=Tchan)return 0;
 for(i=0;i<(unsigned int)c->nf;i++)if(c->functions[i]->pc->pc==(int)p->first){
  fn=c->functions[i];for(j=0;j<3;j++)for(pass=0;pass<2;pass++)for(d=pass?fn->locals:fn->ty->ids;d;d=d->next)
   if(d->offset==a->reg+4*j && d->ty && d->ty->kind==Tchan)found++;
  return found==3;
 }return 0;
}
static int
typedchannelbinding(TypedMulti*c,Dis64TypedBody*p,Inst*binding,unsigned int*base)
{
 unsigned int i,at=0;
 if(binding->sm!=Afp || !typedchannelslot(c,p,&binding->s))return typedfail(p,"channel-result-slot");
 for(i=0;i<3;i++)if(!dis64layoutaddress(&p->frame,binding->s.reg+4*i,4,1,&at) || (i && at!=*base+8*i))return typedfail(p,"channel-result-layout");else if(!i)*base=at;
 return 1;
}

static int
typedmultisite(TypedMulti *c,Dis64TypedBody *p,unsigned int pc,unsigned int stop,unsigned int depth,unsigned int *next,unsigned int *height)
{
    Inst *frame=c->instructions[pc],*call,*in,*binding;
    Dis64TypedBody *child;Dis64TypedSite *site;
    Decl *arg;unsigned int end=pc+1,at,sm,offset,width,contract=0,local=0,inherit=0;
    int target,kind,root,op,actual;long source;
    if(frame->sm!=Adesc || frame->mm!=Anone || frame->dm!=Afp || frame->branch!=nil || frame->d.reg<32 ||
       frame->d.reg>(int)p->frame.legacybytes-4)return typedfail(p,"multi-frame");
    while(end<stop && c->instructions[end]!=nil && (c->instructions[end]->dm==Afpind ||
      (c->loads && ((c->instructions[end]->op==IMOVP && c->instructions[end]->dm==Afp) || c->instructions[end]->op==ISLICEA))))end++;
    if(end>=stop || (call=c->instructions[end])==nil || call->op!=ICALL || call->sm!=Afp || call->s.reg!=frame->d.reg ||
       call->mm!=Anone || call->dm!=Apc || call->d.offset<0 || (unsigned long)call->d.offset>=c->count ||
       (target=c->entries[call->d.offset])<0)return typedfail(p,"multi-call");
    if(c->functions[target]->desc==nil || c->functions[target]->desc->id!=frame->s.offset ||
       (call->branch!=nil && call->branch!=c->functions[target]->pc))return typedfail(p,"multi-target");
    child=c->bodies+target;
    if(c->loads && child==p && c->state[target]==1){
        binding=c->instructions[end-1];
        if(p->resultkind==Tnone || !p->code || !p->returns || p->frame.reason || p->data.reason || p->frame.bytes<64 ||
           end+1>=stop || !c->instructions[end+1] || c->instructions[end+1]->op!=IRET || binding->op!=ILEA || binding->sm!=Afpind ||
           binding->s.reg!=16 || binding->s.offset)return typedfail(p,"multi-self-tail");
    }else if(!typedmultibody(c,target,depth+1))return typedfail(p,"multi-callee");
    if(child!=p && *height<c->height[target]+1)*height=c->height[target]+1;
    if(*height>64)return typedfail(p,"multi-depth");
    p->code[pc-p->first]=(Dis64Instruction){IFRAME,SRC(AIMM)|DST(AFP),0,frame->s.offset,40};at=pc+1;
    for(arg=c->functions[target]->ty->ids;arg!=nil;arg=arg->next){
        if(at>=end || (in=c->instructions[at])==nil)return typedfail(p,"multi-argument");
        kind=arg->ty->kind;root=kind==Tstring || kind==Tref || kind==Tarray || kind==Tlist || kind==Tchan || kind==Tmodule;
        width=kind==Tbyte?1:kind==Tbig || kind==Treal?8:4;
        op=root?IMOVP:kind==Tbyte?IMOVB:kind==Tbig?IMOVL:kind==Treal?IMOVF:IMOVW;
        /* No later allocating argument may follow the pending String root. */
        if(c->loads && kind==Tstring && in->op==IMOVP && in->dm==Afp){
            Inst*slice,*convert,*clear;unsigned int view;
            if(arg!=c->functions[target]->ty->ids || arg->next || at+3>=end || !(slice=c->instructions[at+1]) || !(convert=c->instructions[at+2]) || !(clear=c->instructions[at+3]) ||
               in->mm!=Anone || in->branch || in->sm!=Afp || !typedbytearrayaddr(&in->s) || !typedbytearrayaddr(&in->d) ||
               slice->op!=ISLICEA || slice->dm!=Afp || slice->d.reg!=in->d.reg || slice->d.decl!=in->d.decl ||
               convert->op!=ICVTAC || convert->sm!=Afp || convert->s.reg!=in->d.reg || convert->s.decl!=in->d.decl ||
               convert->mm!=Anone || convert->branch || convert->dm!=Afpind || convert->d.reg!=frame->d.reg || convert->d.offset!=arg->offset ||
               clear->op!=IMOVP || clear->sm!=Amp || clear->dm!=Afp || clear->mm!=Anone || clear->branch ||
               clear->d.reg!=in->d.reg || clear->d.decl!=in->d.decl || !clear->s.decl || !clear->s.decl->ty || clear->s.decl->ty->kind!=Tany ||
               !typedinst(in,stop,p,p->code+at-p->first,p->returns+at-p->first) || !typedloadbytes(slice,p,at+1-p->first) || !typedclientarray(slice,p,at+1-p->first) ||
               !typedbytearrayaddr(&convert->s) || !typedoperand(convert->sm,&convert->s,Tarray,0,0,p,&source,&sm,&actual) ||
               !dis64layoutaddress(&child->frame,arg->offset,4,1,&view) || view>65535 ||
               !typedinst(clear,stop,p,p->code+at+3-p->first,p->returns+at+3-p->first))return typedfail(p,"local-slice-string-argument");
            p->code[at+2-p->first]=(Dis64Instruction){ICVTAC,SRC(sm)|DST(AFP|AIND),0,source,view};p->returns[at+2-p->first]=64;at+=4;continue;
        }
        if(c->loads && kind==Tstring && in->op==ICVTAC){
            if(arg!=c->functions[target]->ty->ids || arg->next || in->mm!=Anone || in->branch || in->dm!=Afpind || in->d.reg!=frame->d.reg || in->d.offset!=arg->offset ||
               !typedbytearrayaddr(&in->s) || !dis64layoutaddress(&child->frame,arg->offset,4,1,&offset) || offset>65535 ||
               !typedoperand(in->sm,&in->s,Tarray,0,0,p,&source,&sm,&actual))return typedfail(p,"local-byte-string-argument");
            p->code[at-p->first]=(Dis64Instruction){ICVTAC,SRC(sm)|DST(AFP|AIND),0,source,offset};p->returns[at++-p->first]=64;continue;
        }
        if(c->loads && child==p && kind==Tstring && in->op==IADDC){
            unsigned int mm;long middle;
            if(in->dm!=Afpind || in->d.reg!=frame->d.reg || in->d.offset!=arg->offset || in->branch ||
               !dis64layoutaddress(&child->frame,arg->offset,4,1,&offset) || offset>65535 ||
               !typedoperand(in->sm,&in->s,Tstring,0,0,p,&source,&sm,&actual) || actual!=Tstring ||
               !typedoperand(in->mm,&in->m,Tstring,0,1,p,&middle,&mm,&actual) || actual!=Tstring)
                return typedfail(p,"self-concat-argument");
            p->code[at-p->first]=(Dis64Instruction){IADDC,SRC(sm)|(mm==AFP?AXINF:AXINM)|DST(AFP|AIND),middle,source,offset};
            p->returns[at++-p->first]=64;continue;
        }
        if(c->loads && root && (in->sm==Afpind || in->sm==Ampind)){
            if(in->dm!=Afpind || in->d.reg!=frame->d.reg || in->d.offset!=arg->offset ||
               !dis64layoutaddress(&child->frame,arg->offset,4,1,&offset) || offset>65535 || !typedselectedfieldarg(in,p,at-p->first,offset,kind))return typedfail(p,"multi-field-argument");
            at++;continue;
        }
        if((!root && kind!=Tbyte && kind!=Tint && kind!=Tfix && kind!=Tbig && kind!=Treal) ||
           in->op!=op || in->mm!=Anone || in->dm!=Afpind || in->d.reg!=frame->d.reg || in->d.offset!=arg->offset || in->branch!=nil ||
           !dis64layoutaddress(&child->frame,arg->offset,width,root,&offset) || offset>65535 ||
           !(kind==Tbyte && c->external>=2?typedclientbyteoperand(in->sm,&in->s,kind,p,&source,&sm):
             typedoperand(in->sm,&in->s,root?TypedRoot:kind,!root && width<8,0,p,&source,&sm,&actual)) ||
           (root && actual!=kind && actual!=Tany))return typedfail(p,"multi-argument");
        p->code[at-p->first]=(Dis64Instruction){op,SRC(sm)|DST(AFP|AIND),0,source,offset};p->returns[at++-p->first]=64;
        if(c->loads && root){unsigned int before=at;if(!typedrootargclear(c,p,in,end,&at))return 0;if(at!=before)p->returns[at-1-p->first]=182;}
    }
    if(child->resultkind!=Tnone){
        if(at+1!=end || (binding=c->instructions[at])==nil || binding->op!=ILEA || binding->mm!=Anone ||
           binding->dm!=Afpind || binding->d.reg!=frame->d.reg || binding->d.offset!=16 || binding->branch!=nil)return typedfail(p,"multi-result");
        kind=child->resultkind;root=kind==Tstring || kind==Tref || kind==Tarray || kind==Tlist || kind==Tchan || kind==Tmodule;
        contract=child->resulttriple?72:child->resultpair?48:child->resultbytes | (root?16:0);
        if(binding->sm==Afp){
            if(child->resulttriple){if(!typedchannelbinding(c,p,binding,&local))return 0;source=local;sm=AFP;}
            else if(child->resultpair){
                unsigned int second;Addr *a=&binding->s;
                if(!typedpairslot(c,p,a) ||
                   !dis64layoutaddress(&p->frame,a->reg,4,1,&offset) ||
                   !dis64layoutaddress(&p->frame,a->reg+4,4,1,&second) || second!=offset+8)return typedfail(p,"pair-call-result");
                source=offset;sm=AFP;
            }else if(!typedoperand(Afp,&binding->s,root?TypedRoot:kind,0,0,p,&source,&sm,&actual) ||
               (root && actual!=kind && actual!=Tany))return typedfail(p,"multi-result");
            local=source;p->code[at-p->first]=(Dis64Instruction){ILEA,SRC(AFP)|DST(AFP|AIND),0,source,32};p->returns[at-p->first]=128|contract;
        }else if(c->loads && root && !child->resultpair && binding->sm==Amp){
            if(!binding->s.decl || binding->s.decl->store!=Dglobal || !typedoperand(Amp,&binding->s,TypedRoot,0,0,p,&source,&sm,&actual) || actual!=kind)return typedfail(p,"multi-global-result");
            local=source;p->code[at-p->first]=(Dis64Instruction){ILEA,SRC(AMP)|DST(AFP|AIND),0,source,32};p->returns[at-p->first]=128|contract;
        }else{
            if(binding->sm!=Afpind || binding->s.reg!=16 || binding->s.offset || kind!=p->resultkind || child->resultpair!=p->resultpair || child->resulttriple!=p->resulttriple)return typedfail(p,"multi-inherit");
            inherit=1;local=32;p->code[at-p->first]=(Dis64Instruction){IMOVL,SRC(AFP)|DST(AFP|AIND),0,32,32};p->returns[at-p->first]=64;
        }
        at++;
    }
    if(at!=end)return typedfail(p,"multi-setup-length");
    p->code[end-p->first]=(Dis64Instruction){ICALL,SRC(AFP)|DST(AIMM),0,40,call->d.offset};p->returns[end-p->first]=inherit?32:0;
    site=p->sites+p->nsites++;*site=(Dis64TypedSite){pc-p->first,end-p->first,call->d.offset,frame->s.offset,local,contract,inherit};
    *next=end+1;return 1;
}

static int
typedmultibody(TypedMulti *c,unsigned int index,unsigned int depth)
{
    Dis64TypedBody *p=c->bodies+index;Decl *fn=c->functions[index];
    unsigned int i,pc,stop=c->count,next,h=1,cost;uchar *inside=nil;Inst *in;
    if(depth>=64)return typedfail(p,"multi-depth");
    if(c->state[index]==1)return typedfail(p,"multi-cycle");
    if(c->state[index])return c->state[index]==2;
    c->state[index]=1;
    for(i=0;i<(unsigned int)c->nf;i++)if(c->functions[i]->pc->pc>fn->pc->pc && c->functions[i]->pc->pc<(int)stop)stop=c->functions[i]->pc->pc;
    i=typedlower(fn,c->globals,c->instructions[0],c->count,stop,p,c->loads);
    cost=p->frame.bytes+p->data.bytes+(p->frame.count+p->data.count)*sizeof(Dis64LayoutLeaf);
    if(cost>32*1024*1024-c->cost){typedfail(p,"multi-storage-budget");goto fail;}c->cost+=cost;
    /* A successful scalar lowering may retain an immutable byte in MP.
     * Command files serialize strings only, so replay through the byte folder. */
    if(i && c->external>=2)for(pc=0;pc<p->count;pc++)
        if((p->code[pc].op==IMOVB || p->code[pc].op==ICVTBW) && USRC(p->code[pc].mode)==AMP){i=0;typedfail(p,"unsupported-opcode");break;}
    if(i){c->state[index]=2;c->height[index]=1;return 1;}
    if(p->reason==nil || (strcmp(p->reason,"unsupported-opcode") && !((c->external>=2 || c->loads) && p->code && !p->frame.reason && !p->data.reason)))goto fail;
    p->reason=nil;p->ntables=0;memset(p->code,0,p->count*sizeof(*p->code));memset(p->returns,0,p->count);
    if(c->loads){
        unsigned int before=p->frame.bytes;
        if(!typedstatframe(c,fn,p,stop))goto fail;
        if(p->frame.bytes-before>32*1024*1024-c->cost){typedfail(p,"multi-storage-budget");goto fail;}
        c->cost+=p->frame.bytes-before;
    }
    p->sites=allocmem((p->count/2+1)*sizeof(*p->sites));
    if(c->external)p->external=allocmem((p->count/2+1)*sizeof(*p->external));
    if(c->loads){p->variadic=allocmem((p->count/2+1)*sizeof(*p->variadic));memset(p->variadic,0,(p->count/2+1)*sizeof(*p->variadic));}
    inside=allocmem(p->count);memset(inside,0,p->count);
    for(pc=p->first;pc<stop;){
        in=c->instructions[pc];p->badpc=pc;
        if(in==nil){typedfail(p,"multi-instruction-order");goto fail;}
        if(in->op==IFRAME || (c->external && in->op==IMFRAME)){
            if(in->op==IMFRAME){if(!typedexternalsite(c,p,pc,stop,&next))goto fail;}
            else if(c->loads && in->native64args){if(!typedvariadicsite(c,p,pc,stop,&next))goto fail;}
            else if(!typedmultisite(c,p,pc,stop,depth,&next,&h))goto fail;
            for(i=pc+1;i<next;i++)inside[i-p->first]=1;
            pc=next;
        }else if(c->loads && p->resulttriple && in->op==IMOVMP){
            Desc*d;unsigned int base=0;
            for(d=descriptors;d && d->id!=in->m.offset;d=d->next);
            if(in->sm!=Afp || in->mm!=Adesc || in->dm!=Afpind || in->d.reg!=16 || in->d.offset || in->branch || !in->m.decl || in->m.decl->desc!=d || !typedchanneltriple(in->m.decl->ty) || !d || d->size!=12 || d->nmap!=1 || d->map[0]!=0xe0 || !typedchannelbinding(c,p,in,&base)){typedfail(p,"channel-result-copy");goto fail;}
            p->code[pc-p->first]=(Dis64Instruction){IMOVMP,SRC(AFP)|AXIMM|DST(AFP|AIND),0,base,32};p->returns[pc++-p->first]=171;
        }else if(c->external>=2 && in->op==IMOVP && in->dm==Afpind && in->d.reg==16 && in->d.offset==0){
            if(!typedinst(in,stop,p,p->code+pc-p->first,p->returns+pc-p->first))goto fail;pc++;
        }else if((c->external>=2 || c->loads) && (in->op==INEWA || in->op==INEWAZ || in->op==ISLICEA)){
            if((c->loads && !typedloadbytes(in,p,pc-p->first)) || !typedclientarray(in,p,pc-p->first))goto fail;pc++;
        }else if(c->external>=2 && (in->op==IINDB || in->op==IINDW)){
            if(pc+1>=stop || !typedclientindex(in,c->instructions[pc+1],p,pc-p->first))goto fail;
            inside[pc+1-p->first]=1;pc+=2;
        }else if(c->external>=2 && (in->op==IMOVB || in->op==ICVTBW || in->op==ICVTWB)){
            if(!typedclientbyte(in,p,pc-p->first))goto fail;pc++;
        }else if(c->loads && in->op==ICVTAC && in->dm==Afpind){
            long source;unsigned int sm;int actual;
            if(!typedloadbytes(in,p,pc-p->first))goto fail;
            if(p->resultkind!=Tstring || in->mm!=Anone || in->branch || in->d.reg!=16 || in->d.offset){typedfail(p,"load-byte-result");goto fail;}
            if(!typedoperand(in->sm,&in->s,Tarray,0,0,p,&source,&sm,&actual))goto fail;
            p->code[pc-p->first]=(Dis64Instruction){ICVTAC,SRC(sm)|DST(AFP|AIND),0,source,32};p->returns[pc-p->first]=1;pc++;
        }else if(c->loads && in->op==IADDC && in->dm==Afpind){
            long source,middle;unsigned int sm,mm;int actual;
            if(p->resultkind!=Tstring || in->d.reg!=16 || in->d.offset || in->branch ||
               !typedoperand(in->sm,&in->s,Tstring,0,0,p,&source,&sm,&actual) ||
               !typedoperand(in->mm,&in->m,Tstring,0,1,p,&middle,&mm,&actual))goto fail;
            p->code[pc-p->first]=(Dis64Instruction){IADDC,SRC(sm)|(mm==AFP?AXINF:AXINM)|DST(AFP|AIND),middle,source,32};p->returns[pc-p->first]=1;pc++;
        }else if((c->external>=2 && (in->op==ICVTCW || in->op==ICVTCL || in->op==ICVTCF || in->op==ICVTWC || in->op==ICVTLC || in->op==ICVTFC || in->op==ICVTCA || in->op==ICVTAC)) || (c->loads && (in->op==ICVTAC || in->op==ICVTWC))){
            if((c->loads && in->op!=ICVTWC && !typedloadbytes(in,p,pc-p->first)) || !typedclientcodec(in,p,pc-p->first))goto fail;pc++;
        }else if((c->external>=2 && (in->op==IADDC || in->op==IINSC || in->op==ISLICEC || in->op==IINDC || in->op==IBEQC || in->op==IBNEC || in->op==IBLTC || in->op==IBLEC || in->op==IBGTC || in->op==IBGEC)) ||
                 (c->loads && (in->op==IADDC || in->op==ISLICEC || in->op==IINDC || in->op==IBEQC || in->op==IBNEC || in->op==IBLTC || in->op==IBLEC || in->op==IBGTC || in->op==IBGEC))){
            if(!typedclientstring(in,p,pc-p->first,stop))goto fail;pc++;
        }else if(c->loads && in->op==IMOVP && (in->sm==Afpind || in->sm==Ampind) && (in->dm==Afpind || in->dm==Ampind)){
            if(!typedcontextcopy(in,p,pc-p->first))goto fail;pc++;
        }else if((c->external>=2 || c->loads) && in->op==IMOVP && (in->sm==Afpind || in->sm==Ampind || (in->dm==Afpind && in->d.reg!=16) || in->dm==Ampind)){
            Addr*field=(in->sm==Afpind || in->sm==Ampind)?&in->s:&in->d;
            int window=c->loads && typedwindowowner(field),draw=c->loads && typeddrawowner(field);
            if(!(window?typedwindowfield(in,p,pc-p->first,0,nil):draw?typeddrawfield(in,p,pc-p->first,0,nil):typedclientfield(in,p,pc-p->first,0)))goto fail;
            if(c->loads){p->returns[pc-p->first]=window?100:draw?113:96;p->code[pc-p->first].middle=p->contextfields[pc-p->first];}pc++;
        }else if(c->loads && (in->op==IBEQW || in->op==IBNEW) &&
          (in->sm==Afpind || in->sm==Ampind || in->mm==Afpind || in->mm==Ampind)){
            if(!typedloadcontextcompare(in,p,pc-p->first,stop))goto fail;pc++;
        }else if((c->external>=2 || c->loads) && (in->op==IBEQW || in->op==IBNEW || in->op==IBEQL || in->op==IBNEL) &&
          ((in->s.decl && in->s.decl->ty && typedclientreferencekind(in->s.decl->ty->kind)) ||
           (in->m.decl && in->m.decl->ty && typedclientreferencekind(in->m.decl->ty->kind)))){
            if(!(c->loads?typedloadcompare(in,p,pc-p->first,stop):typedclientcompare(in,p,pc-p->first,stop)))goto fail;pc++;
        }else if(c->loads && (in->op==INEWCB || in->op==INEWCW || in->op==INEWCL || in->op==INEWCF || in->op==INEWCP)){
            if(!typedclientchannel(in,fn,p,p->code+pc-p->first,p->returns+pc-p->first))goto fail;pc++;
        }else if((c->external>=2 || c->loads) && (in->op==INEW || in->op==INEWZ)){
            if(!typedclientcontext(in,p,p->code+pc-p->first))goto fail;
            if(c->loads){p->code[pc-p->first].source=0;p->returns[pc-p->first]=UDST(p->code[pc-p->first].mode)==(AFP|AIND)?174:172;}pc++;
        }else if((c->external>=2 || c->loads) && (in->op==ICONSP || in->op==IHEADP || in->op==ITAIL)){
            if(!(c->loads?typedloadlist(in,fn,p,p->code+pc-p->first):typedclientlist(in,p,p->code+pc-p->first)))goto fail;
            if(UDST(p->code[pc-p->first].mode)==(AFP|AIND))p->returns[pc-p->first]=1;pc++;
        }else if(c->loads && in->op==IRAISE){
            if(!typedraise(in,p,p->code+pc-p->first))goto fail;pc++;
        }else if((c->external==2 || c->loads) && in->op==ILOAD){
            long source,destination;unsigned int sm,dm;int actual;
            if(in->mm!=Aldt || in->m.decl==nil || in->m.decl->offset<0 || in->m.decl->offset>63 ||
               (!c->loads && in->m.decl->offset!=0) || in->branch!=nil ||
               !typedoperand(in->sm,&in->s,Tstring,0,0,p,&source,&sm,&actual) || actual!=Tstring ||
               !typedoperand(in->dm,&in->d,TypedRoot,0,0,p,&destination,&dm,&actual) || actual!=Tmodule){typedfail(p,"load-roles");goto fail;}
            p->code[pc-p->first]=(Dis64Instruction){ILOAD,SRC(sm)|AXIMM|DST(dm),in->m.decl->offset,source,destination};pc++;
        }else{
            if(!typedinst(in,stop,p,p->code+pc-p->first,p->returns+pc-p->first))goto fail;
            pc++;
        }
    }
    for(pc=p->first;pc<stop;pc++){
        in=c->instructions[pc];
        if(in->op!=ICALL && in->op!=IMCALL && in->branch!=nil && inside[in->branch->pc-p->first]){typedfail(p,"multi-setup-branch");goto fail;}
    }
    for(i=0;i<p->ntables;i++){
        Dis64TypedTable *t=p->tables+i;unsigned int j,target;
        for(j=0;j<(unsigned int)t->table->nlab+(t->op!=IGOTO);j++){
            target=(j<(unsigned int)t->table->nlab?t->table->labs[j].inst:t->table->iwild)->pc-p->first;
            if(inside[target]){typedfail(p,"multi-table-setup-branch");goto fail;}
        }
    }
    if((c->external<2 && !c->loads && !p->nsites && !p->nexternal) || !typedresults(p))goto fail;
    free(inside);p->tail=c->loads?5:c->external?4:3;c->state[index]=2;c->height[index]=h;return 1;
fail:
    free(inside);c->state[index]=3;return 0;
}

static int
typedmultilower(Decl *fn,Decl *globals,Decl **functions,int nf,Inst *input,unsigned int count,unsigned int stop,Dis64TypedBody *p,int external)
{
    TypedMulti c={0};Inst *in;unsigned int i,root=0;int found=0,ok=0;
    USED(stop);
    if(p->code!=nil || p->reason!=nil || p->sites!=nil || p->external!=nil || p->variadic!=nil)return typedfail(p,"nonempty-output");
    if(nf<1 || nf>4096 || !count || count>TypedInstructions || functions==nil || input==nil)return typedfail(p,"multi-limit");
    c.loads=external==4;c.external=c.loads?1:external;c.globals=globals;c.functions=functions;c.nf=nf;c.count=count;
    c.bodies=allocmem(nf*sizeof(*c.bodies));memset(c.bodies,0,nf*sizeof(*c.bodies));
    c.state=allocmem(nf);memset(c.state,0,nf);c.height=allocmem(nf*sizeof(*c.height));memset(c.height,0,nf*sizeof(*c.height));
    c.instructions=allocmem(count*sizeof(*c.instructions));memset(c.instructions,0,count*sizeof(*c.instructions));
    c.entries=allocmem(count*sizeof(*c.entries));for(i=0;i<count;i++)c.entries[i]=-1;
    for(i=0;i<(unsigned int)nf;i++){
        if(functions[i]==nil || functions[i]->pc==nil || functions[i]->pc->pc<0 || (unsigned int)functions[i]->pc->pc>=count || c.entries[functions[i]->pc->pc]>=0){typedfail(p,"multi-function-table");goto done;}
        c.entries[functions[i]->pc->pc]=i;if(functions[i]==fn){root=i;found=1;}
    }
    if(!found){typedfail(p,"multi-function-table");goto done;}
    for(in=input;in!=nil;in=in->next){
        if(in->op==INOOP)continue;
        if(in->pc<0 || (unsigned int)in->pc>=count || c.instructions[in->pc]!=nil){typedfail(p,"multi-instruction-table");goto done;}
        c.instructions[in->pc]=in;
    }
    if(c.instructions[0]==nil){typedfail(p,"multi-instruction-table");goto done;}
    ok=typedmultibody(&c,root,0);*p=c.bodies[root];memset(c.bodies+root,0,sizeof(*p));
    if(!ok && p->reason==nil)typedfail(p,"multi-body");
done:
    for(i=0;i<(unsigned int)nf;i++)dis64typedfree(c.bodies+i);
    free(c.bodies);free(c.state);free(c.height);free(c.instructions);free(c.entries);return ok;
}

int
dis64typedmulti(Decl *fn,Decl *globals,Decl **functions,int nf,Inst *input,unsigned int count,unsigned int stop,Dis64TypedBody *p)
{
    return typedmultilower(fn,globals,functions,nf,input,count,stop,p,0);
}
int
dis64typedexternal(Decl *fn,Decl *globals,Decl **functions,int nf,Inst *input,unsigned int count,unsigned int stop,Dis64TypedBody *p)
{
    return typedmultilower(fn,globals,functions,nf,input,count,stop,p,1);
}

int
dis64typedclient(Decl *fn,Decl *globals,Decl **functions,int nf,Inst *input,unsigned int count,unsigned int stop,Dis64TypedBody *p)
{
 return typedmultilower(fn,globals,functions,nf,input,count,stop,p,2);
}

int dis64typedcommand(Decl *fn,Decl *globals,Decl **functions,int nf,Inst *input,unsigned int count,unsigned int stop,Dis64TypedBody *p){return typedmultilower(fn,globals,functions,nf,input,count,stop,p,3);}

int
dis64typedloads(Decl *fn,Decl *globals,Decl **functions,int nf,Inst *input,unsigned int count,unsigned int stop,Dis64TypedBody *p)
{
    return typedmultilower(fn,globals,functions,nf,input,count,stop,p,4);
}
