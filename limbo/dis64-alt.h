/* Borrowed direct channel guards; every raw slot must belong to an exact Talt. */
static Type*
typedalttype(Addr*a,Decl*fn,unsigned int*base)
{
 Decl*d;for(d=fn->locals;d;d=d->next)if(d->ty && d->ty->kind==Talt && a->reg>=d->offset && a->reg-d->offset<d->ty->size){*base=d->offset;return d->ty;}return nil;
}
static int
typedaltslot(Addr*a,Decl*fn)
{unsigned int base;return typedalttype(a,fn,&base) && a->reg-base>=8;}
/* The selected join payload is exactly (ref, chan of string), not an arbitrary aggregate. */
static int
typedaltpair(Type*t)
{
 Decl*a;Dis64Layout layout={0};int ok=0;
 if(!t || t->kind!=Ttuple || !(a=t->ids) || a->store!=Dfield || a->offset || !a->ty || a->ty->kind!=Tref ||
    !(a=a->next) || a->next || a->store!=Dfield || a->offset!=4 || !a->ty || a->ty->kind!=Tchan || !a->ty->tof || a->ty->tof->kind!=Tstring)return 0;
 ok=dis64layouttype(t,&layout) && layout.legacybytes==8 && layout.bytes==16 && layout.alignment==8 && layout.count==2 &&
  layout.leaves[0].legacy==0 && layout.leaves[0].native==0 && layout.leaves[0].oldbytes==4 && layout.leaves[0].bytes==8 && layout.leaves[0].kind==Tref &&
  layout.leaves[1].legacy==4 && layout.leaves[1].native==8 && layout.leaves[1].oldbytes==4 && layout.leaves[1].bytes==8 && layout.leaves[1].kind==Tchan;
 dis64layoutfree(&layout);return ok;
}
/* Sys.Rread reply: byte array and String, two managed roots. */
static int
typedaltbytepair(Type*t)
{
 Decl*a;Dis64Layout l={0};int ok=0;
 if(!t || t->kind!=Ttuple || !(a=t->ids) || a->store!=Dfield || a->offset || !a->ty || a->ty->kind!=Tarray || !a->ty->tof || a->ty->tof->kind!=Tbyte || !(a=a->next) || a->next || a->store!=Dfield || a->offset!=4 || !a->ty || (a->ty->kind!=Tstring && a->ty->kind!=Tany))return 0;
 ok=dis64layouttype(t,&l) && l.legacybytes==8 && l.bytes==16 && l.alignment==8 && l.count==2 && l.leaves[0].native==0 && l.leaves[0].bytes==8 && l.leaves[1].native==8 && l.leaves[1].bytes==8;
 dis64layoutfree(&l);return ok;
}
/* Wmsrv request: ref Client, byte array and Sys.Rwrite channel. */
static int
typedaltrequest(Type*t)
{
 Decl*a,*r;Dis64Layout l={0};unsigned int i;int ok=0;
 if(!t || t->kind!=Ttuple || !(a=t->ids) || a->store!=Dfield || a->offset || !a->ty || a->ty->kind!=Tref ||
    !(a=a->next) || a->store!=Dfield || a->offset!=4 || !a->ty || a->ty->kind!=Tarray || !a->ty->tof || a->ty->tof->kind!=Tbyte ||
    !(a=a->next) || a->next || a->store!=Dfield || a->offset!=8 || !a->ty || a->ty->kind!=Tchan || !a->ty->tof || a->ty->tof->kind!=Ttuple ||
    !(r=a->ty->tof->ids) || r->store!=Dfield || r->offset || !r->ty || r->ty->kind!=Tint ||
    !(r=r->next) || r->next || r->store!=Dfield || r->offset!=4 || !r->ty || r->ty->kind!=Tstring)return 0;
 if(!dis64layouttype(t,&l) || l.legacybytes!=12 || l.bytes!=24 || l.alignment!=8 || l.count!=3)goto done;
 for(i=0;i<3;i++)if(l.leaves[i].legacy!=4*i || l.leaves[i].native!=8*i || l.leaves[i].oldbytes!=4 || l.leaves[i].bytes!=8 || l.leaves[i].kind!=(i==0?Tref:i==1?Tarray:Tchan))goto done;
 ok=1;
 done:dis64layoutfree(&l);return ok;
}
/* Exact Sys.FileIO read/write payloads, including their reply-channel types. */
static int
typedfileiopayload(Type*t)
{
 Decl*d,*r;Type*reply=nil;Dis64Layout l={0};unsigned int i,kind=0;int ok=0;
 if(!t || t->kind!=Ttuple)return 0;
 for(i=0,d=t->ids;d;d=d->next,i++){
  if(i>=4 || d->store!=Dfield || d->offset!=4*i || !d->ty)return 0;
  if(i==1){if(d->ty->kind==Tint)kind=1;else if(d->ty->kind==Tarray && d->ty->tof && d->ty->tof->kind==Tbyte)kind=2;else return 0;}
  else if(d->ty->kind!=(i==3?Tchan:Tint))return 0;
  if(i==3)reply=d->ty->tof;
 }
 if(i!=4 || !reply || reply->kind!=Ttuple || !(r=reply->ids) || r->store!=Dfield || r->offset || !r->ty ||
    (kind==1?(r->ty->kind!=Tarray || !r->ty->tof || r->ty->tof->kind!=Tbyte):r->ty->kind!=Tint) ||
    !(r=r->next) || r->next || r->store!=Dfield || r->offset!=4 || !r->ty || r->ty->kind!=Tstring)return 0;
 if(!dis64layouttype(t,&l) || l.legacybytes!=16 || l.bytes!=(kind==1?24:32) || l.alignment!=8 || l.count!=4)goto done;
 for(i=0;i<4;i++)if(l.leaves[i].legacy!=4*i || l.leaves[i].native!=(kind==1?(i==3?16:4*i):8*i) ||
   l.leaves[i].oldbytes!=4 || l.leaves[i].bytes!=((i==3 || (i==1 && kind==2))?8:4) || l.leaves[i].kind!=(i==3?Tchan:i==1&&kind==2?Tarray:Tint))goto done;
 ok=kind;
 done:dis64layoutfree(&l);return ok;
}
static int
typedfileiochannel(Addr*a,int mode,Dis64TypedBody*p,long*base,unsigned int*wire,long*field)
{
 Type*t=typedfieldowner(a);Decl*d;Addr owner;Dis64Layout l={0};unsigned int i;int actual,ok=0;
 if((mode!=Afpind && mode!=Ampind) || !typedwmref(t,"Sys","FileIO") || (a->offset!=0 && a->offset!=4) ||
    !dis64layouttype(t->tof,&l) || l.legacybytes!=8 || l.bytes!=16 || l.alignment!=8 || l.count!=2)goto done;
 for(i=0,d=t->tof->ids;i<2;i++,d=d->next)if(!d || d->store!=Dfield || !d->sym || strcmp(d->sym->name,i?"write":"read") || d->offset!=4*i ||
    !d->ty || d->ty->kind!=Tchan || typedfileiopayload(d->ty->tof)!=(i?2:1) || l.leaves[i].legacy!=4*i || l.leaves[i].native!=8*i || l.leaves[i].oldbytes!=4 || l.leaves[i].bytes!=8 || l.leaves[i].kind!=Tchan)goto done;
 if(d)goto done;owner=*a;owner.offset=0;owner.native64type=t;
 if(!typedoperand(mode==Afpind?Afp:Amp,&owner,TypedRoot,0,0,p,base,wire,&actual) || actual!=Tref)goto done;
 *wire|=AIND;*field=2*a->offset;ok=1;
 done:dis64layoutfree(&l);return ok;
}
static int
typedfileiospan(Dis64Layout*l,unsigned int old,unsigned int kind,unsigned int*base)
{
 unsigned int i,at,root,next;
 for(i=0;i<4;i++){
  root=i==3 || (kind==2 && i==1);at=kind==1?(i==3?16:4*i):8*i;
  if(!dis64layoutaddress(l,old+4*i,4,root,&next))return 0;
  if(!i)*base=next;else if(next!=*base+at)return 0;
 }
 return 1;
}
static int
typedfileiocopy(Inst*in,Dis64TypedBody*p,Dis64Instruction*out,uchar*returned)
{
 Decl*d=in->m.decl;unsigned int i,k,kind=d?typedfileiopayload(d->ty):0,off[2];Dis64Layout*l;Addr*a;
 if(!kind || in->op!=IMOVMP || in->mm!=Adesc || !d->desc || d->desc->id!=in->m.offset || d->desc->size!=16 ||
    d->desc->nmap!=1 || !d->desc->map || d->desc->map[0]!=(kind==1?0x10:0x50) || in->branch)return typedfail(p,"fileio-copy-type");
 for(i=0;i<2;i++){k=i?in->dm:in->sm;a=i?&in->d:&in->s;l=k==Afp?&p->frame:&p->data;
  if((k!=Afp && k!=Amp) || !typedfileiospan(l,a->reg,kind,off+i))return typedfail(p,"fileio-copy-span");}
 *out=(Dis64Instruction){IMOVMP,SRC(in->sm==Afp?AFP:AMP)|AXIMM|DST(in->dm==Afp?AFP:AMP),0,off[0],off[1]};*returned=kind==1?202:201;return 1;
}
static int
typedaltpaircopy(Inst*in,Dis64TypedBody*p,Dis64Instruction*out,uchar*returned)
{
 Decl*d=in->m.decl;unsigned int i,j,k,n,off[2],next;Dis64Layout*l;Addr*a;
 if(d && typedfileiopayload(d->ty))return typedfileiocopy(in,p,out,returned);
 if(d && typedreplyliteral(d->ty)){
  if(in->op!=IMOVMP || in->mm!=Adesc || !d->desc || d->desc->id!=in->m.offset || d->desc->size!=8 || d->desc->nmap!=1 || !d->desc->map || d->desc->map[0]!=0x40 || in->branch)return typedfail(p,"reply-copy-type");
  for(i=0;i<2;i++){k=i?in->dm:in->sm;a=i?&in->d:&in->s;if((k!=Afp && k!=Amp) || !typedreplyspan(k==Afp?&p->frame:&p->data,a->reg,off+i))return typedfail(p,"reply-copy-span");}
  *out=(Dis64Instruction){IMOVMP,SRC(in->sm==Afp?AFP:AMP)|AXIMM|DST(in->dm==Afp?AFP:AMP),0,off[0],off[1]};*returned=135;return 1;
 }
 n=d?typedrootcount(d->ty):0;
 if(!n || in->op!=IMOVMP || in->mm!=Adesc || !d || !d->desc || d->desc->id!=in->m.offset || d->desc->size!=4*n || d->desc->nmap!=1 || !d->desc->map || d->desc->map[0]!=(n==2?0xc0:0xe0) || in->branch)return typedfail(p,"alt-pair-copy-type");
 for(i=0;i<2;i++){
  k=i?in->dm:in->sm;a=i?&in->d:&in->s;l=k==Afp?&p->frame:&p->data;
  if((k!=Afp && k!=Amp) || !dis64layoutaddress(l,a->reg,4,1,off+i))return typedfail(p,"alt-pair-copy-span");
  for(j=1;j<n;j++)if(!dis64layoutaddress(l,a->reg+4*j,4,1,&next) || next!=off[i]+8*j)return typedfail(p,"alt-pair-copy-span");
 }
 *out=(Dis64Instruction){IMOVMP,SRC(in->sm==Afp?AFP:AMP)|AXIMM|DST(in->dm==Afp?AFP:AMP),0,off[0],off[1]};*returned=n==2?194:196;return 1;
}
/* A reused tuple temporary retains its old nominal storage class. The
 * following exact Rread payload use proves this first field is a byte array. */
