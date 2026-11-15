/* Trusted compiler IR only. Included after the shared typed operand/CFG checks. */
typedef struct TypedMulti TypedMulti;
struct TypedMulti {
    Decl *globals,**functions;
    Inst **instructions;
    int *entries,nf,external;
    Dis64TypedBody *bodies;
    uchar *state;
    unsigned int *height,count,cost;
};

#include "dis64-external.h"

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

/* Fixed Draw.Context geometry from trusted .m declarations, not a guessed C size. */
static int
typedclientcontext(Inst *in,Dis64TypedBody *p,Dis64Instruction *out)
{
 Dis64Layout layout={0};Decl *d=in->s.decl;unsigned int i,dm;long destination;int actual,ok=0;
 if(in->sm!=Adesc || in->mm!=Anone || in->branch || !d || !d->ty || d->ty->kind!=Tadt || !d->desc || d->desc->id!=in->s.offset ||
    !d->sym || strcmp(d->sym->name,"Context") || !d->dot || !d->dot->sym || strcmp(d->dot->sym->name,"Draw") ||
    !dis64layouttype(d->ty,&layout) || layout.bytes!=24 || layout.count!=3)goto done;
 for(i=0;i<3;i++)if(layout.leaves[i].native!=8*i || layout.leaves[i].oldbytes!=4 || layout.leaves[i].bytes!=8 || layout.leaves[i].kind<0)goto done;
 if(!typedoperand(in->dm,&in->d,TypedRoot,0,0,p,&destination,&dm,&actual) || actual!=Tref || dm!=AFP)goto done;
 *out=(Dis64Instruction){in->op,SRC(AIMM)|DST(AFP),0,1,destination};ok=1;
 done:dis64layoutfree(&layout);return ok?1:typedfail(p,"client-context-geometry");
}

/* One exact Context field and one direct mapped root. No nested dereference. */
static int
typedclientfield(Inst *in,Dis64TypedBody *p,unsigned int pc,int probe)
{
 Addr *a;Type *t;Dis64Layout layout={0};unsigned int i,bm,vm;long base,value=0;int actual,read,kind,ok=0;vm=AFP;
 read=in->sm==Afpind;a=read?&in->s:&in->d;
 if(in->op!=IMOVP || in->mm!=Anone || in->branch || (read?in->dm!=Afp:in->dm!=Afpind) ||
    !a->decl || !(t=a->decl->ty) || t->kind!=Tref || !(t=t->tof) || t->kind!=Tadt ||
    !t->decl || !t->decl->sym || strcmp(t->decl->sym->name,"Context") || !t->decl->dot ||
    !t->decl->dot->sym || strcmp(t->decl->dot->sym->name,"Draw") ||
    a->offset<0 || a->offset>8 || a->offset%4 || !dis64layouttype(t,&layout) || layout.bytes!=24 || layout.count!=3)goto done;
 for(i=0;i<3;i++)if(layout.leaves[i].legacy!=4*i || layout.leaves[i].native!=8*i || layout.leaves[i].oldbytes!=4 || layout.leaves[i].bytes!=8 || layout.leaves[i].kind!=(i==2?Tchan:Tref))goto done;
 if(!typedoperand(Afp,a,TypedRoot,0,0,p,&base,&bm,&actual) || actual!=Tref)goto done;
 kind=a->offset==8?Tchan:Tref;
 if(!probe && (!typedoperand(read?in->dm:in->sm,read?&in->d:&in->s,TypedRoot,0,0,p,&value,&vm,&actual) ||
    (actual!=kind && actual!=Tany) || (read?vm!=AFP:vm!=AFP && vm!=AMP)))goto done;
 if(!p->contextfields){p->contextfields=allocmem(p->count*sizeof(long));memset(p->contextfields,0,p->count*sizeof(long));}
 p->contextfields[pc]=2*a->offset;
 p->code[pc]=(Dis64Instruction){IMOVP,read?SRC(AFP|AIND)|DST(AFP):SRC(vm)|DST(AFP|AIND),0,read?base:value,read?value:base};ok=1;
 done:dis64layoutfree(&layout);return ok?1:typedfail(p,"context-field-roles");
}

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
     !typedoperand(in->dm,&in->d,Tstring,0,0,p,&destination,&dm,&actual) || dm!=AFP ||
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

