/* Borrowed direct channel guards; every raw slot must belong to an exact Talt. */
static Type*
typedalttype(Addr*a,Decl*fn,unsigned int*base)
{
 Decl*d;for(d=fn->locals;d;d=d->next)if(d->ty && d->ty->kind==Talt && a->reg>=d->offset && a->reg-d->offset<d->ty->size){*base=d->offset;return d->ty;}return nil;
}
static int
typedaltslot(Addr*a,Decl*fn)
{unsigned int base;return typedalttype(a,fn,&base) && a->reg-base>=8;}
static int
typedaltinst(Inst*in,Decl*fn,Dis64TypedBody*p,unsigned int pc)
{
 Type*t,*use;unsigned int old,base,slot,sm,dm,off,width;long source;int actual;Inst probe;
 if(in->op==IALT || in->op==INBALT){
  if(in->op!=IALT || in->sm!=Afp || in->mm!=Anone || in->branch || !(t=typedalttype(&in->s,fn,&old)) || in->s.reg!=old || !t->cse || t->cse->nlab<1 || t->cse->nlab>32 ||
     !dis64layoutaddress(&p->frame,old,4,0,&base) || !typedoperand(in->dm,&in->d,Tint,0,0,p,&source,&dm,&actual) || dm!=AFP)return typedfail(p,"alt-header");
  for(slot=0;slot<t->cse->nlab;slot++)if(t->cse->labs[slot].isptr)return typedfail(p,"alt-computed-channel");
  if(pc<2+2*t->cse->nlab)return typedfail(p,"alt-setup-shape");
  for(slot=pc-2-2*t->cse->nlab;slot<pc-2;slot+=2)if((p->returns[slot]!=189 && p->returns[slot]!=192) || p->returns[slot+1]!=190)return typedfail(p,"alt-setup-shape");
  p->code[pc]=(Dis64Instruction){in->op,SRC(AFP)|DST(AFP),t->cse->nlab,base,source};p->returns[pc]=191;return 1;
 }
 if(in->dm!=Afp || in->mm!=Anone || in->branch || !(t=typedalttype(&in->d,fn,&old)) || !t->cse || t->cse->nlab<1 || t->cse->nlab>32)return typedfail(p,"alt-slot");
 slot=in->d.reg-old;if(slot<8 || slot%4 || !dis64layoutaddress(&p->frame,in->d.reg,4,-1,&off))return typedfail(p,"alt-borrowed-slot");
 if(slot%8==0 && in->op==IMOVW){
  if(in->sm==Afpind || in->sm==Ampind){
   probe=*in;probe.op=IMOVP;if(in->s.offset!=52 || !typedwindowfield(&probe,p,pc,1,nil))return typedfail(p,"alt-field-channel");
   source=p->code[pc].source;sm=USRC(p->code[pc].mode);p->code[pc]=(Dis64Instruction){IMOVL,SRC(sm)|DST(AFP),64,source,off};p->returns[pc]=192;
  }else{
   if(!typedoperand(in->sm,&in->s,TypedRoot,0,0,p,&source,&sm,&actual) || actual!=Tchan)return typedfail(p,"alt-channel");
   p->code[pc]=(Dis64Instruction){IMOVL,SRC(sm)|DST(AFP),0,source,off};p->returns[pc]=189;
  }return 1;
 }
 if(slot%8==4 && in->op==ILEA){
  use=in->s.native64type?in->s.native64type:in->s.decl?in->s.decl->ty:nil;
  if(!use || (use->kind!=Tint && use->kind!=Tstring))return typedfail(p,"alt-payload-class");
  width=use->kind==Tint?4:24;
  if(!typedoperand(in->sm,&in->s,use->kind,0,0,p,&source,&sm,&actual) || (sm!=AFP && sm!=AMP))return typedfail(p,"alt-payload");
  p->code[pc]=(Dis64Instruction){ILEA,SRC(sm)|DST(AFP),width,source,off};p->returns[pc]=190;return 1;
 }
 return typedfail(p,"alt-setup-opcode");
}
