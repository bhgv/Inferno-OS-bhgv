/* Exact public/private Client data layout; methods carry no storage. */
static int
typedclientlayout(Type*t,Dis64Layout*l)
{
 static char*names[]={"kbd","ptr","ctl","stop","flags","cursor","wins","znext","images","id","fid","token","wmctxt"};
 static unsigned int native[]={0,8,16,24,32,40,48,56,64,72,76,80,88};
 Type*v;Decl*d,*q;unsigned int i,j,kind;
 if(!t||t->kind!=Tadt||!t->decl||!t->decl->sym||strcmp(t->decl->sym->name,"Client")||!t->decl->dot||!t->decl->dot->sym||strcmp(t->decl->dot->sym->name,"Wmsrv")||!dis64layouttype(t,l)||l->legacybytes!=52||l->bytes!=96||l->alignment!=8||l->count!=13)return 0;
 for(i=0,d=t->ids;i<13;i++,d=d->next){
  kind=i<4||i==8?Tchan:i==4||i==9||i==10||i==11?Tint:i==5?Tstring:i==6?Tlist:Tref;
  if(!d || d->store!=Dfield || !d->sym || strcmp(d->sym->name,names[i]) || d->offset!=4*i || !d->ty || d->ty->kind!=kind ||
     l->leaves[i].legacy!=4*i || l->leaves[i].native!=native[i] || l->leaves[i].oldbytes!=4 || l->leaves[i].bytes!=(kind==Tint?4:8) || l->leaves[i].kind!=kind)return 0;
  v=d->ty;
  if(i<4){if(!v->tof || (i==1?!typedwmref(v->tof,"Draw","Pointer"):v->tof->kind!=(i==2?Tstring:Tint)))return 0;}
  else if(i==6){if(!typedwmref(v->tof,"Wmsrv","Window"))return 0;}
  else if(i==7){if(!typedwmref(v,"Wmsrv","Client"))return 0;}
  else if(i==8){
   if(!v->tof || v->tof->kind!=Ttuple)return 0;
   for(j=0,q=v->tof->ids;j<3;j++,q=q->next)if(!q || q->store!=Dfield || q->offset!=4*j || !q->ty ||
      (j<2?!typedwmref(q->ty,"Draw",j==0?"Point":"Image"):(q->ty->kind!=Tchan || !q->ty->tof || q->ty->tof->kind!=Tint)))return 0;
   if(q)return 0;
  }else if(i==12 && !typedwmref(v,"Draw","Wmcontext"))return 0;
 }
 for(;d;d=d->next)if(d->store!=Dfn || !d->ty || d->ty->kind!=Tfn)return 0;
 return 1;
}
static int
typedclientaddress(Addr*a,int mode,Dis64TypedBody*p,long*base,unsigned int*wire,long*field,int*payload)
{
 static unsigned int native[]={0,8,16,24,32,40,48,56,64,72,76,80,88};
 Type*t=typedfieldowner(a);Addr owner;Dis64Layout l={0};unsigned int i;int actual,ok=0;
 if((mode!=Afpind&&mode!=Ampind)||!typedwmref(t,"Wmsrv","Client")||a->offset<0||a->offset>48||a->offset%4||!typedclientlayout(t->tof,&l))goto done;
 owner=*a;owner.offset=0;owner.native64type=t;
 if(!typedoperand(mode==Afpind?Afp:Amp,&owner,TypedRoot,0,0,p,base,wire,&actual)||actual!=Tref)goto done;
 *wire|=AIND;*field=native[a->offset/4];i=a->offset/4;*payload=i<4||i==8?Tchan:i==4||i==9||i==10||i==11?Tint:i==5?Tstring:i==6?Tlist:Tref;ok=1;
 done:dis64layoutfree(&l);return ok;
}