static int typedmultibody(TypedMulti*,unsigned int,unsigned int);

static int
typedmultisite(TypedMulti *c,Dis64TypedBody *p,unsigned int pc,unsigned int stop,unsigned int depth,unsigned int *next,unsigned int *height)
{
    Inst *frame=c->instructions[pc],*call,*in,*binding;
    Dis64TypedBody *child;Dis64TypedSite *site;
    Decl *arg;unsigned int end=pc+1,at,sm,offset,width,contract=0,local=0,inherit=0;
    int target,kind,root,op,actual;long source;
    if(frame->sm!=Adesc || frame->mm!=Anone || frame->dm!=Afp || frame->branch!=nil || frame->d.reg<32 ||
       frame->d.reg>(int)p->frame.legacybytes-4)return typedfail(p,"multi-frame");
    while(end<stop && c->instructions[end]!=nil && c->instructions[end]->dm==Afpind)end++;
    if(end>=stop || (call=c->instructions[end])==nil || call->op!=ICALL || call->sm!=Afp || call->s.reg!=frame->d.reg ||
       call->mm!=Anone || call->dm!=Apc || call->d.offset<0 || (unsigned long)call->d.offset>=c->count ||
       (target=c->entries[call->d.offset])<0)return typedfail(p,"multi-call");
    if(c->functions[target]->desc==nil || c->functions[target]->desc->id!=frame->s.offset ||
       (call->branch!=nil && call->branch!=c->functions[target]->pc))return typedfail(p,"multi-target");
    if(!typedmultibody(c,target,depth+1))return typedfail(p,"multi-callee");
    child=c->bodies+target;
    if(*height<c->height[target]+1)*height=c->height[target]+1;
    if(*height>64)return typedfail(p,"multi-depth");
    p->code[pc-p->first]=(Dis64Instruction){IFRAME,SRC(AIMM)|DST(AFP),0,frame->s.offset,40};at=pc+1;
    for(arg=c->functions[target]->ty->ids;arg!=nil;arg=arg->next){
        if(at>=end || (in=c->instructions[at])==nil)return typedfail(p,"multi-argument");
        kind=arg->ty->kind;root=kind==Tstring || kind==Tref || kind==Tarray || kind==Tlist || kind==Tchan || kind==Tmodule;
        width=kind==Tbyte?1:kind==Tbig || kind==Treal?8:4;
        op=root?IMOVP:kind==Tbyte?IMOVB:kind==Tbig?IMOVL:kind==Treal?IMOVF:IMOVW;
        if((!root && kind!=Tbyte && kind!=Tint && kind!=Tfix && kind!=Tbig && kind!=Treal) ||
           in->op!=op || in->mm!=Anone || in->dm!=Afpind || in->d.reg!=frame->d.reg || in->d.offset!=arg->offset || in->branch!=nil ||
           !dis64layoutaddress(&child->frame,arg->offset,width,root,&offset) || offset>65535 ||
           !(kind==Tbyte && c->external>=2?typedclientbyteoperand(in->sm,&in->s,kind,p,&source,&sm):
             typedoperand(in->sm,&in->s,root?TypedRoot:kind,!root && width<8,0,p,&source,&sm,&actual)) ||
           (root && actual!=kind && actual!=Tany))return typedfail(p,"multi-argument");
        p->code[at-p->first]=(Dis64Instruction){op,SRC(sm)|DST(AFP|AIND),0,source,offset};p->returns[at++-p->first]=64;
    }
    if(child->resultkind!=Tnone){
        if(at+1!=end || (binding=c->instructions[at])==nil || binding->op!=ILEA || binding->mm!=Anone ||
           binding->dm!=Afpind || binding->d.reg!=frame->d.reg || binding->d.offset!=16 || binding->branch!=nil)return typedfail(p,"multi-result");
        kind=child->resultkind;root=kind==Tstring || kind==Tref || kind==Tarray || kind==Tlist || kind==Tchan || kind==Tmodule;
        contract=child->resultbytes | (root?16:0);
        if(binding->sm==Afp){
            if(!typedoperand(Afp,&binding->s,root?TypedRoot:kind,0,0,p,&source,&sm,&actual) ||
               (root && actual!=kind && actual!=Tany))return typedfail(p,"multi-result");
            local=source;p->code[at-p->first]=(Dis64Instruction){ILEA,SRC(AFP)|DST(AFP|AIND),0,source,32};p->returns[at-p->first]=128|contract;
        }else{
            if(binding->sm!=Afpind || binding->s.reg!=16 || binding->s.offset || kind!=p->resultkind)return typedfail(p,"multi-inherit");
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
    i=dis64typedlower(fn,c->globals,c->instructions[0],c->count,stop,p);
    cost=p->frame.bytes+p->data.bytes+(p->frame.count+p->data.count)*sizeof(Dis64LayoutLeaf);
    if(cost>32*1024*1024-c->cost){typedfail(p,"multi-storage-budget");goto fail;}c->cost+=cost;
    /* A successful scalar lowering may retain an immutable byte in MP.
     * Command files serialize strings only, so replay through the byte folder. */
    if(i && c->external>=2)for(pc=0;pc<p->count;pc++)
        if((p->code[pc].op==IMOVB || p->code[pc].op==ICVTBW) && USRC(p->code[pc].mode)==AMP){i=0;typedfail(p,"unsupported-opcode");break;}
    if(i){c->state[index]=2;c->height[index]=1;return 1;}
    if(p->reason==nil || (strcmp(p->reason,"unsupported-opcode") && !(c->external>=2 && p->code && !p->frame.reason && !p->data.reason)))goto fail;
    p->reason=nil;memset(p->code,0,p->count*sizeof(*p->code));memset(p->returns,0,p->count);
    p->sites=allocmem((p->count/2+1)*sizeof(*p->sites));
    if(c->external)p->external=allocmem((p->count/2+1)*sizeof(*p->external));
    inside=allocmem(p->count);memset(inside,0,p->count);
    for(pc=p->first;pc<stop;){
        in=c->instructions[pc];p->badpc=pc;
        if(in==nil){typedfail(p,"multi-instruction-order");goto fail;}
        if(in->op==IFRAME || (c->external && in->op==IMFRAME)){
            if(in->op==IMFRAME){if(!typedexternalsite(c,p,pc,stop,&next))goto fail;}
            else if(!typedmultisite(c,p,pc,stop,depth,&next,&h))goto fail;
            for(i=pc+1;i<next;i++)inside[i-p->first]=1;
            pc=next;
        }else if(c->external>=2 && in->op==IMOVP && in->dm==Afpind && in->d.reg==16 && in->d.offset==0){
            if(!typedinst(in,stop,p,p->code+pc-p->first,p->returns+pc-p->first))goto fail;pc++;
        }else if(c->external>=2 && (in->op==INEWA || in->op==INEWAZ || in->op==ISLICEA)){
            if(!typedclientarray(in,p,pc-p->first))goto fail;pc++;
        }else if(c->external>=2 && (in->op==IINDB || in->op==IINDW)){
            if(pc+1>=stop || !typedclientindex(in,c->instructions[pc+1],p,pc-p->first))goto fail;
            inside[pc+1-p->first]=1;pc+=2;
        }else if(c->external>=2 && (in->op==IMOVB || in->op==ICVTBW || in->op==ICVTWB)){
            if(!typedclientbyte(in,p,pc-p->first))goto fail;pc++;
        }else if(c->external>=2 && (in->op==ICVTCW || in->op==ICVTCL || in->op==ICVTCF || in->op==ICVTWC || in->op==ICVTLC || in->op==ICVTFC || in->op==ICVTCA || in->op==ICVTAC)){
            if(!typedclientcodec(in,p,pc-p->first))goto fail;pc++;
        }else if(c->external>=2 && (in->op==IADDC || in->op==IINSC || in->op==ISLICEC || in->op==IINDC || in->op==IBEQC || in->op==IBNEC || in->op==IBLTC || in->op==IBLEC || in->op==IBGTC || in->op==IBGEC)){
            if(!typedclientstring(in,p,pc-p->first,stop))goto fail;pc++;
        }else if(c->external>=2 && in->op==IMOVP && (in->sm==Afpind || in->dm==Afpind)){
            if(!typedclientfield(in,p,pc-p->first,0))goto fail;pc++;
        }else if(c->external>=2 && (in->op==IBEQW || in->op==IBNEW || in->op==IBEQL || in->op==IBNEL) &&
          ((in->s.decl && in->s.decl->ty && typedclientreferencekind(in->s.decl->ty->kind)) ||
           (in->m.decl && in->m.decl->ty && typedclientreferencekind(in->m.decl->ty->kind)))){
            if(!typedclientcompare(in,p,pc-p->first,stop))goto fail;pc++;
        }else if(c->external>=2 && (in->op==INEW || in->op==INEWZ)){
            if(!typedclientcontext(in,p,p->code+pc-p->first))goto fail;pc++;
        }else if(c->external>=2 && (in->op==ICONSP || in->op==IHEADP || in->op==ITAIL)){
            if(!typedclientlist(in,p,p->code+pc-p->first))goto fail;
            if(UDST(p->code[pc-p->first].mode)==(AFP|AIND))p->returns[pc-p->first]=1;pc++;
        }else if(c->external==2 && in->op==ILOAD){
            long source,destination;unsigned int sm,dm;int actual;
            if(in->mm!=Aldt || in->m.decl==nil || in->m.decl->offset!=0 || in->branch!=nil ||
               !typedoperand(in->sm,&in->s,Tstring,0,0,p,&source,&sm,&actual) || actual!=Tstring ||
               !typedoperand(in->dm,&in->d,TypedRoot,0,0,p,&destination,&dm,&actual) || actual!=Tmodule){typedfail(p,"load-roles");goto fail;}
            p->code[pc-p->first]=(Dis64Instruction){ILOAD,SRC(sm)|AXIMM|DST(dm),0,source,destination};pc++;
        }else{
            if(!typedinst(in,stop,p,p->code+pc-p->first,p->returns+pc-p->first))goto fail;
            pc++;
        }
    }
    for(pc=p->first;pc<stop;pc++){
        in=c->instructions[pc];
        if(in->op!=ICALL && in->op!=IMCALL && in->branch!=nil && inside[in->branch->pc-p->first]){typedfail(p,"multi-setup-branch");goto fail;}
    }
    if((c->external<2 && !p->nsites && !p->nexternal) || !typedresults(p))goto fail;
    free(inside);p->tail=c->external?4:3;c->state[index]=2;c->height[index]=h;return 1;
fail:
    free(inside);c->state[index]=3;return 0;
}

static int
typedmultilower(Decl *fn,Decl *globals,Decl **functions,int nf,Inst *input,unsigned int count,unsigned int stop,Dis64TypedBody *p,int external)
{
    TypedMulti c={0};Inst *in;unsigned int i,root=0;int found=0,ok=0;
    USED(stop);
    if(p->code!=nil || p->reason!=nil || p->sites!=nil || p->external!=nil)return typedfail(p,"nonempty-output");
    if(nf<1 || nf>4096 || !count || count>TypedInstructions || functions==nil || input==nil)return typedfail(p,"multi-limit");
    c.external=external;c.globals=globals;c.functions=functions;c.nf=nf;c.count=count;
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