static int
typedreadreplyprep(TypedMulti*c,Inst*in,Dis64TypedBody*p,unsigned int pc,unsigned int stop)
{
 unsigned int at,sm,dm,base;long source,dest;int actual;Inst*use;Type*t;
 if((in->op!=IMOVP && in->op!=ISLICEA) || (in->op==IMOVP && in->mm!=Anone) || in->branch || (in->dm!=Afp && in->dm!=Amp) || (in->op==IMOVP && !typedbytearrayaddr(&in->s)))return 0;
 for(at=pc+1;at<stop && at<pc+5;at++){
  use=c->instructions[at];if(!use)break;t=use->s.native64type?use->s.native64type:use->s.decl?use->s.decl->ty:nil;
  if(use->op==ILEA && use->sm==in->dm && use->s.reg==in->d.reg && !use->s.offset && typedaltbytepair(t)){
   if(in->op==ISLICEA){long middle;unsigned int mm;Inst*previous=pc>p->first?c->instructions[pc-1]:nil;
    if(!previous || previous->op!=IMOVP || previous->dm!=in->dm || previous->d.reg!=in->d.reg || !typedbytearrayaddr(&previous->s) || in->dm!=Afp || !typedoperand(in->sm,&in->s,Tint,1,0,p,&source,&sm,&actual) || !typedoperand(in->mm,&in->m,Tint,1,1,p,&middle,&mm,&actual) || (mm!=AIMM && mm!=AFP) || !typedoperand(in->dm,&in->d,TypedRoot,0,0,p,&dest,&dm,&actual))return 0;
    p->code[pc-p->first]=(Dis64Instruction){ISLICEA,SRC(sm)|(mm==AIMM?AXIMM:AXINF)|DST(dm),middle,source,dest};return 1;
   }
   if(!typedoperand(in->sm,&in->s,Tarray,0,0,p,&source,&sm,&actual) || !typedoperand(in->dm,&in->d,TypedRoot,0,0,p,&dest,&dm,&actual))return 0;
   {
    /* Rread has two roots, whereas Rwrite starts with a scalar. */
    unsigned int tail;if(!dis64layoutaddress(in->dm==Afp?&p->frame:&p->data,in->d.reg,4,1,&base) || !dis64layoutaddress(in->dm==Afp?&p->frame:&p->data,in->d.reg+4,4,1,&tail) || tail!=base+8)return 0;
   }
   p->code[pc-p->first]=(Dis64Instruction){IMOVP,SRC(sm)|DST(dm),0,source,dest};return 1;
  }
 }
 return 0;
}
static int
typedaltinst(Inst*in,Decl*fn,Dis64TypedBody*p,unsigned int pc)
{
 Type*t,*use;unsigned int old,base,slot,sm,dm,off,width,n;long source,field;int actual;Inst probe;
 if(in->op==IALT || in->op==INBALT){
  if(in->sm!=Afp || in->mm!=Anone || in->branch || !(t=typedalttype(&in->s,fn,&old)) || in->s.reg!=old || !t->cse || t->cse->nlab<1 || t->cse->nlab>32+(in->op==INBALT) ||
     !dis64layoutaddress(&p->frame,old,4,0,&base) || !typedoperand(in->dm,&in->d,Tint,0,0,p,&source,&dm,&actual) || dm!=AFP)return typedfail(p,"alt-header");
  n=t->cse->nlab;if(n<1 || n>32)return typedfail(p,"alt-guard-count");
  for(slot=0;slot<n;slot++)if(t->cse->labs[slot].isptr)return typedfail(p,"alt-computed-channel");
  if(pc<2+2*n)return typedfail(p,"alt-setup-shape");
  {unsigned int end=pc-2,i,width,limit;Dis64Instruction*q,*prep;
   for(i=0;i<n;i++){
    if(!end)return typedfail(p,"alt-setup-shape");slot=end-1;q=p->code+slot;
    if(p->returns[slot]!=190 && p->returns[slot]!=195 && p->returns[slot]!=197 && p->returns[slot]!=199 && p->returns[slot]!=203 && p->returns[slot]!=134)return typedfail(p,"alt-setup-shape");
    width=q->middle==136 || q->middle==48?16:q->middle==72 || q->middle==120?24:q->middle==104?32:q->middle&15;limit=0;
    while(slot && !p->returns[slot-1]){int length=0;prep=p->code+slot-1;
     if(q->middle==48 && slot+1<end-1 && prep->op==ILENA && p->code[slot].op==IMOVP && p->code[slot+1].op==ISLICEA && p->code[slot].source==prep->source && USRC(p->code[slot].mode)==USRC(prep->mode) && p->code[slot].destination==q->source && p->code[slot+1].destination==q->source && (p->code[slot+1].mode&ARM)==AXINF && prep->destination==p->code[slot+1].middle)length=1;
     if(++limit>8 || (!length && ((prep->op!=IMOVP && prep->op!=IMOVW && !(prep->op==ISLICEA && q->middle==48)) || UDST(prep->mode)!=USRC(q->mode) || prep->destination<q->source || prep->destination+(prep->op==IMOVW?4:8)>q->source+width)))break;slot--;
    }
    if(!slot || (p->returns[slot-1]!=189 && p->returns[slot-1]!=192 && p->returns[slot-1]!=193 && p->returns[slot-1]!=198))return typedfail(p,"alt-setup-shape");end=slot-1;
   }
  }
  p->code[pc]=(Dis64Instruction){in->op,SRC(AFP)|DST(AFP),n,base,source};p->returns[pc]=191;return 1;
 }
 if(in->dm!=Afp || in->mm!=Anone || in->branch || !(t=typedalttype(&in->d,fn,&old)) || !t->cse || t->cse->nlab<1 || t->cse->nlab>32)return typedfail(p,"alt-slot");
 slot=in->d.reg-old;if(slot<8 || slot%4 || !dis64layoutaddress(&p->frame,in->d.reg,4,-1,&off))return typedfail(p,"alt-borrowed-slot");
 if(slot%8==0 && in->op==IMOVW){
  if(in->sm==Afpind || in->sm==Ampind){
   if(typedwmcontextchannel(&in->s,in->sm,p,&source,&sm,&field)){
    p->code[pc]=(Dis64Instruction){IMOVL,SRC(sm)|DST(AFP),field,source,off};p->returns[pc]=193;
   }else if(typedfileiochannel(&in->s,in->sm,p,&source,&sm,&field)){
    p->code[pc]=(Dis64Instruction){IMOVL,SRC(sm)|DST(AFP),field,source,off};p->returns[pc]=198;
   }else{
    probe=*in;probe.op=IMOVP;if(in->s.offset!=52 || !typedwindowfield(&probe,p,pc,1,nil))return typedfail(p,"alt-field-channel");
    source=p->code[pc].source;sm=USRC(p->code[pc].mode);p->code[pc]=(Dis64Instruction){IMOVL,SRC(sm)|DST(AFP),64,source,off};p->returns[pc]=192;
   }
  }else{
   if(!typedoperand(in->sm,&in->s,TypedRoot,0,0,p,&source,&sm,&actual) || actual!=Tchan)return typedfail(p,"alt-channel");
   p->code[pc]=(Dis64Instruction){IMOVL,SRC(sm)|DST(AFP),0,source,off};p->returns[pc]=189;
  }return 1;
 }
 if(slot%8==4 && in->op==ILEA){
  use=in->s.native64type?in->s.native64type:in->s.decl?in->s.decl->ty:nil;
  if(use && use->kind==Ttuple){
   unsigned int first,next,i,n=typedrootcount(use);Dis64Layout*l=in->sm==Afp?&p->frame:&p->data;
   unsigned int fio=typedfileiopayload(use);
   if(typedreplyliteral(use)){
    if((in->sm!=Afp && in->sm!=Amp) || !in->s.decl || in->s.reg!=in->s.decl->offset || in->s.offset || !typedreplyspan(l,in->s.reg,&first))return typedfail(p,"reply-payload-span");source=first;sm=in->sm==Afp?AFP:AMP;width=136;
   }else if(fio){
    if((in->sm!=Afp && in->sm!=Amp) || !in->s.decl || in->s.reg!=in->s.decl->offset || in->s.offset || !typedfileiospan(l,in->s.reg,fio,&first))return typedfail(p,"fileio-payload-span");
    source=first;sm=in->sm==Afp?AFP:AMP;width=fio==1?120:104;
   }else{
   if(!n)return typedfail(p,"alt-payload-class");
   if((in->sm!=Afp && in->sm!=Amp) || !in->s.decl || in->s.reg!=in->s.decl->offset || in->s.offset ||
      !dis64layoutaddress(l,in->s.reg,4,1,&first))return typedfail(p,"alt-pair-span");
   for(i=1;i<n;i++)if(!dis64layoutaddress(l,in->s.reg+4*i,4,1,&next) || next!=first+8*i)return typedfail(p,"alt-pair-span");
   source=first;sm=in->sm==Afp?AFP:AMP;width=n==2?48:72;
   }
  }else{
   if(!use || (use->kind!=Tint && use->kind!=Tstring && use->kind!=Tref))return typedfail(p,"alt-payload-class");
   width=use->kind==Tint?4:24;
   if(!typedoperand(in->sm,&in->s,use->kind==Tref?TypedRoot:use->kind,0,0,p,&source,&sm,&actual) || (sm!=AFP && sm!=AMP))return typedfail(p,"alt-payload");
  }
  p->code[pc]=(Dis64Instruction){ILEA,SRC(sm)|DST(AFP),width,source,off};p->returns[pc]=width==136?134:width==104?199:width==120?203:width==72?197:width==48?195:190;return 1;
 }
 return typedfail(p,"alt-setup-opcode");
}