static int
typedclientchanneladdress(Addr*a,int mode,Dis64TypedBody*p,long*base,unsigned int*wire,long*field,int*payload)
{
 if(a->offset<0 || a->offset>12 || !typedclientaddress(a,mode,p,base,wire,field,payload) || *payload!=Tchan)return 0;
 *payload=a->offset==4?Tref:a->offset==8?Tstring:Tint;return 1;
}
static int
typedclientfieldmove(Inst*in,Dis64TypedBody*p,Dis64Instruction*out,uchar*returned)
{
 int read=in->sm==Afpind || in->sm==Ampind,kind,actual,root;unsigned int bm,vm;long base,value,field;
 Addr*a=read?&in->s:&in->d;
 if(in->mm!=Anone || in->branch || !typedclientaddress(a,read?in->sm:in->dm,p,&base,&bm,&field,&kind))return typedfail(p,"client-field-owner");
 root=kind!=Tint;
 if(in->op!=(root?IMOVP:IMOVW) || !typedoperand(read?in->dm:in->sm,read?&in->d:&in->s,root?TypedRoot:Tint,!read&&!root,0,p,&value,&vm,&actual) ||
    (root && actual!=kind && actual!=Tany) || (vm!=AFP && vm!=AMP && (read || root || vm!=AIMM)))return typedfail(p,"client-field-value");
 *out=(Dis64Instruction){in->op,read?SRC(bm)|DST(vm):SRC(vm)|DST(bm),field,read?base:value,read?value:base};*returned=root?205:206;return 1;
}

/* A selected fused flags read: Word32 AND with an encodable immediate mask. */
static int
typedclientflagsand(Inst*in,Dis64TypedBody*p,unsigned int pc)
{
 long base,field,mask,value;unsigned int wire,mm,dm;int kind,actual;
 if(in->op!=IANDW || in->s.offset!=16 || !in->s.native64type || in->s.native64type->kind!=Tint || in->branch ||
    !typedclientaddress(&in->s,in->sm,p,&base,&wire,&field,&kind) || kind!=Tint || field!=32 ||
    !typedoperand(in->mm,&in->m,Tint,1,1,p,&mask,&mm,&actual) || mm!=AIMM || actual!=Tint ||
    !typedoperand(in->dm,&in->d,Tint,0,0,p,&value,&dm,&actual) || actual!=Tint || (dm!=AFP && dm!=AMP))return typedfail(p,"client-flags-and");
 p->code[pc]=(Dis64Instruction){IANDW,SRC(wire)|AXIMM|DST(dm),(32UL<<16)|((unsigned long)mask&65535),base,value};p->returns[pc]=234;return 1;
}

/* Selected in-place flags OR: mutate only the four-byte flags leaf. */
static int
typedclientflagsor(Inst*in,Dis64TypedBody*p,unsigned int pc)
{
 long base,field,mask;unsigned int wire,sm;int kind,actual;
 if((in->op!=IORW && in->op!=IANDW && in->op!=IXORW) || in->d.offset!=16 || !in->d.native64type || in->d.native64type->kind!=Tint || in->mm!=Anone || in->branch ||
    !typedclientaddress(&in->d,in->dm,p,&base,&wire,&field,&kind) || field!=32 || kind!=Tint ||
    !typedoperand(in->sm,&in->s,Tint,1,1,p,&mask,&sm,&actual) || sm!=AIMM || actual!=Tint)return typedfail(p,"client-flags-or");
 p->code[pc]=(Dis64Instruction){in->op,SRC(AIMM)|DST(wire),32,mask,base};p->returns[pc]=236;return 1;
}

static int
typedclientidstring(Inst*in,Dis64TypedBody*p,unsigned int pc)
{
 long base,field,dest;unsigned int wire,dm;int kind,actual;
 if(in->op!=ICVTWC || in->s.offset!=36 || !in->s.native64type || in->s.native64type->kind!=Tint || in->mm!=Anone || in->branch || in->dm!=Afp || in->d.offset ||
    !typedclientaddress(&in->s,in->sm,p,&base,&wire,&field,&kind) || kind!=Tint || field!=72 || !typedoperand(Afp,&in->d,Tstring,0,0,p,&dest,&dm,&actual) || actual!=Tstring)return typedfail(p,"client-id-string");
 p->code[pc]=(Dis64Instruction){ICVTWC,SRC(wire)|DST(AFP),72,base,dest};p->returns[pc]=237;return 1;
}
