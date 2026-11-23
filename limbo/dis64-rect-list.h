/* Raw Rect nodes contain no payload roots; their tail remains a native List root. */
static int
typedrectlisttype(Addr*a)
{
 Type*t=a->decl?a->decl->ty:nil;Dis64Layout l={0};int ok=0;
 if(a->native64type && a->decl && a->decl->store==Dlocal && a->decl->sym && a->decl->sym->name[0]=='.' && a->decl->sym->name[1]=='t')t=a->native64type;
 if(a->decl && t && t->kind==Tlist && a->reg==a->decl->offset && !a->offset && typedwordgeometry(t->tof,&l) && l.bytes==16)ok=1;
 dis64layoutfree(&l);return ok;
}
static int
typedrectlist(Inst*in,Dis64TypedBody*p,Dis64Instruction*out,uchar*returned)
{
 Dis64Layout l={0};Addr a;Type*t;unsigned int sm=0,dm=0,i,wire;long source=0,destination=0,part;int actual,cons=in->op==ICONSM,head=in->op==IHEADM,ok=0;
 if(in->branch || (!cons&&!typedrectlisttype(&in->s)) || (!head&&!typedrectlisttype(&in->d)))goto done;
 if(in->op==ITAIL){if(in->mm!=Anone || !typedoperand(in->sm,&in->s,Tlist,0,0,p,&source,&sm,&actual) || actual!=Tlist || !typedoperand(in->dm,&in->d,Tlist,0,0,p,&destination,&dm,&actual) || actual!=Tlist)goto done;*out=(Dis64Instruction){ITAIL,SRC(sm)|DST(dm),0,source,destination};*returned=250;ok=1;goto done;}
 if((!cons&&!head) || in->mm!=Aimm || in->m.offset!=16)goto done;
 a=cons?in->s:in->d;t=a.native64type?a.native64type:a.decl?a.decl->ty:nil;
 if(cons && t && t->kind==Ttuple){if(!dis64layouttype(t,&l)||l.bytes!=16||l.legacybytes!=16||l.count!=4)goto done;for(i=0;i<4;i++)if(l.leaves[i].kind!=Tint||l.leaves[i].bytes!=4||l.leaves[i].native!=4*i||l.leaves[i].legacy!=4*i)goto done;}
 else if(!typedwordgeometry(t,&l) || l.bytes!=16)goto done;
 if(cons && (in->sm==Afpind || in->sm==Ampind)){
  if(in->s.offset!=4 || !typedwmsrvwindowbase(&in->s,in->sm,p,&source,&sm))goto done;
 }else{
  int mode=cons?in->sm:in->dm;
  if(mode!=Afp && mode!=Amp)goto done;
  for(i=0;i<16;i+=4){a=cons?in->s:in->d;a.reg+=i;a.offset=0;if(!typedoperand(mode,&a,Tint,0,0,p,&part,&wire,&actual) || actual!=Tint || (i&&part!=(cons?source:destination)+i))goto done;if(!i){if(cons){source=part;sm=wire;}else{destination=part;dm=wire;}}}
 }
 if(cons){if(!typedoperand(in->dm,&in->d,Tlist,0,0,p,&destination,&dm,&actual) || actual!=Tlist)goto done;}
 else if(!typedoperand(in->sm,&in->s,Tlist,0,0,p,&source,&sm,&actual) || actual!=Tlist)goto done;
 *out=(Dis64Instruction){in->op,SRC(sm)|AXIMM|DST(dm),16,source,destination};*returned=cons?248:249;ok=1;
 done:dis64layoutfree(&l);return ok?1:typedfail(p,"rect-list-geometry");
}

/* Aggregate list elements use the same natural layout as local records. */
static Type*
typedaggregatelistelement(Addr *a, int mode)
{
 Type *t;
 if(mode==Afpind||mode==Ampind)t=typedrecordvalue(a,mode);
 else{
  if(!a->decl||a->reg!=a->decl->offset||a->offset)return nil;
  t=typedloadvaluetype(a);
 }
 if(!t||t->kind!=Tlist||!(t=t->tof))return nil;
 return t->kind==Tadt||t->kind==Ttuple?t:nil;
}

static int
typedaggregatelistdescriptor(Inst *in, Type *element, Dis64Layout *part, long *shape)
{
 Decl *d=in->m.decl;
 return in->mm==Adesc&&d&&d->desc&&d->desc->id==in->m.offset&&
        typedrecordlayoutdescriptor(element,d->desc,part,shape)&&native64recordmask(*shape);
}

/* A nil initializer can occupy a compiler temporary whose declared kind is
 * Tany. Its mapped slot still has the destination field's pointer geometry. */
