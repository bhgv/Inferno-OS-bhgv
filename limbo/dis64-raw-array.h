#include "../include/native64-record-layout.h"

/* Layout-based mixed records share the owned array/borrow machinery. */
static unsigned int
typedmixedarray(Type*t,Dis64Layout*l)
{
 unsigned int r=0,w=0,i,c,gap=0;
 if(!t||(t->kind!=Tadt&&t->kind!=Ttuple)||!dis64layouttype(t,l))return 0;
 for(i=0;i<l->count&&typedmanagedkind(l->leaves[i].kind);i++)r++;
 w=l->count-r;if(!r||r>3||w>2||l->legacybytes!=4*l->count||l->alignment!=8)return 0;
 for(i=0;i<r;i++)if(l->leaves[i].legacy!=4*i||l->leaves[i].native!=8*i||l->leaves[i].oldbytes!=4||l->leaves[i].bytes!=8)return 0;
 if(!w)return r>=2&&l->bytes==8*r?r*4:0;
 if(w==2){if(l->leaves[r+1].native==8*r+8)gap=32;else if(l->leaves[r+1].native!=8*r+4)return 0;}
 c=16|(r<<2)|(w-1)|gap;
 for(i=0;i<w;i++)if(l->leaves[r+i].kind!=Tint||l->leaves[r+i].legacy!=4*(r+i)||l->leaves[r+i].native!=native64arraywordoffset(c,i)||l->leaves[r+i].oldbytes!=4||l->leaves[r+i].bytes!=4)return 0;
 return l->bytes==native64arraywidth(c)?c:0;
}
/* Existing raw records keep their canonical layout classes. */
static unsigned int
typedrawarray(Type*t)
{Dis64Layout l={0};Decl*d,*v;unsigned int i,c=0;if(!t||t->kind!=Tarray||!(t=t->tof))return 0;if(t->kind==Tint)return 1;
 c=typedmixedarray(t,&l);dis64layoutfree(&l);if(c)return c;
 if(typedwordgeometry(t,&l)&&l.bytes==16)c=3;dis64layoutfree(&l);if(c)return c;d=t->decl;
 if(t->kind!=Tadt||!d||!d->sym||strcmp(d->sym->name,"Delta")||!((d->dot&&d->dot->sym&&!strcmp(d->dot->sym->name,"Winplace"))||(!d->dot&&impdecl&&impdecl->sym&&!strcmp(impdecl->sym->name,"Winplace"))))return 0;
 if(!dis64layouttype(t,&l)||l.bytes!=12||l.legacybytes!=12||l.count!=3||l.alignment!=4)goto done;
 for(i=0,v=t->ids;i<3;i++,v=v->next){static char*names[]={"d","wid","coord"};if(!v||v->store!=Dfield||!v->sym||strcmp(v->sym->name,names[i])||!v->ty||v->ty->kind!=Tint||v->offset!=4*i||l.leaves[i].kind!=Tint||l.leaves[i].native!=4*i||l.leaves[i].legacy!=4*i||l.leaves[i].bytes!=4||l.leaves[i].oldbytes!=4)goto done;}if(v)goto done;c=2;
 done:dis64layoutfree(&l);return c;}
static unsigned int typedrawclass(Addr*a){return a->decl&&!a->offset&&a->reg==a->decl->offset?typedrawarray(a->native64type?a->native64type:a->decl->ty):0;}
static unsigned int typedrawwidth(unsigned int c){return native64arraywidth(c);}
static Type *typedrecordvalue(Addr*,int);
static int typedrecordowner(Addr*,int);
static int typedrecordoperand(int,Addr*,int,Dis64TypedBody*,long*,unsigned int*);
static int typedrawborrow(Dis64TypedBody*,Addr*,int,unsigned int);
static int typedarrayfield(unsigned int,unsigned int,unsigned int,int,unsigned int*);

/* The declared leaf, including its element type, determines the array class. */
static Type*
typedfieldarray(Addr *addr, int mode)
{
 Type *type;
 unsigned int cl;
 if(mode!=Afpind&&mode!=Ampind)
  return nil;
 type=typedrecordvalue(addr,mode);
 if(!type||type->kind!=Tarray||!type->tof)return nil;
 if(typedrecordowner(addr,mode))return type;
 /* Closed record protocols keep their existing array encodings. */
 cl=typedrawarray(type);
 return native64arrayroots(cl)>=2&&!native64arraywords(cl)?type:nil;
}

