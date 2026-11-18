/* Trusted checked LOAD profile only. Actual instantiated arguments come from
 * the compiler FRAME annotation, never from format text or descriptor bits. */
static int
typedvariadicsite(TypedMulti*c,Dis64TypedBody*p,unsigned int pc,unsigned int stop,unsigned int*next)
{
 Inst *frame=c->instructions[pc],*call,*in,*bind,*later;
 Type *actual=frame->native64args;Decl *fn,*arg,*fixed,*copies;Dis64TypedVariadic*v;
 unsigned int end=pc+1,at,sm,mm,off,n=0,op,contract,offset,fdold=0,i;long source,module;int kind;
 if(frame->sm!=Adesc || frame->mm!=Anone || frame->dm!=Afp || frame->branch ||
    !frame->s.decl || !frame->s.decl->desc || frame->s.decl->desc->id!=frame->s.offset ||
    frame->d.reg<32 || frame->d.reg>(int)p->frame.legacybytes-4 ||
    !actual || actual->kind!=Tfn || !actual->varargs || actual->polys)return typedfail(p,"variadic-frame");
 while(end<stop && (in=c->instructions[end]) && (in->dm==Afpind || (in->op==IMOVP && in->dm==Afp)))end++;
 if(end>=stop || !(call=c->instructions[end]) || call->op!=IMCALL || call->sm!=Afp || call->s.reg!=frame->d.reg ||
    call->mm!=Aoff || !(fn=call->m.decl) || !fn->iface || fn->iface->offset<0 || fn->iface->offset>32767 || call->branch ||
    !fn->ty || !fn->ty->tof || fn->ty->kind!=Tfn || !fn->ty->varargs || fn->ty->polys || !fn->sym || !fn->dot || !fn->dot->sym || strcmp(fn->dot->sym->name,"Sys"))
    return typedfail(p,"variadic-call");
 if(!strcmp(fn->sym->name,"sprint") && (unsigned int)sign(fn)==0x4c0624b6U && fn->ty->tof->kind==Tstring){op=0;contract=24;}
 else if(!strcmp(fn->sym->name,"fprint") && (unsigned int)sign(fn)==0xf46486c8U && fn->ty->tof->kind==Tint){op=3;contract=4;}
 else return typedfail(p,"variadic-signature");
 if(!actual->tof || actual->tof->kind!=fn->ty->tof->kind || !typedoperand(call->dm,&call->d,TypedRoot,0,0,p,&module,&mm,&kind) || kind!=Tmodule)
    return typedfail(p,"variadic-module");
 if(p->nvariadic==256)return typedfail(p,"variadic-limit");
 v=p->variadic+p->nvariadic++;memset(v,0,sizeof(*v));
 for(arg=actual->ids,fixed=fn->ty->ids;arg;arg=arg->next){
    if(n>=258 || !arg->ty || arg->offset!=32+4*n || (arg->ty->kind!=Tstring && !(op==3 && n==0 && arg->ty->kind==Tref)) ||
       (fixed && arg->ty!=fixed->ty))
        return typedfail(p,"variadic-argument-kind");
    n++;if(fixed)fixed=fixed->next;
 }
 if(fixed || n<(op==3?2:1) || n-(op==3?2:1)>256)return typedfail(p,"variadic-arguments");
 copies=allocmem(n*sizeof(*copies));
 for(arg=actual->ids,i=0;arg;arg=arg->next,i++){copies[i]=*arg;copies[i].store=Darg;copies[i].link=nil;copies[i].next=i+1<n?copies+i+1:nil;}
 i=dis64layoutstorage(copies,nil,frame->s.decl->desc,1,&v->frame);free(copies);
 if(!i || v->frame.bytes!=64+8*n)return typedfail(p,"variadic-layout");
 for(arg=actual->ids,i=0;arg;arg=arg->next,i++)if(!dis64layoutaddress(&v->frame,arg->offset,4,1,&off) || off!=64+8*i)return typedfail(p,"variadic-layout");
 off=v->frame.bytes+v->frame.count*sizeof(Dis64LayoutLeaf);
 if(off>32*1024*1024-c->cost)return typedfail(p,"variadic-storage-budget");c->cost+=off;
 p->code[pc-p->first]=(Dis64Instruction){IFRAME,SRC(AIMM)|DST(AFP),0,frame->s.offset,40};
 at=pc+1;
 for(arg=actual->ids,i=0;arg;arg=arg->next,i++){
    if(at>=end || !(in=c->instructions[at]))return typedfail(p,"variadic-setup");
    if(in->op!=IMOVP || in->mm!=Anone || in->branch || in->dm!=Afpind || in->d.reg!=frame->d.reg || in->d.offset!=arg->offset ||
       !typedoperand(in->sm,&in->s,TypedRoot,0,0,p,&source,&sm,&kind) || (kind!=arg->ty->kind && kind!=Tany))return typedfail(p,"variadic-argument");
    if(op==3 && i==0 && in->sm==Afp)fdold=in->s.reg;
    p->code[at-p->first]=(Dis64Instruction){IMOVP,SRC(sm)|DST(AFP|AIND),0,source,64+8*i};p->returns[at++-p->first]=64;
    if(op==3 && i==0 && at<end && (in=c->instructions[at]) && in->op==IMOVP && in->dm==Afp){
        if(!fdold || in->mm!=Anone || in->branch || in->sm!=Amp || in->d.reg!=fdold || in->d.offset ||
           !in->s.decl || !in->s.decl->ty || in->s.decl->ty->kind!=Tany || in->s.reg!=in->s.decl->offset || in->s.offset ||
           !typedinst(in,stop,p,p->code+at-p->first,p->returns+at-p->first))return typedfail(p,"variadic-cleanup");
        for(offset=at+1;offset<end;offset++){later=c->instructions[offset];if(later->sm==Afp && later->s.reg==fdold)return typedfail(p,"variadic-cleanup-live");}
        at++;
    }
 }
 if(at+1!=end || !(bind=c->instructions[at]) || bind->op!=ILEA || bind->mm!=Anone || bind->branch || bind->sm!=Afp ||
    bind->dm!=Afpind || bind->d.reg!=frame->d.reg || bind->d.offset!=16 ||
    !typedoperand(Afp,&bind->s,op==0?Tstring:Tint,0,0,p,&source,&sm,&kind))return typedfail(p,"variadic-result");
 p->code[at-p->first]=(Dis64Instruction){ILEA,SRC(AFP)|DST(AFP|AIND),0,source,32};p->returns[at-p->first]=128|contract;
 p->code[end-p->first]=(Dis64Instruction){IMCALL,SRC(AFP)|AXIMM|DST(mm),fn->iface->offset,40,module};
 v->first=pc-p->first;v->end=end-p->first;v->binding=fn->iface->offset;v->modulemode=mm;v->moduleoffset=module;
 v->resultoffset=source;v->contract=contract;v->operation=op;v->count=n-(op==3?2:1);v->callee=fn;
 *next=end+1;return 1;
}
