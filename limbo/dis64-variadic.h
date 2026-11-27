static int typedwindowowner(Addr*);
static int typedwindowfield(Inst*,Dis64TypedBody*,unsigned int,int,unsigned int*);
static int typedwmcontextaddress(Addr*,int,Dis64TypedBody*,long*,unsigned int*,long*);
static int typedloadlisttype(Addr*);
static Type *typedrecordvalue(Addr*,int);

/* Retain a record field's owner for the adjacent Word read and release it
 * before dispatching to the formatter. */
static int
typedvariadicrecordword(TypedMulti *c, Dis64TypedBody *p, Inst *frame, Decl *arg,
                       unsigned int end, unsigned int *at, unsigned int native)
{
 Inst *read=c->instructions[*at],*word,clear;
 Type *field=typedrecordvalue(&read->s,read->sm),*owner;
 long source,temp;
 unsigned int sm,dm,pc=*at-p->first,start=*at;
 int actual;
 if(end-start<3||read->mm!=Anone||read->branch||!field||field->kind!=Tref||read->d.offset)
  return typedfail(p,"variadic-record-word-owner");
 word=c->instructions[start+1];owner=typedfieldowner(&word->s);
 if(word->sm!=Afpind||word->s.reg!=read->d.reg||word->s.decl!=read->d.decl||!owner||!tequal(owner,field)||
    !typedrecordlayoutoperand(read->sm,&read->s,1,p,&source,&sm)||
    !typedrecordregister(p,pc,0,typedfieldowner(&read->s)->tof)||
    !typedoperand(Afp,&read->d,TypedRoot,0,0,p,&temp,&dm,&actual)||actual!=Tref||
    !typedrecordargument(word,frame,p,arg->ty,pc+1,arg->offset,native,1))
  return typedfail(p,"variadic-record-word-field");
 p->code[pc]=(Dis64Instruction){IMOVP,SRC(sm)|DST(dm),0,source,temp};p->returns[pc]=Native64TypedRecord;
 clear=*word;clear.sm=Afp;clear.s=read->d;
 *at=start+2;
 if(!typedrootargclear(c,p,&clear,end,at)||*at!=start+3)return typedfail(p,"variadic-record-word-clear");
 p->returns[pc+2]=223;
 return 1;
}

/* Trusted checked LOAD profile only. Actual instantiated arguments come from
 * the compiler FRAME annotation, never from format text or descriptor bits. */
/* Only nominal Wmsrv.Window.tag in a typed String tail, not a format/FD. */
static int
typedvariadicwindowtag(Inst*in,Dis64TypedBody*p,unsigned int pc,unsigned int offset)
{
 long base;unsigned int wire;
 if(!in->s.native64type || in->s.native64type->kind!=Tstring || in->s.offset ||
    !typedwmsrvwindowbase(&in->s,in->sm,p,&base,&wire))return typedfail(p,"variadic-window-tag");
 p->code[pc]=(Dis64Instruction){IMOVP,SRC(wire)|DST(AFP|AIND),0,base,offset};p->returns[pc]=222;return 1;
}

