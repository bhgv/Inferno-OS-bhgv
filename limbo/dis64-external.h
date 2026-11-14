/* Fixed-arity imported calls in trusted final compiler IR. No load/file admission. */
static int
typedexternalsite(TypedMulti *c,Dis64TypedBody *p,unsigned int pc,unsigned int stop,unsigned int *next)
{
    Inst *frame=c->instructions[pc],*call,*in,*binding;Decl *fn,*arg;
    unsigned int end=pc+1,at,sm,offset=64,width,contract=0,local=0,inherit=0,ms,mo;
    int kind,root,op,actual;long source,module;
    if(frame->mm!=Aoff || frame->m.decl==nil || frame->m.decl->iface==nil ||
       frame->m.decl->iface->offset<0 || frame->m.decl->iface->offset>32767 ||
       frame->dm!=Afp || frame->branch!=nil || frame->d.reg<32 || frame->d.reg>(int)p->frame.legacybytes-4 ||
       !typedoperand(frame->sm,&frame->s,TypedRoot,0,0,p,&module,&ms,&actual) || actual!=Tmodule)
        return typedfail(p,"external-frame");
    fn=frame->m.decl;
    if(fn->ty==nil || fn->ty->kind!=Tfn || fn->ty->varargs || fn->ty->polys!=nil || fn->ty->tof==nil)
        return typedfail(p,"external-signature");
    while(end<stop && c->instructions[end]!=nil && c->instructions[end]->dm==Afpind)end++;
    if(end>=stop || (call=c->instructions[end])==nil || call->op!=IMCALL || call->sm!=Afp || call->s.reg!=frame->d.reg ||
       call->mm!=Aoff || call->m.decl!=fn || call->dm!=frame->sm || call->d.reg!=frame->s.reg ||
       call->d.decl!=frame->s.decl || call->branch!=nil)return typedfail(p,"external-call");
    mo=module;p->code[pc-p->first]=(Dis64Instruction){IMFRAME,SRC(ms)|AXIMM|DST(AFP),fn->iface->offset,mo,40};at=pc+1;
    for(arg=fn->ty->ids;arg!=nil;arg=arg->next){
        if(at>=end || (in=c->instructions[at])==nil)return typedfail(p,"external-argument");
        kind=arg->ty->kind;root=kind==Tstring || kind==Tref || kind==Tarray || kind==Tlist || kind==Tchan || kind==Tmodule;
        width=root || kind==Tbig || kind==Treal?8:kind==Tbyte?1:4;
        op=root?IMOVP:kind==Tbyte?IMOVB:kind==Tbig?IMOVL:kind==Treal?IMOVF:IMOVW;
        offset=(offset+width-1)&~(width-1);
        if((!root && kind!=Tbyte && kind!=Tint && kind!=Tfix && kind!=Tbig && kind!=Treal) || offset>65535-width ||
           in->op!=op || in->mm!=Anone || in->dm!=Afpind || in->d.reg!=frame->d.reg || in->d.offset!=arg->offset || in->branch!=nil ||
           !typedoperand(in->sm,&in->s,root?TypedRoot:kind,!root && width<8,0,p,&source,&sm,&actual) ||
           (root && actual!=kind && actual!=Tany))return typedfail(p,"external-argument");
        p->code[at-p->first]=(Dis64Instruction){op,SRC(sm)|DST(AFP|AIND),0,source,offset};p->returns[at++-p->first]=64;offset+=width;
    }
    kind=fn->ty->tof->kind;root=kind==Tstring || kind==Tref || kind==Tarray || kind==Tlist || kind==Tchan || kind==Tmodule;
    if(kind!=Tnone){
        if(!root && kind!=Tbyte && kind!=Tint && kind!=Tfix && kind!=Tbig && kind!=Treal)return typedfail(p,"external-result-kind");
        contract=(root || kind==Tbig || kind==Treal?8:kind==Tbyte?1:4)|(root?16:0);
        if(at+1!=end || (binding=c->instructions[at])==nil || binding->op!=ILEA || binding->mm!=Anone ||
           binding->dm!=Afpind || binding->d.reg!=frame->d.reg || binding->d.offset!=16 || binding->branch!=nil)return typedfail(p,"external-result");
        if(binding->sm==Afp){
            if(!typedoperand(Afp,&binding->s,root?TypedRoot:kind,0,0,p,&source,&sm,&actual) ||
               (root && actual!=kind && actual!=Tany))return typedfail(p,"external-result");
            local=source;p->code[at-p->first]=(Dis64Instruction){ILEA,SRC(AFP)|DST(AFP|AIND),0,source,32};p->returns[at-p->first]=128|contract;
        }else{
            if(binding->sm!=Afpind || binding->s.reg!=16 || binding->s.offset || kind!=p->resultkind)return typedfail(p,"external-inherit");
            inherit=1;local=32;p->code[at-p->first]=(Dis64Instruction){IMOVL,SRC(AFP)|DST(AFP|AIND),0,32,32};p->returns[at-p->first]=64;
        }
        at++;
    }
    if(at!=end)return typedfail(p,"external-setup-length");
    p->code[end-p->first]=(Dis64Instruction){IMCALL,SRC(AFP)|AXIMM|DST(ms),fn->iface->offset,40,mo};p->returns[end-p->first]=inherit?32:0;
    p->external[p->nexternal++]=(Dis64TypedExternal){pc-p->first,end-p->first,fn->iface->offset,ms,mo,local,contract,inherit,fn};
    *next=end+1;return 1;
}
