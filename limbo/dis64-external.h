/* Fixed-arity imported calls in trusted final compiler IR. No load/file admission. */
/* One native-only mixed result: Sys.fstat -> (int, Sys.Dir).
 * Prove every scalar/root leaf against both the declaration and caller frame.
 * No aggregate return forwarding or general tuple-call ABI is admitted. */
static int
typedstatresult(Decl *fn,Inst *binding,Dis64TypedBody *p,unsigned int *base,int relocate)
{
 static const unsigned int old[]={0,8,12,16,20,24,32,36,40,44,48,56,64,68};
 static const unsigned int native[]={0,8,16,24,32,40,48,52,56,60,64,72,80,84};
 static const int kinds[]={Tint,Tstring,Tstring,Tstring,Tstring,Tbig,Tint,Tint,Tint,Tint,Tint,Tbig,Tint,Tint};
 Dis64Layout result={0};Dis64LayoutLeaf *leaf,*cell;Type *t,*fd;Decl *field,*formal;unsigned int i,j,first=0,end,delta;int ok=0;
 if(!(formal=fn->ty->ids) || formal->next || !formal->ty || formal->ty->kind!=Tref || !(fd=formal->ty->tof) || fd->kind!=Tadt ||
    !fd->decl || !fd->decl->sym || strcmp(fd->decl->sym->name,"FD") || !fd->decl->dot || !fd->decl->dot->sym || strcmp(fd->decl->dot->sym->name,"Sys") ||
    !fd->ids || fd->ids->next || !fd->ids->ty || fd->ids->ty->kind!=Tint || fd->ids->offset || fd->size!=4)return 0;
 if(!fn->sym || strcmp(fn->sym->name,"fstat") || !fn->dot || !fn->dot->sym || strcmp(fn->dot->sym->name,"Sys") ||
    !(t=fn->ty->tof) || t->kind!=Ttuple || !(field=t->ids) || !field->ty || field->ty->kind!=Tint ||
    !(field=field->next) || field->next || !field->ty || field->ty->kind!=Tadt || !field->ty->decl ||
    !field->ty->decl->sym || strcmp(field->ty->decl->sym->name,"Dir") || !field->ty->decl->dot ||
    !field->ty->decl->dot->sym || strcmp(field->ty->decl->dot->sym->name,"Sys") ||
    binding->sm!=Afp || binding->s.reg<32 || binding->s.reg>Dis64LayoutBytes-72 || !dis64layouttype(t,&result) || result.count!=14 || result.bytes!=88 || result.legacybytes!=72)goto done;
 for(first=0;first<p->frame.count;first++)if(p->frame.leaves[first].legacy==(unsigned int)binding->s.reg)break;
 if(first>p->frame.count || 14>p->frame.count-first)goto done;
 *base=(p->frame.leaves[first].native+7)&~7U;
 for(i=0;i<14;i++){
  leaf=result.leaves+i;cell=p->frame.leaves+first+i;
  if(leaf->legacy!=old[i] || leaf->native!=native[i] || leaf->kind!=kinds[i] ||
     leaf->oldbytes!=(kinds[i]==Tbig?8:4) || leaf->bytes!=(kinds[i]==Tint?4:8) ||
     cell->legacy!=binding->s.reg+old[i] || cell->kind!=kinds[i] || cell->oldbytes!=leaf->oldbytes || cell->bytes!=leaf->bytes ||
     (!relocate && cell->native!=*base+native[i]))goto done;
 }
 end=p->frame.leaves[first+13].native+4;
 if(*base>Dis64LayoutBytes-88 || end>*base+88)goto done;
 delta=*base+88-end;
 if(relocate){
  if(delta>Dis64LayoutBytes-p->frame.bytes)goto done;
  for(i=0;i<14;i++)p->frame.leaves[first+i].native=*base+native[i];
  for(j=first+14;j<p->frame.count;j++)p->frame.leaves[j].native+=delta;
  p->frame.bytes+=delta;
 }
 if(*base%8 || *base>p->frame.bytes || 88>p->frame.bytes-*base)goto done;
 ok=1;
 done:dis64layoutfree(&result);return ok;
}

