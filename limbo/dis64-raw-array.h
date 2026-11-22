/* Closed raw arrays: Word, Winplace.Delta and Draw.Rect. */
static unsigned int
typedrawarray(Type*t)
{Dis64Layout l={0};Decl*d,*v;unsigned int i,c=0;if(!t||t->kind!=Tarray||!(t=t->tof))return 0;if(t->kind==Tint)return 1;
 if(typedwordgeometry(t,&l)&&l.bytes==16)c=3;dis64layoutfree(&l);if(c)return c;d=t->decl;
 if(t->kind!=Tadt||!d||!d->sym||strcmp(d->sym->name,"Delta")||!((d->dot&&d->dot->sym&&!strcmp(d->dot->sym->name,"Winplace"))||(!d->dot&&impdecl&&impdecl->sym&&!strcmp(impdecl->sym->name,"Winplace"))))return 0;
 if(!dis64layouttype(t,&l)||l.bytes!=12||l.legacybytes!=12||l.count!=3||l.alignment!=4)goto done;
 for(i=0,v=t->ids;i<3;i++,v=v->next){static char*names[]={"d","wid","coord"};if(!v||v->store!=Dfield||!v->sym||strcmp(v->sym->name,names[i])||!v->ty||v->ty->kind!=Tint||v->offset!=4*i||l.leaves[i].kind!=Tint||l.leaves[i].native!=4*i||l.leaves[i].legacy!=4*i||l.leaves[i].bytes!=4||l.leaves[i].oldbytes!=4)goto done;}if(v)goto done;c=2;
 done:dis64layoutfree(&l);return c;}
static unsigned int typedrawclass(Addr*a){return a->decl&&!a->offset&&a->reg==a->decl->offset?typedrawarray(a->native64type?a->native64type:a->decl->ty):0;}
static unsigned int typedrawwidth(unsigned int c){return c==1?4:c==2?12:c==3?16:0;}
static int
 typedrawnew(Inst*in,Dis64TypedBody*p,unsigned int pc)
{unsigned int c=typedrawclass(&in->d),sm,dm,i;long src,dst;int k;Decl*d=in->m.decl;
 if(!c||in->branch||in->mm!=Adesc||!d||!d->desc||d->desc->id!=in->m.offset||d->desc->size!=typedrawwidth(c)||!d->ty||d->ty!=(in->d.native64type?in->d.native64type:in->d.decl->ty)->tof)return typedfail(p,"raw-array-descriptor");for(i=0;i<(unsigned int)d->desc->nmap;i++)if(d->desc->map[i])return typedfail(p,"raw-array-descriptor");
 if(!typedoperand(in->sm,&in->s,Tint,1,0,p,&src,&sm,&k)||!typedoperand(in->dm,&in->d,Tarray,0,0,p,&dst,&dm,&k)||(dm!=AFP&&dm!=AMP))return typedfail(p,"raw-array-allocation");p->code[pc]=(Dis64Instruction){INEWAZ,SRC(sm)|AXIMM|DST(dm),c,src,dst};p->returns[pc]=79;return 1;}
static int
 typedrawindex(TypedMulti*c,Dis64TypedBody*p,Inst*in,unsigned int pc,unsigned int stop)
{unsigned int cl=typedrawclass(&in->s),i,j,last=pc,sm,dm;long src,dst;int k;Inst*v;
 if(!cl||in->branch||(in->sm!=Afp&&in->sm!=Amp)||in->mm!=Afp||!in->m.decl||in->m.offset||!typedoperand(in->sm,&in->s,Tarray,0,0,p,&src,&sm,&k)||!typedoperand(in->dm,&in->d,Tint,1,0,p,&dst,&dm,&k))return typedfail(p,"raw-array-index");
 for(j=pc+1;j<stop;j++){v=c->instructions[j];if(!v)break;if(((v->op==IINDX||v->op==IINDW||v->op==IINDB||v->op==IINDL)&&v->mm==Afp&&v->m.reg==in->m.reg)||(v->dm==Afp&&v->d.reg==in->m.reg))break;if((v->sm==Afpind&&v->s.reg==in->m.reg)||(v->dm==Afpind&&v->d.reg==in->m.reg))last=j;}
 if(last==pc)return typedfail(p,"raw-array-unused-index");for(i=0;i<2;i++)if(p->rawclass[i]&&p->rawreg[i]==in->m.reg)break;if(i==2)for(i=0;i<2;i++)if(!p->rawclass[i]||p->rawuntil[i]<pc)break;if(i==2)return typedfail(p,"raw-array-address-pressure");
 p->rawreg[i]=in->m.reg;p->rawclass[i]=cl;p->rawuntil[i]=last;p->code[pc-p->first]=(Dis64Instruction){IINDX,SRC(sm)|AXINF|DST(dm),(cl<<8)|(48+8*i),src,dst};p->returns[pc-p->first]=80;return 1;}
