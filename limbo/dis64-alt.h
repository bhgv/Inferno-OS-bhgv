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
static int
typedaltpaircopy(Inst*in,Dis64TypedBody*p,Dis64Instruction*out,uchar*returned)
{
 Decl*d=in->m.decl;unsigned int i,j,k,n,off[2],next;Dis64Layout*l;Addr*a;
 n=d && typedaltpair(d->ty)?2:d && typedaltrequest(d->ty)?3:0;
 if(!n || in->op!=IMOVMP || in->mm!=Adesc || !d || !d->desc || d->desc->id!=in->m.offset || d->desc->size!=4*n || d->desc->nmap!=1 || !d->desc->map || d->desc->map[0]!=(n==2?0xc0:0xe0) || in->branch)return typedfail(p,"alt-pair-copy-type");
 for(i=0;i<2;i++){
  k=i?in->dm:in->sm;a=i?&in->d:&in->s;l=k==Afp?&p->frame:&p->data;
  if((k!=Afp && k!=Amp) || !dis64layoutaddress(l,a->reg,4,1,off+i))return typedfail(p,"alt-pair-copy-span");
  for(j=1;j<n;j++)if(!dis64layoutaddress(l,a->reg+4*j,4,1,&next) || next!=off[i]+8*j)return typedfail(p,"alt-pair-copy-span");
 }
 *out=(Dis64Instruction){IMOVMP,SRC(in->sm==Afp?AFP:AMP)|AXIMM|DST(in->dm==Afp?AFP:AMP),0,off[0],off[1]};*returned=n==2?194:196;return 1;
}
static int
typedaltinst(Inst*in,Decl*fn,Dis64TypedBody*p,unsigned int pc)
{
 Type*t,*use;unsigned int old,base,slot,sm,dm,off,width;long source,field;int actual;Inst probe;
 if(in->op==IALT || in->op==INBALT){
  if(in->op!=IALT || in->sm!=Afp || in->mm!=Anone || in->branch || !(t=typedalttype(&in->s,fn,&old)) || in->s.reg!=old || !t->cse || t->cse->nlab<1 || t->cse->nlab>32 ||
     !dis64layoutaddress(&p->frame,old,4,0,&base) || !typedoperand(in->dm,&in->d,Tint,0,0,p,&source,&dm,&actual) || dm!=AFP)return typedfail(p,"alt-header");
  for(slot=0;slot<t->cse->nlab;slot++)if(t->cse->labs[slot].isptr)return typedfail(p,"alt-computed-channel");
  if(pc<2+2*t->cse->nlab)return typedfail(p,"alt-setup-shape");
  for(slot=pc-2-2*t->cse->nlab;slot<pc-2;slot+=2)if((p->returns[slot]!=189 && p->returns[slot]!=192 && p->returns[slot]!=193) || (p->returns[slot+1]!=190 && p->returns[slot+1]!=195 && p->returns[slot+1]!=197))return typedfail(p,"alt-setup-shape");
  p->code[pc]=(Dis64Instruction){in->op,SRC(AFP)|DST(AFP),t->cse->nlab,base,source};p->returns[pc]=191;return 1;
 }
 if(in->dm!=Afp || in->mm!=Anone || in->branch || !(t=typedalttype(&in->d,fn,&old)) || !t->cse || t->cse->nlab<1 || t->cse->nlab>32)return typedfail(p,"alt-slot");
 slot=in->d.reg-old;if(slot<8 || slot%4 || !dis64layoutaddress(&p->frame,in->d.reg,4,-1,&off))return typedfail(p,"alt-borrowed-slot");
 if(slot%8==0 && in->op==IMOVW){
  if(in->sm==Afpind || in->sm==Ampind){
   if(typedwmcontextchannel(&in->s,in->sm,p,&source,&sm,&field)){
    p->code[pc]=(Dis64Instruction){IMOVL,SRC(sm)|DST(AFP),field,source,off};p->returns[pc]=193;
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
   unsigned int first,next,i,n=typedaltpair(use)?2:typedaltrequest(use)?3:0;Dis64Layout*l=in->sm==Afp?&p->frame:&p->data;
   if(!n)return typedfail(p,"alt-payload-class");
   if((in->sm!=Afp && in->sm!=Amp) || !in->s.decl || in->s.reg!=in->s.decl->offset || in->s.offset ||
      !dis64layoutaddress(l,in->s.reg,4,1,&first))return typedfail(p,"alt-pair-span");
   for(i=1;i<n;i++)if(!dis64layoutaddress(l,in->s.reg+4*i,4,1,&next) || next!=first+8*i)return typedfail(p,"alt-pair-span");
   source=first;sm=in->sm==Afp?AFP:AMP;width=n==2?48:72;
  }else{
   if(!use || (use->kind!=Tint && use->kind!=Tstring && use->kind!=Tref))return typedfail(p,"alt-payload-class");
   width=use->kind==Tint?4:24;
   if(!typedoperand(in->sm,&in->s,use->kind==Tref?TypedRoot:use->kind,0,0,p,&source,&sm,&actual) || (sm!=AFP && sm!=AMP))return typedfail(p,"alt-payload");
  }
  p->code[pc]=(Dis64Instruction){ILEA,SRC(sm)|DST(AFP),width,source,off};p->returns[pc]=width==72?197:width==48?195:190;return 1;
 }
 return typedfail(p,"alt-setup-opcode");
}