/* Repack only exact fstat caller-result groups before translating any body
 * instruction. Destructured locals can otherwise drop the tuple's status pad.
 * Legacy bytes/descriptors stay unchanged; all native users share this plan. */
static int
typedstatframe(TypedMulti *c,Decl *owner,Dis64TypedBody *p,unsigned int stop)
{
 unsigned int pc,end,base;Inst *in,*binding,*call;Decl *arg,*fn;
 for(pc=p->first;pc<stop;pc++){
  in=c->instructions[pc];if(!in || in->op!=IMFRAME || !(fn=in->m.decl) || !fn->sym || strcmp(fn->sym->name,"fstat") ||
     !fn->dot || !fn->dot->sym || strcmp(fn->dot->sym->name,"Sys"))continue;
  end=pc+1;while(end<stop && c->instructions[end] && c->instructions[end]->dm==Afpind)end++;
  if(end<=pc+1 || end>=stop || !(call=c->instructions[end]) || call->op!=IMCALL || call->m.decl!=fn ||
     !(binding=c->instructions[end-1]) || binding->op!=ILEA || binding->sm!=Afp || binding->s.reg<32 || binding->s.reg>Dis64LayoutBytes-72 ||
     !fn->ty || fn->ty->kind!=Tfn || !fn->ty->tof)return typedfail(p,"external-result-kind");
  for(arg=owner->ty->ids;arg;arg=arg->next)if(!arg->ty || (binding->s.reg<arg->offset+arg->ty->size && binding->s.reg+72>arg->offset))return typedfail(p,"external-result-kind");
  if(!typedstatresult(fn,binding,p,&base,1))return typedfail(p,"external-result-kind");
 }
 return 1;
}

/* Selected by-value Draw geometry: contiguous Word leaves only. No roots,
 * tag/pick payloads, general tuples, implicit widening or nested references. */
static int
typedwordgeometry(Type*t,Dis64Layout*layout)
{
 unsigned int i,n;Decl*d=t?t->decl:nil;
 if(!t || t->kind!=Tadt || !d || !d->sym || !d->dot || !d->dot->sym || strcmp(d->dot->sym->name,"Draw"))return 0;
 n=!strcmp(d->sym->name,"Point")?2:!strcmp(d->sym->name,"Rect")?4:0;
 if(!n || !dis64layouttype(t,layout) || layout->alignment!=4 || layout->legacybytes!=4*n || layout->bytes!=4*n || layout->count!=n)return 0;
 for(i=0;i<n;i++)if(layout->leaves[i].legacy!=4*i || layout->leaves[i].native!=4*i || layout->leaves[i].oldbytes!=4 || layout->leaves[i].bytes!=4 || layout->leaves[i].kind!=Tint)return 0;
 return 1;
}

