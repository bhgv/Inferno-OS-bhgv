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
 if(kind==Tstring && typedwmref(typedfieldowner(&in->s),"Wmsrv","Client")){
  long base,part;unsigned int wire;int actual;
  if(in->s.offset!=20 || !typedclientaddress(&in->s,in->sm,p,&base,&wire,&part,&actual) || part!=40 || actual!=Tstring)return typedfail(p,"client-cursor-argument");
  *out=(Dis64Instruction){IMOVP,SRC(wire)|DST(AFP|AIND),40,base,offset};p->returns[pc]=233;return 1;
 }
 if(kind==Tchan && typedwmref(typedfieldowner(&in->s),"Draw","Wmcontext")){
  long base,part;unsigned int wire;
  if(in->s.offset!=4 || !typedwmcontextchannel(&in->s,in->sm,p,&base,&wire,&part) || part!=8)return typedfail(p,"wmcontext-pointer-argument");
  *out=(Dis64Instruction){IMOVP,SRC(wire)|DST(AFP|AIND),8,base,offset};p->returns[pc]=125;return 1;
 }
 field.dm=Afp;field.d=in->s;
 if(typedwmsrvwindow(typedfieldowner(&in->s))){
  long base;unsigned int wire;
  if((kind==Tstring?in->s.offset!=0:kind!=Tref || in->s.offset!=20) || !typedwmsrvwindowbase(&in->s,in->sm,p,&base,&wire))return typedfail(p,"window-field-argument");
  *out=(Dis64Instruction){IMOVP,SRC(wire)|DST(AFP|AIND),kind==Tstring?0:24,base,offset};p->returns[pc]=125;return 1;
 }
 if(!(typedwindowowner(&in->s)?typedwindowfield(&field,p,pc,1,nil):typeddrawowner(&in->s)?typeddrawfield(&field,p,pc,1,nil):typedclientfield(&field,p,pc,1)))return 0;
 *out=(Dis64Instruction){IMOVP,SRC(USRC(out->mode))|DST(AFP|AIND),p->contextfields[pc],out->source,offset};p->returns[pc]=125;return 1;
}

/* Exact Image.r/clipr or Wmsrv.Window.r and their Point halves. */
static int
typedimagegeometry(Inst*in,Dis64TypedBody*p,unsigned int pc,unsigned int offset)
{
 Inst probe=*in;long field=in->s.offset;Dis64Layout layout={0};int ok;unsigned int bytes;
 long base;unsigned int wire;
 if(typedwmref(typedfieldowner(&in->s),"Draw","Pointer")){
  if(field!=4 || !typedwordgeometry(in->s.native64type,&layout) || layout.bytes!=8 || in->m.offset!=8 || !typedpointeraddress(&in->s,in->sm,p,&base,&wire)){dis64layoutfree(&layout);return typedfail(p,"pointer-point-owner");}
  dis64layoutfree(&layout);p->code[pc]=(Dis64Instruction){IMOVM,SRC(wire|AIND)|AXIMM|DST(AFP|AIND),8,base,offset};p->returns[pc]=230;return 1;
 }
 if(typedwmsrvwindow(typedfieldowner(&in->s))){
  if(!typedwordgeometry(in->s.native64type,&layout) || in->m.offset!=layout.bytes || (layout.bytes==16?field!=4:(field!=4 && field!=12)) || !typedwmsrvwindowbase(&in->s,in->sm,p,&base,&wire)){dis64layoutfree(&layout);return typedfail(p,"window-geometry-owner");}
  bytes=layout.bytes;dis64layoutfree(&layout);
  p->code[pc]=(Dis64Instruction){IMOVM,SRC(wire)|AXIMM|DST(AFP|AIND),bytes,base,offset};p->returns[pc]=bytes==16?217:field==4?218:219;return 1;
 }
 if(typeddrawowner(&in->s)!=1 || !typedwordgeometry(in->s.native64type,&layout) || in->m.offset!=layout.bytes || (layout.bytes==16?(field!=0 && field!=16):(field<0 || field>24 || field%8))){dis64layoutfree(&layout);return typedfail(p,"image-geometry-owner");}
 bytes=layout.bytes;dis64layoutfree(&layout);probe.op=IMOVP;probe.mm=Anone;probe.dm=Afp;probe.s.offset=44;probe.branch=nil;
 ok=typeddrawfield(&probe,p,pc,1,nil);if(!ok)return 0;
 p->code[pc]=(Dis64Instruction){IMOVM,SRC(USRC(p->code[pc].mode))|AXIMM|DST(AFP|AIND),bytes,p->code[pc].source,offset};p->returns[pc]=bytes==16?160+field/16:162+field/8;return 1;
}