static unsigned int
typedarrayclass(Type *type)
{
 if(type&&type->kind==Tarray&&type->tof&&typedmanagedkind(type->tof->kind))
  return Native64ArrayRoot;
 return typedrawarray(type);
}

static unsigned int
typedrawfieldclass(Addr *addr, int mode)
{
 return typedarrayclass(typedfieldarray(addr,mode));
}

static int
typedrawoperand(Dis64TypedBody *p, unsigned int pc, unsigned int side,
                Addr *addr, int mode, int root, long *value, unsigned int *wire)
{
 int actual;
 if(mode!=Afpind&&mode!=Ampind)
  return typedoperand(mode,addr,root?Tarray:Tint,!root,0,p,value,wire,&actual);
 if((root&&!typedfieldarray(addr,mode))||
    !typedrecordlayoutoperand(mode,addr,root,p,value,wire))
  return 0;
 return typedrecordregister(p,pc,side,typedfieldowner(addr)->tof);
}

static int
 typedrawnew(Inst*in,Dis64TypedBody*p,unsigned int pc)
{Type *array=typedfieldarray(&in->d,in->dm);
 unsigned int c=array?typedarrayclass(array):typedrawclass(&in->d),sm,dm,i;long src,dst;Decl*d=in->m.decl;
 if(!array&&c)array=in->d.native64type?in->d.native64type:in->d.decl->ty;
 if(!c||in->branch||in->mm!=Adesc||!d||!d->desc||d->desc->id!=in->m.offset||d->desc->size!=native64arraylegacy(c)||!d->ty||!tequal(d->ty,array->tof))return typedfail(p,"raw-array-descriptor");
 if(native64arrayroots(c)){if(d->desc->nmap!=1||!d->desc->map||d->desc->map[0]!=native64arraymap(c))return typedfail(p,"mixed-array-descriptor");}
 else for(i=0;i<(unsigned int)d->desc->nmap;i++)if(d->desc->map[i])return typedfail(p,"raw-array-descriptor");
 if(!typedrawoperand(p,pc,0,&in->s,in->sm,0,&src,&sm)||!typedrawoperand(p,pc,1,&in->d,in->dm,1,&dst,&dm))return typedfail(p,"raw-array-allocation");p->code[pc]=(Dis64Instruction){INEWAZ,SRC(sm)|AXIMM|DST(dm),c,src,dst};p->returns[pc]=79;return 1;}

static int
typedrawindexslot(Dis64TypedBody *p, Inst *in, Type *array, unsigned int cl,
                  unsigned int pc, unsigned int last)
{
 unsigned int i,sm,dm,off=0,role=80;
 long src,dst;
 int borrow=typedrawborrow(p,&in->d,in->dm,pc);
 if(!cl||in->branch||in->mm!=Afp||!in->m.decl||in->m.offset||
    !typedrawoperand(p,pc-p->first,0,&in->s,in->sm,1,&src,&sm))
  return typedfail(p,"raw-array-index");
 if(borrow>=0){
  if((sm&AIND)||!typedarrayfield(p->rawclass[borrow],in->d.offset,4,0,&off))
   return typedfail(p,"raw-array-index-word");
  dst=48+8*borrow;
  dm=AFP|AIND;
  role=Native64TypedBorrowIndex;
 }else if(!typedrawoperand(p,pc-p->first,1,&in->d,in->dm,0,&dst,&dm))
  return typedfail(p,"raw-array-index");
 for(i=0;i<2;i++)
  if(p->rawclass[i]&&p->rawreg[i]==in->m.reg)
   break;
 if(i==2)
  for(i=0;i<2;i++)
   if(!p->rawclass[i]||p->rawuntil[i]<pc)
    break;
 if(i==2)
  return typedfail(p,"raw-array-address-pressure");
 p->rawreg[i]=in->m.reg;
 p->rawclass[i]=cl;
 p->rawtype[i]=array->tof;
 p->rawuntil[i]=last;
 p->code[pc-p->first]=(Dis64Instruction){IINDX,SRC(sm)|AXINF|DST(dm),(off<<16)|(cl<<8)|(48+8*i),src,dst};
 p->returns[pc-p->first]=role;
 return 1;
}