static int
typedexternalgeometry(TypedMulti*c,Dis64TypedBody*p,Decl*arg,Inst*frame,unsigned int end,unsigned int*at,unsigned int*offset)
{
 Dis64Layout layout={0};Inst*in;Addr sourceaddr;long source=0,part;unsigned int i,sm=0,wire,base;int actual,ok=0;
 if(!typedwordgeometry(arg->ty,&layout))goto done;
 base=(*offset+3)&~3U;if(base>65535-layout.bytes)goto done;
 in=c->instructions[*at];
 if(in->op==IMOVM){
  if(in->mm!=Aimm || in->m.offset!=layout.bytes || in->branch || in->dm!=Afpind || in->d.reg!=frame->d.reg || in->d.offset!=arg->offset ||
     (in->sm!=Afp && in->sm!=Amp) || !in->s.decl || in->s.decl->ty!=arg->ty)goto done;
  for(i=0;i<layout.count;i++){
   sourceaddr=in->s;sourceaddr.reg+=4*i;sourceaddr.offset=0;
   if(!typedoperand(in->sm,&sourceaddr,Tint,0,0,p,&part,&wire,&actual) || actual!=Tint || (i && (wire!=sm || part!=source+4*i)))goto done;
   if(!i){source=part;sm=wire;}
  }
  p->code[*at-p->first]=(Dis64Instruction){IMOVM,SRC(sm)|AXIMM|DST(AFP|AIND),layout.bytes,source,base};p->returns[(*at)++-p->first]=64;
 }else{
  for(i=0;i<layout.count;i++){
   if(*at>=end || !(in=c->instructions[*at]) || in->op!=IMOVW || in->mm!=Anone || in->branch || in->dm!=Afpind ||
      in->d.reg!=frame->d.reg || in->d.offset!=arg->offset+4*i || !typedoperand(in->sm,&in->s,Tint,1,0,p,&source,&sm,&actual))goto done;
   p->code[*at-p->first]=(Dis64Instruction){IMOVW,SRC(sm)|DST(AFP|AIND),0,source,base+4*i};p->returns[(*at)++-p->first]=64;
  }
 }
 *offset=base+layout.bytes;ok=1;
 done:dis64layoutfree(&layout);return ok?1:typedfail(p,"external-geometry-argument");
}

static int typedexternalstringlist(TypedMulti*,Dis64TypedBody*,Inst*,unsigned int,unsigned int*);

