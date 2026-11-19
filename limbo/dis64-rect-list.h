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
 a=cons?in->s:in->d;t=a.native64type?a.native64type:a.decl?a.decl->ty:nil;if(!typedwordgeometry(t,&l) || l.bytes!=16)goto done;
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
