/* Selected Wmclient.Window payload, derived from its complete trusted .m type.
 * Root fields only; scalar/aggregate fields and nested dereferences stay closed. */
static int
typedwindowowner(Addr*a)
{
 Type*t=typedfieldowner(a);Decl*d;
 return t && t->kind==Tref && t->tof && t->tof->kind==Tadt && (d=t->tof->decl) && d->sym &&
  !strcmp(d->sym->name,"Window") && d->dot && d->dot->sym && !strcmp(d->dot->sym->name,"Wmclient");
}
static int
typedwindowfield(Inst*in,Dis64TypedBody*p,unsigned int pc,int probe,unsigned int*which)
{
 static const unsigned int old[]={0,4,8,12,16,20,24,28,32,36,40,44,48,52,56,60,64,68,72,76,80,84,88,92,96,100};
 static const unsigned int native[]={0,8,12,16,20,24,32,36,40,44,48,56,60,64,72,80,84,88,92,96,100,104,112,116,120,124};
 static const unsigned int roots[]={0,5,10,13,14,21};
 Addr*a,owner;Dis64Layout layout={0};unsigned int i,j,index=6,bm,vm=AFP;long base,value=0;int read,kind,actual,ok=0;
 read=in->sm==Afpind || in->sm==Ampind;a=read?&in->s:&in->d;
 if(!typedwindowowner(a) || in->op!=IMOVP || in->mm!=Anone || in->branch ||
    (read?(in->dm!=Afp && in->dm!=Amp):(in->dm!=Afpind && in->dm!=Ampind)) ||
    !dis64layouttype(typedfieldowner(a)->tof,&layout) || layout.legacybytes!=104 || layout.bytes!=128 || layout.alignment!=8 || layout.count!=26)goto done;
 for(i=0;i<26;i++){
  kind=Tint;for(j=0;j<6;j++)if(roots[j]==i)kind=i==13?Tchan:Tref;
  if(layout.leaves[i].legacy!=old[i] || layout.leaves[i].native!=native[i] || layout.leaves[i].oldbytes!=4 ||
     layout.leaves[i].bytes!=(kind==Tint?4:8) || layout.leaves[i].kind!=kind)goto done;
 }
 for(j=0;j<6;j++)if(a->offset==old[roots[j]])index=j;
 if(index==6)goto done;kind=index==3?Tchan:Tref;owner=*a;owner.offset=0;owner.native64type=typedfieldowner(a);
 if(!typedoperand((read?in->sm:in->dm)==Afpind?Afp:Amp,&owner,TypedRoot,0,0,p,&base,&bm,&actual) || actual!=Tref)goto done;
 if(!probe && (!typedoperand(read?in->dm:in->sm,read?&in->d:&in->s,TypedRoot,0,0,p,&value,&vm,&actual) ||
    (actual!=kind && actual!=Tany) || (vm!=AFP && vm!=AMP)))goto done;
 if(!p->contextfields){p->contextfields=allocmem(p->count*sizeof(long));memset(p->contextfields,0,p->count*sizeof(long));}
 p->contextfields[pc]=native[roots[index]];
 p->code[pc]=(Dis64Instruction){IMOVP,read?SRC(bm|AIND)|DST(vm):SRC(vm)|DST(bm|AIND),0,read?base:value,read?value:base};
 if(which)*which=index;ok=1;
 done:dis64layoutfree(&layout);return ok?1:typedfail(p,"window-field-roles");
}