static int
typedexternalsite(TypedMulti *c,Dis64TypedBody *p,unsigned int pc,unsigned int stop,unsigned int *next)
{
    Inst *frame=c->instructions[pc],*call,*in,*binding,*listinit=nil;Decl *fn,*arg;
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
    while(end<stop && c->instructions[end]!=nil && (c->instructions[end]->dm==Afpind ||
      (c->loads && c->instructions[end]->dm==Afp && (c->instructions[end]->op==IMOVP || c->instructions[end]->op==ICONSP))))end++;
    if(end>=stop || (call=c->instructions[end])==nil || call->op!=IMCALL || call->sm!=Afp || call->s.reg!=frame->d.reg ||
       call->mm!=Aoff || call->m.decl!=fn || call->dm!=frame->sm || call->d.reg!=frame->s.reg ||
       call->d.decl!=frame->s.decl || call->branch!=nil)return typedfail(p,"external-call");
    mo=module;p->code[pc-p->first]=(Dis64Instruction){IMFRAME,SRC(ms)|AXIMM|DST(AFP),fn->iface->offset,mo,40};at=pc+1;
    for(arg=fn->ty->ids;arg!=nil;arg=arg->next){
        if(at>=end || (in=c->instructions[at])==nil)return typedfail(p,"external-argument");
        listinit=nil;kind=arg->ty->kind;
        if(c->loads && kind==Tadt){if(!typedexternalgeometry(c,p,arg,frame,end,&at,&offset))return 0;continue;}
        root=kind==Tstring || kind==Tref || kind==Tarray || kind==Tlist || kind==Tchan || kind==Tmodule;
        width=root || kind==Tbig || kind==Treal?8:kind==Tbyte?1:4;
        op=root?IMOVP:kind==Tbyte?IMOVB:kind==Tbig?IMOVL:kind==Treal?IMOVF:IMOVW;
        offset=(offset+width-1)&~(width-1);
        if(c->loads && kind==Tlist && arg->ty->tof && arg->ty->tof->kind==Tstring && in->op==IMOVP && in->dm==Afp){
            listinit=in;
            if(!typedexternalstringlist(c,p,frame,end,&at) || at>=end || !(in=c->instructions[at]))return typedfail(p,"external-list-argument");
        }
        if(c->loads && kind==Tint && in->op==ILENA){
            if(in->mm!=Anone || in->branch || in->dm!=Afpind || in->d.reg!=frame->d.reg || in->d.offset!=arg->offset ||
               !typedbytearrayaddr(&in->s) || !typedoperand(in->sm,&in->s,Tarray,0,0,p,&source,&sm,&actual) || offset>65535-4)
                return typedfail(p,"external-byte-length-argument");
            p->code[at-p->first]=(Dis64Instruction){ILENA,SRC(sm)|DST(AFP|AIND),0,source,offset};p->returns[at++-p->first]=64;offset+=4;continue;
        }
        if(c->loads && kind==Tstring && in->op==IADDC){
            unsigned int mm;long middle;
            if(in->dm!=Afpind || in->d.reg!=frame->d.reg || in->d.offset!=arg->offset || in->branch ||
               !typedoperand(in->sm,&in->s,Tstring,0,0,p,&source,&sm,&actual) || actual!=Tstring ||
               !typedoperand(in->mm,&in->m,Tstring,0,1,p,&middle,&mm,&actual) || actual!=Tstring || offset>65535-8)
                return typedfail(p,"external-concat-argument");
            p->code[at-p->first]=(Dis64Instruction){IADDC,SRC(sm)|(mm==AFP?AXINF:AXINM)|DST(AFP|AIND),middle,source,offset};
            p->returns[at++-p->first]=64;offset+=8;continue;
        }
        if((!root && kind!=Tbyte && kind!=Tint && kind!=Tfix && kind!=Tbig && kind!=Treal) || offset>65535-width ||
           in->op!=op || in->mm!=Anone || in->dm!=Afpind || in->d.reg!=frame->d.reg || in->d.offset!=arg->offset || in->branch!=nil ||
           !typedoperand(in->sm,&in->s,root?TypedRoot:kind,!root && width<8,0,p,&source,&sm,&actual) ||
           (root && actual!=kind && actual!=Tany))return typedfail(p,"external-argument");
        p->code[at-p->first]=(Dis64Instruction){op,SRC(sm)|DST(AFP|AIND),0,source,offset};p->returns[at++-p->first]=64;offset+=width;
        if(listinit){
            if(at>=end || !(in=c->instructions[at]) || in->op!=IMOVP || in->sm!=Amp || in->mm!=Anone || in->dm!=Afp || in->branch ||
               in->s.decl!=listinit->s.decl || in->s.reg!=listinit->s.reg || in->s.offset ||
               in->d.decl!=listinit->d.decl || in->d.reg!=listinit->d.reg || in->d.offset ||
               !typedinst(in,stop,p,p->code+at-p->first,p->returns+at-p->first))return typedfail(p,"external-list-clear");
            p->returns[at++-p->first]=112;
        }
    }
    kind=fn->ty->tof->kind;root=kind==Tstring || kind==Tref || kind==Tarray || kind==Tlist || kind==Tchan || kind==Tmodule;
    if(kind!=Tnone){
        if(!root && kind!=Tbyte && kind!=Tint && kind!=Tfix && kind!=Tbig && kind!=Treal && !(c->loads && kind==Ttuple))return typedfail(p,"external-result-kind");
        contract=(root || kind==Tbig || kind==Treal?8:kind==Tbyte?1:4)|(root?16:0);
        if(at+1!=end || (binding=c->instructions[at])==nil || binding->op!=ILEA || binding->mm!=Anone ||
           binding->dm!=Afpind || binding->d.reg!=frame->d.reg || binding->d.offset!=16 || binding->branch!=nil)return typedfail(p,"external-result");
        if(c->loads && kind==Ttuple){
            if(!typedstatresult(fn,binding,p,&local,0))return typedfail(p,"external-result-kind");
            contract=88;p->code[at-p->first]=(Dis64Instruction){ILEA,SRC(AFP)|DST(AFP|AIND),0,local,32};p->returns[at-p->first]=128|contract;
        }else if(binding->sm==Afp){
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
