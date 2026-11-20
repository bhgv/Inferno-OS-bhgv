/* Complete Wmsrv.Window data layout, distinct from Wmclient.Window. */
static int
typedwmsrvwindow(Type*t)
{
 Type*v;Decl*d;Dis64Layout l={0};unsigned int i;int ok=0;
 if(!typedwmref(t,"Wmsrv","Window") || !dis64layouttype(t->tof,&l) || l.legacybytes!=24 || l.bytes!=32 || l.alignment!=8 || l.count!=6)goto done;
 d=t->tof->ids;
 if(!d || d->store!=Dfield || !d->sym || strcmp(d->sym->name,"tag") || d->offset || !d->ty || d->ty->kind!=Tstring)goto done;
 d=d->next;if(!d || d->store!=Dfield || !d->sym || strcmp(d->sym->name,"r") || d->offset!=4 || !(v=d->ty) || v->kind!=Tadt || !v->decl || !v->decl->sym || strcmp(v->decl->sym->name,"Rect") || !v->decl->dot || !v->decl->dot->sym || strcmp(v->decl->dot->sym->name,"Draw"))goto done;
 d=d->next;if(!d || d->next || d->store!=Dfield || !d->sym || strcmp(d->sym->name,"img") || d->offset!=20 || !typedwmref(d->ty,"Draw","Image"))goto done;
 for(i=0;i<6;i++)if(l.leaves[i].legacy!=(i==0?0:i==5?20:4*i) || l.leaves[i].native!=(i==0?0:i==5?24:4+4*i) || l.leaves[i].oldbytes!=4 || l.leaves[i].bytes!=((i==0||i==5)?8:4) || l.leaves[i].kind!=(i==0?Tstring:i==5?Tref:Tint))goto done;
 ok=1;
 done:dis64layoutfree(&l);return ok;
}
static int
typedwindowlisttype(Addr*a)
{
 Type*t=a->decl?a->decl->ty:nil;
 if(a->native64type && a->decl && a->decl->store==Dlocal && a->decl->sym && a->decl->sym->name[0]=='.' && a->decl->sym->name[1]=='t')t=a->native64type;
 return a->decl && t && t->kind==Tlist && typedwmsrvwindow(t->tof) && a->reg==a->decl->offset && !a->offset;
}
static int
typedwindowlist(Inst*in,Dis64TypedBody*p,Dis64Instruction*out,uchar*returned,Type*result)
{
 unsigned int sm,dm;long source,destination;int actual,cons=in->op==ICONSP,head=in->op==IHEADP;Type*t;
 if(head&&in->dm==Afpind&&in->d.reg==16&&!in->d.offset&&in->mm==Anone&&!in->branch&&typedwindowlisttype(&in->s)&&typedwmsrvwindow(result)){
  if(!typedoperand(in->sm,&in->s,Tlist,0,0,p,&source,&sm,&actual))return 0;*out=(Dis64Instruction){IHEADP,SRC(sm)|DST(AFP|AIND),0,source,32};*returned=1;return 1;
 }
 if(in->mm!=Anone || in->branch || (in->sm!=Afp && in->sm!=Amp) || (in->dm!=Afp && in->dm!=Amp) ||
    (!cons&&!typedwindowlisttype(&in->s)) || (!head&&!typedwindowlisttype(&in->d)))return typedfail(p,"window-list-roles");
 if(cons){t=in->s.native64type?in->s.native64type:in->s.decl?in->s.decl->ty:nil;if(!typedwmsrvwindow(t) && (!t || t->kind!=Tany))return typedfail(p,"window-list-element");}
 if(head){t=in->d.native64type?in->d.native64type:in->d.decl?in->d.decl->ty:nil;if(!typedwmsrvwindow(t))return typedfail(p,"window-list-element");}
 if(!typedoperand(in->sm,&in->s,TypedRoot,0,cons,p,&source,&sm,&actual) || (actual!=(cons?Tref:Tlist) && !(cons&&actual==Tany)) ||
    !typedoperand(in->dm,&in->d,TypedRoot,0,0,p,&destination,&dm,&actual) || actual!=(head?Tref:Tlist))return typedfail(p,"window-list-values");
 *out=(Dis64Instruction){in->op,SRC(sm)|DST(dm),0,source,destination};*returned=213;return 1;
}
static int
typedwmsrvwindowbase(Addr*a,int mode,Dis64TypedBody*p,long*base,unsigned int*wire)
{
 Addr owner=*a;Type*t=typedfieldowner(a);int actual;
 if((mode!=Afpind && mode!=Ampind) || !typedwmsrvwindow(t))return 0;
 owner.offset=0;owner.native64type=t;
 if(!typedoperand(mode==Afpind?Afp:Amp,&owner,TypedRoot,0,0,p,base,wire,&actual) || actual!=Tref)return 0;
 *wire|=AIND;return 1;
}
static int
typedwmsrvwindowfield(Inst*in,Dis64TypedBody*p,Dis64Instruction*out,uchar*returned)
{
 int read=in->sm==Afpind||in->sm==Ampind,actual,kind;Addr*a=read?&in->s:&in->d;unsigned int bm,vm;long base,value;
 if(in->op!=IMOVP || in->mm!=Anone || in->branch || (a->offset!=0 && a->offset!=20) || !typedwmsrvwindowbase(a,read?in->sm:in->dm,p,&base,&bm))return typedfail(p,"wmsrv-window-field");
 kind=a->offset?Tref:Tstring;
 if(!typedoperand(read?in->dm:in->sm,read?&in->d:&in->s,TypedRoot,0,!read,p,&value,&vm,&actual) || (actual!=kind && actual!=Tany) || (vm!=AFP && vm!=AMP))return typedfail(p,"wmsrv-window-value");
 *out=(Dis64Instruction){IMOVP,read?SRC(bm)|DST(vm):SRC(vm)|DST(bm),a->offset?24:0,read?base:value,read?value:base};*returned=214;return 1;
}
