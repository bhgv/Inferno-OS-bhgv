/* Selected Wmclient.Window payload, derived from its complete trusted .m type.
 * Root fields and closed scalar transfers; nested dereferences stay closed. */
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

/* Scalars are relocated only after proving the complete Window record. */
static int
typedwindowwordbase(Addr*a,int mode,Dis64TypedBody*p,unsigned int pc,long*base,unsigned int*wire,long*field)
{static const unsigned int old[]={4,8,12,16,24,28,32,36,44,48,60,64,68,72,76,80,88,92,96,100};
 static const unsigned int native[]={8,12,16,20,32,36,40,44,56,60,80,84,88,92,96,100,112,116,120,124};
 Inst probe;unsigned int i;if(!a->native64type||a->native64type->kind!=Tint)return 0;for(i=0;i<20;i++)if(a->offset==old[i])break;if(i==20)return 0;
 memset(&probe,0,sizeof(probe));probe.op=IMOVP;probe.sm=mode;probe.s=*a;probe.s.offset=0;probe.mm=Anone;probe.dm=Afp;probe.d=*a;
 if(!typedwindowfield(&probe,p,pc,1,nil))return 0;*base=p->code[pc].source;*wire=USRC(p->code[pc].mode);*field=native[i];return 1;}
static int
typedwindowword(Inst*in,Dis64TypedBody*p,unsigned int pc,unsigned int stop)
{long base,field,value,middle=0,dest;unsigned int wire,sm,dm,mm=AXNON;int actual,branch;
 if(in->op==IMOVW&&(in->dm==Afpind||in->dm==Ampind)&&typedwindowowner(&in->d)){
  if(in->mm!=Anone||in->branch||!typedwindowwordbase(&in->d,in->dm,p,pc,&base,&wire,&field)||!typedoperand(in->sm,&in->s,Tint,1,0,p,&value,&sm,&actual))return typedfail(p,"window-word-store");
  p->code[pc]=(Dis64Instruction){IMOVW,SRC(sm)|DST(wire|AIND),field,value,base};p->returns[pc]=88;return 1;
 }
 branch=in->op==IBEQW||in->op==IBNEW||in->op==IBLTW||in->op==IBLEW||in->op==IBGTW||in->op==IBGEW;
 if((in->op!=IMOVW&&!typedwordargop(in->op)&&!branch)||!typedwindowwordbase(&in->s,in->sm,p,pc,&base,&wire,&field)||base>65535)return typedfail(p,"window-word-load");
 if(in->op!=IMOVW){unsigned int mode;if(!typedoperand(in->mm==Anone?in->dm:in->mm,in->mm==Anone?&in->d:&in->m,Tint,1,1,p,&middle,&mode,&actual))return 0;mm=mode==AIMM?AXIMM:mode==AFP?AXINF:AXINM;}else if(in->mm!=Anone)return 0;
 if(branch){if(in->dm!=Apc||in->d.offset<(long)p->first||in->d.offset>=stop)return typedfail(p,"window-word-branch");dest=in->d.offset-p->first;dm=AIMM;}else if(!typedoperand(in->dm,&in->d,Tint,0,0,p,&dest,&dm,&actual))return 0;
 p->code[pc]=(Dis64Instruction){in->op,SRC(wire|AIND)|mm|DST(dm),middle,(field<<16)|base,dest};p->returns[pc]=89;return 1;}

static int
typedwindowwordargument(Inst*in,Inst*frame,Dis64TypedBody*p,unsigned int pc,unsigned int old,unsigned int offset)
{long base,field,middle=0;unsigned int wire,mm=AXNON,mode;int actual;
 if((in->op!=IMOVW&&!typedwordargop(in->op))||in->branch||in->dm!=Afpind||in->d.reg!=frame->d.reg||in->d.offset!=old||!typedwindowwordbase(&in->s,in->sm,p,pc,&base,&wire,&field)||base>65535)return typedfail(p,"window-word-argument");
 if(in->op!=IMOVW){if(!typedoperand(in->mm,&in->m,Tint,1,1,p,&middle,&mode,&actual))return 0;mm=mode==AIMM?AXIMM:mode==AFP?AXINF:AXINM;}
 p->code[pc]=(Dis64Instruction){in->op,SRC(wire|AIND)|mm|DST(AFP|AIND),middle,(field<<16)|base,offset};p->returns[pc]=86;return 1;}

