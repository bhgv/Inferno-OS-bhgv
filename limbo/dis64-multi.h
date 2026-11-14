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
           !typedoperand(in->sm,&in->s,root?TypedRoot:kind,!root && width<8,0,p,&source,&sm,&actual) ||
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
    if(i){c->state[index]=2;c->height[index]=1;return 1;}
    if(p->reason==nil || strcmp(p->reason,"unsupported-opcode"))goto fail;
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
        }else{
            if(!typedinst(in,stop,p,p->code+pc-p->first,p->returns+pc-p->first))goto fail;
            pc++;
        }
    }
    for(pc=p->first;pc<stop;pc++){
        in=c->instructions[pc];
        if(in->op!=ICALL && in->op!=IMCALL && in->branch!=nil && inside[in->branch->pc-p->first]){typedfail(p,"multi-setup-branch");goto fail;}
    }
    if((!p->nsites && !p->nexternal) || !typedresults(p))goto fail;
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
