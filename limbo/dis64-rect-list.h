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

static int
typedstringpair(Type *t)
{
 Decl *d;
 return t&&t->kind==Ttuple&&(d=t->ids)&&d->ty==tstring&&
        d->next&&d->next->ty==tstring&&!d->next->next;
}

static int
typedstringpairlist(Addr *a)
{
 Type *t=typedloadvaluetype(a);
 return a->decl&&a->reg==a->decl->offset&&!a->offset&&
        t&&t->kind==Tlist&&typedstringpair(t->tof);
}

static int
typedstringpairlistop(Inst *in,Dis64TypedBody *p,Dis64Instruction *out,uchar *returned)
{
 int cons=in->op==ICONSMP,tail=in->op==ITAIL,actual,vm;
 unsigned int sm,dm,base,next;
 long list,other;
 Addr *value=cons?&in->s:&in->d;
 Decl *d=in->m.decl;
 Dis64Layout *layout;
 if(in->branch||(!cons&&!typedstringpairlist(&in->s))||
    ((cons||tail)&&!typedstringpairlist(&in->d)))goto bad;
 if(tail){
  if(in->mm!=Anone||
     !typedoperand(in->sm,&in->s,Tlist,0,0,p,&list,&sm,&actual)||
     !typedoperand(in->dm,&in->d,Tlist,0,0,p,&other,&dm,&actual))goto bad;
  *out=(Dis64Instruction){ITAIL,SRC(sm)|DST(dm),0,list,other};
  *returned=250;return 1;
 }
 if(in->mm!=Adesc||!d||!d->desc||d->desc->id!=in->m.offset||
    d->desc->size!=8||d->desc->nmap!=1||!d->desc->map||d->desc->map[0]!=0xc0)goto bad;
 vm=cons?in->sm:in->dm;
 if((vm!=Afp&&vm!=Amp)||value->offset||value->reg<0)goto bad;
 layout=vm==Afp?&p->frame:&p->data;
 if(!dis64layoutaddress(layout,value->reg,4,1,&base)||
    !dis64layoutaddress(layout,value->reg+4,4,1,&next)||next!=base+8)goto bad;
 if(!typedoperand(cons?in->dm:in->sm,cons?&in->d:&in->s,Tlist,0,0,p,&list,&sm,&actual))goto bad;
 dm=vm==Afp?AFP:AMP;
 *out=(Dis64Instruction){in->op,(cons?SRC(dm)|DST(sm):SRC(sm)|DST(dm))|AXIMM,16,
                        cons?base:list,cons?list:base};
 *returned=cons?248:249;return 1;
 bad:return typedfail(p,"string-pair-list-layout");
}
