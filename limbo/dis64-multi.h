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
static int typedwindowlisttype(Addr*);

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

static int
typedintlisttype(Addr*a)
{
 Type*t=a->decl?a->decl->ty:nil;
 if(a->native64type && a->decl && a->decl->store==Dlocal && a->decl->sym && a->decl->sym->name[0]=='.' && a->decl->sym->name[1]=='t')t=a->native64type;
 return a->decl && t && t->kind==Tlist && t->tof && t->tof->kind==Tint && a->reg==a->decl->offset && !a->offset;
}
static int
typedclientintlist(Inst*in,Decl*fn,Dis64TypedBody*p,Dis64Instruction*out,uchar*returned)
{
 unsigned int sm,dm;long source,destination;int actual,head=in->op==IHEADW,cons=in->op==ICONSW,kind=cons?Tint:Tlist;
 if(in->mm!=Anone || in->branch || (!cons&&!typedintlisttype(&in->s)) ||
    !typedoperand(in->sm,&in->s,kind,cons,0,p,&source,&sm,&actual))return typedfail(p,"integer-list-source");
 if(in->dm==Afpind){
  if(cons || in->d.reg!=16 || in->d.offset || (head?fn->ty->tof->kind!=Tint:(fn->ty->tof->kind!=Tlist || !fn->ty->tof->tof || fn->ty->tof->tof->kind!=Tint)))return typedfail(p,"integer-list-result");
  destination=32;dm=AFP|AIND;*returned=1;
 }else if((!head&&!typedintlisttype(&in->d)) || !typedoperand(in->dm,&in->d,head?Tint:Tlist,0,0,p,&destination,&dm,&actual))return typedfail(p,"integer-list-destination");
 *out=(Dis64Instruction){in->op,SRC(sm)|DST(dm),0,source,destination};return 1;
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

/* Prepend a bounded String chain to a borrowed, typed List tail. */
static int
typedexternalstringprefix(TypedMulti*c,Dis64TypedBody*p,Inst*frame,unsigned int end,unsigned int*at)
{
 Inst*in=c->instructions[*at];Addr temp;unsigned int n=0;
 if(in->op!=IMOVP || in->mm!=Anone || in->branch || in->dm!=Afp || !typedloadlisttype(&in->s) || !typedloadlisttype(&in->d) ||
    !in->d.decl || in->d.decl->store!=Dlocal || !in->d.decl->sym || in->d.decl->sym->name[0]!='.' || in->d.decl->sym->name[1]!='t' ||
    !typedinst(in,c->count,p,p->code+*at-p->first,p->returns+*at-p->first))return typedfail(p,"external-list-prefix");
 temp=in->d;p->returns[(*at)++-p->first]=187;
 while(*at<end && (in=c->instructions[*at]) && in->op==ICONSP){
  if(n++==256 || in->dm!=Afp || in->d.reg!=temp.reg || in->d.decl!=temp.decl || in->d.offset ||
     !typedloadlisttype(&in->d) || !typedclientlist(in,p,p->code+*at-p->first))return typedfail(p,"external-list-prefix-cons");
  p->returns[(*at)++-p->first]=187;
 }
 if(!n || *at>=end || !(in=c->instructions[*at]) || in->op!=IMOVP || in->sm!=Afp || in->s.reg!=temp.reg || in->s.decl!=temp.decl ||
    in->s.offset || in->dm!=Afpind || in->d.reg!=frame->d.reg)return typedfail(p,"external-list-prefix-store");return 1;
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

static int typedclientchanneladdress(Addr*,int,Dis64TypedBody*,long*,unsigned int*,long*,int*);

/* Sys.Rwrite: exact (int, string), including its alignment gap. */
static int
typedreplylayout(Type*t,Dis64Layout*l,int nominal)
{
 Decl*a;
 if(!t || t->kind!=Ttuple || !(a=t->ids) || a->store!=Dfield || a->offset || !a->ty || a->ty->kind!=Tint ||
    !(a=a->next) || a->next || a->store!=Dfield || a->offset!=4 || !a->ty ||
    (nominal==1?a->ty->kind!=Tstring:nominal==2?(a->ty->kind!=Tstring && a->ty->kind!=Tany):!(a->ty->kind==Tstring || a->ty->kind==Tref || a->ty->kind==Tchan || a->ty->kind==Tarray || a->ty->kind==Tlist || a->ty->kind==Tmodule)) || !dis64layouttype(t,l))return 0;
 return l->legacybytes==8 && l->bytes==16 && l->alignment==8 && l->count==2 &&
  l->leaves[0].legacy==0 && l->leaves[0].native==0 && l->leaves[0].oldbytes==4 && l->leaves[0].bytes==4 && l->leaves[0].kind==Tint &&
  l->leaves[1].legacy==4 && l->leaves[1].native==8 && l->leaves[1].oldbytes==4 && l->leaves[1].bytes==8;
}
static int
typedreplytype(Type*t)
{Dis64Layout l={0};int ok=typedreplylayout(t,&l,1);dis64layoutfree(&l);return ok;}
static int
typedreplyliteral(Type*t)
{Dis64Layout l={0};int ok=typedreplylayout(t,&l,2);dis64layoutfree(&l);return ok;}
static int
typedreplyspan(Dis64Layout*l,unsigned int old,unsigned int*base)
{unsigned int root;return dis64layoutaddress(l,old,4,0,base) && !(*base%8) && dis64layoutaddress(l,old+4,4,1,&root) && root==*base+8;}
static int
typedreplytransfer(Inst*in,Dis64TypedBody*p,Dis64Instruction*out,uchar*returned)
{
 int send=in->op==ISEND,cm=send?in->dm:in->sm,vm=send?in->sm:in->dm,actual;Addr*ca=send?&in->d:&in->s,*va=send?&in->s:&in->d;Type*t=ca->native64type?ca->native64type:ca->decl?ca->decl->ty:nil;long channel;unsigned int wire,value;
 if((cm!=Afp && cm!=Amp) || (vm!=Afp && vm!=Amp) || in->mm!=Anone || in->branch || !t || t->kind!=Tchan || !typedreplytype(t->tof) || !typedoperand(cm,ca,TypedRoot,0,0,p,&channel,&wire,&actual) || actual!=Tchan || !typedreplyspan(vm==Afp?&p->frame:&p->data,va->reg,&value))return 0;
 *out=(Dis64Instruction){in->op,send?SRC(vm==Afp?AFP:AMP)|DST(wire):SRC(wire)|DST(vm==Afp?AFP:AMP),136,send?value:channel,send?channel:value};*returned=136;return 1;
}

/* Primitive channel transfer; aggregate payloads and borrowed outputs stay closed. */
static int
typedclienttransfer(Inst*in,Dis64TypedBody*p,Dis64Instruction*out,uchar*returned)
{
 Addr*ca=in->op==ISEND?&in->d:&in->s;Addr*va=in->op==ISEND?&in->s:&in->d;int cm=in->op==ISEND?in->dm:in->sm,vm=in->op==ISEND?in->sm:in->dm,actual,kind,root;
 Type*t=ca->native64type?ca->native64type:ca->decl?ca->decl->ty:nil;unsigned int cw,vw,width;long channel,value,field=0;int indirect=cm==Afpind || cm==Ampind;
 if(in->mm!=Anone || in->branch)return typedfail(p,"channel-transfer-owner");
 if(indirect){if(!typedclientchanneladdress(ca,cm,p,&channel,&cw,&field,&kind))return typedfail(p,"client-channel-field");}
 else{if(!t || t->kind!=Tchan || !t->tof || !typedoperand(cm,ca,TypedRoot,0,0,p,&channel,&cw,&actual) || actual!=Tchan)return typedfail(p,"channel-transfer-owner");kind=t->tof->kind;}
 root=kind==Tstring || kind==Tref || kind==Tchan || kind==Tarray || kind==Tlist || kind==Tmodule;
 if(kind==Ttuple && !indirect && typedreplytransfer(in,p,out,returned))return 1;
 if(!root && kind!=Tbyte && kind!=Tint && kind!=Tfix && kind!=Tbig && kind!=Treal)return typedfail(p,"channel-transfer-payload");
 width=root || kind==Tbig || kind==Treal?8:kind==Tbyte?1:4;
 if(!typedoperand(vm,va,root?TypedRoot:kind==Tfix?Tint:kind,in->op==ISEND&&!root&&width<8,0,p,&value,&vw,&actual) || (root && actual!=kind && actual!=Tany))return typedfail(p,"channel-transfer-value");
 *out=(Dis64Instruction){in->op,in->op==ISEND?SRC(vw)|DST(cw):SRC(cw)|DST(vw),(field<<8)|width|(root?16:0),in->op==ISEND?value:channel,in->op==ISEND?channel:value};*returned=indirect?204:188;return 1;
}

/* Shared native channel result/capacity geometry, after nominal payload proof. */
static int
typedchanneloutput(Inst*in,Decl*fn,Dis64TypedBody*p,Dis64Instruction*out,uchar*returned)
{
 unsigned int dm,mm=AXNON,wire;long destination,capacity=0;int actual;
 if(in->branch)return typedfail(p,"channel-operands");
 if(in->dm==Afpind){if(in->d.reg!=16 || in->d.offset || fn->ty->tof->kind!=Tchan)return typedfail(p,"channel-result");destination=32;dm=AFP|AIND;*returned=1;}
 else if(!typedoperand(in->dm,&in->d,TypedRoot,0,0,p,&destination,&dm,&actual) || actual!=Tchan)return typedfail(p,"channel-destination");
 if(in->mm!=Anone){if(!typedoperand(in->mm,&in->m,Tint,1,1,p,&capacity,&wire,&actual))return typedfail(p,"channel-capacity");mm=wire==AIMM?AXIMM:wire==AFP?AXINF:AXINM;}
 *out=(Dis64Instruction){in->op,SRC(in->op==INEWCMP?AIMM:AXXX)|mm|DST(dm),capacity,0,destination};return 1;
}

/* Primitive channel payloads retain their existing builtin Type/mover identity. */
static int
typedclientchannel(Inst*in,Decl*fn,Dis64TypedBody*p,Dis64Instruction*out,uchar*returned)
{
 Type*t=in->dm==Afpind?fn->ty->tof:in->d.native64type?in->d.native64type:in->d.decl?in->d.decl->ty:nil;int kind;
 if(in->sm!=Anone)return typedfail(p,"channel-operands");
 if(!t || t->kind!=Tchan || !t->tof)return typedfail(p,"channel-payload");kind=t->tof->kind;
 if(!((in->op==INEWCB && kind==Tbyte) || (in->op==INEWCW && (kind==Tint || kind==Tfix)) || (in->op==INEWCL && kind==Tbig) || (in->op==INEWCF && kind==Treal) ||
      (in->op==INEWCP && (kind==Tstring || kind==Tref || kind==Tchan || kind==Tarray || kind==Tlist || kind==Tmodule))))return typedfail(p,"channel-payload");
 return typedchanneloutput(in,fn,p,out,returned);
}

/* Both source payload and canonical descriptor must prove this bounded geometry. */
static int
typedmappedpayload(Type*t,Dis64Layout*layout)
{
 Decl*field;unsigned int i,n;
 if(!t || t->kind!=Ttuple || !dis64layouttype(t,layout))return 0;n=layout->count;
 if((n!=2 && n!=3) || layout->legacybytes!=4*n || layout->bytes!=8*n || layout->alignment!=8)return 0;
 for(i=0,field=t->ids;field;field=field->next,i++)if(i>=n || !field->ty || field->ty->kind!=layout->leaves[i].kind)return 0;
 if(i!=n)return 0;
 for(i=0;i<n;i++)if(layout->leaves[i].legacy!=4*i || layout->leaves[i].native!=8*i || layout->leaves[i].oldbytes!=4 || layout->leaves[i].bytes!=8 ||
   (layout->leaves[i].kind!=Tstring && layout->leaves[i].kind!=Tref && layout->leaves[i].kind!=Tchan && layout->leaves[i].kind!=Tarray && layout->leaves[i].kind!=Tlist && layout->leaves[i].kind!=Tmodule))return 0;
 return 1;
}

/* Descriptor deduplication may merge root classes with identical GC geometry. */
static int
typedclientmappedchannel(Inst*in,Decl*fn,Dis64TypedBody*p,Dis64Instruction*out,uchar*returned)
{
 Dis64Layout payload={0},descriptor={0};Decl*d=in->s.decl;Type*t;unsigned int n;int ok=0,mixed;
 if(in->sm!=Adesc || !d || !d->desc || d->desc->id!=in->s.offset || !d->desc->map || d->desc->nmap!=1)goto done;
 t=in->dm==Afpind?fn->ty->tof:in->d.native64type?in->d.native64type:in->d.decl?in->d.decl->ty:nil;
 if(!t || t->kind!=Tchan)goto done;mixed=typedreplytype(t->tof);
 if(mixed){if(!typedreplylayout(t->tof,&payload,1) || !typedreplylayout(d->ty,&descriptor,0))goto done;}
 else if(!typedmappedpayload(t->tof,&payload) || !typedmappedpayload(d->ty,&descriptor))goto done;
 n=payload.count;if(descriptor.count!=n || d->desc->size!=4*n || d->desc->map[0]!=(mixed?0x40:n==2?0xc0:0xe0))goto done;
 if(!typedchanneloutput(in,fn,p,out,returned))goto done;
 out->source=mixed?1:n;*returned=*returned?185:183;ok=1;
 done:dis64layoutfree(&payload);dis64layoutfree(&descriptor);return ok?1:typedfail(p,"mapped-channel-layout");
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
#include "dis64-wmcontext.h"
#include "dis64-client-channels.h"
#include "dis64-window-list.h"
static int typedwordargop(int);
#include "dis64-draw-fields.h"
#include "dis64-rect-list.h"

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

/* Only canonical Draw.Image references; array element slots are mapped roots. */
static int
typedimagearrayaddr(Addr*a)
{
 Type*t=a->native64type?a->native64type:a->decl?a->decl->ty:nil;
 return a->decl && t && t->kind==Tarray && typedwmref(t->tof,"Draw","Image") && a->reg==a->decl->offset && !a->offset;
}
static int
typedimagearraynew(Inst*in,Dis64TypedBody*p,unsigned int pc)
{
 Decl*d=in->m.decl;long source,dest;unsigned int sm,dm;int actual;
 if((in->op!=INEWA && in->op!=INEWAZ) || !typedimagearrayaddr(&in->d) || in->mm!=Adesc || !d || !d->desc || d->desc->id!=in->m.offset || !typedwmref(d->ty,"Draw","Image") || d->desc->size!=4 || d->desc->nmap!=1 || d->desc->map[0]!=0x80 || in->branch ||
    !typedoperand(in->sm,&in->s,Tint,1,0,p,&source,&sm,&actual) || !typedoperand(in->dm,&in->d,Tarray,0,0,p,&dest,&dm,&actual) || (dm!=AFP && dm!=AMP))return typedfail(p,"image-array-allocation");
 p->code[pc]=(Dis64Instruction){INEWAZ,SRC(sm)|AXIMM|DST(dm),2,source,dest};p->returns[pc]=254;return 1;
}
/* An immediate pointer mover consumes the borrowed address in reserved FP48. */
static int
typedimagearrayindex(Inst*in,Inst*use,Dis64TypedBody*p,unsigned int pc)
{
 long array,index,value;unsigned int sm,dm,vm;int actual,read;Addr*a;
 if(in->op!=IINDW || !typedimagearrayaddr(&in->s) || in->mm!=Afp || !in->m.decl || in->m.offset || in->branch ||
    !typedoperand(in->sm,&in->s,Tarray,0,0,p,&array,&sm,&actual) || !typedoperand(in->dm,&in->d,Tint,1,0,p,&index,&dm,&actual) ||
    !use || use->op!=IMOVP || use->mm!=Anone || use->branch)return typedfail(p,"image-array-index");
 read=use->sm==Afpind;a=read?&use->s:&use->d;
 if((!read && use->dm!=Afpind) || a->decl!=in->m.decl || a->reg!=in->m.reg || a->offset ||
    !typedoperand(read?use->dm:use->sm,read?&use->d:&use->s,TypedRoot,0,0,p,&value,&vm,&actual) || (actual!=Tref && actual!=Tany))return typedfail(p,"image-array-mover");
 p->code[pc]=(Dis64Instruction){IINDL,SRC(sm)|AXINF|DST(dm),48,array,index};p->returns[pc]=255;
 p->code[pc+1]=(Dis64Instruction){IMOVP,read?SRC(AFP|AIND)|DST(vm):SRC(vm)|DST(AFP|AIND),0,read?48:value,read?value:48};p->returns[pc+1]=255;return 1;
}

/* Array result owner remains a non-temporary FP root throughout the call. */
static int
typedimagearrayresultindex(Inst*in,Dis64TypedBody*p,unsigned int pc)
{
 long array,index;unsigned int sm,dm;int actual;
 if(in->op!=IINDW || in->sm!=Afp || !typedimagearrayaddr(&in->s) || !in->s.decl->sym || in->s.decl->sym->name[0]=='.' || in->mm!=Afp || !in->m.decl || in->m.offset || in->branch ||
    !typedoperand(Afp,&in->s,Tarray,0,0,p,&array,&sm,&actual) || !typedoperand(in->dm,&in->d,Tint,1,0,p,&index,&dm,&actual))return typedfail(p,"array-result-owner");
 p->code[pc]=(Dis64Instruction){IINDL,SRC(AFP)|AXINF|DST(dm),48,array,index};p->returns[pc]=131;return 1;
}
static int
typedimagearraybinding(TypedMulti*c,Dis64TypedBody*p,Inst*frame,Inst*binding,Type*result,unsigned int pc,unsigned int at)
{
 Inst*index;if(pc==p->first || !(index=c->instructions[pc-1]) || p->returns[pc-1-p->first]!=131 || !typedwmref(result,"Draw","Image") ||
    binding->op!=ILEA || binding->sm!=Afpind || binding->s.decl!=index->m.decl || binding->s.reg!=index->m.reg || binding->s.offset || binding->mm!=Anone || binding->branch ||
    binding->dm!=Afpind || binding->d.reg!=frame->d.reg || binding->d.offset!=16)return 0;
 p->code[at-p->first]=(Dis64Instruction){ILEA,SRC(AFP)|DST(AFP|AIND),0,p->code[pc-1-p->first].source,32};p->returns[at-p->first]=131;return 1;
}
static int
typedimagearrayarg(TypedMulti*c,Dis64TypedBody*p,Inst*frame,unsigned int end,unsigned int*at,unsigned int oldoffset,unsigned int offset)
{
 Inst*in=c->instructions[*at],*use;long array,index;unsigned int sm,dm;int actual;
 if(*at+1>=end || !(use=c->instructions[*at+1]) || in->op!=IINDW || !typedimagearrayaddr(&in->s) || in->mm!=Afp || !in->m.decl || in->m.offset || in->branch ||
    !typedoperand(in->sm,&in->s,Tarray,0,0,p,&array,&sm,&actual) || !typedoperand(in->dm,&in->d,Tint,1,0,p,&index,&dm,&actual) ||
    use->op!=IMOVP || use->sm!=Afpind || use->s.decl!=in->m.decl || use->s.reg!=in->m.reg || use->s.offset || use->mm!=Anone || use->branch ||
    use->dm!=Afpind || use->d.reg!=frame->d.reg || use->d.offset!=oldoffset)return typedfail(p,"array-call-argument");
 p->code[*at-p->first]=(Dis64Instruction){IINDL,SRC(sm)|AXINF|DST(dm),56,array,index};p->returns[(*at)++-p->first]=127;
 p->code[*at-p->first]=(Dis64Instruction){IMOVP,SRC(AFP|AIND)|DST(AFP|AIND),0,56,offset};p->returns[(*at)++-p->first]=127;return 1;
}
static int
typedwordargop(int op)
{
 return op==IADDW || op==ISUBW || op==IMULW || op==IDIVW || op==IMODW || op==IANDW || op==IORW || op==IXORW || op==ISHLW || op==ISHRW;
}
static int
typedwordargument(Inst*in,Inst*frame,Dis64TypedBody*p,unsigned int pc,unsigned int oldoffset,unsigned int offset)
{
 long source,middle;unsigned int sm,mm;int actual;
 if(!typedwordargop(in->op) || in->branch || in->dm!=Afpind || in->d.reg!=frame->d.reg || in->d.offset!=oldoffset ||
    !typedoperand(in->sm,&in->s,Tint,1,0,p,&source,&sm,&actual) || !typedoperand(in->mm,&in->m,Tint,1,1,p,&middle,&mm,&actual))return typedfail(p,"word-call-argument");
 p->code[pc]=(Dis64Instruction){in->op,SRC(sm)|(mm==AIMM?AXIMM:mm==AFP?AXINF:AXINM)|DST(AFP|AIND),middle,source,offset};p->returns[pc]=130;return 1;
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

 if((in->op==IBEQC || in->op==IBNEC) && (in->sm==Afpind || in->sm==Ampind) && typedwmref(typedfieldowner(&in->s),"Wmsrv","Client")){
  long field;int payload;
  if(in->s.offset!=20 || !in->s.native64type || in->s.native64type->kind!=Tstring || !typedclientaddress(&in->s,in->sm,p,&source,&sm,&field,&payload) || field!=40 || payload!=Tstring ||
     !typedoperand(in->mm,&in->m,Tstring,0,1,p,&middle,&mm,&actual) || actual!=Tstring || (mm!=AFP && mm!=AMP) || middle>65535 ||
     in->dm!=Apc || !in->branch || in->branch->pc<(int)p->first || in->branch->pc>=(int)stop || in->d.offset!=in->branch->pc)return typedfail(p,"client-cursor-compare");
  p->code[pc]=(Dis64Instruction){in->op,SRC(sm)|(mm==AFP?AXINF:AXINM)|DST(AIMM),middle,source,in->branch->pc-p->first};p->returns[pc]=232;return 1;
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

/* Bounded String-list traversal, rooted head, raw Word32 argument, nil clear. */
static int
typedexternalinthead(TypedMulti*c,Dis64TypedBody*p,Inst*frame,unsigned int end,unsigned int*at,unsigned int oldoffset,unsigned int offset)
{
 Inst*in;Addr temp;unsigned int n=0,wire;long source;int actual;
 if(*at>=end || !(in=c->instructions[*at]) || (in->op!=ITAIL && in->op!=IHEADP) || in->dm!=Afp || !in->d.decl || in->d.decl->store!=Dlocal || !in->d.decl->sym || in->d.decl->sym->name[0]!='.' || in->d.decl->sym->name[1]!='t' || in->d.offset)return typedfail(p,"external-int-head-temp");
 temp=in->d;
 while(*at<end && (in=c->instructions[*at]) && in->op==ITAIL && in->dm==Afp){
  if(n++==32 || in->dm!=Afp || in->d.decl!=temp.decl || in->d.reg!=temp.reg || in->d.offset || !typedloadlisttype(&in->s) || !typedloadlisttype(&in->d) || (n>1 && (in->sm!=Afp || in->s.decl!=temp.decl || in->s.reg!=temp.reg || in->s.offset)) || !typedclientlist(in,p,p->code+*at-p->first))return typedfail(p,"external-int-head-tail");
  p->returns[(*at)++-p->first]=253;
 }
 if(*at>=end || !(in=c->instructions[*at]) || in->op!=IHEADP || in->dm!=Afp || in->d.decl!=temp.decl || in->d.reg!=temp.reg || in->d.offset || !typedloadlisttype(&in->s) || (n && (in->sm!=Afp || in->s.decl!=temp.decl || in->s.reg!=temp.reg || in->s.offset)) || !typedclientlist(in,p,p->code+*at-p->first))return typedfail(p,"external-int-head-read");
 p->returns[(*at)++-p->first]=253;
 if(*at>=end || !(in=c->instructions[*at]) || in->op!=ICVTCW || in->mm!=Anone || in->branch || in->sm!=Afp || in->s.decl!=temp.decl || in->s.reg!=temp.reg || in->s.offset || in->dm!=Afpind || in->d.reg!=frame->d.reg || in->d.offset!=oldoffset || !typedoperand(Afp,&in->s,Tstring,0,0,p,&source,&wire,&actual) || actual!=Tstring)return typedfail(p,"external-int-head-convert");
 p->code[*at-p->first]=(Dis64Instruction){ICVTCW,SRC(AFP)|DST(AFP|AIND),0,source,offset};p->returns[(*at)++-p->first]=253;
 if(*at>=end || !(in=c->instructions[*at]) || in->op!=IMOVP || in->sm!=Amp || in->mm!=Anone || in->branch || in->dm!=Afp || in->d.decl!=temp.decl || in->d.reg!=temp.reg || in->d.offset || !in->s.decl || !in->s.decl->ty || in->s.decl->ty->kind!=Tany || !in->s.decl->sym || strcmp(in->s.decl->sym->name,"nil") || in->s.reg!=in->s.decl->offset || in->s.offset || !typedinst(in,c->count,p,p->code+*at-p->first,p->returns+*at-p->first))return typedfail(p,"external-int-head-clear");
 p->returns[(*at)++-p->first]=253;return 1;
}

/* Bounded tail chain, head into a mapped argument, exact nil clear. */
static int
typedexternalheadarg(TypedMulti*c,Dis64TypedBody*p,Inst*frame,Decl*arg,unsigned int end,unsigned int*at,unsigned int offset)
{
 Inst*in;Addr temp;unsigned int n=0,wire;long source;int actual;
 if(*at>=end || !(in=c->instructions[*at]) || in->op!=ITAIL || !in->d.decl || in->d.decl->store!=Dlocal || !in->d.decl->sym || in->d.decl->sym->name[0]!='.' || in->d.decl->sym->name[1]!='t' || in->dm!=Afp || !typedloadlisttype(&in->d))return typedfail(p,"external-head-temp");
 temp=in->d;
 while(*at<end && (in=c->instructions[*at]) && in->op==ITAIL && in->dm==Afp){
  if(n++==32 || in->dm!=Afp || in->d.decl!=temp.decl || in->d.reg!=temp.reg || in->d.offset || !typedloadlisttype(&in->s) || (n>1 && (in->sm!=Afp || in->s.decl!=temp.decl || in->s.reg!=temp.reg || in->s.offset)) || !typedclientlist(in,p,p->code+*at-p->first))return typedfail(p,"external-head-tail");
  p->returns[(*at)++-p->first]=252;
 }
 if(*at>=end || !(in=c->instructions[*at]) || in->op!=(arg->ty->kind==Tlist?ITAIL:IHEADP) || in->mm!=Anone || in->branch || in->sm!=Afp || in->s.decl!=temp.decl || in->s.reg!=temp.reg || in->s.offset || !typedloadlisttype(&in->s) || in->dm!=Afpind || in->d.reg!=frame->d.reg || in->d.offset!=arg->offset || !typedoperand(Afp,&in->s,Tlist,0,0,p,&source,&wire,&actual) || actual!=Tlist)return typedfail(p,"external-head-argument");
 p->code[*at-p->first]=(Dis64Instruction){in->op,SRC(AFP)|DST(AFP|AIND),0,source,offset};p->returns[(*at)++-p->first]=252;
 if(*at>=end || !(in=c->instructions[*at]) || in->op!=IMOVP || in->sm!=Amp || in->mm!=Anone || in->branch || in->dm!=Afp || in->d.decl!=temp.decl || in->d.reg!=temp.reg || in->d.offset || !in->s.decl || !in->s.decl->ty || in->s.decl->ty->kind!=Tany || !in->s.decl->sym || strcmp(in->s.decl->sym->name,"nil") || in->s.reg!=in->s.decl->offset || in->s.offset || !typedinst(in,c->count,p,p->code+*at-p->first,p->returns+*at-p->first))return typedfail(p,"external-head-clear");
 p->returns[(*at)++-p->first]=252;return 1;
}

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
 Addr *left=&in->s,*right=&in->m;int lm=in->sm,rm=in->mm,actual,kind;unsigned int mm,bm,index=0,window,draw,family;long middle,base,offset;Inst field=*in;
 if(in->dm!=Apc || !in->branch || in->branch->pc<(int)p->first || in->branch->pc>=(int)stop || in->d.offset!=in->branch->pc)return typedfail(p,"context-nil-branch");
 if((lm==Afp || lm==Amp) && (rm==Afpind || rm==Ampind)){left=&in->m;right=&in->s;lm=in->mm;rm=in->sm;}
 if((lm!=Afpind && lm!=Ampind) || (rm!=Afp && rm!=Amp))return typedfail(p,"context-nil-roles");
 if(!typedoperand(rm,right,TypedRoot,0,1,p,&middle,&mm,&actual) || (actual!=Tany && actual!=Tref))return typedfail(p,"field-reference-roles");
 field.op=IMOVP;field.s=*left;field.sm=lm;field.mm=Anone;field.dm=Afp;field.branch=nil;
 window=typedwindowowner(left);draw=typeddrawowner(left);
 if(actual==Tany && typedwmref(typedfieldowner(left),"Wmsrv","Client")){
  if(mm!=AMP || middle>65535 || !typedclientaddress(left,lm,p,&base,&bm,&offset,&kind) || (kind!=Tchan && !(kind==Tlist && offset==48)))return typedfail(p,"client-root-nil");
  p->code[pc]=(Dis64Instruction){in->op==IBEQW?IBEQL:IBNEL,SRC(bm)|AXINM|DST(AIMM),(offset<<16)|middle,base,in->branch->pc-p->first};p->returns[pc]=kind==Tlist?251:241;return 1;
 }
 if(actual!=Tany && typedwmref(typedfieldowner(left),"Wmsrv","Client")){
  if(!typedclientaddress(left,lm,p,&base,&bm,&offset,&kind) || kind!=Tref)return typedfail(p,"client-reference-field");family=4;
 }else{
  if(!(window?typedwindowfield(&field,p,pc,1,&index):draw?typeddrawfield(&field,p,pc,1,&index):typedclientfield(&field,p,pc,1)))return typedfail(p,"context-nil-roles");
  base=p->code[pc].source;offset=p->contextfields[pc];bm=lm==Afpind?AFP:AMP;family=draw?1:window?2:3;
 }
 if(actual==Tany){
  if(mm!=AMP || (!draw && !window && family!=3))return typedfail(p,"context-nil-roles");
  if(draw && index==2)return typedfail(p,"draw-string-nil-roles");
  p->code[pc]=(Dis64Instruction){in->op==IBEQW?IBEQL:IBNEL,SRC(bm|AIND)|AXINM|DST(AIMM),middle,base,in->branch->pc-p->first};
  p->returns[pc]=window?101+index:draw?114+index:97+left->offset/4;return 1;
 }
 if((draw && index==2) || (window && index==3) || (family==3 && left->offset==8) || middle<0 || middle>65535 || offset<0 || offset>255 || (mm!=AFP && mm!=AMP))return typedfail(p,"field-reference-class");
 p->code[pc]=(Dis64Instruction){in->op==IBEQW?IBEQL:IBNEL,SRC(lm==Afpind?AFP|AIND:AMP|AIND)|(mm==AFP?AXINF:AXINM)|DST(AIMM),(family<<24)|(offset<<16)|middle,base,in->branch->pc-p->first};
 p->returns[pc]=207;return 1;
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

/* Only complete nominal Wmsrv.Client globals join the earlier FP ref route. */
static int
typedloadglobalclient(Addr*a,Dis64TypedBody*p)
{
 long base,field;unsigned int wire;int kind;
 return a->decl && a->decl->store==Dglobal && typedloadreftype(a) &&
  typedclientaddress(a,Ampind,p,&base,&wire,&field,&kind) && wire==(AMP|AIND) && field==0 && kind==Tchan;
}

/* Equality of two exact canonical Client roots, without object dereference. */
static int
typedloadclientcompare(Inst*in,Dis64TypedBody*p,unsigned int pc,unsigned int stop)
{
 Addr*a=&in->s,*b=&in->m;long source,middle,field;unsigned int sm,mm,wire;int kind;
 if((in->op!=IBEQW && in->op!=IBNEW && in->op!=IBEQL && in->op!=IBNEL) || !typedloadreftype(a) || !typedloadreftype(b) || a->decl->ty->tof!=b->decl->ty->tof ||
    !typedwmref(a->decl->ty,"Wmsrv","Client") || (in->sm!=Afp && in->sm!=Amp) || (in->mm!=Afp && in->mm!=Amp) ||
    !typedclientaddress(a,in->sm==Afp?Afpind:Ampind,p,&source,&wire,&field,&kind) || field || kind!=Tchan ||
    !typedclientaddress(b,in->mm==Afp?Afpind:Ampind,p,&middle,&wire,&field,&kind) || field || kind!=Tchan ||
    !typedoperand(in->sm,a,TypedRoot,0,0,p,&source,&sm,&kind) || kind!=Tref || !typedoperand(in->mm,b,TypedRoot,0,1,p,&middle,&mm,&kind) || kind!=Tref || middle>65535 ||
    in->dm!=Apc || !in->branch || in->branch->pc<(int)p->first || in->branch->pc>=(int)stop || in->d.offset!=in->branch->pc)return typedfail(p,"client-root-compare");
 p->code[pc]=(Dis64Instruction){in->op==IBEQW?IBEQL:in->op==IBNEW?IBNEL:in->op,SRC(sm)|(mm==AFP?AXINF:AXINM)|DST(AIMM),middle,source,in->branch->pc-p->first};p->returns[pc]=231;return 1;
}

/* Checked native startup needs direct ref ADT, list-of-String and Module nil tests.
 * Nil is the compiler's readonly Tany MP root; no aggregate dereference or
 * comparison of unrelated reference classes is admitted by this route. */
static int
typedloadcompare(Inst *in,Dis64TypedBody *p,unsigned int pc,unsigned int stop)
{
 Addr *value=&in->s,*empty=&in->m;int vm=in->sm,nm=in->mm,kind,actual;Type*valuetype;
 unsigned int sm,mm;long source,middle;
 if(typedloadreftype(value) && typedloadreftype(empty))return typedloadclientcompare(in,p,pc,stop);
 if(value->decl && value->decl->ty && value->decl->ty->kind==Tany){
  value=&in->m;empty=&in->s;vm=in->mm;nm=in->sm;
 }
 valuetype=value->decl?value->decl->ty:nil;
 if(value->native64type && value->decl && value->decl->store==Dlocal && value->decl->sym && value->decl->sym->name[0]=='.' && value->decl->sym->name[1]=='t' && value->reg==value->decl->offset && !value->offset)valuetype=value->native64type;
 if((in->op!=IBEQW && in->op!=IBNEW && in->op!=IBEQL && in->op!=IBNEL) ||
    !value->decl || !value->decl->ty ||
    (!(value->decl->ty->kind==Tmodule && value->reg==value->decl->offset) && !typedloadlisttype(value) && !typedwindowlisttype(value) && !typedloadreftype(value) && !(value->decl->ty->kind==Tchan && value->reg==value->decl->offset && !value->offset)) ||
    !empty->decl || !empty->decl->ty || empty->decl->ty->kind!=Tany || nm!=Amp ||
    !typedoperand(vm,value,TypedRoot,0,0,p,&source,&sm,&kind) || (!valuetype || kind!=valuetype->kind) || (typedloadreftype(value) && sm!=AFP && !(sm==AMP && typedloadglobalclient(value,p))) ||
    !typedoperand(nm,empty,TypedRoot,0,1,p,&middle,&mm,&actual) || actual!=Tany || (kind==Tchan && middle>65535) ||
    in->dm!=Apc || !in->branch || in->branch->pc<(int)p->first || in->branch->pc>=(int)stop ||
    in->d.offset!=in->branch->pc)return typedfail(p,"load-nil-roles");
 p->code[pc]=(Dis64Instruction){in->op==IBEQW?IBEQL:in->op==IBNEW?IBNEL:in->op,
  SRC(sm)|AXINM|DST(AIMM),middle,source,in->branch->pc-p->first};if(kind==Tchan)p->returns[pc]=239;return 1;
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

/* Exact by-value Draw geometry in a normal local CALL, not SPAWN. */
static int
typedlocalgeometryarg(Inst*in,Inst*frame,Decl*arg,Dis64TypedBody*p,Dis64TypedBody*child,unsigned int pc)
{
 Dis64Layout l={0};Addr a;Type*t;unsigned int i,base=0,at,wire,sm=0;long source=0,part;int actual,ok=0;
 if(!typedwordgeometry(arg->ty,&l) || in->op!=IMOVM || in->mm!=Aimm || in->m.offset!=l.bytes || in->branch || in->dm!=Afpind || in->d.reg!=frame->d.reg || in->d.offset!=arg->offset)goto done;
 for(i=0;i<l.bytes;i+=4){if(!dis64layoutaddress(&child->frame,arg->offset+i,4,0,&at) || at<64 || at>65535 || (i && at!=base+i))goto done;if(!i)base=at;}
 if(in->sm==Afpind || in->sm==Ampind){ok=typedimagegeometry(in,p,pc,base);goto done;}
 t=in->s.native64type?in->s.native64type:in->s.decl?in->s.decl->ty:nil;
 if((in->sm!=Afp && in->sm!=Amp) || t!=arg->ty)goto done;
 for(i=0;i<l.bytes;i+=4){a=in->s;a.reg+=i;a.offset=0;if(!typedoperand(in->sm,&a,Tint,0,0,p,&part,&wire,&actual) || actual!=Tint || (i && (wire!=sm || part!=source+i)))goto done;if(!i){source=part;sm=wire;}}
 p->code[pc]=(Dis64Instruction){IMOVM,SRC(sm)|AXIMM|DST(AFP|AIND),l.bytes,source,base};p->returns[pc]=64;ok=1;
 done:dis64layoutfree(&l);return ok?1:typedfail(p,"local-geometry-argument");
}

/* Literal Draw.Point/Rect leaves relocated against the actual local frame. */
static int
typedlocalgeometryliteral(TypedMulti*c,Dis64TypedBody*p,Dis64TypedBody*child,Inst*frame,Decl*arg,unsigned int end,unsigned int*at)
{
 Dis64Layout l={0};unsigned int i,base=0,part,k,sm=0,wire,n;long source=0,value;int actual,ok=0;Inst*in;Addr a;
 if(!typedwordgeometry(arg->ty,&l))goto done;
 for(i=0;i<l.bytes;i+=4){if(!dis64layoutaddress(&child->frame,arg->offset+i,4,0,&part) || part<64 || part>65535 || (i && part!=base+i))goto done;if(!i)base=part;}
 for(i=0;i<l.bytes;){
  if(*at>=end || !(in=c->instructions[*at]))goto done;
  if(in->op==IHEADP || in->op==ITAIL){if(!typedexternalinthead(c,p,frame,end,at,arg->offset+i,base+i))goto done;i+=4;continue;}
  if(typedwordargop(in->op)){if(!typedwordargument(in,frame,p,*at-p->first,arg->offset+i,base+i))goto done;(*at)++;i+=4;continue;}
  if(in->op==IMOVM){
   n=in->m.offset;if(in->mm!=Aimm || (n!=8 && n!=16) || n>l.bytes-i || in->branch || in->dm!=Afpind || in->d.reg!=frame->d.reg || in->d.offset!=arg->offset+i || (in->sm!=Afp && in->sm!=Amp))goto done;
   for(k=0;k<n;k+=4){a=in->s;a.reg+=k;a.offset=0;if(!typedoperand(in->sm,&a,Tint,0,0,p,&value,&wire,&actual) || actual!=Tint || (k && (wire!=sm || value!=source+k)))goto done;if(!k){source=value;sm=wire;}}
   p->code[*at-p->first]=(Dis64Instruction){IMOVM,SRC(sm)|AXIMM|DST(AFP|AIND),n,source,base+i};p->returns[(*at)++-p->first]=64;i+=n;continue;
  }
  if(in->op!=IMOVW || in->mm!=Anone || in->branch || in->dm!=Afpind || in->d.reg!=frame->d.reg || in->d.offset!=arg->offset+i || !typedoperand(in->sm,&in->s,Tint,1,0,p,&source,&sm,&actual))goto done;
  p->code[*at-p->first]=(Dis64Instruction){IMOVW,SRC(sm)|DST(AFP|AIND),0,source,base+i};p->returns[(*at)++-p->first]=64;i+=4;
 }
 ok=1;
 done:dis64layoutfree(&l);return ok?1:typedfail(p,"local-geometry-literal");
}

static int
typedmultisite(TypedMulti *c,Dis64TypedBody *p,unsigned int pc,unsigned int stop,unsigned int depth,unsigned int *next,unsigned int *height)
{
    Inst *frame=c->instructions[pc],*call,*in,*binding;
    Dis64TypedBody *child;Dis64TypedSite *site;
    Decl *arg;unsigned int end=pc+1,at,sm,offset,width,contract=0,local=0,inherit=0;
    int target,kind,root,op,actual,spawning;long source;
    if(frame->sm!=Adesc || frame->mm!=Anone || frame->dm!=Afp || frame->branch!=nil || frame->d.reg<32 ||
       frame->d.reg>(int)p->frame.legacybytes-4)return typedfail(p,"multi-frame");
    while(end<stop && c->instructions[end]!=nil && (c->instructions[end]->dm==Afpind ||
      (c->loads && (((c->instructions[end]->op==IMOVP || c->instructions[end]->op==IHEADP || c->instructions[end]->op==ITAIL) && c->instructions[end]->dm==Afp) || c->instructions[end]->op==IINDW || c->instructions[end]->op==ISLICEA || (c->instructions[end]->op==ICVTWC && c->instructions[end]->dm==Afp)))))end++;
    if(end>=stop || (call=c->instructions[end])==nil || (call->op!=ICALL && !(c->loads && call->op==ISPAWN)) || call->sm!=Afp || call->s.reg!=frame->d.reg ||
       call->mm!=Anone || call->dm!=Apc || call->d.offset<0 || (unsigned long)call->d.offset>=c->count ||
       (target=c->entries[call->d.offset])<0)return typedfail(p,"multi-call");
    if(c->functions[target]->desc==nil || c->functions[target]->desc->id!=frame->s.offset ||
       (call->branch!=nil && call->branch!=c->functions[target]->pc))return typedfail(p,"multi-target");
    spawning=call->op==ISPAWN;
    if(spawning && (c->functions[target]->ty->tof->kind!=Tnone || c->state[target]==1))return typedfail(p,"spawn-callee");
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
        if(c->loads && !spawning && kind==Tadt){
            int prepared=0;
            if(in->op!=IMOVP && (in->op!=IMOVM || in->m.offset!=arg->ty->size)){if(!typedlocalgeometryliteral(c,p,child,frame,arg,end,&at))return 0;continue;}
            if(in->op==IMOVP){if(at+2>=end || !typedlocalscreenimageprep(in,c->instructions[at+1],p,at-p->first))return 0;in=c->instructions[++at];prepared=1;}
            if(!typedlocalgeometryarg(in,frame,arg,p,child,at-p->first))return 0;at++;
            if(prepared){unsigned int clear=at;Inst store=*in;store.op=IMOVP;store.sm=Afp;store.s.offset=0;if(!typedrootargclear(c,p,&store,end,&at) || clear==at)return typedfail(p,"local-screen-image-clear");}
            continue;
        }
        if(c->loads && !spawning && kind==Tref && typedwmref(arg->ty,"Draw","Image") && in->op==IINDW){
            if(!dis64layoutaddress(&child->frame,arg->offset,4,1,&offset) || !typedimagearrayarg(c,p,frame,end,&at,arg->offset,offset))return 0;continue;
        }
        if(c->loads && !spawning && kind==Tint && typedwordargop(in->op)){
            if(!dis64layoutaddress(&child->frame,arg->offset,4,0,&offset) || !typedwordargument(in,frame,p,at-p->first,arg->offset,offset))return 0;at++;continue;
        }
        if(c->loads && !spawning && kind==Tint && (in->op==ITAIL || in->op==IHEADP)){
            if(!dis64layoutaddress(&child->frame,arg->offset,4,0,&offset) || offset<64 || offset>65535 || !typedexternalinthead(c,p,frame,end,&at,arg->offset,offset))return typedfail(p,"local-int-head-argument");
            continue;
        }
        if(spawning && (in->op!=op || (in->sm!=Afp && in->sm!=Amp && in->sm!=Aimm)))return typedfail(p,"spawn-argument");
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
        if(c->loads && !spawning && kind==Tstring && in->op==ICVTWC){
            Inst*first=in,*concat,*clear;unsigned int mm;long middle;
            if(c->functions[target]->ty->tof->kind!=Tnone || arg!=c->functions[target]->ty->ids || arg->next || at+2>=end || in->dm!=Afp || !in->d.decl || in->d.decl->store!=Dlocal || !in->d.decl->sym || in->d.decl->sym->name[0]!='.' || in->d.decl->sym->name[1]!='t' ||
               !typedclientidstring(in,p,at-p->first) || !dis64layoutaddress(&child->frame,arg->offset,4,1,&offset) || offset!=64)return typedfail(p,"local-id-string-prefix");
            concat=c->instructions[at+1];clear=c->instructions[at+2];
            if(concat->op!=IADDC || concat->branch || concat->dm!=Afpind || concat->d.reg!=frame->d.reg || concat->d.offset!=arg->offset ||
               !((concat->sm==Afp && concat->s.decl==first->d.decl && concat->s.reg==first->d.reg && !concat->s.offset) || (concat->mm==Afp && concat->m.decl==first->d.decl && concat->m.reg==first->d.reg && !concat->m.offset)) ||
               !typedoperand(concat->sm,&concat->s,Tstring,0,0,p,&source,&sm,&actual) || actual!=Tstring || !typedoperand(concat->mm,&concat->m,Tstring,0,1,p,&middle,&mm,&actual) || actual!=Tstring ||
               clear->op!=IMOVP || clear->sm!=Amp || clear->dm!=Afp || clear->mm!=Anone || clear->branch || clear->d.decl!=first->d.decl || clear->d.reg!=first->d.reg || clear->d.offset || !clear->s.decl || !clear->s.decl->ty || clear->s.decl->ty->kind!=Tany ||
               !typedinst(clear,stop,p,p->code+at+2-p->first,p->returns+at+2-p->first))return typedfail(p,"local-id-string-argument");
            p->code[at+1-p->first]=(Dis64Instruction){IADDC,SRC(sm)|(mm==AFP?AXINF:AXINM)|DST(AFP|AIND),middle,source,offset};p->returns[at+1-p->first]=64;p->returns[at+2-p->first]=238;at+=3;continue;
        }
        if(c->loads && kind==Tstring && in->op==ICVTAC){
            if(arg->next || in->mm!=Anone || in->branch || in->dm!=Afpind || in->d.reg!=frame->d.reg || in->d.offset!=arg->offset ||
               !typedbytearrayaddr(&in->s) || !dis64layoutaddress(&child->frame,arg->offset,4,1,&offset) || offset>65535 ||
               !typedoperand(in->sm,&in->s,Tarray,0,0,p,&source,&sm,&actual))return typedfail(p,"local-byte-string-argument");
            p->code[at-p->first]=(Dis64Instruction){ICVTAC,SRC(sm)|DST(AFP|AIND),0,source,offset};p->returns[at++-p->first]=64;continue;
        }
        if(c->loads && kind==Tstring && in->op==IADDC && (child==p || (arg==c->functions[target]->ty->ids && !arg->next))){
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
               !dis64layoutaddress(&child->frame,arg->offset,4,1,&offset) || offset>65535 || !typedselectedfieldarg(in,p,at-p->first,offset,kind) || p->returns[at-p->first]==233)return typedfail(p,"multi-field-argument");
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
        contract=child->resultgeometry?(child->resultgeometry==8?96:112):child->resulttriple?72:child->resultpair?48:child->resultbytes | (root?16:0);
        if(c->loads && typedimagearraybinding(c,p,frame,binding,c->functions[target]->ty->tof,pc,at)){
            local=p->code[at-p->first].source;
        }else if(binding->sm==Afp){
            if(child->resultgeometry){if(!typedgeometrybinding(binding,c->functions[target]->ty->tof,p,&local,&contract))return typedfail(p,"local-geometry-result");source=local;sm=AFP;}
            else if(child->resulttriple){if(!typedchannelbinding(c,p,binding,&local))return 0;source=local;sm=AFP;}
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
            if(child->resultgeometry || binding->sm!=Afpind || binding->s.reg!=16 || binding->s.offset || kind!=p->resultkind || child->resultpair!=p->resultpair || child->resulttriple!=p->resulttriple)return typedfail(p,"multi-inherit");
            inherit=1;local=32;p->code[at-p->first]=(Dis64Instruction){IMOVL,SRC(AFP)|DST(AFP|AIND),0,32,32};p->returns[at-p->first]=64;
        }
        at++;
    }
    if(at!=end)return typedfail(p,"multi-setup-length");
    p->code[end-p->first]=(Dis64Instruction){spawning?ISPAWN:ICALL,SRC(AFP)|DST(AIMM),0,40,call->d.offset};p->returns[end-p->first]=spawning?186:inherit?32:0;
    site=p->sites+p->nsites++;*site=(Dis64TypedSite){pc-p->first,end-p->first,call->d.offset,frame->s.offset,local,contract,inherit};
    *next=end+1;return 1;
}

#include "dis64-alt.h"

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
            if(pc>p->first && p->returns[pc-1-p->first]==131)inside[pc-p->first]=1;
            for(i=pc+1;i<next;i++)inside[i-p->first]=1;
            pc=next;
        }else if(c->loads && (in->op==IALT || in->op==INBALT || ((in->op==IMOVW || in->op==ILEA) && in->dm==Afp && in->d.reg>=0 && typedaltslot(&in->d,fn)))){
            if(!typedaltinst(in,fn,p,pc-p->first))goto fail;
            if(in->op==IALT || in->op==INBALT){unsigned int start=pc-p->first-2,n=p->code[pc-p->first].middle;while(start && n){start--;if(p->returns[start]==189 || p->returns[start]==192 || p->returns[start]==193 || p->returns[start]==198)n--;}if(n){typedfail(p,"alt-setup-shape");goto fail;}for(i=start+1;i<=pc-p->first;i++)inside[i]=1;}
            pc++;
        }else if(c->loads && p->resultgeometry && in->op==IMOVM && in->dm==Afpind && in->d.reg==16){
            if(!typedlocalgeometryreturn(in,p,p->code+pc-p->first,p->returns+pc-p->first))goto fail;pc++;
        }else if(c->loads && in->op==IMOVM){
            if(!typedlocalgeometry(in,p,p->code+pc-p->first,p->returns+pc-p->first))goto fail;pc++;
        }else if(c->loads && p->resulttriple && in->op==IMOVMP){
            Desc*d;unsigned int base=0;
            for(d=descriptors;d && d->id!=in->m.offset;d=d->next);
            if(in->sm!=Afp || in->mm!=Adesc || in->dm!=Afpind || in->d.reg!=16 || in->d.offset || in->branch || !in->m.decl || in->m.decl->desc!=d || !typedchanneltriple(in->m.decl->ty) || !d || d->size!=12 || d->nmap!=1 || d->map[0]!=0xe0 || !typedchannelbinding(c,p,in,&base)){typedfail(p,"channel-result-copy");goto fail;}
            p->code[pc-p->first]=(Dis64Instruction){IMOVMP,SRC(AFP)|AXIMM|DST(AFP|AIND),0,base,32};p->returns[pc++-p->first]=171;
        }else if(c->loads && (in->op==IMOVP || in->op==ISLICEA) && typedreadreplyprep(c,in,p,pc,stop)){pc++;
        }else if(c->loads && in->op==IMOVMP){
            if(!typedaltpaircopy(in,p,p->code+pc-p->first,p->returns+pc-p->first))goto fail;pc++;
        }else if(c->external>=2 && in->op==IMOVP && in->dm==Afpind && in->d.reg==16 && in->d.offset==0){
            if(!typedinst(in,stop,p,p->code+pc-p->first,p->returns+pc-p->first))goto fail;pc++;
        }else if(c->loads && (in->op==INEWA || in->op==INEWAZ) && typedimagearrayaddr(&in->d)){
            if(!typedimagearraynew(in,p,pc-p->first))goto fail;pc++;
        }else if(c->loads && in->op==IINDW && typedimagearrayaddr(&in->s) && pc+1<stop && (c->instructions[pc+1]->op==IFRAME || c->instructions[pc+1]->op==IMFRAME)){
            if(!typedimagearrayresultindex(in,p,pc-p->first))goto fail;pc++;
        }else if(c->loads && in->op==IINDW && typedimagearrayaddr(&in->s)){
            if(pc+1>=stop || !typedimagearrayindex(in,c->instructions[pc+1],p,pc-p->first))goto fail;inside[pc+1-p->first]=1;pc+=2;
        }else if((c->external>=2 || c->loads) && (in->op==INEWA || in->op==INEWAZ || in->op==ISLICEA)){
            if((c->loads && !typedloadbytes(in,p,pc-p->first)) || !typedclientarray(in,p,pc-p->first))goto fail;pc++;
        }else if(c->external>=2 && (in->op==IINDB || in->op==IINDW)){
            if(pc+1>=stop || !typedclientindex(in,c->instructions[pc+1],p,pc-p->first))goto fail;
            inside[pc+1-p->first]=1;pc+=2;
        }else if(c->external>=2 && (in->op==IMOVB || in->op==ICVTBW || in->op==ICVTWB)){
            if(!typedclientbyte(in,p,pc-p->first))goto fail;pc++;
        }else if(c->loads && in->op==ICVTWC && (in->sm==Afpind || in->sm==Ampind) && typedwmref(typedfieldowner(&in->s),"Wmsrv","Client")){
            if(!typedclientidstring(in,p,pc-p->first))goto fail;pc++;
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
        }else if(c->loads && in->op==ICVTCW && (in->dm==Afpind || in->dm==Ampind) && typedwmref(typedfieldowner(&in->d),"Draw","Pointer")){
            if(!typedpointerparse(in,p,pc-p->first))goto fail;pc++;
        }else if((c->external>=2 && (in->op==ICVTCW || in->op==ICVTCL || in->op==ICVTCF || in->op==ICVTWC || in->op==ICVTLC || in->op==ICVTFC || in->op==ICVTCA || in->op==ICVTAC)) || (c->loads && (in->op==ICVTCA || in->op==ICVTAC || in->op==ICVTWC || in->op==ICVTCW))){
            if((c->loads && (in->op==ICVTCA || in->op==ICVTAC) && !typedloadbytes(in,p,pc-p->first)) || !typedclientcodec(in,p,pc-p->first))goto fail;pc++;
        }else if((c->external>=2 && (in->op==IADDC || in->op==IINSC || in->op==ISLICEC || in->op==IINDC || in->op==IBEQC || in->op==IBNEC || in->op==IBLTC || in->op==IBLEC || in->op==IBGTC || in->op==IBGEC)) ||
                 (c->loads && (in->op==IADDC || in->op==ISLICEC || in->op==IINDC || in->op==IBEQC || in->op==IBNEC || in->op==IBLTC || in->op==IBLEC || in->op==IBGTC || in->op==IBGEC))){
            if(!typedclientstring(in,p,pc-p->first,stop))goto fail;pc++;
        }else if(c->loads && (in->sm==Afpind || in->sm==Ampind) && (typeddrawowner(&in->s)==1 || (typedwmref(typedfieldowner(&in->s),"Wmsrv","Client") && in->s.native64type && in->s.native64type->kind==Tint && (in->op==IBEQW || in->op==IBNEW || in->op==IBLTW || in->op==IBLEW || in->op==IBGTW || in->op==IBGEW))) && (in->op==IMOVW || typedwordargop(in->op) || in->op==IBEQW || in->op==IBNEW || in->op==IBLTW || in->op==IBLEW || in->op==IBGTW || in->op==IBGEW)){
            if(!typedimageword(in,p,pc-p->first,stop))goto fail;pc++;
        }else if(c->loads && (in->op==IORW || in->op==IANDW || in->op==IXORW) && (in->dm==Afpind || in->dm==Ampind) && typedwmref(typedfieldowner(&in->d),"Wmsrv","Client")){
            if(!typedclientflagsor(in,p,pc-p->first))goto fail;pc++;
        }else if(c->loads && in->op==IANDW && (in->sm==Afpind || in->sm==Ampind) && typedwmref(typedfieldowner(&in->s),"Wmsrv","Client")){
            if(!typedclientflagsand(in,p,pc-p->first))goto fail;pc++;
        }else if(c->loads && in->op==IMOVW && (in->dm==Afpind || in->dm==Ampind) && typedwmref(typedfieldowner(&in->d),"Draw","Pointer")){
            if(!typedpointerstore(in,p,pc-p->first))goto fail;pc++;
        }else if(c->loads && (in->op==IMOVW || in->op==IBEQW || in->op==IBNEW) && (in->sm==Afpind || in->sm==Ampind) && typedwmref(typedfieldowner(&in->s),"Draw","Pointer")){
            if(!typedpointerword(in,p,pc-p->first,stop))goto fail;pc++;
        }else if(c->loads && in->op==IMOVP && (in->sm==Afpind || in->sm==Ampind) && (in->dm==Afpind || in->dm==Ampind)){
            if(!typedcontextcopy(in,p,pc-p->first))goto fail;pc++;
        }else if(c->loads && in->op==IMOVP && (in->sm==Afpind || in->sm==Ampind || in->dm==Afpind || in->dm==Ampind) &&
          typedwmsrvwindow(typedfieldowner((in->sm==Afpind || in->sm==Ampind)?&in->s:&in->d))){
            if(!typedwmsrvwindowfield(in,p,p->code+pc-p->first,p->returns+pc-p->first))goto fail;pc++;
        }else if(c->loads && (in->op==IMOVP || in->op==IMOVW) &&
          (in->sm==Afpind || in->sm==Ampind || in->dm==Afpind || in->dm==Ampind) &&
          typedwmref(typedfieldowner((in->sm==Afpind || in->sm==Ampind)?&in->s:&in->d),"Wmsrv","Client")){
            if(!typedclientfieldmove(in,p,p->code+pc-p->first,p->returns+pc-p->first))goto fail;pc++;
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
        }else if(c->loads && (in->op==ISEND || in->op==IRECV)){
            if(!typedclienttransfer(in,p,p->code+pc-p->first,p->returns+pc-p->first))goto fail;pc++;
        }else if(c->loads && in->op==INEWCMP){
            if(!typedclientmappedchannel(in,fn,p,p->code+pc-p->first,p->returns+pc-p->first))goto fail;pc++;
        }else if(c->loads && (in->op==INEWCB || in->op==INEWCW || in->op==INEWCL || in->op==INEWCF || in->op==INEWCP)){
            if(!typedclientchannel(in,fn,p,p->code+pc-p->first,p->returns+pc-p->first))goto fail;pc++;
        }else if((c->external>=2 || c->loads) && (in->op==INEW || in->op==INEWZ)){
            if(c->loads && in->s.decl && in->s.decl->sym && !strcmp(in->s.decl->sym->name,"Pointer")){
                if(!typedpointernew(in,p,p->code+pc-p->first))goto fail;p->returns[pc-p->first]=242;pc++;continue;
            }
            if(!typedclientcontext(in,p,p->code+pc-p->first))goto fail;
            if(c->loads){p->code[pc-p->first].source=0;p->returns[pc-p->first]=UDST(p->code[pc-p->first].mode)==(AFP|AIND)?174:172;}pc++;
        }else if(c->loads && (in->op==ICONSM || in->op==IHEADM || (in->op==ITAIL && typedrectlisttype(&in->s)))){
            if(!typedrectlist(in,p,p->code+pc-p->first,p->returns+pc-p->first))goto fail;pc++;
        }else if(c->loads && (in->op==ICONSW || in->op==IHEADW || (in->op==ITAIL && typedintlisttype(&in->s)))){
            if(!typedclientintlist(in,fn,p,p->code+pc-p->first,p->returns+pc-p->first))goto fail;pc++;
        }else if(c->loads && (in->op==ICONSP || in->op==IHEADP || in->op==ITAIL) &&
          ((in->op!=ICONSP && typedwindowlisttype(&in->s)) || (in->op!=IHEADP && typedwindowlisttype(&in->d)))){
            if(!typedwindowlist(in,p,p->code+pc-p->first,p->returns+pc-p->first))goto fail;pc++;
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
        for(j=0;j<(unsigned int)t->table->nlab+(t->op!=IGOTO || t->altdefault);j++){
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
