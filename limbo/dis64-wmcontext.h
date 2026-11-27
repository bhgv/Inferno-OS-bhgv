/* Exact Draw.Wmcontext owner and all seven classic/native reference fields. */
static int
typedwmref(Type*t,char*module,char*name)
{
 Decl*d;return t && t->kind==Tref && t->tof && t->tof->kind==Tadt && (d=t->tof->decl) && d->sym && d->dot && d->dot->sym && !strcmp(d->sym->name,name) && !strcmp(d->dot->sym->name,module);
}
static int
typedwmcontextaddress(Addr*a,int mode,Dis64TypedBody*p,long*base,unsigned int*wire,long*field)
{
 static char*names[]={"kbd","ptr","ctl","wctl","images","connfd","ctxt"};
 Type*t=typedfieldowner(a);Decl*d;Addr owner;Dis64Layout layout={0};unsigned int i;int actual,ok=0;
 if((mode!=Afpind && mode!=Ampind) || !typedwmref(t,"Draw","Wmcontext") || a->offset<0 || a->offset>24 || a->offset%4 ||
    !dis64layouttype(t->tof,&layout) || layout.legacybytes!=28 || layout.bytes!=56 || layout.alignment!=8 || layout.count!=7)goto done;
 for(i=0,d=t->tof->ids;i<7;i++,d=d->next){
  if(!d || d->store!=Dfield || !d->sym || strcmp(d->sym->name,names[i]) || d->offset!=4*i || !d->ty ||
     layout.leaves[i].legacy!=4*i || layout.leaves[i].native!=8*i || layout.leaves[i].oldbytes!=4 || layout.leaves[i].bytes!=8 || layout.leaves[i].kind!=(i<5?Tchan:Tref))goto done;
  if(i<5){if(d->ty->kind!=Tchan || !d->ty->tof)goto done;
   if(i==0?d->ty->tof->kind!=Tint:i==2||i==3?d->ty->tof->kind!=Tstring:!typedwmref(d->ty->tof,"Draw",i==1?"Pointer":"Image"))goto done;
  }else if(!typedwmref(d->ty,i==5?"Sys":"Draw",i==5?"FD":"Context"))goto done;
 }
 if(d)goto done;owner=*a;owner.offset=0;owner.native64type=t;
 if(!typedoperand(mode==Afpind?Afp:Amp,&owner,TypedRoot,0,0,p,base,wire,&actual) || actual!=Tref)goto done;
 *wire|=AIND;*field=2*a->offset;ok=1;
 done:dis64layoutfree(&layout);return ok;
}

static int
typedwmcontextchannel(Addr*a,int mode,Dis64TypedBody*p,long*base,unsigned int*wire,long*field)
{return a->offset<=16&&typedwmcontextaddress(a,mode,p,base,wire,field);}