/* Screen.fill or Wmsrv.Window.img rooted for an immediate Image geometry copy. */
static int
typedimagegeometryprep(Inst*in,Inst*copy,Dis64TypedBody*p,unsigned int pc)
{
 int window=typedwmsrvwindow(typedfieldowner(&in->s));uchar role;
 if(!copy || (window?in->s.offset!=20:typeddrawowner(&in->s)!=2 || in->s.offset!=8) || in->dm!=Afp || in->mm!=Anone || in->branch || !in->d.decl || in->d.decl->store!=Dlocal || !in->d.decl->sym || in->d.decl->sym->name[0]!='.' || in->d.decl->sym->name[1]!='t' || in->d.offset ||
    copy->op!=IMOVM || copy->sm!=Afpind || copy->s.reg!=in->d.reg || copy->s.decl!=in->d.decl || typeddrawowner(&copy->s)!=1 || in->s.native64type!=typedfieldowner(&copy->s))return typedfail(p,"image-geometry-prep");
 if(window){if(!typedwmsrvwindowfield(in,p,p->code+pc,&role))return 0;}
 else{if(!typeddrawfield(in,p,pc,0,nil))return 0;p->code[pc].middle=p->contextfields[pc];}
 p->returns[pc]=166;return 1;
}

/* Screen.image rooted temporarily for a numeric argument of a local CALL. */
static int
typedlocalscreenimageprep(Inst*in,Inst*copy,Dis64TypedBody*p,unsigned int pc)
{
 if(!copy || typeddrawowner(&in->s)!=2 || in->s.offset!=4 || in->dm!=Afp || in->mm!=Anone || in->branch || !in->d.decl || in->d.decl->store!=Dlocal || !in->d.decl->sym || in->d.decl->sym->name[0]!='.' || in->d.decl->sym->name[1]!='t' || in->d.offset ||
    copy->op!=IMOVM || copy->sm!=Afpind || copy->s.reg!=in->d.reg || copy->s.decl!=in->d.decl || typeddrawowner(&copy->s)!=1 || in->s.native64type!=typedfieldowner(&copy->s) || !typeddrawfield(in,p,pc,0,nil))return typedfail(p,"local-screen-image-prep");
 p->code[pc].middle=8;p->returns[pc]=235;return 1;
}

/* Numeric Image geometry copied to/from ordinary FP/MP geometry, not a call frame. */
/* A whole raw geometry value, never a heap owner or a partially written result. */
static int
typedlocalgeometryreturn(Inst*in,Dis64TypedBody*p,Dis64Instruction*out,uchar*returned)
{
 Dis64Layout l={0};Type*t=in->s.native64type?in->s.native64type:in->s.decl?in->s.decl->ty:nil;Addr a;unsigned int i,wire,mode=0;long base=0,part;int actual,ok=0;
 if(!p->resultgeometry || in->op!=IMOVM || in->mm!=Aimm || in->m.offset!=p->resultgeometry || in->dm!=Afpind || in->d.reg!=16 || in->d.offset || in->branch || (in->sm!=Afp && in->sm!=Amp) || !typedwordgeometry(t,&l) || l.bytes!=p->resultgeometry)goto done;
 for(i=0;i<l.bytes;i+=4){a=in->s;a.reg+=i;a.offset=0;if(!typedoperand(in->sm,&a,Tint,0,0,p,&part,&wire,&actual) || actual!=Tint || (i && part!=base+i))goto done;if(!i){base=part;mode=wire;}}
 *out=(Dis64Instruction){IMOVM,SRC(mode)|AXIMM|DST(AFP|AIND),l.bytes,base,32};*returned=l.bytes==16?246:247;ok=1;
 done:dis64layoutfree(&l);return ok?1:typedfail(p,"local-geometry-return");
}