static int
typedaggregatelistbind(Dis64Layout *storage, Dis64Layout *part, unsigned int old,
                       int cons, int relocate, unsigned int *base)
{
 unsigned int i,first;
 if(cons){
  for(first=0;first<storage->count;first++)if(storage->leaves[first].legacy==old)break;
  if(part->count>storage->count-first)return 0;
  for(i=0;i<part->count;i++)
   if(storage->leaves[first+i].kind==Tany&&typedmanagedkind(part->leaves[i].kind))
    part->leaves[i].kind=Tany;
 }
 return dis64layoutbindresult(storage,part,old,8,relocate,base);
}

/* Reserve padding once before instruction lowering, so destructuring cannot
 * leave the next Word in the list element's tail padding. */
static int
typedaggregatelistframe(TypedMulti *c, Decl *fn, Dis64TypedBody *p, unsigned int stop)
{
 unsigned int pc,base,last=32;
 int cons,ok;
 long shape;
 Inst *in;
 Addr *value;
 Decl *arg;
 for(arg=fn->ty->ids;arg;arg=arg->next)if(arg->offset+arg->ty->size>last)last=arg->offset+arg->ty->size;
 for(pc=p->first;pc<stop;pc++){
  Dis64Layout part={0};
  in=c->instructions[pc];
  if(!in||(in->op!=ICONSMP&&in->op!=IHEADMP))continue;
  cons=in->op==ICONSMP;
  value=cons?&in->s:&in->d;
  if((cons?in->sm:in->dm)!=Afp||value->offset||value->reg<last)continue;
  p->badpc=pc;
  ok=typedaggregatelistdescriptor(in,typedaggregatelistelement(cons?&in->d:&in->s,cons?in->dm:in->sm),&part,&shape)&&
     typedaggregatelistbind(&p->frame,&part,value->reg,cons,1,&base);
  dis64layoutfree(&part);
  if(!ok)return typedfail(p,"aggregate-list-frame");
 }
 return 1;
}

static int
typedaggregatelist(Inst *in, Dis64TypedBody *p, unsigned int pc)
{
 int cons=in->op==ICONSMP,tail=in->op==ITAIL,actual,valuemode,ok=0;
 int listform=cons?in->dm:in->sm,indirect=listform==Afpind||listform==Ampind;
 unsigned int listmode,mode,base,i;
 long list,other,shape;
 Addr *value=cons?&in->s:&in->d;
 Addr *listaddr=cons?&in->d:&in->s;
 Type *element=typedaggregatelistelement(listaddr,listform),*dest;
 Dis64Layout part={0},*storage;
 if(in->branch||!element)goto done;
 if(indirect){
  if(tail||!typedrecordlayoutoperand(listform,listaddr,1,p,&list,&listmode)||
     !typedrecordregister(p,pc,cons,typedfieldowner(listaddr)->tof))goto done;
 }else if(!typedoperand(listform,listaddr,Tlist,0,0,p,&list,&listmode,&actual))goto done;
 if(tail){
  dest=typedaggregatelistelement(&in->d,in->dm);
  if(in->mm!=Anone||!dest||!tequal(dest,element)||
     !typedoperand(in->dm,&in->d,Tlist,0,0,p,&other,&mode,&actual))goto done;
  p->code[pc]=(Dis64Instruction){ITAIL,SRC(listmode)|DST(mode),0,list,other};
  p->returns[pc]=250;ok=1;goto done;
 }
 valuemode=cons?in->sm:in->dm;
 if(!typedaggregatelistdescriptor(in,element,&part,&shape)||
    (valuemode!=Afp&&valuemode!=Amp)||value->offset||value->reg<0)goto done;
 storage=valuemode==Afp?&p->frame:&p->data;
 if(!typedaggregatelistbind(storage,&part,value->reg,cons,0,&base))goto done;
 for(i=0;i<storage->count;i++)
  if(storage->leaves[i].legacy>=value->reg+part.legacybytes&&storage->leaves[i].native<base+part.bytes)
   goto done;
 if(!indirect&&part.bytes==16&&native64recordmask(shape)==3)shape=16;
 else if(!typedrecordregister(p,pc,2,element))goto done;
 mode=valuemode==Afp?AFP:AMP;
 p->code[pc]=(Dis64Instruction){in->op,(cons?SRC(mode)|DST(listmode):SRC(listmode)|DST(mode))|AXIMM,shape,
                                cons?base:list,cons?list:base};
 p->returns[pc]=cons?248:249;ok=1;
done:
 dis64layoutfree(&part);
 return ok?1:typedfail(p,"aggregate-list-layout");
}