/* The exact Window.ctxt -> Wmcontext.connfd prefix and compiler nil clear. */
static int
typedvariadicfd(TypedMulti*c,Dis64TypedBody*p,Inst*frame,unsigned int*at,unsigned int end,unsigned int old)
{Inst*a,*b,*z;long base,temp,field;unsigned int wire;int actual;if(*at+2>=end)return 0;a=c->instructions[*at];b=c->instructions[*at+1];z=c->instructions[*at+2];
 if(a->op!=IMOVP||a->sm!=Afpind||a->s.offset!=40||!typedwindowowner(&a->s)||a->dm!=Afp||a->mm!=Anone||a->branch||!typedwmref(a->s.native64type,"Draw","Wmcontext")||!typedwindowfield(a,p,*at-p->first,0,nil))return typedfail(p,"variadic-fd-owner");
 temp=p->code[*at-p->first].destination;p->code[*at-p->first].middle=48;p->returns[*at-p->first]=59;
 if(b->op!=IMOVP||b->sm!=Afpind||b->s.reg!=a->d.reg||b->s.decl!=a->d.decl||b->s.offset!=20||!typedwmref(b->s.native64type,"Sys","FD")||b->dm!=Afpind||b->d.reg!=frame->d.reg||b->d.offset!=old||b->mm!=Anone||b->branch||!typedwmcontextaddress(&b->s,b->sm,p,&base,&wire,&field)||base!=temp)return typedfail(p,"variadic-fd-field");
 p->code[*at+1-p->first]=(Dis64Instruction){IMOVP,SRC(wire)|DST(AFP|AIND),40,base,64};p->returns[*at+1-p->first]=59;
 if(z->op!=IMOVP||z->sm!=Amp||z->dm!=Afp||z->d.reg!=a->d.reg||z->d.decl!=a->d.decl||z->mm!=Anone||z->branch||!z->s.decl||!z->s.decl->ty||z->s.decl->ty->kind!=Tany||!typedoperand(Amp,&z->s,TypedRoot,0,0,p,&base,&wire,&actual)||!typedinst(z,end,p,p->code+*at+2-p->first,p->returns+*at+2-p->first))return typedfail(p,"variadic-fd-clear");
 p->returns[*at+2-p->first]=59;*at+=3;return 1;}

/* Exact private Client.dir String expression in one variadic tail slot. */
static int
typedvariadicclientexpr(TypedMulti*c,Dis64TypedBody*p,Inst*frame,unsigned int*at,unsigned int end,unsigned int old,unsigned int native)
{Inst*a=c->instructions[*at],*b,*z;long source,middle;unsigned int sm,mm;int actual;
 if(*at+2>=end||a->op!=IMOVP||a->sm!=Afpind||typedlibraryrecord(typedfieldowner(&a->s))!=12||a->s.offset!=16||a->dm!=Afp||a->mm!=Anone||a->branch||!a->d.decl||a->d.decl->store!=Dlocal||!a->d.decl->sym||a->d.decl->sym->name[0]!='.'||a->d.decl->sym->name[1]!='t'||a->d.offset||!typedlibraryinst(a,p,*at-p->first,end))return typedfail(p,"variadic-client-string-owner");
 b=c->instructions[*at+1];z=c->instructions[*at+2];
 if(b->op!=IADDC||b->sm!=Amp||b->mm!=Afp||b->m.reg!=a->d.reg||b->m.decl!=a->d.decl||b->m.offset||b->dm!=Afpind||b->d.reg!=frame->d.reg||b->d.offset!=old||b->branch||!typedoperand(Amp,&b->s,Tstring,0,0,p,&source,&sm,&actual)||actual!=Tstring||!typedoperand(Afp,&b->m,Tstring,0,1,p,&middle,&mm,&actual)||actual!=Tstring)return typedfail(p,"variadic-client-string-value");
 p->code[*at+1-p->first]=(Dis64Instruction){IADDC,SRC(AMP)|AXINF|DST(AFP|AIND),middle,source,native};p->returns[*at+1-p->first]=57;
 if(z->op!=IMOVP||z->sm!=Amp||z->dm!=Afp||z->d.reg!=a->d.reg||z->d.decl!=a->d.decl||z->d.offset||z->mm!=Anone||z->branch||!z->s.decl||!z->s.decl->ty||z->s.decl->ty->kind!=Tany||!z->s.decl->sym||strcmp(z->s.decl->sym->name,"nil")||!typedinst(z,end,p,p->code+*at+2-p->first,p->returns+*at+2-p->first))return typedfail(p,"variadic-client-string-clear");p->returns[*at+2-p->first]=223;*at+=3;return 1;}
