static int typedchannelbinding(TypedMulti*,Dis64TypedBody*,Inst*,unsigned int*);
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

/* A Pointer is nominally distinct from Rect despite four raw Word leaves. */
static int
typedpointerlayout(Type*t,Dis64Layout*l)
{
 Decl*d=t?t->decl:nil,*f;Dis64Layout point={0};unsigned int i;int ok=0;
 if(!t || t->kind!=Tadt || !d || !d->sym || strcmp(d->sym->name,"Pointer") || !d->dot || !d->dot->sym || strcmp(d->dot->sym->name,"Draw"))return 0;
 f=t->ids;if(!f || f->store!=Dfield || f->offset || !f->sym || strcmp(f->sym->name,"buttons") || !f->ty || f->ty->kind!=Tint)goto done;
 f=f->next;if(!f || f->store!=Dfield || f->offset!=4 || !f->sym || strcmp(f->sym->name,"xy") || !typedwordgeometry(f->ty,&point) || point.bytes!=8)goto done;
 {Decl*q=f->ty->ids;if(!q || q->store!=Dfield || q->offset || !q->sym || strcmp(q->sym->name,"x") || !q->next || q->next->store!=Dfield || q->next->offset!=4 || !q->next->sym || strcmp(q->next->sym->name,"y"))goto done;for(q=q->next->next;q;q=q->next)if(!q->ty || q->ty->kind!=Tfn)goto done;}
 f=f->next;if(!f || f->store!=Dfield || f->offset!=12 || !f->sym || strcmp(f->sym->name,"msec") || !f->ty || f->ty->kind!=Tint || f->next)goto done;
 if(!dis64layouttype(t,l) || l->alignment!=4 || l->legacybytes!=16 || l->bytes!=16 || l->count!=4)goto done;
 for(i=0;i<4;i++)if(l->leaves[i].legacy!=4*i || l->leaves[i].native!=4*i || l->leaves[i].oldbytes!=4 || l->leaves[i].bytes!=4 || l->leaves[i].kind!=Tint)goto done;
 ok=1;
 done:dis64layoutfree(&point);return ok;
}

/* Only the canonical root-free Draw.Pointer receives this allocation role. */
static int
typedpointernew(Inst*in,Dis64TypedBody*p,Dis64Instruction*out)
{
 Decl*d=in->s.decl;Dis64Layout l={0};unsigned int mode;long dst;int actual,ok=0;
 if(in->sm!=Adesc || in->mm!=Anone || in->branch || !d || !d->desc || d->desc->id!=in->s.offset || d->desc->size!=16 || d->desc->nmap || !typedpointerlayout(d->ty,&l))goto done;
 if(!typedoperand(in->dm,&in->d,TypedRoot,0,0,p,&dst,&mode,&actual) || actual!=Tref || mode!=AFP)goto done;
 *out=(Dis64Instruction){in->op,SRC(AIMM)|DST(AFP),0,0,dst};ok=1;
 done:dis64layoutfree(&l);return ok?1:typedfail(p,"pointer-allocation-geometry");
}

