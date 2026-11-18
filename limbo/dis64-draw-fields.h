/* Complete selected public Draw payloads, not guessed C pointer strides. */
static int
typeddrawowner(Addr*a)
{
 Type*t=typedfieldowner(a);Decl*d;
 if(!t || t->kind!=Tref || !t->tof || t->tof->kind!=Tadt || !(d=t->tof->decl) || !d->sym || !d->dot || !d->dot->sym || strcmp(d->dot->sym->name,"Draw"))return 0;
 return !strcmp(d->sym->name,"Image")?1:!strcmp(d->sym->name,"Screen")?2:!strcmp(d->sym->name,"Display")?3:0;
}
static int
typeddrawfield(Inst*in,Dis64TypedBody*p,unsigned int pc,int probe,unsigned int*which)
{
 Addr*a,owner;Dis64Layout layout={0};unsigned int i,tag,bm,vm=AFP,old,native,count,size,oldsize,index=11;long base,value=0;int read,kind,actual,ok=0;
 read=in->sm==Afpind || in->sm==Ampind;a=read?&in->s:&in->d;tag=typeddrawowner(a);
 if(!tag || in->op!=IMOVP || in->mm!=Anone || in->branch || (read?(in->dm!=Afp && in->dm!=Amp):(in->dm!=Afpind && in->dm!=Ampind)))goto done;
 count=tag==1?14:tag==2?4:5;oldsize=tag==1?56:tag==2?16:20;size=tag==1?72:tag==2?32:40;
 if(!dis64layouttype(typedfieldowner(a)->tof,&layout) || layout.legacybytes!=oldsize || layout.bytes!=size || layout.alignment!=8 || layout.count!=count)goto done;
 for(i=0;i<count;i++){
  old=4*i;kind=tag==1?(i<11?Tint:i==13?Tstring:Tref):tag==2?(i?Tref:Tint):Tref;
  native=tag==1?(i<11?4*i:48+8*(i-11)):tag==2?(i?8*i:0):8*i;
  if(layout.leaves[i].legacy!=old || layout.leaves[i].native!=native || layout.leaves[i].oldbytes!=4 || layout.leaves[i].bytes!=(kind==Tint?4:8) || layout.leaves[i].kind!=kind)goto done;
  if(a->offset==(long)old && kind!=Tint)index=tag==1?i-11:tag==2?i+2:i+6;
 }
 if(index==11)goto done;kind=index==2?Tstring:Tref;owner=*a;owner.offset=0;owner.native64type=typedfieldowner(a);
 if(!typedoperand((read?in->sm:in->dm)==Afpind?Afp:Amp,&owner,TypedRoot,0,0,p,&base,&bm,&actual) || actual!=Tref)goto done;
 if(!probe && (!typedoperand(read?in->dm:in->sm,read?&in->d:&in->s,TypedRoot,0,0,p,&value,&vm,&actual) || (actual!=kind && actual!=Tany) || (vm!=AFP && vm!=AMP)))goto done;
 if(!p->contextfields){p->contextfields=allocmem(p->count*sizeof(long));memset(p->contextfields,0,p->count*sizeof(long));}
 p->contextfields[pc]=tag==1?48+8*(index):tag==2?8*(index-2):8*(index-6);
 p->code[pc]=(Dis64Instruction){IMOVP,read?SRC(bm|AIND)|DST(vm):SRC(vm)|DST(bm|AIND),0,read?base:value,read?value:base};
 if(which)*which=index;ok=1;
 done:dis64layoutfree(&layout);return ok?1:typedfail(p,"draw-field-roles");
}

static int
typedselectedfieldarg(Inst*in,Dis64TypedBody*p,unsigned int pc,unsigned int offset,int kind)
{
 Inst field=*in;Type*value=in->s.native64type;Dis64Instruction*out=p->code+pc;
 if(in->op!=IMOVP || in->mm!=Anone || in->branch || (in->sm!=Afpind && in->sm!=Ampind) || !value || value->kind!=kind)return typedfail(p,"field-argument-kind");
 field.dm=Afp;field.d=in->s;
 if(!(typedwindowowner(&in->s)?typedwindowfield(&field,p,pc,1,nil):typeddrawowner(&in->s)?typeddrawfield(&field,p,pc,1,nil):typedclientfield(&field,p,pc,1)))return 0;
 *out=(Dis64Instruction){IMOVP,SRC(USRC(out->mode))|DST(AFP|AIND),p->contextfields[pc],out->source,offset};p->returns[pc]=125;return 1;
}