static int
typedarrayindexop(int op)
{
 return op==IINDX||op==IINDW||op==IINDB||op==IINDL||op==IINDF;
}

static int
typedrawindexprep(Inst *in, Inst *index)
{
 return in&&index&&(in->op==IADDW||in->op==ISUBW)&&in->dm==Afp&&
  !in->d.offset&&!in->branch&&typedarrayindexop(index->op)&&
  index->dm==Afp&&!index->d.offset&&index->d.reg==in->d.reg&&
  index->d.decl==in->d.decl&&typedrawclass(&index->s);
}

static unsigned int
typedrawlast(TypedMulti *c, Inst *index, unsigned int pc, unsigned int stop)
{
 unsigned int j,last=pc;
 Inst *use;
 for(j=pc+1;j<stop;j++){
  use=c->instructions[j];
  if(!use)break;
  if((use->sm==Afpind&&use->s.reg==index->m.reg)||
     (use->dm==Afpind&&use->d.reg==index->m.reg))
   last=j;
  if((typedarrayindexop(use->op)&&use->mm==Afp&&use->m.reg==index->m.reg)||
     (use->dm==Afp&&use->d.reg==index->m.reg))
   break;
 }
 return last;
}

static int
typedrawindex(TypedMulti*c,Dis64TypedBody*p,Inst*in,unsigned int pc,unsigned int stop)
{unsigned int cl=typedrawclass(&in->s),last;Type *array;
 if(!cl||(in->sm!=Afp&&in->sm!=Amp))return typedfail(p,"raw-array-index");
 array=in->s.native64type?in->s.native64type:in->s.decl->ty;
 last=typedrawlast(c,in,pc,stop);
 if(last==pc)return typedfail(p,"raw-array-unused-index");
 return typedrawindexslot(p,in,array,cl,pc,last);}
static int typedrawborrow(Dis64TypedBody*p,Addr*a,int mode,unsigned int pc){unsigned int i;if(mode!=Afpind)return -1;for(i=0;i<2;i++)if(p->rawclass[i]&&p->rawreg[i]==a->reg&&pc<=p->rawuntil[i])return i;return -1;}
static int
 typedrawspan(Dis64TypedBody*p,Addr*a,int mode,unsigned int n,long*out,unsigned int*wire)
{unsigned int i,m;long x;int k;Addr v;if(mode!=Afp&&mode!=Amp)return 0;for(i=0;i<n;i+=4){v=*a;v.reg+=i;v.offset=0;if(!typedoperand(mode,&v,Tint,0,0,p,&x,&m,&k)||k!=Tint||(i&&x!=*out+i))return 0;if(!i){*out=x;*wire=m;}}return 1;}
static int
typedarrayfield(unsigned int c,unsigned int old,unsigned int n,int root,unsigned int*off)
{
 unsigned int r=native64arrayroots(c),word;
 if(old%4)return 0;
 if(!r){if(root||old>native64arraywidth(c)||n>native64arraywidth(c)-old)return 0;*off=old;return 1;}
 if(old%4)return 0;
 if(root){if(n!=8||old/4>=r)return 0;*off=8*(old/4);return 1;}
 if(n!=4||old/4<r||(word=old/4-r)>=native64arraywords(c))return 0;
 *off=native64arraywordoffset(c,word);return 1;
}
static int
typedmixedspan(Dis64TypedBody*p,Addr*a,int mode,Type*t,long*out,unsigned int*wire)
{
 Dis64Layout l={0};unsigned int base,c;int ok=0;
 if((mode==Afp||mode==Amp)&&!a->offset&&(c=typedmixedarray(t,&l)))ok=dis64layoutbindresult(mode==Afp?&p->frame:&p->data,&l,a->reg,8,0,&base);
 dis64layoutfree(&l);if(ok){*out=base;*wire=mode==Afp?AFP:AMP;}return ok;
}
/* Resolve a declared nested field, retaining referent/payload identity. */
static Type*
typedarrayleaf(Type*t,unsigned int off)
{Decl*d;if(!t)return nil;if(t->kind!=Tadt&&t->kind!=Ttuple)return off?nil:t;for(d=t->ids;d;d=d->next)if(d->store==Dfield&&d->ty&&off>=(unsigned int)d->offset&&off<(unsigned int)(d->offset+d->ty->size))return typedarrayleaf(d->ty,off-d->offset);return nil;}
static int
typedarraysubtype(Type*owner,unsigned int off,Type*copy)
{Decl*d;if(!owner)return 0;if(!off&&tequal(owner,copy))return 1;if(owner->kind!=Tadt&&owner->kind!=Ttuple)return 0;for(d=owner->ids;d;d=d->next)if(d->store==Dfield&&d->ty&&off>=(unsigned int)d->offset&&off<(unsigned int)(d->offset+d->ty->size))return typedarraysubtype(d->ty,off-d->offset,copy);return 0;}
static int
typedarraycopyoffset(unsigned int owner,unsigned int old,unsigned int copy,unsigned int*out)
{unsigned int roots=native64arrayroots(owner);if(old%4||old/4>=roots)return 0;*out=8*(old/4);return native64arraycopyspan(owner,*out,copy);}