static Type*typedfieldowner(Addr*);
/* Selected raw Pointer leaves, keeping the reference slot at native width. */
static int
typedpointeraddress(Addr*a,int mode,Dis64TypedBody*p,long*base,unsigned int*wire)
{
 Addr owner=*a;Dis64Layout l={0};Type*t=typedfieldowner(a);int actual,ok=0;
 if((mode!=Afpind && mode!=Ampind) || !t || t->kind!=Tref || !typedpointerlayout(t->tof,&l))goto done;
 owner.offset=0;owner.native64type=t;
 ok=typedoperand(mode==Afpind?Afp:Amp,&owner,TypedRoot,0,0,p,base,wire,&actual) && actual==Tref;
 done:dis64layoutfree(&l);return ok;
}
static int
typedpointerparse(Inst*in,Dis64TypedBody*p,unsigned int pc)
{
 long base,value;unsigned int wire,vm;int actual;
 if(in->op!=ICVTCW || in->mm!=Anone || in->branch || !in->d.native64type || in->d.native64type->kind!=Tint || in->d.offset<0 || in->d.offset>12 || in->d.offset%4 || !typedpointeraddress(&in->d,in->dm,p,&base,&wire) ||
    !typedoperand(in->sm,&in->s,TypedRoot,0,0,p,&value,&vm,&actual) || actual!=Tstring || (vm!=AFP && vm!=AMP))return typedfail(p,"pointer-word-parse");
 p->code[pc]=(Dis64Instruction){ICVTCW,SRC(vm)|DST(wire|AIND),in->d.offset,value,base};p->returns[pc]=244;return 1;
}
static int
typedpointerstore(Inst*in,Dis64TypedBody*p,unsigned int pc)
{
 long base,value;unsigned int wire,vm;int actual;
 if(in->op!=IMOVW || in->mm!=Anone || in->branch || !in->d.native64type || in->d.native64type->kind!=Tint || in->d.offset<0 || in->d.offset>12 || in->d.offset%4 || !typedpointeraddress(&in->d,in->dm,p,&base,&wire) ||
    !typedoperand(in->sm,&in->s,Tint,1,0,p,&value,&vm,&actual) || actual!=Tint || (vm!=AFP && vm!=AMP && vm!=AIMM))return typedfail(p,"pointer-word-store");
 p->code[pc]=(Dis64Instruction){IMOVW,SRC(vm)|DST(wire|AIND),in->d.offset,value,base};p->returns[pc]=243;return 1;
}
static int
typedpointerword(Inst*in,Dis64TypedBody*p,unsigned int pc,unsigned int stop)
{
 long base,value;unsigned int wire,vm;int actual,branch=in->op==IBEQW || in->op==IBNEW;
 if(!in->s.native64type || in->s.native64type->kind!=Tint || in->s.offset<0 || in->s.offset>12 || in->s.offset%4 || !typedpointeraddress(&in->s,in->sm,p,&base,&wire))return typedfail(p,"pointer-word-owner");
 if(branch){
  if(in->mm!=Aimm || in->m.offset || in->dm!=Apc || !in->branch || in->d.offset!=in->branch->pc || in->branch->pc<(int)p->first || in->branch->pc>=(int)stop)return typedfail(p,"pointer-word-branch");
  p->code[pc]=(Dis64Instruction){in->op,SRC(wire|AIND)|AXIMM|DST(AIMM),in->s.offset,base,in->branch->pc-p->first};p->returns[pc]=229;
 }else{
  if(in->op!=IMOVW || in->mm!=Anone || in->branch || !typedoperand(in->dm,&in->d,Tint,0,0,p,&value,&vm,&actual) || actual!=Tint || (vm!=AFP && vm!=AMP))return typedfail(p,"pointer-word-destination");
  p->code[pc]=(Dis64Instruction){IMOVW,SRC(wire|AIND)|DST(vm),in->s.offset,base,value};p->returns[pc]=228;
 }
 return 1;
}

/* Exact direct Draw.Point/Rect storage; inherited ADT results stay closed. */
static int
typedgeometrybinding(Inst*binding,Type*result,Dis64TypedBody*p,unsigned int*base,unsigned int*contract)
{
 Dis64Layout l={0};Addr a;Type*t;unsigned int i,wire;long part;int actual,ok=0;
 t=binding->s.native64type?binding->s.native64type:binding->s.decl?binding->s.decl->ty:nil;
 if((binding->sm!=Afp && binding->sm!=Amp) || (binding->sm==Amp && (!binding->s.decl || binding->s.decl->store!=Dglobal)) || t!=result || !typedwordgeometry(t,&l))goto done;
 for(i=0;i<l.bytes;i+=4){a=binding->s;a.reg+=i;a.offset=0;
  if(!typedoperand(binding->sm,&a,Tint,0,0,p,&part,&wire,&actual) || actual!=Tint || wire!=(binding->sm==Afp?AFP:AMP) || part<(binding->sm==Afp?64:0) || (i && part!=*base+i))goto done;
  if(!i)*base=part;
 }
 *contract=l.bytes==8?96:112;ok=1;
 done:dis64layoutfree(&l);return ok;
}

static int typedwmref(Type*,char*,char*);
static int typedwmsrvwindowbase(Addr*,int,Dis64TypedBody*,long*,unsigned int*);
static int typedrootargclear(TypedMulti*,Dis64TypedBody*,Inst*,unsigned int,unsigned int*);
static int typedimagegeometryprep(Inst*,Inst*,Dis64TypedBody*,unsigned int);
static int typedimagegeometry(Inst*,Dis64TypedBody*,unsigned int,unsigned int);