static int
typedlocalgeometry(Inst*in,Dis64TypedBody*p,Dis64Instruction*out,uchar*returned)
{
 Dis64Layout l={0},other={0};Addr a,*image=nil,*direct;Inst probe=*in;Type*t;long base=0,value=0,part,field=0;unsigned int mode,wire,i,bytes=0,read,indirect,window=0,pointer=0;int actual,ok=0;
 read=in->sm==Afpind || in->sm==Ampind;indirect=read || in->dm==Afpind || in->dm==Ampind;
 if(in->op==IMOVM&&in->mm==Aimm&&!in->branch&&in->m.offset==16&&read&&(in->dm==Afpind||in->dm==Ampind)&&typeddrawowner(&in->s)==1&&!in->s.offset&&in->d.offset==4&&typedwmsrvwindow(typedfieldowner(&in->d))){long dst;unsigned int dw;Addr src=in->s;Type*owner=typedfieldowner(&src);
  src.offset=0;src.native64type=owner;if(!typedoperand(in->sm==Afpind?Afp:Amp,&src,TypedRoot,0,0,p,&base,&wire,&actual)||actual!=Tref||!typedwmsrvwindowbase(&in->d,in->dm,p,&dst,&dw))goto done;
  *out=(Dis64Instruction){IMOVM,SRC(wire|AIND)|AXIMM|DST(dw),16,base,dst};*returned=110;ok=1;goto done;
 }
 if(in->op!=IMOVM || in->mm!=Aimm || in->branch || (read && (in->dm==Afpind || in->dm==Ampind)))goto done;
 if(indirect){
  image=read?&in->s:&in->d;direct=read?&in->d:&in->s;mode=read?in->dm:in->sm;field=image->offset;
  t=image->native64type?image->native64type:image->decl?image->decl->ty:nil;
  window=typedwmsrvwindow(typedfieldowner(image));pointer=typedwmref(typedfieldowner(image),"Draw","Pointer");
  {Type*owner=typedfieldowner(image);if(typedwmref(owner,"Draw","Point")){Addr copy=*image;copy.offset=0;copy.native64type=owner;if(!typedwordgeometry(owner->tof,&l)||l.bytes!=8||field||in->m.offset!=8||!typedoperand(read?in->sm==Afpind?Afp:Amp:in->dm==Afpind?Afp:Amp,&copy,TypedRoot,0,0,p,&base,&wire,&actual)||actual!=Tref)goto done;bytes=8;pointer=2;}}
  if(window&&field==4&&in->m.offset==16)bytes=16;
  else{if((typeddrawowner(image)!=1 && !window && !pointer) || (pointer!=2&&!typedwordgeometry(t,&l)))goto done;bytes=l.bytes;}
  if(window)field+=4;
  if(in->m.offset!=bytes || (pointer?(bytes!=8 || field!=(pointer==2?0:4)):window?(bytes==16?field!=8:(field!=8 && field!=16)):(bytes==16?(field!=0 && field!=16):(field<0 || field>24 || field%8))))goto done;
  probe.op=IMOVP;probe.s=*image;probe.sm=read?in->sm:in->dm;probe.s.offset=44;probe.mm=Anone;probe.dm=Afp;probe.branch=nil;
  if(pointer==2){}
  else if(pointer){if(!typedpointeraddress(image,read?in->sm:in->dm,p,&base,&wire))goto done;}
  else if(window){if(!typedwmsrvwindowbase(image,read?in->sm:in->dm,p,&base,&wire))goto done;}
  else{if(!typeddrawfield(&probe,p,in->pc-p->first,1,nil))goto done;base=p->code[in->pc-p->first].source;}
  t=direct->native64type?direct->native64type:direct->decl?direct->decl->ty:nil;
  if(window&&bytes==16)other.bytes=16;
  else if(!typedwordgeometry(t,&other))goto done;
  if(other.bytes!=bytes || (mode!=Afp && mode!=Amp))goto done;
  for(i=0;i<bytes;i+=4){a=*direct;a.reg+=i;a.offset=0;if(!typedoperand(mode,&a,Tint,0,0,p,&part,&wire,&actual) || actual!=Tint || (i && part!=value+i))goto done;if(!i)value=part;}
  *out=(Dis64Instruction){IMOVM,read?SRC((in->sm==Afpind?AFP:AMP)|AIND)|AXIMM|DST(mode==Afp?AFP:AMP):SRC(mode==Afp?AFP:AMP)|AXIMM|DST((in->dm==Afpind?AFP:AMP)|AIND),(field<<8)|bytes,read?base:value,read?value:base};
  *returned=(window||pointer)?215:read?(bytes==16?208:209):(bytes==16?210:211);
 }else{
  if((in->sm!=Afp && in->sm!=Amp) || (in->dm!=Afp && in->dm!=Amp))goto done;
  t=in->s.native64type?in->s.native64type:in->s.decl?in->s.decl->ty:nil;if(!typedwordgeometry(t,&l))goto done;bytes=l.bytes;
  t=in->d.native64type?in->d.native64type:in->d.decl?in->d.decl->ty:nil;if(!typedwordgeometry(t,&other) || other.bytes!=bytes || in->m.offset!=bytes)goto done;
  for(i=0;i<bytes;i+=4){a=in->s;a.reg+=i;a.offset=0;if(!typedoperand(in->sm,&a,Tint,0,0,p,&part,&wire,&actual) || actual!=Tint || (i && part!=base+i))goto done;if(!i)base=part;
   a=in->d;a.reg+=i;a.offset=0;if(!typedoperand(in->dm,&a,Tint,0,0,p,&part,&wire,&actual) || actual!=Tint || (i && part!=value+i))goto done;if(!i)value=part;}
  *out=(Dis64Instruction){IMOVM,SRC(in->sm==Afp?AFP:AMP)|AXIMM|DST(in->dm==Afp?AFP:AMP),bytes,base,value};*returned=212;
 }
 ok=1;
 done:dis64layoutfree(&l);dis64layoutfree(&other);return ok?1:typedfail(p,"local-image-geometry");
}