/* Exact whole geometry leaves inside the complete canonical Window. */
static int
typedwindowgeometrybase(Addr*a,int mode,Dis64TypedBody*p,unsigned int pc,long*base,unsigned int*wire,long*field,unsigned int*bytes)
{Dis64Layout l={0};Inst probe;unsigned int old=a->offset;int ok=0;
 if(!typedwindowowner(a)||!typedwordgeometry(a->native64type,&l))goto done;*bytes=l.bytes;
 if(l.bytes==16){if(old!=4&&old!=24&&old!=68)goto done;*field=old==4?8:old==24?32:88;}
 else{switch(old){case 4:*field=8;break;case 12:*field=16;break;case 24:*field=32;break;case 32:*field=40;break;case 60:*field=80;break;case 68:*field=88;break;case 76:*field=96;break;case 96:*field=120;break;default:goto done;}}
 memset(&probe,0,sizeof(probe));probe.op=IMOVP;probe.sm=mode;probe.s=*a;probe.s.offset=0;probe.mm=Anone;probe.dm=Afp;probe.d=*a;if(!typedwindowfield(&probe,p,pc,1,nil))goto done;*base=p->code[pc].source;*wire=USRC(p->code[pc].mode);ok=1;
 done:dis64layoutfree(&l);return ok;}
/* A named formal keeps this heap result alive across suspended callees.
 * LEA records a field-value type; prove the original formal owner separately. */
static int
typedwindowgeometrybinding(Inst*b,Type*t,Dis64TypedBody*p,unsigned int pc,unsigned int*local,unsigned int*contract)
{long base,field;unsigned int wire,bytes;Addr a=b->s;if(b->sm!=Afpind||b->s.reg==16||!a.decl||a.decl->store!=Darg||a.reg!=a.decl->offset)return 0;a.native64base=a.decl->ty;a.native64type=a.decl->ty;if(!typedwindowowner(&a))return 0;a.native64type=t;if(!typedwindowgeometrybase(&a,b->sm,p,pc,&base,&wire,&field,&bytes)||(wire&~AIND)!=AFP)return 0;
 *local=base;*contract=bytes==8?96:112;p->code[pc]=(Dis64Instruction){ILEA,SRC(AFP|AIND)|DST(AFP|AIND),field,base,32};p->returns[pc]=81;return 1;}
static int
typedwindowgeometrycopy(Inst*in,Dis64TypedBody*p,Dis64Instruction*out,uchar*ret)
{Addr*a,*direct,word;unsigned int read,mode,bytes,wire,i;long base,field,value=0,part;int actual;
 read=in->sm==Afpind||in->sm==Ampind;a=read?&in->s:&in->d;direct=read?&in->d:&in->s;mode=read?in->dm:in->sm;
 if(in->op!=IMOVM||in->mm!=Aimm||in->branch||(mode!=Afp&&mode!=Amp)||!typedwindowgeometrybase(a,read?in->sm:in->dm,p,in->pc-p->first,&base,&wire,&field,&bytes)||in->m.offset!=bytes)return 0;
 {Dis64Layout l={0};int ok=typedwordgeometry(direct->native64type,&l)&&l.bytes==bytes;dis64layoutfree(&l);if(!ok)return 0;}
 for(i=0;i<bytes;i+=4){word=*direct;word.reg+=i;word.offset=0;if(!typedoperand(mode,&word,Tint,0,0,p,&part,&wire,&actual)||actual!=Tint||(i&&part!=value+i))return 0;if(!i)value=part;}
 *out=(Dis64Instruction){IMOVM,read?SRC((in->sm==Afpind?AFP:AMP)|AIND)|AXIMM|DST(mode==Afp?AFP:AMP):SRC(mode==Afp?AFP:AMP)|AXIMM|DST((in->dm==Afpind?AFP:AMP)|AIND),(field<<8)|bytes,read?base:value,read?value:base};*ret=read?83:82;return 1;}
/* Fresh root results retain their named formal/local Window owner across the call. */
static int
typedwindowrootbinding(Inst*b,Type*t,Dis64TypedBody*p,unsigned int pc,unsigned int*base)
{Addr a=b->s;Inst probe;Decl*d;unsigned int index;if(b->sm!=Afpind||!a.decl||(a.decl->store!=Darg&&(a.decl->store!=Dlocal||!a.decl->sym||a.decl->sym->name[0]=='.'))||a.reg!=a.decl->offset||!t||(t->kind!=Tref&&t->kind!=Tchan))return 0;a.native64base=a.decl->ty;if(!typedwindowowner(&a))return 0;for(d=a.decl->ty->tof->ids;d;d=d->next)if(d->store==Dfield&&d->offset==a.offset)break;if(!d||!tequal(d->ty,t))return 0;memset(&probe,0,sizeof(probe));probe.op=IMOVP;probe.mm=Anone;probe.sm=Afpind;probe.s=a;probe.s.native64type=t;probe.dm=Afp;probe.d=a;if(!typedwindowfield(&probe,p,pc,1,&index))return 0;*base=p->code[pc].source;p->code[pc]=(Dis64Instruction){ILEA,SRC(AFP|AIND)|DST(AFP|AIND),p->contextfields[pc],*base,32};p->returns[pc]=60;return 1;}