static int
typedmixedfield(Dis64TypedBody *p, Addr *a, int mode, Type *copy,
                unsigned int pc, unsigned int side, long *out, unsigned int *wire)
{
 Dis64Layout record={0},part={0};
 Type *owner;
 unsigned int i,base,last;
 int ok=0;

 if(!typedrecordlayoutoperand(mode,a,1,p,out,wire))
  return 0;
 owner=typedfieldowner(a)->tof;
 if(copy){
  if(!typedarraysubtype(owner,a->offset,copy)||!dis64layouttype(owner,&record)||
     !dis64layouttype(copy,&part)||
     !dis64layoutbindresult(&record,&part,a->offset,8,0,&base)||
     base!=(unsigned long)*out>>16)
   goto done;
  last=a->offset+part.legacybytes;
  for(i=0;i<record.count;i++)
   if(record.leaves[i].legacy>=last&&record.leaves[i].native<base+part.bytes)
    goto done;
 }
 ok=typedrecordregister(p,pc-p->first,side,owner);
done:
 dis64layoutfree(&record);
 dis64layoutfree(&part);
 return ok;
}

static int
typedmappeduse(Dis64TypedBody*p,Inst*in,unsigned int pc,long argument,int si,int di)
{
 unsigned int sm,dm,so=0,doff=0,copy=0,n,role=65;long src=0,dst=0;int k;Type*t=nil,*a,*b;Decl*d;Dis64Layout l={0};
 if(in->branch||(argument>=0&&(in->op!=IMOVP||si<0||in->dm!=Afpind)))
  return typedfail(p,"mixed-array-use-shape");
 if(in->op==IMOVMP){
  d=in->m.decl;if(in->mm!=Adesc||!d||!d->desc||d->desc->id!=in->m.offset||!(copy=typedmixedarray(d->ty,&l))||d->desc->size!=l.legacybytes||d->desc->nmap!=1||!d->desc->map||d->desc->map[0]!=native64arraymap(copy)){dis64layoutfree(&l);return typedfail(p,"mixed-array-copy-descriptor");}
  dis64layoutfree(&l);t=d->ty;n=native64arraywidth(copy);
 }else{if(in->op!=IMOVP||in->mm!=Anone)return typedfail(p,"mixed-array-root-opcode");n=8;}
 if(si>=0){
  if(copy?(!typedarraysubtype(p->rawtype[si],in->s.offset,t)||!typedarraycopyoffset(p->rawclass[si],in->s.offset,copy,&so)):!typedarrayfield(p->rawclass[si],in->s.offset,n,1,&so))return typedfail(p,"mixed-array-source-field");
  src=48+8*si;sm=AFP|AIND;
 }else if(typedmixedfield(p,&in->s,in->sm,t,pc,0,&src,&sm)){
  role=Native64TypedRecordArray;
 }else if(copy){if(!typedmixedspan(p,&in->s,in->sm,t,&src,&sm))return typedfail(p,"mixed-array-copy-source");}
 else if(!typedoperand(in->sm,&in->s,TypedRoot,0,0,p,&src,&sm,&k))return 0;
 if(argument>=0){dst=argument;dm=AFP|AIND;}
 else if(di>=0){
  if(copy?(!typedarraysubtype(p->rawtype[di],in->d.offset,t)||!typedarraycopyoffset(p->rawclass[di],in->d.offset,copy,&doff)):!typedarrayfield(p->rawclass[di],in->d.offset,n,1,&doff))return typedfail(p,"mixed-array-destination-field");
  dst=48+8*di;dm=AFP|AIND;
 }else if(!copy&&si>=0&&in->dm==Afpind&&in->d.reg==16&&!in->d.offset){
  a=typedarrayleaf(p->rawtype[si],in->s.offset);if(!a||!tequal(a,p->resulttype))return typedfail(p,"mixed-array-result-type");dst=32;dm=AFP|AIND;
 }else if(typedmixedfield(p,&in->d,in->dm,t,pc,1,&dst,&dm)){
  role=Native64TypedRecordArray;
 }else if(copy){if(!typedmixedspan(p,&in->d,in->dm,t,&dst,&dm))return typedfail(p,"mixed-array-copy-destination");}
 else if(!typedoperand(in->dm,&in->d,TypedRoot,0,0,p,&dst,&dm,&k))return 0;
 if(!copy){
  a=si>=0?typedarrayleaf(p->rawtype[si],in->s.offset):typedrecordvalue(&in->s,in->sm);
  b=argument>=0?a:di>=0?typedarrayleaf(p->rawtype[di],in->d.offset):
   dm==(AFP|AIND)&&dst==32?p->resulttype:typedrecordvalue(&in->d,in->dm);
  if(!a||!b||(!tequal(a,b)&&a->kind!=Tany))return typedfail(p,"mixed-array-root-type");
 }
 p->code[pc-p->first]=(Dis64Instruction){in->op,SRC(sm)|(copy?AXIMM:0)|DST(dm),(so<<24)|(doff<<16)|copy,src,dst};p->returns[pc-p->first]=role;return 1;
}
static int
 typedrawuse(Dis64TypedBody*p,Inst*in,unsigned int pc,unsigned int stop,long argument)
{int si=typedrawborrow(p,&in->s,in->sm,pc),di=argument>=0?-1:typedrawborrow(p,&in->d,in->dm,pc),branch=0,binary=0,k;unsigned int n=4,sm,dm,mm=0,so=0,doff=0;long src=0,dst=0,mid=0;
 if(si<0&&di<0)return 0;
 if(in->op==IMOVP||in->op==IMOVMP)return typedmappeduse(p,in,pc,argument,si,di);
 switch(in->op){case IMOVM:n=in->m.offset;if(in->mm!=Aimm||(n!=4&&n!=8&&n!=12&&n!=16))return typedfail(p,"raw-array-copy-width");mid=n;mm=AXIMM;break;
 case IHEADM:n=16;if(si>=0||di<0||in->mm!=Aimm||in->m.offset!=16||p->rawclass[di]!=3||!typedrectlisttype(&in->s))return typedfail(p,"raw-array-list-head");mid=16;mm=AXIMM;break;
 case IMOVW:if(in->mm!=Anone)return typedfail(p,"raw-array-word-copy");break;
 case IBEQW:case IBNEW:case IBLTW:case IBLEW:case IBGTW:case IBGEW:branch=1;binary=1;break;
 case IADDW:case ISUBW:case IMULW:case IDIVW:case IMODW:case IANDW:case IORW:case IXORW:case ISHLW:case ISHRW:binary=1;break;
 default:return typedfail(p,"raw-array-use-opcode");}
 if(si>=0){if(in->s.offset<0||!typedarrayfield(p->rawclass[si],in->s.offset,n,0,&so))return typedfail(p,"raw-array-source-field");src=48+8*si;sm=AFP|AIND;}
 else if(in->op==IHEADM){if(!typedoperand(in->sm,&in->s,Tlist,0,0,p,&src,&sm,&k))return 0;}
 else if(n>4){if(!typedrawspan(p,&in->s,in->sm,n,&src,&sm))return typedfail(p,"raw-array-source-span");}
 else if(!typedoperand(in->sm,&in->s,Tint,1,0,p,&src,&sm,&k))return 0;
 if(argument>=0){if(branch||argument<64||argument>65535||in->dm!=Afpind)return typedfail(p,"raw-array-call-argument");dst=argument;dm=AFP|AIND;}
 else if(branch){if(!in->branch||in->dm!=Apc||in->d.offset!=in->branch->pc||in->d.offset<(int)p->first||in->d.offset>=(int)stop)return typedfail(p,"raw-array-branch");dst=in->d.offset-p->first;dm=AIMM;}
 else if(si>=0 && in->dm==Afpind && in->d.reg==16 && !in->d.offset && in->op==IMOVW){
  if(p->resultkind!=Tint || p->resultpair || p->resultaggregate)return typedfail(p,"raw-array-result-type");dst=32;dm=AFP|AIND;
 }
 else if(di>=0){if(in->d.offset<0||!typedarrayfield(p->rawclass[di],in->d.offset,n,0,&doff))return typedfail(p,"raw-array-destination-field");dst=48+8*di;dm=AFP|AIND;}
 else if(n>4){if(!typedrawspan(p,&in->d,in->dm,n,&dst,&dm))return typedfail(p,"raw-array-destination-span");}
 else if(!typedoperand(in->dm,&in->d,Tint,0,0,p,&dst,&dm,&k))return 0;
 if(!branch&&in->branch)return typedfail(p,"raw-array-hidden-branch");if(binary&&in->mm!=Anone){unsigned int m;if(!typedoperand(in->mm,&in->m,Tint,1,1,p,&mid,&m,&k))return 0;mm=m==AIMM?AXIMM:m==AFP?AXINF:AXINM;}else if(branch)return typedfail(p,"raw-array-branch-middle");
 p->code[pc-p->first]=(Dis64Instruction){in->op,SRC(sm)|mm|DST(dm),(so<<24)|(doff<<16)|(mid&65535),src,dst};p->returns[pc-p->first]=65;return 1;}