static int
typedvariadicsite(TypedMulti*c,Dis64TypedBody*p,unsigned int pc,unsigned int stop,unsigned int*next)
{
 Inst *frame=c->instructions[pc],*call,*in,*bind,*later;
 Type *actual=frame->native64args;Decl *fn,*arg,*fixed,*copies;Dis64TypedVariadic*v;
 unsigned int end=pc+1,at,sm,mm,off,n=0,op,contract,offset,fdold=0,i,oldend=32,width,store,root;long source,module;int kind;
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
 else if(!strcmp(fn->sym->name,"aprint") && (unsigned int)sign(fn)==0x77442d46U && fn->ty->tof->kind==Tarray && fn->ty->tof->tof && fn->ty->tof->tof->kind==Tbyte){op=1;contract=24;}
 else if(!strcmp(fn->sym->name,"print") && (unsigned int)sign(fn)==0xac849033U && fn->ty->tof->kind==Tint){op=2;contract=4;}
 else if(!strcmp(fn->sym->name,"fprint") && (unsigned int)sign(fn)==0xf46486c8U && fn->ty->tof->kind==Tint){op=3;contract=4;}
 else return typedfail(p,"variadic-signature");
 if(!actual->tof || actual->tof->kind!=fn->ty->tof->kind || !typedoperand(call->dm,&call->d,TypedRoot,0,0,p,&module,&mm,&kind) || kind!=Tmodule)
    return typedfail(p,"variadic-module");
 if(p->nvariadic==256)return typedfail(p,"variadic-limit");
 fixed=fn->ty->ids;
 if(!fixed || !fixed->ty || fixed->ty->kind!=(op==3?Tref:Tstring) ||
    (op==3?(!fixed->next || !fixed->next->ty || fixed->next->ty->kind!=Tstring || fixed->next->next):fixed->next!=nil))return typedfail(p,"variadic-prefix");
 v=p->variadic+p->nvariadic++;memset(v,0,sizeof(*v));
 for(arg=actual->ids,fixed=fn->ty->ids;arg;arg=arg->next){
    if(n>=258 || !arg->ty || arg->ty->size!=(arg->ty->kind==Tbig || arg->ty->kind==Treal?8:4) || arg->ty->align<1 || arg->ty->align>8 || (arg->ty->align&(arg->ty->align-1)) ||
       arg->offset!=((oldend+arg->ty->align-1)&~(arg->ty->align-1)) ||
       (arg->ty->kind!=Tstring && arg->ty->kind!=Tint && arg->ty->kind!=Tfix && arg->ty->kind!=Tbig && arg->ty->kind!=Treal && !(op==3 && n==0 && arg->ty->kind==Tref)) ||
       (fixed && arg->ty!=fixed->ty))return typedfail(p,"variadic-argument-kind");
    oldend=arg->offset+arg->ty->size;n++;if(fixed)fixed=fixed->next;
 }
 if(fixed || n<(op==3?2:1) || n-(op==3?2:1)>256)return typedfail(p,"variadic-arguments");
 copies=allocmem(n*sizeof(*copies));
 for(arg=actual->ids,i=0;arg;arg=arg->next,i++){copies[i]=*arg;copies[i].store=Darg;copies[i].link=nil;copies[i].next=i+1<n?copies+i+1:nil;}
 i=dis64layoutstorage(copies,nil,frame->s.decl->desc,1,&v->frame);free(copies);
 if(!i || v->frame.count!=n)return typedfail(p,"variadic-layout");
 /* The existing private typed formatter ABI uses one eight-byte tail slot,
  * including sign-extended Word/Fix. Ordinary frame layouts stay unchanged. */
 for(arg=actual->ids,i=0;arg;arg=arg->next,i++){
    Dis64LayoutLeaf*leaf=v->frame.leaves+i;
    if(leaf->legacy!=arg->offset || leaf->oldbytes!=arg->ty->size || leaf->kind!=arg->ty->kind)return typedfail(p,"variadic-layout");
    leaf->native=64+8*i;leaf->bytes=8;
 }
 v->frame.bytes=64+8*n;v->frame.alignment=8;
 off=v->frame.bytes+v->frame.count*sizeof(Dis64LayoutLeaf);
 if(off>32*1024*1024-c->cost)return typedfail(p,"variadic-storage-budget");c->cost+=off;
 p->code[pc-p->first]=(Dis64Instruction){IFRAME,SRC(AIMM)|DST(AFP),0,frame->s.offset,40};
 at=pc+1;
 for(arg=actual->ids,i=0;arg;arg=arg->next,i++){
    if(at>=end || !(in=c->instructions[at]))return typedfail(p,"variadic-setup");
    if(typedrecordargument(in,frame,p,arg->ty,at-p->first,arg->offset,64+8*i,1)){
        at++;continue;
    }
    if(op==3&&i==0&&(in->sm==Afpind||in->sm==Ampind)&&in->dm==Afpind){
     unsigned int id=typedlibraryrecord(typedfieldowner(&in->s));
     if(!((id==8&&in->s.offset==20)||(id==12&&(in->s.offset==20||in->s.offset==24)))||!typedwmref(in->s.native64type,"Sys","FD")||!typedlibraryargument(in,frame,p,at-p->first,arg->offset,64,Tref))return typedfail(p,"variadic-direct-fd");
     p->returns[at++-p->first]=53;continue;
    }
    if(op==3&&i==0&&in->dm==Afp){if(!typedvariadicfd(c,p,frame,&at,end,arg->offset))return 0;continue;}
    if(arg->ty->kind==Tstring&&i>=(op==3?2:1)&&in->op==IHEADP){
     if(in->mm!=Anone||in->branch||in->dm!=Afpind||in->d.reg!=frame->d.reg||in->d.offset!=arg->offset||!typedloadlisttype(&in->s)||!typedoperand(in->sm,&in->s,Tlist,0,0,p,&source,&sm,&kind)||kind!=Tlist)return typedfail(p,"variadic-string-head");
     p->code[at-p->first]=(Dis64Instruction){IHEADP,SRC(sm)|DST(AFP|AIND),0,source,64+8*i};p->returns[at++-p->first]=58;continue;
    }
    if(arg->ty->kind==Tstring&&i>=(op==3?2:1)&&in->op==IMOVP&&in->dm==Afp&&typedlibraryrecord(typedfieldowner(&in->s))==12){if(!typedvariadicclientexpr(c,p,frame,&at,end,arg->offset,64+8*i))return 0;continue;}
    root=arg->ty->kind==Tstring || arg->ty->kind==Tref;
    width=arg->ty->kind==Tbig || arg->ty->kind==Treal?8:4;
    store=root?IMOVP:arg->ty->kind==Treal?IMOVF:width==8?IMOVL:IMOVW;
    if(arg->ty->kind==Tstring && i>=(op==3?2:1) && (in->sm==Afpind || in->sm==Ampind)){
        if(in->op!=IMOVP || in->mm!=Anone || in->branch || in->dm!=Afpind || in->d.reg!=frame->d.reg || in->d.offset!=arg->offset || !typedvariadicwindowtag(in,p,at-p->first,64+8*i))return 0;
        at++;continue;
    }
    if(arg->ty->kind==Tint&&in->op==IMOVP&&in->dm==Afp&&(in->sm==Afpind||in->sm==Ampind)){
        if(!typedvariadicrecordword(c,p,frame,arg,end,&at,64+8*i))return 0;
        continue;
    }
    if(arg->ty->kind==Tint && i>=(op==3?2:1) && (in->sm==Afpind || in->sm==Ampind)){
        long field;int payload;
        if(in->op!=IMOVW || in->mm!=Anone || in->branch || in->dm!=Afpind || in->d.reg!=frame->d.reg || in->d.offset!=arg->offset ||
           !typedclientaddress(&in->s,in->sm,p,&source,&sm,&field,&payload) || payload!=Tint || (field!=32 && field!=72 && field!=76 && field!=80))return typedfail(p,"variadic-client-word");
        p->code[at-p->first]=(Dis64Instruction){ICVTWL,SRC(sm)|DST(AFP|AIND),field,source,64+8*i};p->returns[at++-p->first]=95;continue;
    }
    if(in->op!=store || in->mm!=Anone || in->branch || in->dm!=Afpind || in->d.reg!=frame->d.reg || in->d.offset!=arg->offset ||
       !typedoperand(in->sm,&in->s,root?TypedRoot:arg->ty->kind,!root && width==4,0,p,&source,&sm,&kind) ||
       (root && kind!=arg->ty->kind && kind!=Tany))return typedfail(p,"variadic-argument");
    if(op==3 && i==0 && in->sm==Afp)fdold=in->s.reg;
    p->code[at-p->first]=(Dis64Instruction){!root && width==4?ICVTWL:store,SRC(sm)|DST(AFP|AIND),0,source,64+8*i};p->returns[at++-p->first]=64;
    if(arg->ty->kind==Tstring){unsigned int before=at;if(!typedrootargclear(c,p,in,end,&at))return 0;if(at!=before)p->returns[at-1-p->first]=223;}
    if(op==3 && i==0 && at<end && (in=c->instructions[at]) && in->op==IMOVP && in->dm==Afp){
        if(!fdold || in->mm!=Anone || in->branch || in->sm!=Amp || in->d.reg!=fdold || in->d.offset ||
           !in->s.decl || !in->s.decl->ty || in->s.decl->ty->kind!=Tany || in->s.reg!=in->s.decl->offset || in->s.offset ||
           !typedinst(in,stop,p,p->code+at-p->first,p->returns+at-p->first))return typedfail(p,"variadic-cleanup");
        for(offset=at+1;offset<end;offset++){later=c->instructions[offset];if(later->sm==Afp && later->s.reg==fdold)return typedfail(p,"variadic-cleanup-live");}
        at++;
    }
 }
 if(at+1!=end || !(bind=c->instructions[at]) || bind->op!=ILEA || bind->mm!=Anone || bind->branch ||
    bind->dm!=Afpind || bind->d.reg!=frame->d.reg || bind->d.offset!=16)return typedfail(p,"variadic-result");
 if(bind->sm==Afp){
    if(!typedoperand(Afp,&bind->s,op==0?Tstring:op==1?TypedRoot:Tint,0,0,p,&source,&sm,&kind))return typedfail(p,"variadic-result");
    if(op==1 && (kind!=Tarray && !typedroottemporary(&bind->s,Tarray)))return typedfail(p,"variadic-array-result");
    p->code[at-p->first]=(Dis64Instruction){ILEA,SRC(AFP)|DST(AFP|AIND),0,source,32};
 }else{
    unsigned int bias=0;
    if(bind->sm!=Afpind || bind->s.reg!=16)return typedfail(p,"variadic-inherited-result");
    if(op==0&&((p->resultaggregate==48&&bind->s.offset==4)||(p->resultaggregate==80&&!bind->s.offset)||(p->resultaggregate==124&&(bind->s.offset==0||bind->s.offset==8))))bias=2*bind->s.offset;
    else if(bind->s.offset || p->resultkind!=(op==0?Tstring:op==1?Tarray:Tint))return typedfail(p,"variadic-inherited-result");
    v->inherited=1;source=32+bias;p->code[at-p->first]=(Dis64Instruction){IMOVL,SRC(AFP)|DST(AFP|AIND),bias,32,32};
 }
 p->returns[at-p->first]=128|contract;
 p->code[end-p->first]=(Dis64Instruction){IMCALL,SRC(AFP)|AXIMM|DST(mm),fn->iface->offset,40,module};
 p->returns[end-p->first]=v->inherited?(op==0&&(p->resultaggregate==48||p->resultaggregate==80||p->resultaggregate==124)?78:32):0;
 v->first=pc-p->first;v->end=end-p->first;v->binding=fn->iface->offset;v->modulemode=mm;v->moduleoffset=module;
 v->resultoffset=source;v->contract=contract;v->operation=op;v->count=n-(op==3?2:1);v->callee=fn;
 *next=end+1;return 1;
}
