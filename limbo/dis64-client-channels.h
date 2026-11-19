/* Exact public/private Client data layout; methods carry no storage. */
static int
typedclientaddress(Addr*a,int mode,Dis64TypedBody*p,long*base,unsigned int*wire,long*field,int*payload)
{
 static char*names[]={"kbd","ptr","ctl","stop","flags","cursor","wins","znext","images","id","fid","token","wmctxt"};
 static unsigned int native[]={0,8,16,24,32,40,48,56,64,72,76,80,88};
 Type*t=typedfieldowner(a),*v;Decl*d,*q;Addr owner;Dis64Layout l={0};unsigned int i,j,kind;int actual,ok=0;
 if((mode!=Afpind && mode!=Ampind) || !typedwmref(t,"Wmsrv","Client") || a->offset<0 || a->offset>48 || a->offset%4 ||
    !dis64layouttype(t->tof,&l) || l.legacybytes!=52 || l.bytes!=96 || l.alignment!=8 || l.count!=13)goto done;
 for(i=0,d=t->tof->ids;i<13;i++,d=d->next){
  kind=i<4||i==8?Tchan:i==4||i==9||i==10||i==11?Tint:i==5?Tstring:i==6?Tlist:Tref;
  if(!d || d->store!=Dfield || !d->sym || strcmp(d->sym->name,names[i]) || d->offset!=4*i || !d->ty || d->ty->kind!=kind ||
     l.leaves[i].legacy!=4*i || l.leaves[i].native!=native[i] || l.leaves[i].oldbytes!=4 || l.leaves[i].bytes!=(kind==Tint?4:8) || l.leaves[i].kind!=kind)goto done;
  v=d->ty;
  if(i<4){if(!v->tof || (i==1?!typedwmref(v->tof,"Draw","Pointer"):v->tof->kind!=(i==2?Tstring:Tint)))goto done;}
  else if(i==6){if(!typedwmref(v->tof,"Wmsrv","Window"))goto done;}
  else if(i==7){if(!typedwmref(v,"Wmsrv","Client"))goto done;}
  else if(i==8){
   if(!v->tof || v->tof->kind!=Ttuple)goto done;
   for(j=0,q=v->tof->ids;j<3;j++,q=q->next)if(!q || q->store!=Dfield || q->offset!=4*j || !q->ty ||
      (j<2?!typedwmref(q->ty,"Draw",j==0?"Point":"Image"):(q->ty->kind!=Tchan || !q->ty->tof || q->ty->tof->kind!=Tint)))goto done;
   if(q)goto done;
  }else if(i==12 && !typedwmref(v,"Draw","Wmcontext"))goto done;
 }
 for(;d;d=d->next)if(d->store!=Dfn || !d->ty || d->ty->kind!=Tfn)goto done;
 owner=*a;owner.offset=0;owner.native64type=t;
 if(!typedoperand(mode==Afpind?Afp:Amp,&owner,TypedRoot,0,0,p,base,wire,&actual) || actual!=Tref)goto done;
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