static int
typedexternalgeometry(TypedMulti*c,Dis64TypedBody*p,Decl*arg,Inst*frame,unsigned int end,unsigned int*at,unsigned int*offset)
{
 Dis64Layout layout={0};Inst*in;Addr sourceaddr;long source=0,part;unsigned int i,sm=0,wire,base;int actual,ok=0,prepared=0;
 if(!typedwordgeometry(arg->ty,&layout)){
  dis64layoutfree(&layout);
  if(!c->loads || !typedpointerlayout(arg->ty,&layout))goto done;
  base=(*offset+3)&~3U;if(base>65535-16)goto done;
  in=c->instructions[*at];sourceaddr=in->s;sourceaddr.offset=0;
  if(in->op!=IMOVM || (in->sm!=Afpind && in->sm!=Ampind) || in->mm!=Aimm || in->m.offset!=16 || in->branch || in->s.offset || in->dm!=Afpind || in->d.reg!=frame->d.reg || in->d.offset!=arg->offset ||
     !sourceaddr.decl || !sourceaddr.decl->ty || sourceaddr.decl->ty->kind!=Tref || sourceaddr.decl->ty->tof!=arg->ty ||
     !typedoperand(in->sm==Afpind?Afp:Amp,&sourceaddr,TypedRoot,0,0,p,&source,&wire,&actual) || actual!=Tref)goto done;
  p->code[*at-p->first]=(Dis64Instruction){IMOVM,SRC(wire|AIND)|AXIMM|DST(AFP|AIND),16,source,base};p->returns[(*at)++-p->first]=227;
  *offset=base+16;ok=1;goto done;
 }
 base=(*offset+3)&~3U;if(base>65535-layout.bytes)goto done;
 in=c->instructions[*at];
 if(c->loads && in->op==IMOVP && *at+1<end){
  if(!typedimagegeometryprep(in,c->instructions[*at+1],p,*at-p->first))goto done;
  in=c->instructions[++*at];prepared=1;
 }
 if(in->op==IMOVM && c->loads && (in->sm==Afpind || in->sm==Ampind)){
  if(in->mm!=Aimm || in->m.offset!=layout.bytes || in->branch || in->dm!=Afpind || in->d.reg!=frame->d.reg || in->d.offset!=arg->offset || !typedimagegeometry(in,p,*at-p->first,base))goto done;
  (*at)++;
  if(in->sm==Afpind){unsigned int clear=*at;Inst store=*in;store.op=IMOVP;store.sm=Afp;store.s.offset=0;if(!typedrootargclear(c,p,&store,end,at) || (prepared && clear==*at))goto done;}
 }else if(in->op==IMOVM){
  /* A selected Point can belong to a Rect declaration; prove the expression
   * type, then every contiguous relocated Tint leaf below. */
  if(in->mm!=Aimm || in->m.offset!=layout.bytes || in->branch || in->dm!=Afpind || in->d.reg!=frame->d.reg || in->d.offset!=arg->offset ||
     (in->sm!=Afp && in->sm!=Amp) || !in->s.decl || (in->s.native64type?in->s.native64type:in->s.decl->ty)!=arg->ty)goto done;
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

static int typedexternalheadarg(TypedMulti*,Dis64TypedBody*,Inst*,Decl*,unsigned int,unsigned int*,unsigned int);
static int typedexternalstringexpr(TypedMulti*,Dis64TypedBody*,Inst*,Decl*,unsigned int,unsigned int*,unsigned int);
static int typedselectedfieldarg(Inst*,Dis64TypedBody*,unsigned int,unsigned int,int);
static int typedloadlisttype(Addr*);
static int typedexternalstringprefix(TypedMulti*,Dis64TypedBody*,Inst*,unsigned int,unsigned int*);
static int typedexternalstringlist(TypedMulti*,Dis64TypedBody*,Inst*,unsigned int,unsigned int*);

static int
typedexternalsite(TypedMulti *c,Dis64TypedBody *p,unsigned int pc,unsigned int stop,unsigned int *next)
{
    Inst *frame=c->instructions[pc],*call,*in,*binding,*listinit=nil;Decl *fn,*arg;
    unsigned int end=pc+1,at,sm,offset=64,width,contract=0,local=0,inherit=0,ms,mo,listrole=112;
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
      (c->loads && c->instructions[end]->dm==Afp && (c->instructions[end]->op==ITAIL || c->instructions[end]->op==IMOVP || c->instructions[end]->op==ICONSP || c->instructions[end]->op==ICVTWC || c->instructions[end]->op==IADDC))))end++;
    if(end>=stop || (call=c->instructions[end])==nil || call->op!=IMCALL || call->sm!=Afp || call->s.reg!=frame->d.reg ||
       call->mm!=Aoff || call->m.decl!=fn || call->dm!=frame->sm || call->d.reg!=frame->s.reg ||
       call->d.decl!=frame->s.decl || call->branch!=nil)return typedfail(p,"external-call");
    mo=module;p->code[pc-p->first]=(Dis64Instruction){IMFRAME,SRC(ms)|AXIMM|DST(AFP),fn->iface->offset,mo,40};at=pc+1;
    for(arg=fn->ty->ids;arg!=nil;arg=arg->next){
        if(at>=end || (in=c->instructions[at])==nil)return typedfail(p,"external-argument");
        listinit=nil;listrole=112;kind=arg->ty->kind;
        if(c->loads && kind==Tadt){if(!typedexternalgeometry(c,p,arg,frame,end,&at,&offset))return 0;continue;}
        root=kind==Tstring || kind==Tref || kind==Tarray || kind==Tlist || kind==Tchan || kind==Tmodule;
        width=root || kind==Tbig || kind==Treal?8:kind==Tbyte?1:4;
        op=root?IMOVP:kind==Tbyte?IMOVB:kind==Tbig?IMOVL:kind==Treal?IMOVF:IMOVW;
        offset=(offset+width-1)&~(width-1);
        if(c->loads && kind==Tstring && in->op==ITAIL){
            if(offset>65535-8 || !typedexternalheadarg(c,p,frame,arg,end,&at,offset))return 0;
            offset+=8;continue;
        }
        if(c->loads && kind==Tstring && in->op==ICVTWC){
            if(offset>65535-8 || !typedexternalstringexpr(c,p,frame,arg,end,&at,offset))return 0;
            offset+=8;continue;
        }
        if(c->loads && kind==Tlist && arg->ty->tof && arg->ty->tof->kind==Tstring && in->op==IMOVP && in->dm==Afp){
            listinit=in;
            if(typedloadlisttype(&in->s))listrole=187;
            if(!(listrole==187?typedexternalstringprefix(c,p,frame,end,&at):typedexternalstringlist(c,p,frame,end,&at)) || at>=end || !(in=c->instructions[at]))return typedfail(p,"external-list-argument");
        }
        if(c->loads && root && (in->sm==Afpind || in->sm==Ampind)){
            if(in->dm!=Afpind || in->d.reg!=frame->d.reg || in->d.offset!=arg->offset || offset>65535-8 || !typedselectedfieldarg(in,p,at-p->first,offset,kind))return typedfail(p,"external-field-argument");
            at++;offset+=8;continue;
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
               (listrole==112?(in->s.decl!=listinit->s.decl || in->s.reg!=listinit->s.reg):(!in->s.decl || in->s.decl->store!=Dglobal || !in->s.decl->ty || in->s.decl->ty->kind!=Tany || !in->s.decl->sym || strcmp(in->s.decl->sym->name,"nil") || in->s.reg!=in->s.decl->offset)) || in->s.offset ||
               in->d.decl!=listinit->d.decl || in->d.reg!=listinit->d.reg || in->d.offset ||
               !typedinst(in,stop,p,p->code+at-p->first,p->returns+at-p->first))return typedfail(p,"external-list-clear");
            p->returns[at++-p->first]=listrole;
        }else if(c->loads && root && !typedrootargclear(c,p,c->instructions[at-1],end,&at))return 0;
    }
    kind=fn->ty->tof->kind;root=kind==Tstring || kind==Tref || kind==Tarray || kind==Tlist || kind==Tchan || kind==Tmodule;
    if(kind!=Tnone){
        if(!root && kind!=Tbyte && kind!=Tint && kind!=Tfix && kind!=Tbig && kind!=Treal && !(c->loads && (kind==Ttuple || kind==Tadt)))return typedfail(p,"external-result-kind");
        contract=(root || kind==Tbig || kind==Treal?8:kind==Tbyte?1:4)|(root?16:0);
        if(at+1!=end || (binding=c->instructions[at])==nil || binding->op!=ILEA || binding->mm!=Anone ||
           binding->dm!=Afpind || binding->d.reg!=frame->d.reg || binding->d.offset!=16 || binding->branch!=nil)return typedfail(p,"external-result");
        if(c->loads && kind==Tadt){
            if(!typedgeometrybinding(binding,fn->ty->tof,p,&local,&contract))return typedfail(p,"external-geometry-result");
            p->code[at-p->first]=(Dis64Instruction){ILEA,SRC(binding->sm==Afp?AFP:AMP)|DST(AFP|AIND),0,local,32};p->returns[at-p->first]=128|contract;
        }else if(c->loads && kind==Ttuple && typedchanneltriple(fn->ty->tof)){
            contract=72;
            if(binding->sm==Afp){if(!typedchannelbinding(c,p,binding,&local))return 0;p->code[at-p->first]=(Dis64Instruction){ILEA,SRC(AFP)|DST(AFP|AIND),0,local,32};p->returns[at-p->first]=128|contract;}
            else{if(binding->sm!=Afpind || binding->s.reg!=16 || binding->s.offset || !p->resulttriple)return typedfail(p,"channel-result-inherit");inherit=1;local=32;p->code[at-p->first]=(Dis64Instruction){IMOVL,SRC(AFP)|DST(AFP|AIND),0,32,32};p->returns[at-p->first]=64;}
        }else if(c->loads && kind==Ttuple){
            if(!typedstatresult(fn,binding,p,&local,0))return typedfail(p,"external-result-kind");
            contract=88;p->code[at-p->first]=(Dis64Instruction){ILEA,SRC(AFP)|DST(AFP|AIND),0,local,32};p->returns[at-p->first]=128|contract;
        }else if(c->loads && kind==Tint && binding->sm==Afpind && binding->s.reg!=16){
            unsigned int wire;long base;
            if(binding->s.offset!=12 || !binding->s.native64type || binding->s.native64type->kind!=Tint ||
               !typedpointeraddress(&binding->s,Afpind,p,&base,&wire) || wire!=AFP)return typedfail(p,"pointer-word-result");
            local=base;p->code[at-p->first]=(Dis64Instruction){ILEA,SRC(AFP|AIND)|DST(AFP|AIND),12,base,32};p->returns[at-p->first]=245;
        }else if(c->loads && root && binding->sm==Afpind && binding->s.reg!=16){
            unsigned int wire;long base;
            if(kind!=Tref || !typedwmref(fn->ty->tof,"Draw","Image") || binding->s.offset!=20 ||
               !typedwmsrvwindowbase(&binding->s,Afpind,p,&base,&wire) || wire!=(AFP|AIND))return typedfail(p,"window-image-result");
            local=base;p->code[at-p->first]=(Dis64Instruction){ILEA,SRC(AFP|AIND)|DST(AFP|AIND),24,base,32};p->returns[at-p->first]=220;
        }else if(binding->sm==Afp){
            if(!typedoperand(Afp,&binding->s,root?TypedRoot:kind,0,0,p,&source,&sm,&actual) ||
               (root && actual!=kind && actual!=Tany))return typedfail(p,"external-result");
            local=source;p->code[at-p->first]=(Dis64Instruction){ILEA,SRC(AFP)|DST(AFP|AIND),0,source,32};p->returns[at-p->first]=128|contract;
        }else if(c->loads && root && binding->sm==Amp){
            if(!binding->s.decl || binding->s.decl->store!=Dglobal || !typedoperand(Amp,&binding->s,TypedRoot,0,0,p,&source,&sm,&actual) || actual!=kind)return typedfail(p,"external-global-result");
            local=source;p->code[at-p->first]=(Dis64Instruction){ILEA,SRC(AMP)|DST(AFP|AIND),0,source,32};p->returns[at-p->first]=128|contract;
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