/* Numeric Image geometry words in ordinary copies, arithmetic and comparisons. */
static int
typedimageword(Inst*in,Dis64TypedBody*p,unsigned int pc,unsigned int stop)
{
 Inst probe=*in;long base,source,middle=0,dest,field;unsigned int sm,mm=AXNON,dm;int actual,branch;
 branch=in->op==IBEQW || in->op==IBNEW || in->op==IBLTW || in->op==IBLEW || in->op==IBGTW || in->op==IBGEW;
 if((in->op!=IMOVW && !typedwordargop(in->op) && !branch) || (in->sm!=Afpind && in->sm!=Ampind) || in->branch && !branch)return typedfail(p,"image-word-field");
 field=in->s.offset;
 if(typedwmref(typedfieldowner(&in->s),"Wmsrv","Client")){if(!typedclientaddress(&in->s,in->sm,p,&base,&sm,&field,&actual) || actual!=Tint)return typedfail(p,"client-word-field");}
 else{
  if(typeddrawowner(&in->s)!=1 || field<0 || field>28 || field%4)return typedfail(p,"image-word-field");
  probe.op=IMOVP;probe.s.offset=44;probe.mm=Anone;probe.dm=Afp;probe.branch=nil;
  if(!typeddrawfield(&probe,p,pc,1,nil))return 0;base=p->code[pc].source;sm=USRC(p->code[pc].mode);
 }
 if(base>65535)return typedfail(p,"image-word-owner");
 if(in->op!=IMOVW){unsigned int mode;if(!typedoperand(in->mm,&in->m,Tint,1,1,p,&middle,&mode,&actual))return 0;mm=mode==AIMM?AXIMM:mode==AFP?AXINF:AXINM;}else if(in->mm!=Anone)return 0;
 if(branch){if(in->dm!=Apc || in->d.offset<(long)p->first || in->d.offset>=stop)return typedfail(p,"image-word-branch");dest=in->d.offset-p->first;dm=AIMM;}
 else if(!typedoperand(in->dm,&in->d,Tint,0,0,p,&dest,&dm,&actual) || (dm!=AFP && dm!=AMP))return typedfail(p,"image-word-destination");
 source=(field<<16)|base;p->code[pc]=(Dis64Instruction){in->op,SRC(sm)|mm|DST(dm),middle,source,dest};p->returns[pc]=133;return 1;
}