static int typedrawborrow(Dis64TypedBody*p,Addr*a,int mode,unsigned int pc){unsigned int i;if(mode!=Afpind)return -1;for(i=0;i<2;i++)if(p->rawclass[i]&&p->rawreg[i]==a->reg&&pc<=p->rawuntil[i])return i;return -1;}
static int
 typedrawspan(Dis64TypedBody*p,Addr*a,int mode,unsigned int n,long*out,unsigned int*wire)
{unsigned int i,m;long x;int k;Addr v;if(mode!=Afp&&mode!=Amp)return 0;for(i=0;i<n;i+=4){v=*a;v.reg+=i;v.offset=0;if(!typedoperand(mode,&v,Tint,0,0,p,&x,&m,&k)||k!=Tint||(i&&x!=*out+i))return 0;if(!i){*out=x;*wire=m;}}return 1;}
static int
 typedrawuse(Dis64TypedBody*p,Inst*in,unsigned int pc,unsigned int stop,long argument)
{int si=typedrawborrow(p,&in->s,in->sm,pc),di=argument>=0?-1:typedrawborrow(p,&in->d,in->dm,pc),branch=0,binary=0,k;unsigned int n=4,sm,dm,mm=0,so=0,doff=0;long src=0,dst=0,mid=0;
 if(si<0&&di<0)return 0;
 switch(in->op){case IMOVM:n=in->m.offset;if(in->mm!=Aimm||(n!=4&&n!=8&&n!=12&&n!=16))return typedfail(p,"raw-array-copy-width");mid=n;mm=AXIMM;break;
 case IHEADM:n=16;if(si>=0||di<0||in->mm!=Aimm||in->m.offset!=16||p->rawclass[di]!=3||!typedrectlisttype(&in->s))return typedfail(p,"raw-array-list-head");mid=16;mm=AXIMM;break;
 case IMOVW:if(in->mm!=Anone)return typedfail(p,"raw-array-word-copy");break;
 case IBEQW:case IBNEW:case IBLTW:case IBLEW:case IBGTW:case IBGEW:branch=1;binary=1;break;
 case IADDW:case ISUBW:case IMULW:case IDIVW:case IMODW:case IANDW:case IORW:case IXORW:case ISHLW:case ISHRW:binary=1;break;
 default:return typedfail(p,"raw-array-use-opcode");}
 if(si>=0){if(in->s.offset<0||in->s.offset%4||(unsigned long)in->s.offset>typedrawwidth(p->rawclass[si])||n>typedrawwidth(p->rawclass[si])-in->s.offset)return typedfail(p,"raw-array-source-field");src=48+8*si;so=in->s.offset;sm=AFP|AIND;}
 else if(in->op==IHEADM){if(!typedoperand(in->sm,&in->s,Tlist,0,0,p,&src,&sm,&k))return 0;}
 else if(n>4){if(!typedrawspan(p,&in->s,in->sm,n,&src,&sm))return typedfail(p,"raw-array-source-span");}
 else if(!typedoperand(in->sm,&in->s,Tint,1,0,p,&src,&sm,&k))return 0;
 if(argument>=0){if(branch||argument<64||argument>65535||in->dm!=Afpind)return typedfail(p,"raw-array-call-argument");dst=argument;dm=AFP|AIND;}
 else if(branch){if(!in->branch||in->dm!=Apc||in->d.offset!=in->branch->pc||in->d.offset<(int)p->first||in->d.offset>=(int)stop)return typedfail(p,"raw-array-branch");dst=in->d.offset-p->first;dm=AIMM;}
 else if(si>=0 && in->dm==Afpind && in->d.reg==16 && !in->d.offset && in->op==IMOVW){
  if(p->resultkind!=Tint || p->resultpair || p->resultaggregate)return typedfail(p,"raw-array-result-type");dst=32;dm=AFP|AIND;
 }
 else if(di>=0){if(in->d.offset<0||in->d.offset%4||(unsigned long)in->d.offset>typedrawwidth(p->rawclass[di])||n>typedrawwidth(p->rawclass[di])-in->d.offset)return typedfail(p,"raw-array-destination-field");dst=48+8*di;doff=in->d.offset;dm=AFP|AIND;}
 else if(n>4){if(!typedrawspan(p,&in->d,in->dm,n,&dst,&dm))return typedfail(p,"raw-array-destination-span");}
 else if(!typedoperand(in->dm,&in->d,Tint,0,0,p,&dst,&dm,&k))return 0;
 if(!branch&&in->branch)return typedfail(p,"raw-array-hidden-branch");if(binary&&in->mm!=Anone){unsigned int m;if(!typedoperand(in->mm,&in->m,Tint,1,1,p,&mid,&m,&k))return 0;mm=m==AIMM?AXIMM:m==AFP?AXINF:AXINM;}else if(branch)return typedfail(p,"raw-array-branch-middle");
 p->code[pc-p->first]=(Dis64Instruction){in->op,SRC(sm)|mm|DST(dm),(so<<24)|(doff<<16)|(mid&65535),src,dst};p->returns[pc-p->first]=65;return 1;}

static int typedrawdelta(Type*t){Type a;memset(&a,0,sizeof(a));a.kind=Tarray;a.tof=t;return typedrawarray(&a)==2;}
static int
typedrawslice(Inst*in,Dis64TypedBody*p,unsigned int pc)
{unsigned int c=typedrawclass(&in->d),sm,dm,mm;long src,dst,mid;int k;
 if(!c||in->branch||!typedoperand(in->dm,&in->d,Tarray,0,0,p,&dst,&dm,&k)||(dm!=AFP&&dm!=AMP))return typedfail(p,"raw-array-slice");
 if(in->op==ISLICELA){if(typedrawclass(&in->s)!=c||!typedoperand(in->sm,&in->s,Tarray,0,0,p,&src,&sm,&k)||(sm!=AFP&&sm!=AMP))return typedfail(p,"raw-array-slice-source");}
 else if(!typedoperand(in->sm,&in->s,Tint,1,0,p,&src,&sm,&k))return 0;
 if(!typedoperand(in->mm,&in->m,Tint,1,1,p,&mid,&mm,&k))return 0;
 p->code[pc]=(Dis64Instruction){in->op,SRC(sm)|(mm==AIMM?AXIMM:mm==AFP?AXINF:AXINM)|DST(dm),(c<<24)|(mid&65535),src,dst};p->returns[pc]=79;return 1;}