/* Each address loaded through a mutable heap field has one adjacent user.
 * The VM dispatches both before a different process can replace the field. */
static int
typedrawfieldindex(TypedMulti *c, Dis64TypedBody *p, Inst *in,
                   unsigned int pc, unsigned int stop)
{
 Type *array=typedfieldarray(&in->s,in->sm);
 unsigned int cl=typedarrayclass(array);
 Inst *use;
 int source,destination;

 if(!cl||pc+1>=stop||typedrawlast(c,in,pc,stop)!=pc+1)
  return typedfail(p,"field-array-use-span");
 use=c->instructions[pc+1];
 if(!typedrawindexslot(p,in,array,cl,pc,pc+1))
  return 0;
 source=typedrawborrow(p,&use->s,use->sm,pc+1);
 destination=typedrawborrow(p,&use->d,use->dm,pc+1);
 if((source>=0)==(destination>=0))
  return typedfail(p,"field-array-use-owner");
 return typedrawuse(p,use,pc+1,stop,-1);
}

static int typedrawdelta(Type*t){Type a;memset(&a,0,sizeof(a));a.kind=Tarray;a.tof=t;return typedrawarray(&a)==2;}

static Type*
typedarrayaddress(Addr *addr, int mode)
{
 if(mode==Afp||mode==Amp)
  return addr->decl&&!addr->offset&&addr->reg==addr->decl->offset?
   (addr->native64type?addr->native64type:addr->decl->ty):nil;
 return typedfieldarray(addr,mode);
}

static int
typedrawslice(Inst*in,Dis64TypedBody*p,unsigned int pc)
{Type *array=typedarrayaddress(&in->d,in->dm);
 unsigned int c=typedarrayclass(array),sm,dm,mm;long src,dst,mid;int k;
 if(!c||in->branch||!typedrawoperand(p,pc,1,&in->d,in->dm,1,&dst,&dm))return typedfail(p,"raw-array-slice");
 if(in->op==ISLICELA){
  Type *source=typedarrayaddress(&in->s,in->sm);
  if(!source||!tequal(source,array)||!typedrawoperand(p,pc,0,&in->s,in->sm,1,&src,&sm))return typedfail(p,"raw-array-slice-source");
 }
 else if(!typedrawoperand(p,pc,0,&in->s,in->sm,0,&src,&sm))return 0;
 if(!typedoperand(in->mm,&in->m,Tint,1,1,p,&mid,&mm,&k))return 0;
 p->code[pc]=(Dis64Instruction){in->op,SRC(sm)|(mm==AIMM?AXIMM:mm==AFP?AXINF:AXINM)|DST(dm),(c<<24)|(mid&65535),src,dst};p->returns[pc]=79;return 1;}
