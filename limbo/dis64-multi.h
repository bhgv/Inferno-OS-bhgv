/* Trusted compiler IR only. Included after the shared typed operand/CFG checks. */
typedef struct TypedMulti TypedMulti;
struct TypedMulti {
    Decl *globals,**functions;
    Inst **instructions;
    int *entries,nf,external,loads;
    Dis64TypedBody *bodies;
    uchar *state;
    unsigned int *height,count,cost;
};

static int
typedbytearrayaddr(Addr *a)
{
 Type*t;
 return a->decl && (t=a->native64type?a->native64type:a->decl->ty) && t->kind==Tarray && t->tof && t->tof->kind==Tbyte && a->reg==a->decl->offset && !a->offset;
}

static int typedimagescreenarg(TypedMulti*,Dis64TypedBody*,Inst*,unsigned int,unsigned int*,unsigned int,unsigned int);
static int typedlibraryarraystore(Inst*,Dis64TypedBody*,unsigned int);
static int typedlibraryinst(Inst*,Dis64TypedBody*,unsigned int,unsigned int);
static int typedrecordresult(Inst*,Dis64TypedBody*,Type*,unsigned int,unsigned int*,int);
static int typedrecordpath(Inst*,Dis64TypedBody*,unsigned int);
static int typedrecordnew(Inst*,Dis64TypedBody*,unsigned int);
static int typedrecordinst(Inst*,Dis64TypedBody*,unsigned int,unsigned int);
static int typedrecordwordcopy(Inst*,Dis64TypedBody*,unsigned int);
static int typedlibraryslicearg(Inst*,Inst*,Dis64TypedBody*,unsigned int,unsigned int,unsigned int);
static int typedlibrarymodule(Inst*,Dis64TypedBody*,long*,unsigned int*,int*);
static int typedlibraryargument(Inst*,Inst*,Dis64TypedBody*,unsigned int,unsigned int,unsigned int,int);
static int typedrecordargument(Inst*,Inst*,Dis64TypedBody*,Type*,unsigned int,unsigned int,unsigned int,int);
static Type *typedloadvaluetype(Addr*);

/* Canonical Dis spills large middle literals into RTemp. Read the record
 * field into the final Word cell and apply the literal as the source. */
static int
typedrecordmask(Inst *literal, Inst *use, Dis64TypedBody *p)
{
 Type *owner;
 long source,destination;
 unsigned int pc=literal->pc-p->first,sm,dm;
 int kind;
 if(!use||literal->sm!=Aimm||literal->mm!=Anone||literal->branch||
    literal->s.offset< -536870912L||literal->s.offset>536870911L||
    (literal->s.offset>=-32768&&literal->s.offset<=32767)||
    use->op!=IANDW||use->mm!=Afp||use->m.reg!=RTemp||use->m.decl||use->m.offset!=literal->s.offset||
    use->dm!=Afp||(use->sm!=Afpind&&use->sm!=Ampind)||use->branch)return 0;
 owner=typedfieldowner(&use->s);
 if(!owner||!typedrecordlayoutoperand(use->sm,&use->s,0,p,&source,&sm)||
    !typedoperand(Afp,&use->d,Tint,0,0,p,&destination,&dm,&kind)||
    !typedrecordregister(p,pc,0,owner->tof))return 0;
 p->code[pc]=(Dis64Instruction){IMOVW,SRC(sm)|DST(dm),0,source,destination};
 p->returns[pc]=Native64TypedRecord;
 p->code[pc+1]=(Dis64Instruction){IANDW,SRC(AIMM)|DST(AFP),0,literal->s.offset,p->code[pc].destination};
 p->returns[pc+1]=Native64TypedRecordMask;
 return 1;
}

static int
typedlistargument(Inst *in, Inst *frame, Decl *arg, Dis64TypedBody *p,
                  unsigned int pc, unsigned int offset)
{
 Type *list=typedloadvaluetype(&in->s);
 unsigned int mode;
 long source;
 int actual;
 if(!list||list->kind!=Tlist||!list->tof||!tequal(in->op==ITAIL?list:list->tof,arg->ty)||
    in->mm!=Anone||in->branch||in->dm!=Afpind||in->d.reg!=frame->d.reg||
    in->d.offset!=arg->offset||offset>65535||
    !typedoperand(in->sm,&in->s,Tlist,0,0,p,&source,&mode,&actual)||actual!=Tlist)
  return typedfail(p,"direct-list-argument");
 p->code[pc]=(Dis64Instruction){in->op,SRC(mode)|DST(AFP|AIND),0,source,offset};
 p->returns[pc]=64;
 return 1;
}

static int
typedheadfieldstart(TypedMulti *c, unsigned int at, unsigned int end)
{
 Inst *head,*field;
 if(at>=end||end-at<2)return 0;
 head=c->instructions[at];field=c->instructions[at+1];
 return head&&field&&head->op==IHEADP&&head->dm==Afp&&field->op==IMOVP&&field->sm==Afpind&&
  field->s.decl==head->d.decl&&field->s.reg==head->d.reg;
}

/* Limbo materializes Boolean arguments with a compare, two Word writes and
 * a join. Keep the evaluation order and all original instruction positions. */
static int
typedwordchoiceshape(TypedMulti *c, unsigned int pc, unsigned int stop, Inst *frame)
{
 Inst *test,*one,*jump,*other;
 if(pc>=stop||stop-pc<=4)return 0;
 test=c->instructions[pc];
 if(test==nil||test->op<IBEQW||test->op>IBGEW||test->dm!=Apc||
    test->branch!=c->instructions[pc+3]||test->d.offset!=(long)pc+3)return 0;
 one=c->instructions[pc+1];jump=c->instructions[pc+2];other=c->instructions[pc+3];
 if(one==nil||jump==nil||other==nil||c->instructions[pc+4]==nil)return 0;
 return jump->op==IJMP&&jump->sm==Anone&&jump->mm==Anone&&jump->dm==Apc&&
  jump->branch==c->instructions[pc+4]&&jump->d.offset==(long)pc+4&&
  one->op==IMOVW&&one->sm==Aimm&&one->mm==Anone&&one->dm==Afpind&&!one->branch&&
  other->op==IMOVW&&other->sm==Aimm&&other->mm==Anone&&other->dm==Afpind&&!other->branch&&
  one->s.offset>=0&&one->s.offset<=1&&other->s.offset==1-one->s.offset&&
  one->d.reg==frame->d.reg&&other->d.reg==frame->d.reg&&one->d.offset==other->d.offset;
}

static int
typedwordchoice(TypedMulti *c, Dis64TypedBody *p, Inst *frame, unsigned int stop,
                unsigned int *at, unsigned int old, unsigned int offset)
{
 unsigned int pc=*at-p->first,i;
 Dis64Instruction *out=p->code+pc;
 if(offset>65535||!typedwordchoiceshape(c,*at,stop,frame)||
    c->instructions[*at+1]->d.offset!=old||
    !typedinst(c->instructions[*at],stop,p,out,p->returns+pc))
  return typedfail(p,"word-choice-argument");
 if(out->op<IBEQW||out->op>IBGEW)return typedfail(p,"word-choice-comparison");
 out[1]=(Dis64Instruction){IMOVW,SRC(AIMM)|DST(AFP|AIND),0,c->instructions[*at+1]->s.offset,offset};
 out[2]=(Dis64Instruction){IJMP,SRC(AXXX)|DST(AIMM),0,0,pc+4};
 out[3]=(Dis64Instruction){IMOVW,SRC(AIMM)|DST(AFP|AIND),0,c->instructions[*at+3]->s.offset,offset};
 for(i=0;i<4;i++)p->returns[pc+i]=Native64TypedWordChoice;
 *at+=4;
 return 1;
}

#include "dis64-external.h"
static int typedclientaddress(Addr*,int,Dis64TypedBody*,long*,unsigned int*,long*,int*);
#include "dis64-variadic.h"
static int typedwindowlisttype(Addr*);

/* Lists of the canonical Draw.Pointer use one native root per element. */
static int
typedpointertype(Type*t)
{Dis64Layout l={0};int ok=t && t->kind==Tref && typedpointerlayout(t->tof,&l);dis64layoutfree(&l);return ok;}
static int
typedpointerlisttype(Addr*a)
{Type*t=a->native64type?a->native64type:a->decl?a->decl->ty:nil;return a->decl && t && t->kind==Tlist && typedpointertype(t->tof) && a->reg==a->decl->offset && !a->offset;}
static int
typedpointerlist(Inst*in,Dis64TypedBody*p,Dis64Instruction*out,uchar*returned)
{int cons=in->op==ICONSP,head=in->op==IHEADP,actual;unsigned int sm,dm;long source,dest;Addr*a=cons?&in->s:&in->d;Type*t=a->native64type?a->native64type:a->decl?a->decl->ty:nil;
 if(in->branch||in->mm!=Anone||(!cons&&!typedpointerlisttype(&in->s))||(!head&&!typedpointerlisttype(&in->d))||((cons||head)&&!typedpointertype(t))||!typedoperand(in->sm,&in->s,TypedRoot,0,0,p,&source,&sm,&actual)||!typedoperand(in->dm,&in->d,TypedRoot,0,0,p,&dest,&dm,&actual))return typedfail(p,"pointer-list");
 *out=(Dis64Instruction){in->op,SRC(sm)|DST(dm),0,source,dest};*returned=213;return 1;}

/* List primitives belong only to the selected importing-client lowering. */
static int
typedclientlist(Inst *in,Dis64TypedBody *p,Dis64Instruction *out)
{
 unsigned int sm,dm;long source,destination;int actual;
 int sk=in->op==ICONSP?Tstring:Tlist,dk=in->op==IHEADP?Tstring:Tlist;
 if(in->mm!=Anone || in->branch!=nil ||
    !typedoperand(in->sm,&in->s,sk,0,0,p,&source,&sm,&actual))return typedfail(p,"client-list-roles");
 if(in->dm==Afpind){
  if(in->op==ICONSP || in->d.reg!=16 || in->d.offset || p->resultkind!=dk)return typedfail(p,"client-list-result");
  destination=32;dm=AFP|AIND;
 }else if(!typedoperand(in->dm,&in->d,dk,0,0,p,&destination,&dm,&actual))return typedfail(p,"client-list-roles");
 *out=(Dis64Instruction){in->op,SRC(sm)|DST(dm),0,source,destination};return 1;
}

/* Native checked-LOAD diagnostics admit only list of string here. A map bit
 * alone cannot distinguish List/String or prove the element class. */
static int
typedloadlisttype(Addr *a)
{
 Type *t;
 if(!a->decl)return 0;t=a->decl->ty;
 if(a->native64type && a->decl->store==Dlocal && a->decl->sym && a->decl->sym->name[0]=='.' && a->decl->sym->name[1]=='t')t=a->native64type;
 return t && t->kind==Tlist && t->tof && t->tof->kind==Tstring && a->reg==a->decl->offset && !a->offset;
}
/* Lists of canonical Sh.Listnode references. */
static int
typedlistnodetype(Addr *a)
{
 Type *t=a->decl?a->decl->ty:nil;
 if(a->native64type && a->decl && a->decl->store==Dlocal && a->decl->sym &&
    a->decl->sym->name[0]=='.' && a->decl->sym->name[1]=='t')t=a->native64type;
 return a->decl&&t&&t->kind==Tlist&&t->tof&&t->tof->kind==Tref&&
        typedlibraryrecord(t->tof)==13&&a->reg==a->decl->offset&&!a->offset;
}
static int
typedvarlist(Addr *a)
{
 Type *t=a->native64type?a->native64type:a->decl?a->decl->ty:nil;
 return a->decl&&a->reg==a->decl->offset&&!a->offset&&t&&t->kind==Tlist&&
        typedwmref(t->tof,"Sh","Var");
}
static int
typedshellbuiltin(Type *t)
{
 return t&&t->kind==Tmodule&&t->decl&&t->decl->sym&&
        !strcmp(t->decl->sym->name,"Shellbuiltin");
}
static int
typedbuiltinlist(Addr *a)
{
 Type *t=a->native64type?a->native64type:a->decl?a->decl->ty:nil;
 return a->decl&&a->reg==a->decl->offset&&!a->offset&&t&&
        t->kind==Tlist&&typedshellbuiltin(t->tof);
}
static int
typedshellrootlist(Addr *a)
{
 return typedlistnodetype(a)||typedvarlist(a)||typedbuiltinlist(a);
}
static int
typedshelllistop(Inst *in,Dis64TypedBody *p,Dis64Instruction *out,uchar *returned)
{
 unsigned int sm,dm;long source,dest;int actual,head=in->op==IHEADP,cons=in->op==ICONSP;
 Addr *entry=cons?&in->s:&in->d;
 Addr *list=cons?&in->d:&in->s;
 int vars=typedvarlist(list), modules=typedbuiltinlist(list);
 int tupletail=in->op==ITAIL&&in->dm==Afp&&typedroottemporary(&in->d,Tlist)&&
               typedloadvaluetype(&in->s)&&tequal(in->d.native64type,typedloadvaluetype(&in->s));
 int element=modules?Tmodule:Tref;
 Type *t=entry->native64type?entry->native64type:entry->decl?entry->decl->ty:nil;
 if(in->branch||in->mm!=Anone||
    ((head||cons)&&(modules?!typedshellbuiltin(t):!t||t->kind!=Tref||typedlibraryrecord(t)!=(vars?19:13)))||
    (!head&&!tupletail&&!(modules?typedbuiltinlist(&in->d):vars?typedvarlist(&in->d):typedlistnodetype(&in->d)))||
    !typedoperand(in->sm,&in->s,cons?TypedRoot:Tlist,0,0,p,&source,&sm,&actual)||actual!=(cons?element:Tlist)||
    !typedoperand(in->dm,&in->d,head||tupletail?TypedRoot:Tlist,0,0,p,&dest,&dm,&actual)||
    (!tupletail&&actual!=(head?element:Tlist)))return typedfail(p,"shell-list-operation");
 *out=(Dis64Instruction){in->op,SRC(sm)|DST(dm),0,source,dest};*returned=213;return 1;
}
static int
typedloadlist(Inst *in,Decl *fn,Dis64TypedBody *p,Dis64Instruction *out)
{
 Type *result=fn->ty->tof;
 if((in->op!=ICONSP && !typedloadlisttype(&in->s)) ||
    (in->op!=IHEADP && (in->dm==Afpind?
      (result->kind!=Tlist || !result->tof || result->tof->kind!=Tstring):!typedloadlisttype(&in->d))))
  return typedfail(p,"load-list-element-class");
 return typedclientlist(in,p,out);
}

static int
typedintlisttype(Addr*a)
{
 Type*t=a->decl?a->decl->ty:nil;
 if(a->native64type && a->decl && a->decl->store==Dlocal && a->decl->sym && a->decl->sym->name[0]=='.' && a->decl->sym->name[1]=='t')t=a->native64type;
 return a->decl && t && t->kind==Tlist && t->tof && t->tof->kind==Tint && a->reg==a->decl->offset && !a->offset;
}
static int
typedclientintlist(Inst*in,Decl*fn,Dis64TypedBody*p,Dis64Instruction*out,uchar*returned)
{
 unsigned int sm,dm;long source,destination;int actual,head=in->op==IHEADW,cons=in->op==ICONSW,kind=cons?Tint:Tlist;
 if(in->mm!=Anone || in->branch || (!cons&&!typedintlisttype(&in->s)) ||
    !typedoperand(in->sm,&in->s,kind,cons,0,p,&source,&sm,&actual))return typedfail(p,"integer-list-source");
 if(in->dm==Afpind){
  if(cons || in->d.reg!=16 || in->d.offset || (head?fn->ty->tof->kind!=Tint:(fn->ty->tof->kind!=Tlist || !fn->ty->tof->tof || fn->ty->tof->tof->kind!=Tint)))return typedfail(p,"integer-list-result");
  destination=32;dm=AFP|AIND;*returned=1;
 }else if((!head&&!typedintlisttype(&in->d)) || !typedoperand(in->dm,&in->d,head?Tint:Tlist,0,0,p,&destination,&dm,&actual))return typedfail(p,"integer-list-destination");
 *out=(Dis64Instruction){in->op,SRC(sm)|DST(dm),0,source,destination};return 1;
}

/* One bounded nil -> String cons chain in a local root, then its argument
 * store. Keep the pending frame rooted through all intervening allocations. */
static int
typedexternalstringlist(TypedMulti*c,Dis64TypedBody*p,Inst*frame,unsigned int end,unsigned int*at)
{
 Inst*in=c->instructions[*at];Addr temp;unsigned int start=*at,n=0;int word=typedintlisttype(&in->d);
 if(in->sm!=Amp || in->mm!=Anone || in->branch || in->dm!=Afp || !in->s.decl || in->s.decl->store!=Dglobal ||
    in->s.reg!=in->s.decl->offset || !in->s.decl->ty || in->s.decl->ty->kind!=Tany ||
    !in->s.decl->sym || strcmp(in->s.decl->sym->name,"nil") || in->s.offset || in->d.offset || !(word?typedintlisttype(&in->d):typedloadlisttype(&in->d)) ||
    !typedinst(in,c->count,p,p->code+*at-p->first,p->returns+*at-p->first))return typedfail(p,"external-list-init");
 temp=in->d;p->returns[(*at)++-p->first]=112;
 while(*at<end && (in=c->instructions[*at]) && in->op==(word?ICONSW:ICONSP)){
  if(n++==256 || in->dm!=Afp || in->d.reg!=temp.reg || in->d.decl!=temp.decl || in->d.offset ||
     !(word?typedintlisttype(&in->d):typedloadlisttype(&in->d)) || !(word?typedclientintlist(in,nil,p,p->code+*at-p->first,p->returns+*at-p->first):typedclientlist(in,p,p->code+*at-p->first)))return typedfail(p,"external-list-cons");
  p->returns[(*at)++-p->first]=112;
 }
 if(!n || *at>=end || !(in=c->instructions[*at]) || in->op!=IMOVP || in->sm!=Afp || in->s.reg!=temp.reg || in->s.decl!=temp.decl ||
    in->s.offset || in->dm!=Afpind || in->d.reg!=frame->d.reg)return typedfail(p,"external-list-store");
 USED(start);return 1;
}

/* Prepend a bounded String chain to a borrowed, typed List tail. */
static int
typedexternalstringprefix(TypedMulti*c,Dis64TypedBody*p,Inst*frame,unsigned int end,unsigned int*at)
{
 Inst*in=c->instructions[*at];Addr temp;unsigned int n=0;
 if(in->op!=IMOVP || in->mm!=Anone || in->branch || in->dm!=Afp || !typedloadlisttype(&in->s) || !typedloadlisttype(&in->d) ||
    !in->d.decl || in->d.decl->store!=Dlocal || !in->d.decl->sym || in->d.decl->sym->name[0]!='.' || in->d.decl->sym->name[1]!='t' ||
    !typedinst(in,c->count,p,p->code+*at-p->first,p->returns+*at-p->first))return typedfail(p,"external-list-prefix");
 temp=in->d;p->returns[(*at)++-p->first]=187;
 while(*at<end && (in=c->instructions[*at]) && in->op==ICONSP){
  if(n++==256 || in->dm!=Afp || in->d.reg!=temp.reg || in->d.decl!=temp.decl || in->d.offset ||
     !typedloadlisttype(&in->d) || !typedclientlist(in,p,p->code+*at-p->first))return typedfail(p,"external-list-prefix-cons");
  p->returns[(*at)++-p->first]=187;
 }
 if(!n || *at>=end || !(in=c->instructions[*at]) || in->op!=IMOVP || in->sm!=Afp || in->s.reg!=temp.reg || in->s.decl!=temp.decl ||
    in->s.offset || in->dm!=Afpind || in->d.reg!=frame->d.reg)return typedfail(p,"external-list-prefix-store");return 1;
}

static int
typedrootargclear(TypedMulti*c,Dis64TypedBody*p,Inst*store,unsigned int end,unsigned int*at)
{
 Inst*in;
 if(*at>=end || store->sm!=Afp || !(in=c->instructions[*at]) || in->op!=IMOVP || in->dm!=Afp || in->d.reg!=store->s.reg)return 1;
 if(!store->s.decl || store->s.decl->store!=Dlocal || !store->s.decl->sym || store->s.decl->sym->name[0]!='.' || store->s.decl->sym->name[1]!='t' ||
    in->sm!=Amp || in->mm!=Anone || in->branch || in->d.decl!=store->s.decl || in->d.offset || !in->s.decl || in->s.decl->store!=Dglobal ||
    !in->s.decl->ty || in->s.decl->ty->kind!=Tany || !in->s.decl->sym || strcmp(in->s.decl->sym->name,"nil") || in->s.reg!=in->s.decl->offset || in->s.offset ||
    !typedinst(in,c->count,p,p->code+*at-p->first,p->returns+*at-p->first))return typedfail(p,"external-root-clear");
 p->returns[(*at)++-p->first]=126;return 1;
}

/* Fixed Draw.Context geometry from trusted .m declarations, not a guessed C size. */
static int
typedclientcontext(Inst *in,Dis64TypedBody *p,Dis64Instruction *out)
{
 Dis64Layout layout={0};Decl *d=in->s.decl;unsigned int i,dm;long destination;int actual,ok=0;
 if(in->sm!=Adesc || in->mm!=Anone || in->branch || !d || !d->ty || d->ty->kind!=Tadt || !d->desc || d->desc->id!=in->s.offset || d->desc->size!=12 || d->desc->nmap!=1 || !d->desc->map || d->desc->map[0]!=0xe0 ||
    !d->sym || strcmp(d->sym->name,"Context") || !d->dot || !d->dot->sym || strcmp(d->dot->sym->name,"Draw") ||
    !dis64layouttype(d->ty,&layout) || layout.legacybytes!=12 || layout.bytes!=24 || layout.alignment!=8 || layout.count!=3)goto done;
 for(i=0;i<3;i++)if(layout.leaves[i].legacy!=4*i || layout.leaves[i].native!=8*i || layout.leaves[i].oldbytes!=4 || layout.leaves[i].bytes!=8 || layout.leaves[i].kind!=(i==2?Tchan:Tref))goto done;
 if(p->sourceroles && in->dm==Afpind){if(in->d.reg!=16 || in->d.offset || p->resultkind!=Tref)goto done;destination=32;dm=AFP|AIND;}
 else if(!typedoperand(in->dm,&in->d,TypedRoot,0,0,p,&destination,&dm,&actual) || actual!=Tref || dm!=AFP)goto done;
 *out=(Dis64Instruction){in->op,SRC(AIMM)|DST(dm),0,1,destination};ok=1;
 done:dis64layoutfree(&layout);return ok;
}

static int typedwmcontextchannel(Addr*,int,Dis64TypedBody*,long*,unsigned int*,long*);
static int typedclientchanneladdress(Addr*,int,Dis64TypedBody*,long*,unsigned int*,long*,int*);

/* Mixed Word/root pair, including its alignment gap. Descriptor and literal
 * types may have been deduplicated by map or contain a nil root. */
static int
typedreplylayout(Type*t,Dis64Layout*l,int allownil)
{
 Decl*a;
 if(!t || t->kind!=Ttuple || !(a=t->ids) || a->store!=Dfield || a->offset || !a->ty || a->ty->kind!=Tint ||
    !(a=a->next) || a->next || a->store!=Dfield || a->offset!=4 || !a->ty ||
    !(typedmanagedkind(a->ty->kind)||(allownil&&a->ty->kind==Tany))||
    !dis64layouttype(t,l))return 0;
 return l->legacybytes==8 && l->bytes==16 && l->alignment==8 && l->count==2 &&
  l->leaves[0].legacy==0 && l->leaves[0].native==0 && l->leaves[0].oldbytes==4 && l->leaves[0].bytes==4 && l->leaves[0].kind==Tint &&
  l->leaves[1].legacy==4 && l->leaves[1].native==8 && l->leaves[1].oldbytes==4 && l->leaves[1].bytes==8;
}
static int
typedreplytype(Type*t)
{Dis64Layout l={0};int ok=typedreplylayout(t,&l,0);dis64layoutfree(&l);return ok;}
static int
typedreplyliteral(Type*t)
{Dis64Layout l={0};int ok=typedreplylayout(t,&l,1);dis64layoutfree(&l);return ok;}
static int
typedreplyspan(Dis64Layout*l,unsigned int old,unsigned int*base)
{unsigned int root;return dis64layoutaddress(l,old,4,0,base) && !(*base%8) && dis64layoutaddress(l,old+4,4,1,&root) && root==*base+8;}
static int
typedreplytransfer(Inst*in,Dis64TypedBody*p,Dis64Instruction*out,uchar*returned)
{
 int send=in->op==ISEND,cm=send?in->dm:in->sm,vm=send?in->sm:in->dm,actual;Addr*ca=send?&in->d:&in->s,*va=send?&in->s:&in->d;Type*t=ca->native64type?ca->native64type:ca->decl?ca->decl->ty:nil;long channel;unsigned int wire,value;
 if((cm!=Afp && cm!=Amp) || (vm!=Afp && vm!=Amp) || va->offset || in->mm!=Anone || in->branch || !t || t->kind!=Tchan || !typedreplytype(t->tof) || !typedoperand(cm,ca,TypedRoot,0,0,p,&channel,&wire,&actual) || actual!=Tchan || !typedreplyspan(vm==Afp?&p->frame:&p->data,va->reg,&value))return 0;
 *out=(Dis64Instruction){in->op,send?SRC(vm==Afp?AFP:AMP)|DST(wire):SRC(wire)|DST(vm==Afp?AFP:AMP),136,send?value:channel,send?channel:value};*returned=136;return 1;
}

static int typedroottransfer(Inst*,Dis64TypedBody*,Dis64Instruction*,uchar*);
static unsigned int typedlibraryrecord(Type*);

static int
typedpointpayload(Type*t)
{Dis64Layout l={0};int ok=typedwordgeometry(t,&l)&&l.bytes==8;dis64layoutfree(&l);return ok;}
static int
typedpointspan(Addr*a,int mode,Dis64TypedBody*p,unsigned int*base)
{Dis64Layout*l=mode==Afp?&p->frame:&p->data;unsigned int next;
 return (mode==Afp||mode==Amp)&&!a->offset&&dis64layoutaddress(l,a->reg,4,0,base)&&dis64layoutaddress(l,a->reg+4,4,0,&next)&&next==*base+4;}

static int typedclientreferencekind(int);
/* Cursor switch: exactly (String, Word32, chan of String). */
static int
typedcursorlayout(Type*t,Dis64Layout*l,int descriptor)
{Decl*d;unsigned int i;if(!t||t->kind!=Ttuple||!dis64layouttype(t,l)||l->legacybytes!=12||l->bytes!=24||l->alignment!=8||l->count!=3)return 0;
 for(i=0,d=t->ids;i<3;i++,d=d->next){Dis64LayoutLeaf*v=l->leaves+i;if(!d||!d->ty||v->legacy!=4*i||v->native!=8*i||v->oldbytes!=4||v->bytes!=(i==1?4:8))return 0;if(i==1){if(d->ty->kind!=Tint)return 0;}else if(descriptor){if(!typedclientreferencekind(d->ty->kind)&&d->ty->kind!=Tstring&&d->ty->kind!=Tany)return 0;}else if(i==0?d->ty->kind!=Tstring:(d->ty->kind!=Tchan||!d->ty->tof||d->ty->tof->kind!=Tstring))return 0;}return !d;}
static int
typedcursortype(Type*t,int descriptor)
{Dis64Layout l={0};int ok=typedcursorlayout(t,&l,descriptor);dis64layoutfree(&l);return ok;}
static int
typedcursorspan(Dis64Layout*l,unsigned int old,unsigned int*base)
{unsigned int i,next;for(i=0;i<3;i++){if(!dis64layoutaddress(l,old+4*i,4,i!=1,&next))return 0;if(!i)*base=next;else if(next!=*base+8*i)return 0;}return !(*base%8);}
static int
typedcursortransfer(Inst*in,Dis64TypedBody*p,Dis64Instruction*out,uchar*returned)
{int send=in->op==ISEND,cm=send?in->dm:in->sm,vm=send?in->sm:in->dm,actual;Addr*ca=send?&in->d:&in->s,*va=send?&in->s:&in->d;Type*t=ca->native64type?ca->native64type:ca->decl?ca->decl->ty:nil;long channel;unsigned int wire,value;
 if(!t||t->kind!=Tchan||!typedcursortype(t->tof,0)||(cm!=Afp&&cm!=Amp)||(vm!=Afp&&vm!=Amp)||va->offset||in->mm!=Anone||in->branch||!typedoperand(cm,ca,TypedRoot,0,0,p,&channel,&wire,&actual)||actual!=Tchan||!typedcursorspan(vm==Afp?&p->frame:&p->data,va->reg,&value))return 0;
 *out=(Dis64Instruction){in->op,send?SRC(vm==Afp?AFP:AMP)|DST(wire):SRC(wire)|DST(vm==Afp?AFP:AMP),152,send?value:channel,send?channel:value};*returned=52;return 1;}

/* Primitive channel transfer; aggregate payloads and borrowed outputs stay closed. */
static int
typedclienttransfer(Inst*in,Dis64TypedBody*p,Dis64Instruction*out,uchar*returned)
{
 Addr*ca=in->op==ISEND?&in->d:&in->s;Addr*va=in->op==ISEND?&in->s:&in->d;int cm=in->op==ISEND?in->dm:in->sm,vm=in->op==ISEND?in->sm:in->dm,actual,kind,root;
 Type*t=ca->native64type?ca->native64type:ca->decl?ca->decl->ty:nil;unsigned int cw,vw,width;long channel,value,field=0;int indirect=cm==Afpind || cm==Ampind;
 if(typedcursortransfer(in,p,out,returned)||typedroottransfer(in,p,out,returned))return 1;
 if(in->mm!=Anone || in->branch)return typedfail(p,"channel-transfer-owner");
 if(indirect){if(typedwmref(typedfieldowner(ca),"Draw","Wmcontext")){if(!typedwmcontextchannel(ca,cm,p,&channel,&cw,&field))return typedfail(p,"wmcontext-channel-transfer");kind=field==0?Tint:(field==8||field==32)?Tref:Tstring;}else if(!typedclientchanneladdress(ca,cm,p,&channel,&cw,&field,&kind))return typedfail(p,"client-channel-field");}
 else{if(!t || t->kind!=Tchan || !t->tof || !typedoperand(cm,ca,TypedRoot,0,0,p,&channel,&cw,&actual) || actual!=Tchan)return typedfail(p,"channel-transfer-owner");kind=t->tof->kind;}
 if(!indirect&&typedpointpayload(t->tof)){unsigned int base;if(!typedpointspan(va,vm,p,&base))return typedfail(p,"point-channel-span");*out=(Dis64Instruction){in->op,in->op==ISEND?SRC(vm==Afp?AFP:AMP)|DST(cw):SRC(cw)|DST(vm==Afp?AFP:AMP),8,in->op==ISEND?base:channel,in->op==ISEND?channel:base};*returned=54;return 1;}
 root=kind==Tstring || kind==Tref || kind==Tchan || kind==Tarray || kind==Tlist || kind==Tmodule;
 if(kind==Ttuple && !indirect && typedreplytransfer(in,p,out,returned))return 1;
 if(!root && kind!=Tbyte && kind!=Tint && kind!=Tfix && kind!=Tbig && kind!=Treal)return typedfail(p,"channel-transfer-payload");
 width=root || kind==Tbig || kind==Treal?8:kind==Tbyte?1:4;
 if(in->op==IRECV&&vm==Afpind&&va->reg==16&&va->offset==8&&!indirect&&kind==Tstring&&p->resultaggregate==124){*out=(Dis64Instruction){IRECV,SRC(cw)|DST(AFP|AIND),16,channel,32};*returned=45;return 1;}
 if(in->op==IRECV&&vm==Afpind&&va->reg==16&&!va->offset&&!indirect&&kind==Tint){*out=(Dis64Instruction){IRECV,SRC(cw)|DST(AFP|AIND),4,channel,32};*returned=109;return 1;}
 if(in->op==IRECV&&vm==Afpind&&typedlibraryrecord(typedfieldowner(va))==12&&(va->offset==0||va->offset==4||va->offset==8)&&!indirect&&kind==Tint){Addr a=*va;a.offset=0;a.native64type=typedfieldowner(va);if(!typedoperand(Afp,&a,TypedRoot,0,0,p,&value,&vw,&actual)||actual!=Tref||vw!=AFP)return typedfail(p,"client-pid-receive-owner");*out=(Dis64Instruction){IRECV,SRC(cw)|DST(AFP|AIND),65536|(va->offset<<8)|4,channel,value};*returned=109;return 1;}
 if(!typedoperand(vm,va,root?TypedRoot:kind==Tfix?Tint:kind,in->op==ISEND&&!root&&width<8,0,p,&value,&vw,&actual) || (root && actual!=kind && actual!=Tany))return typedfail(p,"channel-transfer-value");
 *out=(Dis64Instruction){in->op,in->op==ISEND?SRC(vw)|DST(cw):SRC(cw)|DST(vw),(field<<8)|width|(root?16:0),in->op==ISEND?value:channel,in->op==ISEND?channel:value};*returned=indirect?(typedwmref(typedfieldowner(ca),"Draw","Wmcontext")?94:204):188;return 1;
}

/* Shared native channel result/capacity geometry, after nominal payload proof. */
static int typedwindowowner(Addr*);
static int typedwindowfield(Inst*,Dis64TypedBody*,unsigned int,int,unsigned int*);
static int
typedchanneloutput(Inst*in,Decl*fn,Dis64TypedBody*p,Dis64Instruction*out,uchar*returned)
{
 unsigned int dm,mm=AXNON,wire;long destination,capacity=0;int actual;
 if(in->branch)return typedfail(p,"channel-operands");
 if((in->dm==Afpind||in->dm==Ampind)&&typedwindowowner(&in->d)){Addr a=in->d;Inst probe=*in;if(in->op!=INEWCP||in->sm!=Anone||in->mm!=Aimm||in->m.offset||a.offset!=52||!a.native64type||a.native64type->kind!=Tchan||!a.native64type->tof||a.native64type->tof->kind!=Tstring)return typedfail(p,"window-channel-field");probe.op=IMOVP;probe.sm=in->dm;probe.s=a;probe.dm=Afp;probe.d=a;probe.mm=Anone;probe.branch=nil;if(!typedwindowfield(&probe,p,in->pc-p->first,1,nil))return 0;*out=(Dis64Instruction){INEWCP,SRC(AXXX)|AXIMM|DST(USRC(p->code[in->pc-p->first].mode)),0,512|64,p->code[in->pc-p->first].source};*returned=108;return 1;}

 if((in->dm==Afpind||in->dm==Ampind)&&(typedlibraryrecord(typedfieldowner(&in->d))==8||typedlibraryrecord(typedfieldowner(&in->d))==6)){Addr a=in->d;Type*t=typedfieldowner(&a);unsigned int field=a.offset,id=typedlibraryrecord(t);long owner;int k;
  if((id==6?(field!=0&&field!=4&&field!=8&&field!=12):(field!=0&&field!=4&&field!=8&&field!=16))||in->mm!=Aimm||in->m.offset!=0||in->sm!=Anone||in->op==INEWCMP||in->op!=((field==0||(id==6&&field==12))?INEWCW:INEWCP))return typedfail(p,"wmcontext-channel-field");a.offset=0;a.native64type=t;if(!typedoperand(in->dm==Afpind?Afp:Amp,&a,TypedRoot,0,0,p,&owner,&dm,&k)||k!=Tref)return 0;
  *out=(Dis64Instruction){in->op,SRC(AXXX)|AXIMM|DST(dm|AIND),0,(id==6?256:0)|2*field,owner};*returned=108;return 1;
 }
 if(in->dm==Afpind){if(in->d.reg!=16 || in->d.offset || fn->ty->tof->kind!=Tchan)return typedfail(p,"channel-result");destination=32;dm=AFP|AIND;*returned=1;}
 else if(!typedoperand(in->dm,&in->d,TypedRoot,0,0,p,&destination,&dm,&actual) || actual!=Tchan)return typedfail(p,"channel-destination");
 if(in->mm!=Anone){if(!typedoperand(in->mm,&in->m,Tint,1,1,p,&capacity,&wire,&actual))return typedfail(p,"channel-capacity");mm=wire==AIMM?AXIMM:wire==AFP?AXINF:AXINM;}
 *out=(Dis64Instruction){in->op,SRC(in->op==INEWCMP?AIMM:AXXX)|mm|DST(dm),capacity,0,destination};return 1;
}

/* Primitive channel payloads retain their existing builtin Type/mover identity. */
static int
typedclientchannel(Inst*in,Decl*fn,Dis64TypedBody*p,Dis64Instruction*out,uchar*returned)
{
 Type*t=in->dm==Afpind?fn->ty->tof:in->d.native64type?in->d.native64type:in->d.decl?in->d.decl->ty:nil;int kind;
 if((in->dm==Afpind||in->dm==Ampind)&&typedwindowowner(&in->d))return typedchanneloutput(in,fn,p,out,returned);
 if((in->dm==Afpind||in->dm==Ampind)&&(typedlibraryrecord(typedfieldowner(&in->d))==8||typedlibraryrecord(typedfieldowner(&in->d))==6))return typedchanneloutput(in,fn,p,out,returned);
 if(in->sm!=Anone)return typedfail(p,"channel-operands");
 if(!t || t->kind!=Tchan || !t->tof)return typedfail(p,"channel-payload");kind=t->tof->kind;
 if(!((in->op==INEWCB && kind==Tbyte) || (in->op==INEWCW && (kind==Tint || kind==Tfix)) || (in->op==INEWCL && kind==Tbig) || (in->op==INEWCF && kind==Treal) ||
      (in->op==INEWCP && (kind==Tstring || kind==Tref || kind==Tchan || kind==Tarray || kind==Tlist || kind==Tmodule))))return typedfail(p,"channel-payload");
 return typedchanneloutput(in,fn,p,out,returned);
}

/* Both source payload and canonical descriptor must prove this bounded geometry. */
static int
typedmappedpayload(Type*t,Dis64Layout*layout)
{
 Decl*field;unsigned int i,n;
 if(!t || t->kind!=Ttuple || !dis64layouttype(t,layout))return 0;n=layout->count;
 if((n!=2 && n!=3) || layout->legacybytes!=4*n || layout->bytes!=8*n || layout->alignment!=8)return 0;
 for(i=0,field=t->ids;field;field=field->next,i++)if(i>=n || !field->ty || field->ty->kind!=layout->leaves[i].kind)return 0;
 if(i!=n)return 0;
 for(i=0;i<n;i++)if(layout->leaves[i].legacy!=4*i || layout->leaves[i].native!=8*i || layout->leaves[i].oldbytes!=4 || layout->leaves[i].bytes!=8 ||
   (layout->leaves[i].kind!=Tany && layout->leaves[i].kind!=Tstring && layout->leaves[i].kind!=Tref && layout->leaves[i].kind!=Tchan && layout->leaves[i].kind!=Tarray && layout->leaves[i].kind!=Tlist && layout->leaves[i].kind!=Tmodule))return 0;
 return 1;
}

static unsigned int
typedrootcount(Type*t)
{Dis64Layout l={0};unsigned int n=typedmappedpayload(t,&l)?l.count:0;dis64layoutfree(&l);return n;}
/* A complete two/three-root tuple crossing an ordinary channel. */
static int
typedroottransfer(Inst*in,Dis64TypedBody*p,Dis64Instruction*out,uchar*returned)
{int send=in->op==ISEND,cm=send?in->dm:in->sm,vm=send?in->sm:in->dm,actual,payload;Addr*ca=send?&in->d:&in->s,*va=send?&in->s:&in->d;Type*t=ca->native64type?ca->native64type:ca->decl?ca->decl->ty:nil;Dis64Layout l={0},*storage;long channel,field=0;unsigned int cw,base,next,i,n;int ok=0;
 if(in->mm!=Anone||in->branch||!t||t->kind!=Tchan||!typedmappedpayload(t->tof,&l))goto done;n=l.count;
 if(cm==Afpind||cm==Ampind){if(typedlibraryrecord(typedfieldowner(ca))==12&&ca->offset==12&&n==2){Addr a=*ca;a.offset=0;a.native64type=typedfieldowner(ca);if(!typedoperand(cm==Afpind?Afp:Amp,&a,TypedRoot,0,0,p,&channel,&cw,&actual)||actual!=Tref)goto done;cw|=AIND;field=16;}else if(typedwmref(typedfieldowner(ca),"Draw","Context")&&ca->offset==8&&n==2){Addr a=*ca;a.offset=0;a.native64type=typedfieldowner(ca);if(!typedoperand(cm==Afpind?Afp:Amp,&a,TypedRoot,0,0,p,&channel,&cw,&actual)||actual!=Tref)goto done;cw|=AIND;field=16;}else if(ca->offset!=32||n!=3||!typedclientaddress(ca,cm,p,&channel,&cw,&field,&payload)||payload!=Tchan||field!=64)goto done;}
 else if(!typedoperand(cm,ca,TypedRoot,0,0,p,&channel,&cw,&actual)||actual!=Tchan)goto done;
 if(!send&&vm==Afpind&&va->reg==16&&!va->offset&&p->resultaggregate==48&&n==2&&t&&t->kind==Tchan){Dis64Layout result={0};unsigned int contract=typedaggregatecontract(t->tof,&result);dis64layoutfree(&result);if(contract!=48)goto done;*out=(Dis64Instruction){IRECV,SRC(cw)|DST(AFP|AIND),16,channel,32};*returned=47;ok=1;goto done;}
 if(vm!=Afp&&vm!=Amp)goto done;storage=vm==Afp?&p->frame:&p->data;
 for(i=0;i<n;i++){if(!dis64layoutaddress(storage,va->reg+4*i,4,1,&next)||(i&&next!=base+8*i))goto done;if(!i)base=next;}
 *out=(Dis64Instruction){in->op,send?SRC(vm==Afp?AFP:AMP)|DST(cw):SRC(cw)|DST(vm==Afp?AFP:AMP),(field<<8)|8*n,send?base:channel,send?channel:base};*returned=107;ok=1;
 done:dis64layoutfree(&l);return ok;}

/* Descriptor deduplication may merge root classes with identical GC geometry. */
static int typedfileiopayload(Type*);
static int typedfileiochannel(Addr*,int,Dis64TypedBody*,long*,unsigned int*,long*);
static int
typedclientmappedchannel(Inst*in,Decl*fn,Dis64TypedBody*p,Dis64Instruction*out,uchar*returned)
{
 Dis64Layout payload={0},descriptor={0};Decl*d=in->s.decl;Type*t;unsigned int n;int ok=0,mixed;
 if(in->sm!=Adesc || !d || !d->desc || d->desc->id!=in->s.offset || !d->desc->map || d->desc->nmap!=1)goto done;
 if(typedlibraryrecord(typedfieldowner(&in->d))==14){
  long base,field;unsigned int mode,kind;
  t=in->d.native64type;
  if(!t||t->kind!=Tchan||!(kind=typedfileiopayload(t->tof))||
     !typedfileiochannel(&in->d,in->dm,p,&base,&mode,&field)||
     field!=8*(kind-1)||d->desc->size!=16||d->desc->map[0]!=(kind==1?0x10:0x50)||
     in->mm!=Aimm||in->m.offset||in->branch)goto done;
  *out=(Dis64Instruction){INEWCMP,SRC(AIMM)|AXIMM|DST(mode),0,4+kind,base};
  *returned=92;ok=1;goto done;
 }
 if((in->dm==Afpind||in->dm==Ampind)&&typedlibraryrecord(typedfieldowner(&in->d))==12&&in->d.offset==12){Addr a=in->d;Type*owner=typedfieldowner(&a);long base;unsigned int dm;int actual;
  t=in->d.native64type;if(!t||t->kind!=Tchan||!typedmappedpayload(t->tof,&payload)||payload.count!=2||!typedmappedpayload(d->ty,&descriptor)||descriptor.count!=2||d->desc->size!=8||d->desc->map[0]!=0xc0||in->mm!=Aimm||in->m.offset||in->branch)goto done;
  a.offset=0;a.native64type=owner;if(!typedoperand(in->dm==Afpind?Afp:Amp,&a,TypedRoot,0,0,p,&base,&dm,&actual)||actual!=Tref)goto done;
  *out=(Dis64Instruction){INEWCMP,SRC(AIMM)|AXIMM|DST(dm|AIND),0,2,base};*returned=92;ok=1;goto done;
 }
 if((in->dm==Afpind||in->dm==Ampind)&&typedlibraryrecord(typedfieldowner(&in->d))==6&&in->d.offset==32){Addr a=in->d;Type*owner=typedfieldowner(&a);long base;unsigned int dm;int actual;
  t=in->d.native64type;if(!t||t->kind!=Tchan||!typedmappedpayload(t->tof,&payload)||payload.count!=3||!typedmappedpayload(d->ty,&descriptor)||descriptor.count!=3||d->desc->size!=12||d->desc->map[0]!=0xe0||in->mm!=Aimm||in->m.offset||in->branch)goto done;
  a.offset=0;a.native64type=owner;if(!typedoperand(in->dm==Afpind?Afp:Amp,&a,TypedRoot,0,0,p,&base,&dm,&actual)||actual!=Tref)goto done;
  *out=(Dis64Instruction){INEWCMP,SRC(AIMM)|AXIMM|DST(dm|AIND),0,3,base};*returned=92;ok=1;goto done;
 }
 t=in->dm==Afpind?fn->ty->tof:in->d.native64type?in->d.native64type:in->d.decl?in->d.decl->ty:nil;
 if(t&&t->kind==Tchan&&typedcursortype(t->tof,0)){if(!typedcursortype(d->ty,1)||d->desc->size!=12||d->desc->map[0]!=0xa0||!typedchanneloutput(in,fn,p,out,returned))goto done;out->source=4;*returned=*returned?185:183;ok=1;goto done;}

 if(!t || t->kind!=Tchan)goto done;mixed=typedreplylayout(t->tof,&payload,0);
 if(mixed){if(!typedreplylayout(d->ty,&descriptor,0))goto done;}
 else{
  dis64layoutfree(&payload);
  if(!typedmappedpayload(t->tof,&payload) || !typedmappedpayload(d->ty,&descriptor))goto done;
 }
 n=payload.count;if(descriptor.count!=n || d->desc->size!=4*n || d->desc->map[0]!=(mixed?0x40:n==2?0xc0:0xe0))goto done;
 if(!typedchanneloutput(in,fn,p,out,returned))goto done;
 out->source=mixed?1:n;*returned=*returned?185:183;ok=1;
 done:dis64layoutfree(&payload);dis64layoutfree(&descriptor);return ok?1:typedfail(p,"mapped-channel-layout");
}

/* A scratch Decl retains its storage type after reuse. Indirect access needs
 * the original base owner, not the field value's type or the scratch's first type. */
static Type*
typedfieldowner(Addr*a)
{
 if(a->native64base)return a->native64base;
 if(!a->decl || (a->decl->store==Dlocal && a->decl->sym && a->decl->sym->name[0]=='.' && a->decl->sym->name[1]=='t'))return nil;
 return a->decl->ty;
}

/* One exact Context field and one direct mapped root. No nested dereference. */
static int
typedclientfield(Inst *in,Dis64TypedBody *p,unsigned int pc,int probe)
{
 Addr *a,owner;Type *t;Dis64Layout layout={0};unsigned int i,bm,vm;long base,value=0;int actual,read,kind,ok=0;vm=AFP;
 read=in->sm==Afpind || in->sm==Ampind;a=read?&in->s:&in->d;
 if(in->op!=IMOVP || in->mm!=Anone || in->branch || (read?(in->dm!=Afp && in->dm!=Amp):(in->dm!=Afpind && in->dm!=Ampind)) ||
    !a->decl || !(t=typedfieldowner(a)) || t->kind!=Tref || !(t=t->tof) || t->kind!=Tadt ||
    !t->decl || !t->decl->sym || strcmp(t->decl->sym->name,"Context") || !t->decl->dot ||
    !t->decl->dot->sym || strcmp(t->decl->dot->sym->name,"Draw") ||
    a->offset<0 || a->offset>8 || a->offset%4 || !dis64layouttype(t,&layout) || layout.bytes!=24 || layout.count!=3)goto done;
 for(i=0;i<3;i++)if(layout.leaves[i].legacy!=4*i || layout.leaves[i].native!=8*i || layout.leaves[i].oldbytes!=4 || layout.leaves[i].bytes!=8 || layout.leaves[i].kind!=(i==2?Tchan:Tref))goto done;
 owner=*a;owner.offset=0;owner.native64type=typedfieldowner(a);
 if(!typedoperand((read?in->sm:in->dm)==Afpind?Afp:Amp,&owner,TypedRoot,0,0,p,&base,&bm,&actual) || actual!=Tref)goto done;
 kind=a->offset==8?Tchan:Tref;
 if(!probe && (!typedoperand(read?in->dm:in->sm,read?&in->d:&in->s,TypedRoot,0,0,p,&value,&vm,&actual) ||
    (actual!=kind && actual!=Tany) || (vm!=AFP && vm!=AMP)))goto done;
 if(!p->contextfields){p->contextfields=allocmem(p->count*sizeof(long));memset(p->contextfields,0,p->count*sizeof(long));}
 p->contextfields[pc]=2*a->offset;
 p->code[pc]=(Dis64Instruction){IMOVP,read?SRC(bm|AIND)|DST(vm):SRC(vm)|DST(bm|AIND),0,read?base:value,read?value:base};ok=1;
 done:dis64layoutfree(&layout);return ok?1:typedfail(p,"context-field-roles");
}

/* One Context field copied to the same field in another Context. */
static int
typedcontextcopy(Inst*in,Dis64TypedBody*p,unsigned int pc)
{
 Inst probe=*in;Dis64Instruction read;long displacement;
 if(in->op!=IMOVP || in->mm!=Anone || in->branch || (in->sm!=Afpind && in->sm!=Ampind) || (in->dm!=Afpind && in->dm!=Ampind) || in->s.offset!=in->d.offset)return typedfail(p,"context-copy-roles");
 probe.dm=Afp;probe.d=in->s;if(!typedclientfield(&probe,p,pc,1))return 0;read=p->code[pc];displacement=p->contextfields[pc];
 probe=*in;probe.sm=Afp;probe.s=in->d;if(!typedclientfield(&probe,p,pc,1))return 0;
 p->code[pc]=(Dis64Instruction){IMOVP,SRC(USRC(read.mode))|DST(UDST(p->code[pc].mode)),displacement,read.source,p->code[pc].destination};p->returns[pc]=173;return 1;
}

#include "dis64-window.h"
#include "dis64-wmcontext.h"
#include "dis64-client-channels.h"
#include "dis64-window-list.h"
static int typedwordargop(int);
#include "dis64-draw-fields.h"
#include "dis64-rect-list.h"
#include "dis64-raw-array.h"

/* Immutable byte constants are immediate operands, never mutable MP storage. */
static int
typedclientbyteoperand(int mode,Addr *a,int kind,Dis64TypedBody *p,long *value,unsigned int *wire)
{
 int actual;Decl *d=a->decl;
 if(kind==Tbyte && mode==Amp && d && d->ty && d->ty->kind==Tbyte && d->init && d->init->op==Oconst &&
    a->reg==d->offset && a->offset==0 && d->init->val>=0 && d->init->val<=255){*value=d->init->val;*wire=AIMM;return 1;}
 return typedoperand(mode,a,kind,kind==Tint || kind==Tbyte,0,p,value,wire,&actual);
}
static int
typedclientbyte(Inst *in,Dis64TypedBody *p,unsigned int pc)
{
 unsigned int sm,dm;long source,destination;int sk=in->op==ICVTWB?Tint:Tbyte,dk=in->op==ICVTBW?Tint:Tbyte,actual;
 if(in->mm!=Anone || in->branch || !typedclientbyteoperand(in->sm,&in->s,sk,p,&source,&sm))return typedfail(p,"client-byte-roles");
 if(in->dm==Afpind){
  if(p->resultkind!=dk || in->d.reg!=16 || in->d.offset)return typedfail(p,"client-byte-result");
  destination=32;dm=AFP|AIND;
 }else if(!typedoperand(in->dm,&in->d,dk,0,0,p,&destination,&dm,&actual) || dm!=AFP)return typedfail(p,"client-byte-roles");
 p->code[pc]=(Dis64Instruction){in->op,SRC(sm)|DST(dm),0,source,destination};if(dm==(AFP|AIND))p->returns[pc]=1;return 1;
}
/* Reuse the existing reserved first tail cell; the scalar result uses the second. */
static int
typedclientindex(Inst *in,Inst *use,Dis64TypedBody *p,unsigned int pc)
{
 unsigned int sm,dm,um;long source,index,value;int actual,read;Addr *a;
 if((in->op!=IINDB && in->op!=IINDW) || in->branch || in->mm!=Afp || !in->m.decl || in->m.offset ||
    !typedoperand(in->sm,&in->s,Tarray,0,0,p,&source,&sm,&actual) || sm!=AFP ||
    !typedoperand(in->dm,&in->d,Tint,1,0,p,&index,&dm,&actual) || (dm!=AFP && dm!=AIMM))return typedfail(p,"client-index-roles");
 if(!use || use->branch || use->mm!=Anone)return typedfail(p,"client-index-consumer");
 read=use->sm==Afpind;a=read?&use->s:&use->d;
 if(a->decl!=in->m.decl || a->reg!=in->m.reg || a->offset ||
    (in->op==IINDW?use->op!=IMOVW:(read?(use->op!=IMOVB && use->op!=ICVTBW):(use->op!=IMOVB && use->op!=ICVTWB))) || (!read && use->dm!=Afpind))return typedfail(p,"client-index-address");
 if(read){if(!typedoperand(use->dm,&use->d,use->op==IMOVB?Tbyte:Tint,0,0,p,&value,&um,&actual) || um!=AFP)return 0;}
 else if(!typedclientbyteoperand(use->sm,&use->s,use->op==IMOVB?Tbyte:Tint,p,&value,&um) || (um!=AFP && um!=AIMM))return 0;
 p->code[pc]=(Dis64Instruction){in->op,SRC(AFP)|AXINF|DST(dm),p->frame.bytes,source,index};
 p->code[pc+1]=(Dis64Instruction){use->op,read?SRC(AFP|AIND)|DST(AFP):SRC(um)|DST(AFP|AIND),0,read?p->frame.bytes:value,read?value:p->frame.bytes};
 if(!p->contextfields){p->contextfields=allocmem(p->count*sizeof(long));memset(p->contextfields,0,p->count*sizeof(long));}
 return 1;
}

/* Only canonical Draw.Image references; array element slots are mapped roots. */
static int
typedimagearrayaddr(Addr*a)
{
 Type*t=a->native64type?a->native64type:a->decl?a->decl->ty:nil;
 return a->decl && t && t->kind==Tarray && typedwmref(t->tof,"Draw","Image") && a->reg==a->decl->offset && !a->offset;
}
static int
typedstringarray(Addr*a)
{Type*t=a->native64type?a->native64type:a->decl?a->decl->ty:nil;return a->decl&&t&&t->kind==Tarray&&t->tof&&t->tof->kind==Tstring&&a->reg==a->decl->offset&&!a->offset;}
static int
typedstringarraystable(TypedMulti*c,Inst*in)
{unsigned int i;Inst*v;if(in->sm==Afp)return 1;if(in->sm!=Amp||!in->s.decl||in->s.decl->store!=Dglobal)return 0;for(i=0;i<c->count;i++){v=c->instructions[i];if(v&&((v->dm==Amp&&v->d.decl==in->s.decl)||(v->op==ILEA&&v->sm==Amp&&v->s.decl==in->s.decl)))return 0;}return 1;}
/* Every closed managed element occupies one mapped native pointer cell. */
static Type*
typedrootarrayelement(Addr *a)
{
 Type *t=typedloadvaluetype(a);
 return a->decl && t && t->kind==Tarray && t->tof && typedmanagedkind(t->tof->kind) && a->reg==a->decl->offset && !a->offset?t->tof:nil;
}
static int
typedrootarrayindex(Inst*in,Inst*use,Dis64TypedBody*p,unsigned int pc)
{long array,index,value;unsigned int sm,dm,vm;int actual,read;Addr*a;Type*element=typedrootarrayelement(&in->s),*other;
 if(in->op!=IINDW || !element || (in->sm!=Afp && in->sm!=Amp) || in->mm!=Afp || !in->m.decl || in->m.offset || in->branch || !use || !typedoperand(in->sm,&in->s,Tarray,0,0,p,&array,&sm,&actual) || !typedoperand(in->dm,&in->d,Tint,1,0,p,&index,&dm,&actual))return typedfail(p,"root-array-index");
 if(use->op==IBEQW || use->op==IBNEW || use->op==IBEQC || use->op==IBNEC){
  long mid;unsigned int mm;
  if(((use->op==IBEQC || use->op==IBNEC) != (element->kind==Tstring)) || use->sm!=Afpind || use->s.decl!=in->m.decl || use->s.reg!=in->m.reg || use->s.offset || use->mm!=Amp || !use->m.decl || !use->m.decl->ty || use->m.decl->ty->kind!=Tany || !use->branch || use->dm!=Apc || use->branch->pc<(int)p->first || use->branch->pc>=(int)(p->first+p->count) || use->d.offset!=use->branch->pc || !typedoperand(Amp,&use->m,TypedRoot,0,1,p,&mid,&mm,&actual))return typedfail(p,"root-array-nil-compare");
  p->code[pc]=(Dis64Instruction){IINDL,SRC(sm)|AXINF|DST(dm),48,array,index};p->returns[pc]=255;
  p->code[pc+1]=(Dis64Instruction){use->op==IBEQW?IBEQL:use->op==IBNEW?IBNEL:use->op,SRC(AFP|AIND)|AXINM|DST(AIMM),mid,48,use->branch->pc-p->first};p->returns[pc+1]=111;return 1;
 }
 if(use->op!=IMOVP || use->mm!=Anone || use->branch)return typedfail(p,"root-array-element-use");
 read=use->sm==Afpind;a=read?&use->s:&use->d;
 if((!read && use->dm!=Afpind) || a->decl!=in->m.decl || a->reg!=in->m.reg || a->offset)return typedfail(p,"root-array-borrow");
 p->code[pc]=(Dis64Instruction){IINDL,SRC(sm)|AXINF|DST(dm),48,array,index};p->returns[pc]=255;
 if(read && use->dm==Afpind && use->d.reg==16 && !use->d.offset){
  if(!tequal(p->resulttype,element))return typedfail(p,"root-array-result-type");
  p->code[pc+1]=(Dis64Instruction){IMOVP,SRC(AFP|AIND)|DST(AFP|AIND),0,48,32};p->returns[pc+1]=111;return 1;
 }
 a=read?&use->d:&use->s;other=typedloadvaluetype(a);
 if(!typedoperand(read?use->dm:use->sm,a,TypedRoot,0,0,p,&value,&vm,&actual) || (actual!=Tany && (!other || !tequal(other,element))))return typedfail(p,"root-array-element-type");
 p->code[pc+1]=(Dis64Instruction){IMOVP,read?SRC(AFP|AIND)|DST(vm):SRC(vm)|DST(AFP|AIND),0,read?48:value,read?value:48};p->returns[pc+1]=255;return 1;}
/* Borrow one element of the shell's builtin table for the adjacent mapped copy. */
static int
typedbuiltinarray(Addr *a)
{
 Type *t=a->native64type?a->native64type:a->decl?a->decl->ty:nil;
 Decl *fields;
 if(!a->decl||a->offset||a->reg!=a->decl->offset||!t||t->kind!=Tarray||
    !(t=t->tof)||t->kind!=Ttuple)return 0;
 fields=t->ids;
 return fields&&fields->ty==tstring&&fields->next&&!fields->next->next&&
        fields->next->ty->kind==Tlist&&typedshellbuiltin(fields->next->ty->tof);
}

static int
typedbuiltinarrayindex(Inst *in,Inst *use,Dis64TypedBody *p,unsigned int pc)
{
 Addr *borrow,*value;
 Decl *d=use->m.decl;
 unsigned int sm,dm,vm,base,next;
 long array,index;
 int actual,read=use->sm==Afpind;
 if(in->sm!=Afp||in->mm!=Afp||in->m.offset||!in->m.decl||in->branch||
    use->op!=IMOVMP||use->mm!=Adesc||use->branch||!d||!d->desc||
    d->desc->id!=use->m.offset||d->desc->size!=8||d->desc->nmap!=1||
    !d->desc->map||d->desc->map[0]!=0xc0||
    !typedoperand(in->sm,&in->s,Tarray,0,0,p,&array,&sm,&actual)||
    !typedoperand(in->dm,&in->d,Tint,1,0,p,&index,&dm,&actual))goto bad;
 borrow=read?&use->s:&use->d;
 value=read?&use->d:&use->s;
 vm=read?use->dm:use->sm;
 if((!read&&use->dm!=Afpind)||borrow->decl!=in->m.decl||
    borrow->reg!=in->m.reg||borrow->offset||vm!=Afp||value->offset||
    !dis64layoutaddress(&p->frame,value->reg,4,1,&base)||
    !dis64layoutaddress(&p->frame,value->reg+4,4,1,&next)||next!=base+8)goto bad;
 p->code[pc]=(Dis64Instruction){IINDX,SRC(sm)|AXINF|DST(dm),48,array,index};
 p->code[pc+1]=(Dis64Instruction){IMOVMP,
  (read?SRC(AFP|AIND)|DST(AFP):SRC(AFP)|DST(AFP|AIND))|AXIMM,0,
  read?48:base,read?base:48};
 p->returns[pc]=p->returns[pc+1]=255;
 return 1;
 bad:return typedfail(p,"builtin-array-layout");
}

static int
typedexternalstringarray(TypedMulti*c,Dis64TypedBody*p,Inst*frame,unsigned int end,unsigned int*at,unsigned int old,unsigned int native)
{Inst*in=c->instructions[*at],*use;long array,index,middle;unsigned int sm,dm,mm;int actual;
 if(*at+1>=end||!typedstringarray(&in->s)||!typedstringarraystable(c,in)||in->mm!=Afp||!in->m.decl||in->m.offset||in->branch||!typedoperand(in->sm,&in->s,Tarray,0,0,p,&array,&sm,&actual)||!typedoperand(in->dm,&in->d,Tint,1,0,p,&index,&dm,&actual))return typedfail(p,"external-string-array-index");use=c->instructions[*at+1];
 if(use->op!=IADDC||use->sm!=Afpind||use->s.decl!=in->m.decl||use->s.reg!=in->m.reg||use->s.offset||use->branch||use->dm!=Afpind||use->d.reg!=frame->d.reg||use->d.offset!=old||!typedoperand(use->mm,&use->m,Tstring,0,1,p,&middle,&mm,&actual))return typedfail(p,"external-string-array-concat");
 p->code[*at-p->first]=(Dis64Instruction){IINDL,SRC(sm)|AXINF|DST(dm),48,array,index};p->returns[(*at)++-p->first]=76;p->code[*at-p->first]=(Dis64Instruction){IADDC,SRC(AFP|AIND)|(mm==AFP?AXINF:AXINM)|DST(AFP|AIND),middle,48,native};p->returns[(*at)++-p->first]=76;return 1;}
static int
typedregistryarray(Addr*a)
{Type*t=a->native64type?a->native64type:a->decl?a->decl->ty:nil;return a->decl&&t&&t->kind==Tarray&&(typedwmref(t->tof,"Wmsrv","Client")||typedlibraryrecord(t->tof)==12)&&a->reg==a->decl->offset&&!a->offset;}
static int
typedrootarraycopy(Inst*in,Dis64TypedBody*p,unsigned int pc)
{long source,dest,mid;unsigned int sm,dm,mm;int actual;
 if(in->op!=ISLICELA||in->branch||!typedrootarrayelement(&in->s)||!typedrootarrayelement(&in->d)||!tequal(typedloadvaluetype(&in->s),typedloadvaluetype(&in->d))||!typedoperand(in->sm,&in->s,Tarray,0,0,p,&source,&sm,&actual)||!typedoperand(in->dm,&in->d,Tarray,0,0,p,&dest,&dm,&actual)||!typedoperand(in->mm,&in->m,Tint,1,1,p,&mid,&mm,&actual))return typedfail(p,"root-array-copy");
 p->code[pc]=(Dis64Instruction){ISLICELA,SRC(sm)|(mm==AIMM?AXIMM:mm==AFP?AXINF:AXINM)|DST(dm),mid,source,dest};p->returns[pc]=111;return 1;}
/* Registry element addresses live only until the immediately following use. */
static int
typedregistryindex(Inst*in,Inst*use,Dis64TypedBody*p,unsigned int pc,unsigned int stop,Type*result)
{long array,index,value,mid;unsigned int sm,dm,vm,mm;int actual;Addr*a;
 if(in->op!=IINDW||!typedregistryarray(&in->s)||in->mm!=Afp||!in->m.decl||in->m.offset||in->branch||!use||!typedoperand(in->sm,&in->s,Tarray,0,0,p,&array,&sm,&actual))return typedfail(p,"registry-array-index");
 if(in->dm==Afpind||in->dm==Ampind){long field;int payload;
  if(in->d.offset!=36||!typedclientaddress(&in->d,in->dm,p,&index,&dm,&field,&payload)||field!=72||payload!=Tint)return typedfail(p,"registry-client-id");index|=72L<<16;
  p->code[pc]=(Dis64Instruction){IINDL,SRC(sm)|AXINF|DST(dm),48,array,index};p->returns[pc]=111;
 }else{if(!typedoperand(in->dm,&in->d,Tint,1,0,p,&index,&dm,&actual))return 0;p->code[pc]=(Dis64Instruction){IINDL,SRC(sm)|AXINF|DST(dm),48,array,index};p->returns[pc]=255;}
 if(use->op==IBEQW||use->op==IBNEW){
  if(use->sm!=Afpind||use->s.decl!=in->m.decl||use->s.reg!=in->m.reg||use->s.offset||use->mm!=Amp||!use->m.decl||!use->m.decl->ty||use->m.decl->ty->kind!=Tany||!use->branch||use->dm!=Apc||use->branch->pc<(int)p->first||use->branch->pc>=(int)stop||use->d.offset!=use->branch->pc||!typedoperand(use->mm,&use->m,TypedRoot,0,1,p,&mid,&mm,&actual))return typedfail(p,"registry-element-nil");
  p->code[pc+1]=(Dis64Instruction){use->op==IBEQW?IBEQL:IBNEL,SRC(AFP|AIND)|AXINM|DST(AIMM),mid,48,use->branch->pc-p->first};p->returns[pc+1]=111;return 1;
 }
 if(use->op==IMOVP&&use->sm==Afpind&&use->dm==Afpind&&use->d.reg==16&&!use->d.offset&&typedwmref(result,"Wmsrv","Client")){
  if(use->s.decl!=in->m.decl||use->s.reg!=in->m.reg||use->s.offset||use->mm!=Anone||use->branch)return 0;
  p->code[pc+1]=(Dis64Instruction){IMOVP,SRC(AFP|AIND)|DST(AFP|AIND),0,48,32};p->returns[pc+1]=111;return 1;
 }
 if(use->op!=IMOVP||use->mm!=Anone||use->branch)return typedfail(p,"registry-element-use");
 a=use->sm==Afpind?&use->s:&use->d;if(a->decl!=in->m.decl||a->reg!=in->m.reg||a->offset)return 0;
 if(!typedoperand(use->sm==Afpind?use->dm:use->sm,use->sm==Afpind?&use->d:&use->s,TypedRoot,0,0,p,&value,&vm,&actual)||(actual!=Tref&&actual!=Tany))return 0;
 p->code[pc+1]=(Dis64Instruction){IMOVP,use->sm==Afpind?SRC(AFP|AIND)|DST(vm):SRC(vm)|DST(AFP|AIND),0,use->sm==Afpind?48:value,use->sm==Afpind?value:48};p->returns[pc+1]=255;return 1;
}
static int
typedrootarraynew(Inst*in,Dis64TypedBody*p,unsigned int pc)
{
 Decl*d=in->m.decl;Type*element=typedrootarrayelement(&in->d);long source,dest;unsigned int sm,dm;int actual;
 if((in->op!=INEWA && in->op!=INEWAZ) || !element || in->mm!=Adesc || !d || !d->desc || d->desc->id!=in->m.offset || !tequal(d->ty,element) || d->desc->size!=4 || d->desc->nmap!=1 || !d->desc->map || d->desc->map[0]!=0x80 || in->branch ||
    !typedoperand(in->sm,&in->s,Tint,1,0,p,&source,&sm,&actual) || !typedoperand(in->dm,&in->d,Tarray,0,0,p,&dest,&dm,&actual) || (dm!=AFP && dm!=AMP))return typedfail(p,"root-array-allocation");
 p->code[pc]=(Dis64Instruction){INEWAZ,SRC(sm)|AXIMM|DST(dm),2,source,dest};p->returns[pc]=254;return 1;
}
/* A rooted Window list head may prepare the following borrowed array store. */
static int
typedimagearraywindowindex(Inst*in,Inst*head,Inst*store,Dis64TypedBody*p,unsigned int pc)
{long array,index;unsigned int sm,dm;int actual;Addr*a=&store->d;
 if(in->op!=IINDW||!typedimagearrayaddr(&in->s)||in->mm!=Afp||!in->m.decl||in->m.offset||in->branch||!typedoperand(in->sm,&in->s,Tarray,0,0,p,&array,&sm,&actual)||!typedoperand(in->dm,&in->d,Tint,1,0,p,&index,&dm,&actual)||head->op!=IHEADP||head->dm!=Afp||store->sm!=Afpind||store->s.reg!=head->d.reg||store->s.decl!=head->d.decl||store->dm!=Afpind||a->decl!=in->m.decl||a->reg!=in->m.reg||a->offset||!typedwindowlist(head,p,p->code+pc+1,p->returns+pc+1,nil)||!typedlibraryarraystore(store,p,pc+2))return typedfail(p,"window-array-index");
 p->code[pc]=(Dis64Instruction){IINDL,SRC(sm)|AXINF|DST(dm),48,array,index};p->returns[pc]=255;return 1;}
static int
typedrootarrayslice(Inst*in,Dis64TypedBody*p,unsigned int pc,Type*result)
{long source,middle;unsigned int sm,mm;int actual;
 if(in->op!=ISLICEA||in->dm!=Afpind||in->d.reg!=16||in->d.offset||!result||result->kind!=Tarray||!result->tof||!typedmanagedkind(result->tof->kind)||in->branch||!typedoperand(in->sm,&in->s,Tint,1,0,p,&source,&sm,&actual)||!typedoperand(in->mm,&in->m,Tint,1,1,p,&middle,&mm,&actual)||(mm!=AIMM&&mm!=AFP))return typedfail(p,"root-array-result-slice");
 p->code[pc]=(Dis64Instruction){ISLICEA,SRC(sm)|(mm==AIMM?AXIMM:AXINF)|DST(AFP|AIND),middle,source,32};p->returns[pc]=1;return 1;}

/* Array result owner remains a non-temporary FP root throughout the call. */
static int
typedimagearrayresultindex(Inst*in,Dis64TypedBody*p,unsigned int pc)
{
 long array,index;unsigned int sm,dm;int actual;
 if(in->op!=IINDW || in->sm!=Afp || !typedimagearrayaddr(&in->s) || !in->s.decl->sym || in->s.decl->sym->name[0]=='.' || in->mm!=Afp || !in->m.decl || in->m.offset || in->branch ||
    !typedoperand(Afp,&in->s,Tarray,0,0,p,&array,&sm,&actual) || !typedoperand(in->dm,&in->d,Tint,1,0,p,&index,&dm,&actual))return typedfail(p,"array-result-owner");
 p->code[pc]=(Dis64Instruction){IINDL,SRC(AFP)|AXINF|DST(dm),48,array,index};p->returns[pc]=131;return 1;
}
static int
typedimagearraybinding(TypedMulti*c,Dis64TypedBody*p,Inst*frame,Inst*binding,Type*result,unsigned int pc,unsigned int at)
{
 Inst*index;if(pc==p->first || !(index=c->instructions[pc-1]) || p->returns[pc-1-p->first]!=131 || !typedwmref(result,"Draw","Image") ||
    binding->op!=ILEA || binding->sm!=Afpind || binding->s.decl!=index->m.decl || binding->s.reg!=index->m.reg || binding->s.offset || binding->mm!=Anone || binding->branch ||
    binding->dm!=Afpind || binding->d.reg!=frame->d.reg || binding->d.offset!=16)return 0;
 p->code[at-p->first]=(Dis64Instruction){ILEA,SRC(AFP)|DST(AFP|AIND),0,p->code[pc-1-p->first].source,32};p->returns[at-p->first]=131;return 1;
}
static int
typedrefarrayarg(TypedMulti*c,Dis64TypedBody*p,Inst*frame,Decl*arg,unsigned int end,unsigned int*at,unsigned int offset)
{
 Inst*in=c->instructions[*at],*use;long array,index;unsigned int sm,dm;int actual;
 Type *element=typedrootarrayelement(&in->s);
 if(*at+1>=end || !(use=c->instructions[*at+1]) || in->op!=IINDW || !element || !tequal(element,arg->ty) || in->mm!=Afp || !in->m.decl || in->m.offset || in->branch ||
    !typedoperand(in->sm,&in->s,Tarray,0,0,p,&array,&sm,&actual) || !typedoperand(in->dm,&in->d,Tint,1,0,p,&index,&dm,&actual) ||
    use->op!=IMOVP || use->sm!=Afpind || use->s.decl!=in->m.decl || use->s.reg!=in->m.reg || use->s.offset || use->mm!=Anone || use->branch ||
    use->dm!=Afpind || use->d.reg!=frame->d.reg || use->d.offset!=arg->offset)return typedfail(p,"array-call-argument");
 p->code[*at-p->first]=(Dis64Instruction){IINDL,SRC(sm)|AXINF|DST(dm),56,array,index};p->returns[(*at)++-p->first]=127;
 p->code[*at-p->first]=(Dis64Instruction){IMOVP,SRC(AFP|AIND)|DST(AFP|AIND),0,56,offset};p->returns[(*at)++-p->first]=127;return 1;
}
/* Retain an Image from an array before publishing its Screen argument. */
static int
typedimagescreenarg(TypedMulti*c,Dis64TypedBody*p,Inst*frame,unsigned int end,unsigned int*at,unsigned int old,unsigned int native)
{Inst*in=c->instructions[*at],*head,*field,store;long array,index,temp;unsigned int sm,dm,tm;int actual;
 if(*at+3>=end||in->op!=IINDW||!typedimagearrayaddr(&in->s)||in->mm!=Afp||!in->m.decl||in->m.offset||in->branch||!typedoperand(in->sm,&in->s,Tarray,0,0,p,&array,&sm,&actual)||!typedoperand(in->dm,&in->d,Tint,1,0,p,&index,&dm,&actual))return typedfail(p,"array-screen-index");
 head=c->instructions[*at+1];field=c->instructions[*at+2];
 if(head->op!=IMOVP||head->sm!=Afpind||head->s.reg!=in->m.reg||head->s.decl!=in->m.decl||head->s.offset||head->dm!=Afp||head->mm!=Anone||head->branch||!typedoperand(Afp,&head->d,TypedRoot,0,0,p,&temp,&tm,&actual)||actual!=Tref||field->sm!=Afpind||field->s.reg!=head->d.reg||field->s.decl!=head->d.decl||field->s.offset!=48||field->dm!=Afpind||field->d.reg!=frame->d.reg||field->d.offset!=old||!typedselectedfieldarg(field,p,*at+2-p->first,native,Tref))return typedfail(p,"array-screen-field");
 p->code[*at-p->first]=(Dis64Instruction){IINDL,SRC(sm)|AXINF|DST(dm),56,array,index};p->returns[*at-p->first]=143;
 p->code[*at+1-p->first]=(Dis64Instruction){IMOVP,SRC(AFP|AIND)|DST(AFP),0,56,temp};p->returns[*at+1-p->first]=143;
 *at+=3;store=*head;store.sm=Afp;store.s=head->d;store.s.offset=0;if(!typedrootargclear(c,p,&store,end,at)||p->returns[*at-1-p->first]!=126)return typedfail(p,"array-screen-clear");return 1;}

static int
typedwordargop(int op)
{
 return op==IADDW || op==ISUBW || op==IMULW || op==IDIVW || op==IMODW || op==IANDW || op==IORW || op==IXORW || op==ISHLW || op==ISHRW;
}
static int
typedwordargument(Inst*in,Inst*frame,Dis64TypedBody*p,unsigned int pc,unsigned int oldoffset,unsigned int offset)
{
 long source,middle;unsigned int sm,mm;int actual;
 if(!typedwordargop(in->op) || in->branch || in->dm!=Afpind || in->d.reg!=frame->d.reg || in->d.offset!=oldoffset ||
    !typedoperand(in->sm,&in->s,Tint,1,0,p,&source,&sm,&actual) || !typedoperand(in->mm,&in->m,Tint,1,1,p,&middle,&mm,&actual))return typedfail(p,"word-call-argument");
 p->code[pc]=(Dis64Instruction){in->op,SRC(sm)|(mm==AIMM?AXIMM:mm==AFP?AXINF:AXINM)|DST(AFP|AIND),middle,source,offset};p->returns[pc]=130;return 1;
}

static int
typedloadbytes(Inst *in,Dis64TypedBody *p,unsigned int pc)
{
 Addr *a=in->op==ICVTAC?&in->s:&in->d;
 if(!typedbytearrayaddr(a))return typedfail(p,"load-byte-array-class");
 if(in->op==INEWA || in->op==INEWAZ){
  Decl*d=in->m.decl;
  if(!d || !d->ty || d->ty->kind!=Tbyte)return typedfail(p,"load-byte-array-class");
 }
 return 1;
}

static int
typedclientarray(Inst *in,Dis64TypedBody *p,unsigned int pc)
{
 unsigned int sm,mm,dm;long source,middle,destination;int actual;Decl *d=in->m.decl;
 if(in->branch || !typedoperand(in->sm,&in->s,Tint,1,0,p,&source,&sm,&actual) ||
    !typedoperand(in->dm,&in->d,Tarray,0,0,p,&destination,&dm,&actual) || dm!=AFP)return typedfail(p,"byte-array-roles");
 if(in->op==INEWA || in->op==INEWAZ){
  if(in->mm!=Adesc || !d || !d->ty || (d->ty->kind!=Tbyte && d->ty->kind!=Tint) || !d->desc || d->desc->id!=in->m.offset)return typedfail(p,"byte-array-descriptor");
  middle=d->ty->kind==Tint?1:0;mm=AIMM;
 }else if(!typedoperand(in->mm,&in->m,Tint,1,1,p,&middle,&mm,&actual) || (mm!=AIMM && mm!=AFP))return typedfail(p,"byte-array-slice");
 p->code[pc]=(Dis64Instruction){in->op==INEWA?INEWAZ:in->op,SRC(sm)|(mm==AIMM?AXIMM:AXINF)|DST(dm),middle,source,destination};return 1;
}

static int
typedclientcodec(Inst *in,Dis64TypedBody *p,unsigned int pc)
{
 unsigned int sk,dk,sm,dm;long source,destination;int actual;
 switch(in->op){
 case ICVTCW:sk=Tstring;dk=Tint;break;case ICVTCL:sk=Tstring;dk=Tbig;break;case ICVTCF:sk=Tstring;dk=Treal;break;
 case ICVTWC:sk=Tint;dk=Tstring;break;case ICVTLC:sk=Tbig;dk=Tstring;break;case ICVTFC:sk=Treal;dk=Tstring;break;
 case ICVTCA:sk=Tstring;dk=Tarray;break;case ICVTAC:sk=Tarray;dk=Tstring;break;
 default:return 0;
 }
 if(p->sourceroles&&in->mm==Anone&&!in->branch){long field;unsigned int wire;
  if(in->op==ICVTCW&&(in->dm==Afpind||in->dm==Ampind)&&typedwindowowner(&in->d)){
   if(!typedwindowwordbase(&in->d,in->dm,p,pc,&destination,&wire,&field)||!typedoperand(in->sm,&in->s,Tstring,0,0,p,&source,&sm,&actual)||actual!=Tstring)return typedfail(p,"window-word-parse");p->code[pc]=(Dis64Instruction){ICVTCW,SRC(sm)|DST(wire|AIND),field,source,destination};p->returns[pc]=63;return 1;
  }
  if(in->op==ICVTWC&&(in->sm==Afpind||in->sm==Ampind)&&typedwindowowner(&in->s)){
   if(!typedwindowwordbase(&in->s,in->sm,p,pc,&source,&wire,&field)||!typedoperand(in->dm,&in->d,Tstring,0,0,p,&destination,&dm,&actual)||dm!=AFP)return typedfail(p,"window-word-format");p->code[pc]=(Dis64Instruction){ICVTWC,SRC(wire|AIND)|DST(dm),field,source,destination};p->returns[pc]=63;return 1;
  }
 }
 if(in->dm==Afpind && p->resultpair>=3 && !in->d.offset && in->d.reg==16 && dk==(p->resultpair==3?Tint:p->resultpair==4?Tbig:Treal)){
  if(in->mm!=Anone || in->branch || !typedoperand(in->sm,&in->s,sk,sk==Tint,0,p,&source,&sm,&actual))return typedfail(p,"numeric-string-result");p->code[pc]=(Dis64Instruction){in->op,SRC(sm)|DST(AFP|AIND),0,source,32};p->returns[pc]=8;return 1;
 }
 if(in->mm!=Anone || in->branch || !typedoperand(in->sm,&in->s,sk,sk==Tint,0,p,&source,&sm,&actual) ||
    !typedoperand(in->dm,&in->d,dk,0,0,p,&destination,&dm,&actual) || dm!=AFP)return typedfail(p,"client-codec-roles");
 p->code[pc]=(Dis64Instruction){in->op,SRC(sm)|DST(dm),0,source,destination};return 1;
}

static int
typedclientstring(Inst *in,Dis64TypedBody *p,unsigned int pc,unsigned int stop)
{
 unsigned int sm,mm,dm;long source,middle,destination;int actual,index=in->op==IINDC;
 if(in->op==ISLICEC && in->dm==Afpind && in->d.reg==16 && (p->resultkind==Tstring || (p->resultpair && (in->d.offset?p->resulttype->ids->next->ty:p->resulttype->ids->ty)->kind==Tstring))){
  if(in->branch || (in->d.offset!=0 && !(p->resultpair && in->d.offset==(p->resultpair>=4?8:4))) || !typedoperand(in->sm,&in->s,Tint,1,0,p,&source,&sm,&actual) || !typedoperand(in->mm,&in->m,Tint,1,1,p,&middle,&mm,&actual) || (mm!=AIMM && mm!=AFP))return typedfail(p,"string-result-slice");
  p->code[pc]=(Dis64Instruction){ISLICEC,SRC(sm)|(mm==AIMM?AXIMM:AXINF)|DST(AFP|AIND),middle,source,in->d.offset?40:32};p->returns[pc]=p->resultpair?(in->d.offset?16:8):1;return 1;
 }
 if(in->op==IADDC || in->op==IINSC || in->op==ISLICEC){
  int numeric=in->op!=IADDC;
  if(in->branch || !typedoperand(in->sm,&in->s,numeric?Tint:Tstring,numeric,0,p,&source,&sm,&actual) ||
     !typedoperand(in->mm==Anone?in->dm:in->mm,in->mm==Anone?&in->d:&in->m,numeric?Tint:Tstring,numeric,1,p,&middle,&mm,&actual) ||
     !typedoperand(in->dm,&in->d,Tstring,0,0,p,&destination,&dm,&actual) || (dm!=AFP && !(p->sourceroles && in->op==ISLICEC && dm==AMP)) ||
     (numeric?(mm!=AIMM && mm!=AFP):(mm!=AFP && mm!=AMP)))return typedfail(p,"string-write-roles");
  p->code[pc]=(Dis64Instruction){in->op,SRC(sm)|(mm==AIMM?AXIMM:mm==AFP?AXINF:AXINM)|DST(dm),middle,source,destination};return 1;
 }

 if((in->op==IBEQC || in->op==IBNEC) && (in->sm==Afpind || in->sm==Ampind) && typedwmref(typedfieldowner(&in->s),"Wmsrv","Client")){
  long field;int payload;
  if(in->s.offset!=20 || !in->s.native64type || in->s.native64type->kind!=Tstring || !typedclientaddress(&in->s,in->sm,p,&source,&sm,&field,&payload) || field!=40 || payload!=Tstring ||
     !typedoperand(in->mm,&in->m,Tstring,0,1,p,&middle,&mm,&actual) || actual!=Tstring || (mm!=AFP && mm!=AMP) || middle>65535 ||
     in->dm!=Apc || !in->branch || in->branch->pc<(int)p->first || in->branch->pc>=(int)stop || in->d.offset!=in->branch->pc)return typedfail(p,"client-cursor-compare");
  p->code[pc]=(Dis64Instruction){in->op,SRC(sm)|(mm==AFP?AXINF:AXINM)|DST(AIMM),middle,source,in->branch->pc-p->first};p->returns[pc]=232;return 1;
 }
 if(!typedoperand(in->sm,&in->s,Tstring,0,0,p,&source,&sm,&actual) ||
    !typedoperand(in->mm,&in->m,index?Tint:Tstring,index,1,p,&middle,&mm,&actual))return 0;
 if(index){if(in->branch || !typedoperand(in->dm,&in->d,Tint,0,0,p,&destination,&dm,&actual) || dm!=AFP || (mm!=AIMM && mm!=AFP))return typedfail(p,"string-index-roles");}
 else{
  if(in->dm!=Apc || !in->branch || in->branch->pc<(int)p->first || in->branch->pc>=(int)stop || in->d.offset!=in->branch->pc || (mm!=AFP && mm!=AMP))return typedfail(p,"string-branch-roles");
  destination=in->branch->pc-p->first;dm=AIMM;
 }
 p->code[pc]=(Dis64Instruction){in->op,SRC(sm)|(mm==AIMM?AXIMM:mm==AFP?AXINF:AXINM)|DST(dm),middle,source,destination};return 1;
}

/* Bounded String-list traversal, rooted head, raw Word32 argument, nil clear. */
static int
typedexternalinthead(TypedMulti*c,Dis64TypedBody*p,Inst*frame,unsigned int end,unsigned int*at,unsigned int oldoffset,unsigned int offset)
{
 Inst*in;Addr temp;unsigned int n=0,wire;long source;int actual;
 if(*at>=end || !(in=c->instructions[*at]) || (in->op!=ITAIL && in->op!=IHEADP) || in->dm!=Afp || !in->d.decl || in->d.decl->store!=Dlocal || !in->d.decl->sym || in->d.decl->sym->name[0]!='.' || in->d.decl->sym->name[1]!='t' || in->d.offset)return typedfail(p,"external-int-head-temp");
 temp=in->d;
 while(*at<end && (in=c->instructions[*at]) && in->op==ITAIL && in->dm==Afp){
  if(n++==32 || in->dm!=Afp || in->d.decl!=temp.decl || in->d.reg!=temp.reg || in->d.offset || !typedloadlisttype(&in->s) || !typedloadlisttype(&in->d) || (n>1 && (in->sm!=Afp || in->s.decl!=temp.decl || in->s.reg!=temp.reg || in->s.offset)) || !typedclientlist(in,p,p->code+*at-p->first))return typedfail(p,"external-int-head-tail");
  p->returns[(*at)++-p->first]=253;
 }
 if(*at>=end || !(in=c->instructions[*at]) || in->op!=IHEADP || in->dm!=Afp || in->d.decl!=temp.decl || in->d.reg!=temp.reg || in->d.offset || !typedloadlisttype(&in->s) || (n && (in->sm!=Afp || in->s.decl!=temp.decl || in->s.reg!=temp.reg || in->s.offset)) || !typedclientlist(in,p,p->code+*at-p->first))return typedfail(p,"external-int-head-read");
 p->returns[(*at)++-p->first]=253;
 if(*at>=end || !(in=c->instructions[*at]) || in->op!=ICVTCW || in->mm!=Anone || in->branch || in->sm!=Afp || in->s.decl!=temp.decl || in->s.reg!=temp.reg || in->s.offset || in->dm!=Afpind || in->d.reg!=frame->d.reg || in->d.offset!=oldoffset || !typedoperand(Afp,&in->s,Tstring,0,0,p,&source,&wire,&actual) || actual!=Tstring)return typedfail(p,"external-int-head-convert");
 p->code[*at-p->first]=(Dis64Instruction){ICVTCW,SRC(AFP)|DST(AFP|AIND),0,source,offset};p->returns[(*at)++-p->first]=253;
 if(*at>=end || !(in=c->instructions[*at]) || in->op!=IMOVP || in->sm!=Amp || in->mm!=Anone || in->branch || in->dm!=Afp || in->d.decl!=temp.decl || in->d.reg!=temp.reg || in->d.offset || !in->s.decl || !in->s.decl->ty || in->s.decl->ty->kind!=Tany || !in->s.decl->sym || strcmp(in->s.decl->sym->name,"nil") || in->s.reg!=in->s.decl->offset || in->s.offset || !typedinst(in,c->count,p,p->code+*at-p->first,p->returns+*at-p->first))return typedfail(p,"external-int-head-clear");
 p->returns[(*at)++-p->first]=253;return 1;
}

/* Bounded tail chain, head into a mapped argument, exact nil clear. */
static int
typedexternalheadarg(TypedMulti*c,Dis64TypedBody*p,Inst*frame,Decl*arg,unsigned int end,unsigned int*at,unsigned int offset)
{
 Inst*in;Addr temp;unsigned int n=0,wire;long source;int actual;
 if(*at>=end || !(in=c->instructions[*at]) || in->op!=ITAIL || !in->d.decl || in->d.decl->store!=Dlocal || !in->d.decl->sym || in->d.decl->sym->name[0]!='.' || in->d.decl->sym->name[1]!='t' || in->dm!=Afp || !typedloadlisttype(&in->d))return typedfail(p,"external-head-temp");
 temp=in->d;
 while(*at<end && (in=c->instructions[*at]) && in->op==ITAIL && in->dm==Afp){
  if(n++==32 || in->dm!=Afp || in->d.decl!=temp.decl || in->d.reg!=temp.reg || in->d.offset || !typedloadlisttype(&in->s) || (n>1 && (in->sm!=Afp || in->s.decl!=temp.decl || in->s.reg!=temp.reg || in->s.offset)) || !typedclientlist(in,p,p->code+*at-p->first))return typedfail(p,"external-head-tail");
  p->returns[(*at)++-p->first]=252;
 }
 if(*at>=end || !(in=c->instructions[*at]) || in->op!=(arg->ty->kind==Tlist?ITAIL:IHEADP) || in->mm!=Anone || in->branch || in->sm!=Afp || in->s.decl!=temp.decl || in->s.reg!=temp.reg || in->s.offset || !typedloadlisttype(&in->s) || in->dm!=Afpind || in->d.reg!=frame->d.reg || in->d.offset!=arg->offset || !typedoperand(Afp,&in->s,Tlist,0,0,p,&source,&wire,&actual) || actual!=Tlist)return typedfail(p,"external-head-argument");
 p->code[*at-p->first]=(Dis64Instruction){in->op,SRC(AFP)|DST(AFP|AIND),0,source,offset};p->returns[(*at)++-p->first]=252;
 if(*at>=end || !(in=c->instructions[*at]) || in->op!=IMOVP || in->sm!=Amp || in->mm!=Anone || in->branch || in->dm!=Afp || in->d.decl!=temp.decl || in->d.reg!=temp.reg || in->d.offset || !in->s.decl || !in->s.decl->ty || in->s.decl->ty->kind!=Tany || !in->s.decl->sym || strcmp(in->s.decl->sym->name,"nil") || in->s.reg!=in->s.decl->offset || in->s.offset || !typedinst(in,c->count,p,p->code+*at-p->first,p->returns+*at-p->first))return typedfail(p,"external-head-clear");
 p->returns[(*at)++-p->first]=252;return 1;
}

static int
typedexternalstringexpr(TypedMulti*c,Dis64TypedBody*p,Inst*frame,Decl*arg,unsigned int end,unsigned int*at,unsigned int offset)
{
 Inst*in=c->instructions[*at],*first=in;unsigned int n=0,sm,mm,ntemp=1,j;Addr temps[8];long source,middle;int actual;
 if(in->op==IMOVP&&typedlibraryrecord(typedfieldowner(&in->s))==12&&in->s.offset==16){if(in->dm!=Afp||in->mm!=Anone||in->branch||!in->d.decl||in->d.decl->store!=Dlocal||!in->d.decl->sym||in->d.decl->sym->name[0]!='.'||in->d.decl->sym->name[1]!='t'||in->d.offset||!typedlibraryinst(in,p,*at-p->first,end))return typedfail(p,"client-dir-expression");(*at)++;}
 else if(in->op==IHEADP){if(in->dm!=Afp||in->mm!=Anone||in->branch||!typedloadlisttype(&in->s)||!in->d.decl||in->d.decl->store!=Dlocal||!in->d.decl->sym||in->d.decl->sym->name[0]!='.'||in->d.decl->sym->name[1]!='t'||!typedclientlist(in,p,p->code+*at-p->first))return typedfail(p,"local-head-string-expression");p->returns[(*at)++-p->first]=44;}
 else if(in->op==ILENC){Inst*copy,*slice;long len;unsigned int lm;if(*at+2>=end||in->mm!=Anone||in->branch||in->dm!=Afp||!typedoperand(in->sm,&in->s,Tstring,0,0,p,&source,&sm,&actual)||actual!=Tstring||!typedoperand(Afp,&in->d,Tint,0,0,p,&len,&lm,&actual)||!typedinst(in,end,p,p->code+*at-p->first,p->returns+*at-p->first))return typedfail(p,"external-slice-length");p->returns[(*at)++-p->first]=87;
  copy=c->instructions[*at];slice=c->instructions[*at+1];if(copy->op!=IMOVP||copy->sm!=in->sm||copy->s.reg!=in->s.reg||copy->s.decl!=in->s.decl||copy->s.offset||copy->dm!=Afp||copy->mm!=Anone||copy->branch||!copy->d.decl||copy->d.decl->store!=Dlocal||!copy->d.decl->sym||copy->d.decl->sym->name[0]!='.'||copy->d.decl->sym->name[1]!='t'||copy->d.offset||!typedinst(copy,end,p,p->code+*at-p->first,p->returns+*at-p->first))return typedfail(p,"external-slice-copy");p->returns[(*at)++-p->first]=87;
  if(slice->op!=ISLICEC||slice->dm!=Afp||slice->d.reg!=copy->d.reg||slice->d.decl!=copy->d.decl||slice->d.offset||slice->mm!=Afp||slice->m.reg!=in->d.reg||slice->m.decl!=in->d.decl||!typedclientstring(slice,p,*at-p->first,end))return typedfail(p,"external-slice-range");p->returns[(*at)++-p->first]=87;first=copy;
 }else{if(in->op!=ICVTWC || in->dm!=Afp || !in->d.decl || in->d.decl->store!=Dlocal || !in->d.decl->sym || in->d.decl->sym->name[0]!='.' || in->d.decl->sym->name[1]!='t' || in->d.offset || !typedclientcodec(in,p,*at-p->first))return typedfail(p,"external-string-expression");if(p->returns[*at-p->first]!=63)p->returns[*at-p->first]=180;(*at)++;}
 temps[0]=first->d;
 while(*at<end&&(in=c->instructions[*at])&&in->dm==Afp&&(in->op==IADDC||in->op==ICVTWC)){
  if(n++>=16)return typedfail(p,"external-string-expression");
  if(in->op==ICVTWC){if(ntemp==8||!in->d.decl||in->d.decl->store!=Dlocal||!in->d.decl->sym||in->d.decl->sym->name[0]!='.'||in->d.decl->sym->name[1]!='t'||in->d.offset||!typedclientcodec(in,p,*at-p->first))return typedfail(p,"external-string-expression");for(j=0;j<ntemp;j++)if(in->d.reg==temps[j].reg)return typedfail(p,"external-string-temp-alias");temps[ntemp++]=in->d;}
  else{for(j=0;j<ntemp;j++)if(in->d.reg==temps[j].reg&&in->d.decl==temps[j].decl)break;if(j==ntemp||in->d.offset||!((in->sm==Afp&&in->s.reg==temps[j].reg)||in->mm==Anone||(in->mm==Afp&&in->m.reg==temps[j].reg))||!typedclientstring(in,p,*at-p->first,end))return typedfail(p,"external-string-expression");}
  if(p->returns[*at-p->first]!=63)p->returns[*at-p->first]=180;(*at)++;
 }
 if(*at>=end || !(in=c->instructions[*at]) || in->op!=IADDC || in->dm!=Afpind || in->d.reg!=frame->d.reg || in->d.offset!=arg->offset || in->branch ||
    !((in->sm==Afp && in->s.decl==first->d.decl && in->s.reg==first->d.reg && !in->s.offset) || (in->mm==Afp && in->m.decl==first->d.decl && in->m.reg==first->d.reg && !in->m.offset)) ||
    !typedoperand(in->sm,&in->s,Tstring,0,0,p,&source,&sm,&actual) || actual!=Tstring ||
    !typedoperand(in->mm,&in->m,Tstring,0,1,p,&middle,&mm,&actual) || actual!=Tstring || (mm!=AFP && mm!=AMP))return typedfail(p,"external-string-argument");
 p->code[*at-p->first]=(Dis64Instruction){IADDC,SRC(sm)|(mm==AFP?AXINF:AXINM)|DST(AFP|AIND),middle,source,offset};p->returns[(*at)++-p->first]=64;
 for(j=0;j<ntemp;j++){
 if(*at>=end || !(in=c->instructions[*at]) || in->op!=IMOVP || in->sm!=Amp || in->mm!=Anone || in->dm!=Afp || in->branch ||
    !in->s.decl || in->s.decl->store!=Dglobal || !in->s.decl->ty || in->s.decl->ty->kind!=Tany || !in->s.decl->sym || strcmp(in->s.decl->sym->name,"nil") || in->s.reg!=in->s.decl->offset || in->s.offset ||
    in->d.decl!=temps[j].decl || in->d.reg!=temps[j].reg || in->d.offset || !typedinst(in,end,p,p->code+*at-p->first,p->returns+*at-p->first))return typedfail(p,"external-string-clear");
 p->returns[(*at)++-p->first]=181;
 }return 1;
}

static int
typedexternalbytestring(TypedMulti*c,Dis64TypedBody*p,Inst*frame,unsigned int end,unsigned int*at,unsigned int old,unsigned int native)
{Inst*a,*b,*v,store;long source;unsigned int sm,pc=*at-p->first,clear;int actual;if(*at+3>=end)return typedfail(p,"external-byte-string-shape");a=c->instructions[*at];b=c->instructions[*at+1];v=c->instructions[*at+2];
 if(a->op!=IMOVP||a->dm!=Afp||a->mm!=Anone||a->branch||!a->d.decl||a->d.decl->store!=Dlocal||!a->d.decl->sym||a->d.decl->sym->name[0]!='.'||a->d.decl->sym->name[1]!='t'||!typedbytearrayaddr(&a->s)||!typedbytearrayaddr(&a->d)||!typedinst(a,end,p,p->code+pc,p->returns+pc))return typedfail(p,"external-byte-string-copy");
 if(b->op!=ISLICEA||b->dm!=Afp||b->d.reg!=a->d.reg||b->d.decl!=a->d.decl||!typedloadbytes(b,p,pc+1)||!typedclientarray(b,p,pc+1))return typedfail(p,"external-byte-string-slice");
 if(v->op!=ICVTAC||v->mm!=Anone||v->branch||v->sm!=Afp||v->s.reg!=a->d.reg||v->s.decl!=a->d.decl||v->dm!=Afpind||v->d.reg!=frame->d.reg||v->d.offset!=old||!typedoperand(Afp,&v->s,Tarray,0,0,p,&source,&sm,&actual))return typedfail(p,"external-byte-string-convert");
 p->returns[pc]=p->returns[pc+1]=69;p->code[pc+2]=(Dis64Instruction){ICVTAC,SRC(AFP)|DST(AFP|AIND),0,source,native};p->returns[pc+2]=70;*at+=3;clear=*at;store=*v;if(!typedrootargclear(c,p,&store,end,at)||*at!=clear+1)return typedfail(p,"external-byte-string-clear");p->returns[clear-p->first]=71;return 1;}

static int typedlibraryoperand(int,Addr*,int,Dis64TypedBody*,long*,unsigned int*,int*,unsigned int*);
static int typedclientreferencekind(int);
static int
typedrootrecordaddress(Addr*a,int mode,Dis64TypedBody*p,unsigned int pc,long*base,unsigned int*wire,long*field,unsigned int*id)
{Inst probe;unsigned int lib;int actual;long v;memset(&probe,0,sizeof(probe));probe.op=IMOVP;probe.sm=mode;probe.s=*a;probe.mm=Anone;probe.dm=Afp;probe.d=*a;
 if(typedwindowowner(a)){if(!typedwindowfield(&probe,p,pc,1,nil))return 0;*id=1;}
 else if(typeddrawowner(a)==3){if(!typeddrawfield(&probe,p,pc,1,nil))return 0;*id=4;}
 else if((lib=typedlibraryrecord(typedfieldowner(a)))==8||lib==10){if(!typedlibraryoperand(mode,a,TypedRoot,p,&v,wire,&actual,&lib))return 0;*id=lib==8?2:5;*field=(unsigned long)v>>16;*base=v&65535;return 1;}
 else{if(!typedwmref(typedfieldowner(a),"Draw","Context")||!typedclientfield(&probe,p,pc,1))return 0;*id=3;}
 *base=p->code[pc].source;*wire=USRC(p->code[pc].mode);*field=p->contextfields[pc];return 1;}
static int
typedrootfieldcopy(Inst*in,Dis64TypedBody*p,unsigned int pc)
{long src,dst,sf,df;unsigned int sm,dm,si,di;if(in->op!=IMOVP||in->mm!=Anone||in->branch||(in->sm!=Afpind&&in->sm!=Ampind)||(in->dm!=Afpind&&in->dm!=Ampind)||!in->s.native64type||!in->d.native64type||!typedclientreferencekind(in->s.native64type->kind)||!tequal(in->s.native64type,in->d.native64type)||!typedrootrecordaddress(&in->s,in->sm,p,pc,&src,&sm,&sf,&si)||!typedrootrecordaddress(&in->d,in->dm,p,pc,&dst,&dm,&df,&di))return 0;p->code[pc]=(Dis64Instruction){IMOVP,SRC(sm)|DST(dm),(si<<24)|(di<<20)|(sf<<8)|df,src,dst};p->returns[pc]=73;return 1;}

/* Every nested owner is retained in one compiler scratch root until handoff. */
static int
typedexternalrootpath(TypedMulti*c,Dis64TypedBody*p,Inst*frame,unsigned int end,unsigned int*at,unsigned int old,unsigned int native,Type *type)
{Inst*in,*first=nil;unsigned int n=0,id,pc;Inst field;
 if(typedheadfieldstart(c,*at,end)){
  long source,dest;
  unsigned int sm,dm;
  int actual;
  Type *list,*item;
  in=c->instructions[*at];list=typedloadvaluetype(&in->s);item=typedloadvaluetype(&in->d);
  if(!list||list->kind!=Tlist||!list->tof||list->tof->kind!=Tref||!item||!tequal(list->tof,item)||
     in->mm!=Anone||in->branch||!in->d.decl||in->d.decl->store!=Dlocal||!in->d.decl->sym||
     in->d.decl->sym->name[0]!='.'||in->d.decl->sym->name[1]!='t'||in->s.offset||in->d.offset||
     !typedoperand(in->sm,&in->s,Tlist,0,0,p,&source,&sm,&actual)||actual!=Tlist||
     !typedoperand(Afp,&in->d,TypedRoot,0,0,p,&dest,&dm,&actual)||actual!=Tref)
   return typedfail(p,"root-path-head");
  pc=(*at)++-p->first;first=in;n++;
  p->code[pc]=(Dis64Instruction){IHEADP,SRC(sm)|DST(dm),0,source,dest};p->returns[pc]=61;
 }
 while(*at<end&&(in=c->instructions[*at])&&in->op==IMOVP&&in->dm==Afp&&(in->sm==Afpind||in->sm==Ampind)){
  pc=*at-p->first;if(n++==8||in->mm!=Anone||in->branch||!in->d.decl||in->d.decl->store!=Dlocal||!in->d.decl->sym||in->d.decl->sym->name[0]!='.'||in->d.decl->sym->name[1]!='t'||in->d.offset||!in->s.native64type||in->s.native64type->kind!=Tref)return typedfail(p,"root-path-owner");
  if(first&&(in->sm!=Afpind||in->s.reg!=first->d.reg||in->s.decl!=first->d.decl||in->d.reg!=first->d.reg||in->d.decl!=first->d.decl))return typedfail(p,"root-path-alias");
  if(typedwindowowner(&in->s)){if(!typedwindowfield(in,p,pc,0,nil))return 0;id=1;p->code[pc].middle=(id<<16)|p->contextfields[pc];}
  else if(typedlibraryrecord(typedfieldowner(&in->s))==8||typedlibraryrecord(typedfieldowner(&in->s))==10){id=typedlibraryrecord(typedfieldowner(&in->s))==8?2:5;if(!typedlibraryinst(in,p,pc,end))return typedfail(p,"root-path-record");p->code[pc].middle=(id<<16)|((unsigned long)p->code[pc].source>>16);p->code[pc].source&=65535;}
  else if(typeddrawowner(&in->s)==3){if(!typeddrawfield(in,p,pc,0,nil))return 0;id=4;p->code[pc].middle=(id<<16)|p->contextfields[pc];}
  else if(typedwmref(typedfieldowner(&in->s),"Draw","Context")){if(!typedclientfield(in,p,pc,0))return 0;id=3;p->code[pc].middle=(id<<16)|p->contextfields[pc];}
  else if(!typedrecordpath(in,p,pc))return typedfail(p,"root-path-context");
  p->returns[pc]=61;if(!first)first=in;(*at)++;
 }
 if(!first||*at>=end||!(in=c->instructions[*at])||in->op!=IMOVP||in->sm!=Afpind||in->s.reg!=first->d.reg||in->s.decl!=first->d.decl||in->dm!=Afpind||in->d.reg!=frame->d.reg||in->d.offset!=old||
    !(typedlibraryargument(in,frame,p,*at-p->first,old,native,type->kind)||
      typedrecordargument(in,frame,p,type,*at-p->first,old,native,0)||
      typedselectedfieldarg(in,p,*at-p->first,native,type->kind)))return typedfail(p,"root-path-argument");
 (*at)++;field=*in;field.sm=Afp;field.s=first->d;{unsigned int before=*at;if(!typedrootargclear(c,p,&field,end,at)||*at!=before+1)return typedfail(p,"root-path-clear");p->returns[before-p->first]=66;}return 1;}

/* Pointer comparisons use the complete native reference, including fused
 * Context field reads. The nil constant remains a readonly mapped MP root. */
static int
typedclientreferencekind(int kind)
{
 return kind==Tref || kind==Tchan || kind==Tarray || kind==Tlist || kind==Tmodule || kind==Tany;
}
static int
typedclientcompare(Inst *in,Dis64TypedBody *p,unsigned int pc,unsigned int stop)
{
 Addr *left=&in->s,*right=&in->m;int lm=in->sm,rm=in->mm,sk,mk;unsigned int sm,mm;long source,middle;
 if(in->dm!=Apc || !in->branch || in->branch->pc<(int)p->first || in->branch->pc>=(int)stop || in->d.offset!=in->branch->pc)return typedfail(p,"reference-branch");
 if(lm==Amp && rm==Afpind){Addr *a=left;int m=lm;left=right;right=a;lm=rm;rm=m;}
 if(lm==Afpind){
  Inst field=*in;Dis64Instruction *out=p->code+pc;long base;
  /* Reuse the same exact Context owner/field geometry, with a temporary
   * direct destination solely for deriving its checked base/displacement. */
  field.op=IMOVP;field.s=*left;field.sm=Afpind;field.mm=Anone;field.dm=Afp;field.d=*left;field.branch=nil;
  if(!typedclientfield(&field,p,pc,1) || !typedoperand(rm,right,TypedRoot,0,1,p,&middle,&mm,&mk) || mk!=Tany || mm!=AMP)return typedfail(p,"field-nil-roles");
  base=out->source;*out=(Dis64Instruction){in->op==IBEQW?IBEQL:in->op==IBNEW?IBNEL:in->op,SRC(AFP|AIND)|AXINM|DST(AIMM),middle,base,in->branch->pc-p->first};return 1;
 }
 if(!typedoperand(lm,left,TypedRoot,0,0,p,&source,&sm,&sk) || !typedoperand(rm,right,TypedRoot,0,1,p,&middle,&mm,&mk))return 0;
 if(sk==Tany && mk!=Tany){long v=source;unsigned int mode=sm;int kind=sk;source=middle;sm=mm;sk=mk;middle=v;mm=mode;mk=kind;}
 if(!typedclientreferencekind(sk) || !typedclientreferencekind(mk) || sm!=AFP ||
    (mk==Tany?mm!=AMP:mm!=AFP || sk!=mk || sk==Tmodule))return typedfail(p,"reference-compare-roles");
 p->code[pc]=(Dis64Instruction){in->op==IBEQW?IBEQL:in->op==IBNEW?IBNEL:in->op,SRC(sm)|(mm==AMP?AXINM:AXINF)|DST(AIMM),middle,source,in->branch->pc-p->first};return 1;
}

/* Exact Context field/nil tests in the checked LOAD path. */
static int
typedloadcontextcompare(Inst*in,Dis64TypedBody*p,unsigned int pc,unsigned int stop)
{
 Addr *left=&in->s,*right=&in->m;int lm=in->sm,rm=in->mm,actual,kind;unsigned int mm,bm,index=0,window,draw,family;long middle,base,offset;Inst field=*in;
 if(in->dm!=Apc || !in->branch || in->branch->pc<(int)p->first || in->branch->pc>=(int)stop || in->d.offset!=in->branch->pc)return typedfail(p,"context-nil-branch");
 if((lm==Afp || lm==Amp) && (rm==Afpind || rm==Ampind)){left=&in->m;right=&in->s;lm=in->mm;rm=in->sm;}
 if((lm!=Afpind && lm!=Ampind) || (rm!=Afp && rm!=Amp))return typedfail(p,"context-nil-roles");
 if(!typedoperand(rm,right,TypedRoot,0,1,p,&middle,&mm,&actual) || (actual!=Tany && actual!=Tref))return typedfail(p,"field-reference-roles");
 field.op=IMOVP;field.s=*left;field.sm=lm;field.mm=Anone;field.dm=Afp;field.branch=nil;
 window=typedwindowowner(left);draw=typeddrawowner(left);
 if(actual==Tany && typedwmref(typedfieldowner(left),"Wmsrv","Client")){
  if(mm!=AMP || middle>65535 || !typedclientaddress(left,lm,p,&base,&bm,&offset,&kind) || (kind!=Tchan && !(kind==Tlist && offset==48)))return typedfail(p,"client-root-nil");
  p->code[pc]=(Dis64Instruction){in->op==IBEQW?IBEQL:IBNEL,SRC(bm)|AXINM|DST(AIMM),(offset<<16)|middle,base,in->branch->pc-p->first};p->returns[pc]=kind==Tlist?251:241;return 1;
 }
 if(actual!=Tany && typedwmref(typedfieldowner(left),"Wmsrv","Client")){
  if(!typedclientaddress(left,lm,p,&base,&bm,&offset,&kind) || kind!=Tref)return typedfail(p,"client-reference-field");family=4;
 }else{
  if(!(window?typedwindowfield(&field,p,pc,1,&index):draw?typeddrawfield(&field,p,pc,1,&index):typedclientfield(&field,p,pc,1)))return typedfail(p,"context-nil-roles");
  base=p->code[pc].source;offset=p->contextfields[pc];bm=lm==Afpind?AFP:AMP;family=draw?1:window?2:3;
 }
 if(actual==Tany){
  if(mm!=AMP || (!draw && !window && family!=3))return typedfail(p,"context-nil-roles");
  if(draw && index==2)return typedfail(p,"draw-string-nil-roles");
  p->code[pc]=(Dis64Instruction){in->op==IBEQW?IBEQL:IBNEL,SRC(bm|AIND)|AXINM|DST(AIMM),middle,base,in->branch->pc-p->first};
  p->returns[pc]=window?101+index:draw?114+index:97+left->offset/4;return 1;
 }
 if((draw && index==2) || (window && index==3) || (family==3 && left->offset==8) || middle<0 || middle>65535 || offset<0 || offset>255 || (mm!=AFP && mm!=AMP))return typedfail(p,"field-reference-class");
 p->code[pc]=(Dis64Instruction){in->op==IBEQW?IBEQL:IBNEL,SRC(lm==Afpind?AFP|AIND:AMP|AIND)|(mm==AFP?AXINF:AXINM)|DST(AIMM),(family<<24)|(offset<<16)|middle,base,in->branch->pc-p->first};
 p->returns[pc]=207;return 1;
}

/* Comparing a direct ref ADT with nil only observes the root pointer.
 * It neither admits the ADT's fields nor assumes a C payload geometry. */
static Type*
typedloadvaluetype(Addr*a)
{if(a->native64type&&a->decl&&a->decl->store==Dlocal&&a->decl->sym&&a->decl->sym->name[0]=='.'&&a->decl->sym->name[1]=='t'&&a->reg==a->decl->offset&&!a->offset)return a->native64type;return a->decl?a->decl->ty:nil;}
static int
typedloadreftype(Addr *a)
{
 Type *t;
 return a->decl && (t=typedloadvaluetype(a)) && t->kind==Tref && t->tof && t->tof->kind==Tadt &&
  a->reg==a->decl->offset && a->offset==0;
}

/* Only complete nominal Wmsrv.Client globals join the earlier FP ref route. */
static int
typedloadglobalclient(Addr*a,Dis64TypedBody*p)
{
 long base,field;unsigned int wire;int kind;
 return a->decl && a->decl->store==Dglobal && typedloadreftype(a) &&
  typedclientaddress(a,Ampind,p,&base,&wire,&field,&kind) && wire==(AMP|AIND) && field==0 && kind==Tchan;
}

/* Compare two direct roots of the same supported nominal type. */
static int
typedloadrootcompare(Inst*in,Dis64TypedBody*p,unsigned int pc,unsigned int stop,int element)
{
 Addr *a=&in->s,*b=&in->m;
 long source,middle,field,base;
 unsigned int sm,mm,wire;
 int kind;
 if((in->op!=IBEQW && in->op!=IBNEW && in->op!=IBEQL && in->op!=IBNEL) ||
    (in->sm!=Afp && in->sm!=Amp) || (in->mm!=Afp && in->mm!=Amp) ||
    !typedoperand(in->sm,a,TypedRoot,0,0,p,&source,&sm,&kind) || kind!=element ||
    !typedoperand(in->mm,b,TypedRoot,0,1,p,&middle,&mm,&kind) || kind!=element || middle>65535 ||
    in->dm!=Apc || !in->branch || in->branch->pc<(int)p->first ||
    in->branch->pc>=(int)stop || in->d.offset!=in->branch->pc)
  return typedfail(p,"load-root-compare");
 if(element==Tref &&
    (typedloadvaluetype(a)->tof!=typedloadvaluetype(b)->tof ||
     !typedwmref(a->decl->ty,"Wmsrv","Client") ||
     !typedclientaddress(a,in->sm==Afp?Afpind:Ampind,p,&base,&wire,&field,&kind) || field || kind!=Tchan ||
     !typedclientaddress(b,in->mm==Afp?Afpind:Ampind,p,&base,&wire,&field,&kind) || field || kind!=Tchan))
  return typedfail(p,"client-root-compare");
 p->code[pc]=(Dis64Instruction){in->op==IBEQW?IBEQL:in->op==IBNEW?IBNEL:in->op,
  SRC(sm)|(mm==AFP?AXINF:AXINM)|DST(AIMM),middle,source,in->branch->pc-p->first};
 p->returns[pc]=231;return 1;
}

/* Checked native startup needs direct ref ADT, list-of-String and Module nil tests.
 * Nil is the compiler's readonly Tany MP root; no aggregate dereference or
 * comparison of unrelated reference classes is admitted by this route. */
static int
typedloadcompare(Inst *in,Dis64TypedBody *p,unsigned int pc,unsigned int stop)
{
 Addr *value=&in->s,*empty=&in->m;int vm=in->sm,nm=in->mm,kind,actual;Type*valuetype;
 unsigned int sm,mm;long source,middle;
 if(typedloadreftype(value) && typedloadreftype(empty))return typedloadrootcompare(in,p,pc,stop,Tref);
 if(typedshellbuiltin(typedloadvaluetype(value)) && typedshellbuiltin(typedloadvaluetype(empty)))
  return typedloadrootcompare(in,p,pc,stop,Tmodule);
 if(typedloadvaluetype(value) && typedloadvaluetype(value)->kind==Tany){
  value=&in->m;empty=&in->s;vm=in->mm;nm=in->sm;
 }
 valuetype=value->decl?value->decl->ty:nil;
 if(value->native64type && value->decl && value->decl->store==Dlocal && value->decl->sym && value->decl->sym->name[0]=='.' && value->decl->sym->name[1]=='t' && value->reg==value->decl->offset && !value->offset)valuetype=value->native64type;
 if((in->op!=IBEQW && in->op!=IBNEW && in->op!=IBEQL && in->op!=IBNEL) ||
    !value->decl || !value->decl->ty ||
    (!(value->decl->ty->kind==Tmodule && value->reg==value->decl->offset) && !typedloadlisttype(value) && !typedshellrootlist(value) && !typedintlisttype(value) && !typedpointerlisttype(value) && !typedwindowlisttype(value) && !typedrectlisttype(value) && !typedshellmappedpairlist(value) && !typedloadreftype(value) && !(value->decl->ty->kind==Tchan && value->reg==value->decl->offset && !value->offset)) ||
    !empty->decl || !empty->decl->ty || empty->decl->ty->kind!=Tany || nm!=Amp ||
    !typedoperand(vm,value,TypedRoot,0,0,p,&source,&sm,&kind) || (!valuetype || kind!=valuetype->kind) || (typedloadreftype(value) && sm!=AFP && !(sm==AMP && typedloadglobalclient(value,p))) ||
    !typedoperand(nm,empty,TypedRoot,0,1,p,&middle,&mm,&actual) || actual!=Tany || (kind==Tchan && middle>65535) ||
    in->dm!=Apc || !in->branch || in->branch->pc<(int)p->first || in->branch->pc>=(int)stop ||
    in->d.offset!=in->branch->pc)return typedfail(p,"load-nil-roles");
 p->code[pc]=(Dis64Instruction){in->op==IBEQW?IBEQL:in->op==IBNEW?IBNEL:in->op,
  SRC(sm)|AXINM|DST(AIMM),middle,source,in->branch->pc-p->first};if(kind==Tchan)p->returns[pc]=239;return 1;
}

static int typedmultibody(TypedMulti*,unsigned int,unsigned int);

static int
typedchannelslot(TypedMulti*c,Dis64TypedBody*p,Addr*a)
{
 unsigned int i,j,found=0;Decl*fn,*d;int pass;
 if(!a->decl || !a->decl->ty || a->reg!=a->decl->offset || a->offset)return 0;
 if(typedchanneltriple(a->decl->ty))return 1;
 if(a->decl->ty->kind!=Tchan)return 0;
 for(i=0;i<(unsigned int)c->nf;i++)if(c->functions[i]->pc->pc==(int)p->first){
  fn=c->functions[i];for(j=0;j<3;j++)for(pass=0;pass<2;pass++)for(d=pass?fn->locals:fn->ty->ids;d;d=d->next)
   if(d->offset==a->reg+4*j && d->ty && d->ty->kind==Tchan)found++;
  return found==3;
 }return 0;
}
static int
typedchannelbinding(TypedMulti*c,Dis64TypedBody*p,Inst*binding,unsigned int*base)
{
 unsigned int i,at=0;
 if(binding->sm!=Afp || !typedchannelslot(c,p,&binding->s))return typedfail(p,"channel-result-slot");
 for(i=0;i<3;i++)if(!dis64layoutaddress(&p->frame,binding->s.reg+4*i,4,1,&at) || (i && at!=*base+8*i))return typedfail(p,"channel-result-layout");else if(!i)*base=at;
 return 1;
}

/* Exact by-value Draw geometry in a normal local CALL, not SPAWN. */
static int
typedlocalgeometryarg(Inst*in,Inst*frame,Decl*arg,Dis64TypedBody*p,Dis64TypedBody*child,unsigned int pc)
{
 Dis64Layout l={0};Addr a;Type*t;unsigned int i,base=0,at,wire,sm=0;long source=0,part;int actual,ok=0;
 if(!typedwordgeometry(arg->ty,&l) || in->op!=IMOVM || in->mm!=Aimm || in->m.offset!=l.bytes || in->branch || in->dm!=Afpind || in->d.reg!=frame->d.reg || in->d.offset!=arg->offset)goto done;
 for(i=0;i<l.bytes;i+=4){if(!dis64layoutaddress(&child->frame,arg->offset+i,4,0,&at) || at<64 || at>65535 || (i && at!=base+i))goto done;if(!i)base=at;}
 if(in->sm==Afpind || in->sm==Ampind){ok=typedimagegeometry(in,p,pc,base);goto done;}
 t=in->s.native64type?in->s.native64type:in->s.decl?in->s.decl->ty:nil;
 if((in->sm!=Afp && in->sm!=Amp) || t!=arg->ty)goto done;
 for(i=0;i<l.bytes;i+=4){a=in->s;a.reg+=i;a.offset=0;if(!typedoperand(in->sm,&a,Tint,0,0,p,&part,&wire,&actual) || actual!=Tint || (i && (wire!=sm || part!=source+i)))goto done;if(!i){source=part;sm=wire;}}
 p->code[pc]=(Dis64Instruction){IMOVM,SRC(sm)|AXIMM|DST(AFP|AIND),l.bytes,source,base};p->returns[pc]=64;ok=1;
 done:dis64layoutfree(&l);return ok?1:typedfail(p,"local-geometry-argument");
}

/* Literal Draw.Point/Rect leaves relocated against the actual local frame. */
static int
typedlocalgeometryliteral(TypedMulti*c,Dis64TypedBody*p,Dis64TypedBody*child,Inst*frame,Decl*arg,unsigned int end,unsigned int*at)
{
 Dis64Layout l={0};unsigned int i,base=0,part,k,sm=0,wire,n;long source=0,value;int actual,ok=0;Inst*in;Addr a;
 if(!typedwordgeometry(arg->ty,&l))goto done;
 for(i=0;i<l.bytes;i+=4){if(!dis64layoutaddress(&child->frame,arg->offset+i,4,0,&part) || part<64 || part>65535 || (i && part!=base+i))goto done;if(!i)base=part;}
 for(i=0;i<l.bytes;){
  if(*at>=end || !(in=c->instructions[*at]))goto done;
  if(in->op==IHEADM && i==0 && l.bytes==16){unsigned int mode;long src;int actual;if(in->mm!=Aimm||in->m.offset!=16||in->branch||in->dm!=Afpind||in->d.reg!=frame->d.reg||in->d.offset!=arg->offset||!typedrectlisttype(&in->s)||!typedoperand(in->sm,&in->s,Tlist,0,0,p,&src,&mode,&actual))goto done;p->code[*at-p->first]=(Dis64Instruction){IHEADM,SRC(mode)|AXIMM|DST(AFP|AIND),16,src,base};p->returns[(*at)++-p->first]=64;i=16;continue;}
  if(in->op==IHEADP || in->op==ITAIL){if(!typedexternalinthead(c,p,frame,end,at,arg->offset+i,base+i))goto done;i+=4;continue;}
  if(typedwordargop(in->op)){if(!typedwordargument(in,frame,p,*at-p->first,arg->offset+i,base+i))goto done;(*at)++;i+=4;continue;}
  if(in->op==IMOVM){
   n=in->m.offset;if(in->mm!=Aimm || (n!=8 && n!=16) || n>l.bytes-i || in->branch || in->dm!=Afpind || in->d.reg!=frame->d.reg || in->d.offset!=arg->offset+i || (in->sm!=Afp && in->sm!=Amp))goto done;
   for(k=0;k<n;k+=4){a=in->s;a.reg+=k;a.offset=0;if(!typedoperand(in->sm,&a,Tint,0,0,p,&value,&wire,&actual) || actual!=Tint || (k && (wire!=sm || value!=source+k)))goto done;if(!k){source=value;sm=wire;}}
   p->code[*at-p->first]=(Dis64Instruction){IMOVM,SRC(sm)|AXIMM|DST(AFP|AIND),n,source,base+i};p->returns[(*at)++-p->first]=64;i+=n;continue;
  }
  if(in->op!=IMOVW || in->mm!=Anone || in->branch || in->dm!=Afpind || in->d.reg!=frame->d.reg || in->d.offset!=arg->offset+i || !typedoperand(in->sm,&in->s,Tint,1,0,p,&source,&sm,&actual))goto done;
  p->code[*at-p->first]=(Dis64Instruction){IMOVW,SRC(sm)|DST(AFP|AIND),0,source,base+i};p->returns[(*at)++-p->first]=64;i+=4;
 }
 ok=1;
 done:dis64layoutfree(&l);return ok?1:typedfail(p,"local-geometry-literal");
}

/* A string-labelled handler refines only its own exception slot. The
 * front end records the per-use String type before restoring rtexception. */
static int
typedcaughtstring(Inst *in, Dis64TypedBody *p)
{
 Except *e;
 Label *label;
 int i;
 Addr *a=&in->s;
 if(in->op!=IMOVP||in->sm!=Afp||!a->decl||a->decl->ty!=rtexception||
    a->native64type!=tstring||a->offset||a->reg!=a->decl->offset)return 0;
 for(e=typedreportcatches;e;e=e->next){
  if(e->d!=a->decl||e->ne||!e->c||getpc(e->p1)<p->first||
     getpc(e->p2)>in->pc)continue;
  for(i=0;i<e->c->nlab;i++){
   label=e->c->labs+i;
   if(label->start&&label->start==label->stop&&label->start->ty==tstring&&
      label->inst&&label->inst->pc>=getpc(e->p2)&&label->inst->pc<=in->pc)return 1;
  }
 }
 return 0;
}

static int
typedlocalrecordlist(TypedMulti *c, Dis64TypedBody *p, Dis64TypedBody *child,
                     Inst *frame, Inst *alloc, Decl *arg, unsigned int end, unsigned int *at)
{
 Inst *init,*cons,*store,clear;
 Type *list;
 unsigned int start=*at,offset,mode,pc;
 long record,slot;
 int actual;

 if(end-start<5)return typedfail(p,"local-record-list-length");
 init=c->instructions[start];cons=c->instructions[start+1];store=c->instructions[start+2];
 list=typedloadvaluetype(&init->d);
 if(!list||!tequal(list,arg->ty)||init->d.reg==alloc->d.reg||
    cons->op!=ICONSP||cons->sm!=Afp||cons->mm!=Anone||cons->dm!=Afp||cons->branch||
    cons->s.decl!=alloc->d.decl||cons->s.reg!=alloc->d.reg||cons->s.offset||
    cons->d.decl!=init->d.decl||cons->d.reg!=init->d.reg||cons->d.offset||
    store->op!=IMOVP||store->sm!=Afp||store->mm!=Anone||store->dm!=Afpind||store->branch||
    store->s.decl!=init->d.decl||store->s.reg!=init->d.reg||store->s.offset||
    store->d.reg!=frame->d.reg||store->d.offset!=arg->offset||
    !typedoperand(Afp,&cons->s,TypedRoot,0,0,p,&record,&mode,&actual)||actual!=Tref||
    !typedoperand(Afp,&cons->d,TypedRoot,0,0,p,&slot,&mode,&actual)||actual!=Tlist||
    !dis64layoutaddress(&child->frame,arg->offset,4,1,&offset)||offset>65535)
  return typedfail(p,"local-record-list-shape");
 clear=*store;
 clear.s=init->d;
 if(!typedrootargclear(c,p,&clear,end,at)||*at!=start+1)
  return typedfail(p,"local-record-list-init");
 pc=start-p->first;
 p->returns[pc]=Native64TypedRecordList;
 p->code[pc+1]=(Dis64Instruction){ICONSP,SRC(AFP)|DST(AFP),0,record,slot};
 p->returns[pc+1]=Native64TypedRecordList;
 p->code[pc+2]=(Dis64Instruction){IMOVP,SRC(AFP)|DST(AFP|AIND),0,slot,offset};
 p->returns[pc+2]=64;
 *at=start+3;
 clear.s=alloc->d;
 if(!typedrootargclear(c,p,&clear,end,at)||*at!=start+4||
    !typedrootargclear(c,p,store,end,at)||*at!=start+5)
  return typedfail(p,"local-record-list-clear");
 p->returns[pc+3]=p->returns[pc+4]=Native64TypedRecordList;
 return 1;
}

static int
typedlocalrecordprep(TypedMulti *c, Dis64TypedBody *p, Dis64TypedBody *child, Inst *frame, Decl *arg,
                     unsigned int first, unsigned int end, unsigned int *at)
{
 Inst *alloc=c->instructions[*at],*in;
 Decl *descriptor=alloc->s.decl;
 Type *type=descriptor?descriptor->ty:nil;
 Type *reference=arg->ty->kind==Tlist?arg->ty->tof:arg->ty;
 unsigned int j,pc;
 int generic;

 if(!reference||reference->kind!=Tref||!type||!descriptor->desc||descriptor->desc->id!=alloc->s.offset||
    alloc->dm!=Afp||alloc->d.offset||!alloc->d.decl||
    !tequal(reference->tof,type->kind==Tref?type->tof:type))
  return typedfail(p,"local-record-argument-type");
 for(j=first+1;j<*at;j++){
  unsigned int role=p->returns[j-p->first],op=p->code[j-p->first].op;
  if(!(role==64&&(op==IMOVW||op==IMOVP))&&
     !(op==IMOVP&&(role==155||role==Native64TypedRecordArg)))
   return typedfail(p,"local-record-after-argument");
 }
 pc=*at-p->first;
 generic=!typedlibraryrecord(type);
 if(generic?!typedrecordnew(alloc,p,pc):!typedlibraryinst(alloc,p,pc,end))
  return typedfail(p,"local-record-argument-allocation");
 for(++*at;*at<end;(*at)++){
  in=c->instructions[*at];
  if(in->dm!=Afpind||in->d.reg!=alloc->d.reg||in->d.decl!=alloc->d.decl)
   break;
  if((in->op!=IMOVW&&in->op!=IMOVP)||in->branch||in->mm!=Anone||
     (in->sm!=Afp&&in->sm!=Amp&&in->sm!=Aimm)||
     (generic?!typedrecordinst(in,p,*at-p->first,end):!typedlibraryinst(in,p,*at-p->first,end)))
   return typedfail(p,"local-record-initializer");
  if(generic){
   Type *owner=typedfieldowner(&in->d);
   if(!owner||!tequal(owner,reference))return typedfail(p,"local-record-initializer-owner");
   p->returns[*at-p->first]=Native64TypedRecordInit;
  }
 }
 if(arg->ty->kind==Tlist)
  return typedlocalrecordlist(c,p,child,frame,alloc,arg,end,at);
 if(*at>=end||in->op!=IMOVP||in->sm!=Afp||in->s.reg!=alloc->d.reg||
    in->s.decl!=alloc->d.decl||in->s.offset||in->dm!=Afpind||in->d.reg!=frame->d.reg||
    in->d.offset!=arg->offset)
  return typedfail(p,"local-record-argument-store");
 return 1;
}

static int
typedmultisite(TypedMulti *c,Dis64TypedBody *p,unsigned int pc,unsigned int stop,unsigned int depth,unsigned int *next,unsigned int *height)
{
    Inst *frame=c->instructions[pc],*call,*in,*binding;
    Dis64TypedBody *child;Dis64TypedSite *site;
    Decl *arg;unsigned int end=pc+1,at,sm,offset,width,contract=0,local=0,inherit=0;
    int target,kind,root,op,actual,spawning;long source;
    if(frame->sm!=Adesc || frame->mm!=Anone || frame->dm!=Afp || frame->branch!=nil || frame->d.reg<32 ||
       frame->d.reg>(int)p->frame.legacybytes-4)return typedfail(p,"multi-frame");
    while(end<stop && c->instructions[end]!=nil){
      if(c->loads&&typedwordchoiceshape(c,end,stop,frame)){end+=4;continue;}
      if(!(c->instructions[end]->dm==Afpind ||
      (c->loads && (((c->instructions[end]->op==IMOVP || c->instructions[end]->op==IHEADP || c->instructions[end]->op==ITAIL ||
                     c->instructions[end]->op==ICONSP || c->instructions[end]->op==ICONSW) && c->instructions[end]->dm==Afp) || c->instructions[end]->op==IINDW || c->instructions[end]->op==IINDX || c->instructions[end]->op==ISLICEA || (c->instructions[end]->op==IADDW&&c->instructions[end]->dm==Afp) || (c->instructions[end]->op==ICVTWC && c->instructions[end]->dm==Afp) ||
        ((c->instructions[end]->op==INEW||c->instructions[end]->op==INEWZ)&&c->instructions[end]->dm==Afp) ||
        (end+1<stop&&typedrawindexprep(c->instructions[end],c->instructions[end+1]))))))break;
      end++;
    }
    if(end>=stop || (call=c->instructions[end])==nil || (call->op!=ICALL && !(c->loads && call->op==ISPAWN)) || call->sm!=Afp || call->s.reg!=frame->d.reg ||
       call->mm!=Anone || call->dm!=Apc || call->d.offset<0 || (unsigned long)call->d.offset>=c->count ||
       (target=c->entries[call->d.offset])<0)return typedfail(p,"multi-call");
    if(c->functions[target]->desc==nil || c->functions[target]->desc->id!=frame->s.offset ||
       (call->branch!=nil && call->branch!=c->functions[target]->pc))return typedfail(p,"multi-target");
    spawning=call->op==ISPAWN;
    if(spawning && c->functions[target]->ty->tof->kind!=Tnone)return typedfail(p,"spawn-callee");
    child=c->bodies+target;
    if(c->loads && c->state[target]==1){
        /* An active callee already has its frame and result layout. Its
         * original lowering must still finish before this graph is accepted. */
        if(!child->code || !child->returns || child->frame.reason || child->data.reason || child->frame.bytes<64)
            return typedfail(p,"multi-recursive-frame");
    }else if(!typedmultibody(c,target,depth+1))return typedfail(p,"multi-callee");
    if(c->state[target]==2 && *height<c->height[target]+1)*height=c->height[target]+1;
    if(*height>64)return typedfail(p,"multi-depth");
    p->code[pc-p->first]=(Dis64Instruction){IFRAME,SRC(AIMM)|DST(AFP),0,frame->s.offset,40};at=pc+1;
    for(arg=c->functions[target]->ty->ids;arg!=nil;arg=arg->next){
        if(at>=end || (in=c->instructions[at])==nil)return typedfail(p,"multi-argument");
        if(c->loads&&!spawning&&(in->op==INEW||in->op==INEWZ)){
            if(!typedlocalrecordprep(c,p,child,frame,arg,pc,end,&at))return 0;
            if(arg->ty->kind==Tlist)continue;
            in=c->instructions[at];
        }
        if(c->loads&&!spawning&&at+2<end&&typedrawindexprep(in,c->instructions[at+1])){
            if(!typedinst(in,end,p,p->code+at-p->first,p->returns+at-p->first))
                return typedfail(p,"local-array-index-prep");
            p->returns[at++-p->first]=Native64TypedArrayArgIndex;
            in=c->instructions[at];
        }
        if(c->loads&&(in->op==IINDX||in->op==IINDW||in->op==IINDL)&&typedrawclass(&in->s)){if(!typedrawindex(c,p,in,at,end)||++at>=end||!(in=c->instructions[at]))return typedfail(p,"local-raw-index");}
        kind=arg->ty->kind;root=kind==Tstring || kind==Tref || kind==Tarray || kind==Tlist || kind==Tchan || kind==Tmodule;
        if(c->loads&&!spawning&&kind==Tint&&in->op>=IBEQW&&in->op<=IBGEW){
            if(!dis64layoutaddress(&child->frame,arg->offset,4,0,&offset)||
               !typedwordchoice(c,p,frame,end+1,&at,arg->offset,offset))return 0;
            continue;
        }
        if(c->loads&&!spawning&&at==pc+1&&kind==Tlist&&arg->ty->tof&&
           (arg->ty->tof->kind==Tstring||arg->ty->tof->kind==Tint)&&in->op==IMOVP&&in->dm==Afp){
            Type *list=typedloadvaluetype(&in->d);
            if(!list||!tequal(arg->ty,list)||
               !typedexternalstringlist(c,p,frame,end,&at))
                return typedfail(p,"local-list-argument");
            in=c->instructions[at];
        }
        width=kind==Tbyte?1:kind==Tbig || kind==Treal?8:4;
        op=root?IMOVP:kind==Tbyte?IMOVB:kind==Tbig?IMOVL:kind==Treal?IMOVF:IMOVW;
        if(c->loads && !spawning && (in->sm==Afpind) && typedrawborrow(p,&in->s,in->sm,at)>=0){
            unsigned int bytes=kind==Tadt?arg->ty->size:4;
            int slot=typedrawborrow(p,&in->s,in->sm,at);
            Type *source=typedarrayleaf(p->rawtype[slot],in->s.offset);
            if(in->dm!=Afpind||in->d.reg!=frame->d.reg||in->d.offset!=arg->offset||
               (root?in->op!=IMOVP||!source||!tequal(source,arg->ty):
                (kind!=Tadt&&kind!=Tint)||(in->op==IMOVM?(unsigned int)in->m.offset:4)!=bytes)||
               !dis64layoutaddress(&child->frame,arg->offset,4,root,&offset)||
               !typedrawuse(p,in,at,end,offset))return typedfail(p,"local-raw-array-argument");
            at++;continue;
        }
        if(c->loads && !spawning && kind==Tadt && typedrawdelta(arg->ty)){
            unsigned int k,part;long src;unsigned int wire;
            if(in->op!=IMOVM){
                for(k=0;k<3;k++){if(at>=end||!(in=c->instructions[at])||in->op!=IMOVW||in->mm!=Anone||in->branch||in->dm!=Afpind||in->d.reg!=frame->d.reg||in->d.offset!=arg->offset+4*k||!typedoperand(in->sm,&in->s,Tint,1,0,p,&src,&wire,&actual)||!dis64layoutaddress(&child->frame,arg->offset+4*k,4,0,&part))return typedfail(p,"local-delta-literal");p->code[at-p->first]=(Dis64Instruction){IMOVW,SRC(wire)|DST(AFP|AIND),0,src,part};p->returns[at++-p->first]=64;}continue;
            }
            if(in->op!=IMOVM||in->mm!=Aimm||in->m.offset!=12||in->branch||in->dm!=Afpind||in->d.reg!=frame->d.reg||in->d.offset!=arg->offset||!typedrawspan(p,&in->s,in->sm,12,&src,&wire)||!dis64layoutaddress(&child->frame,arg->offset,4,0,&offset))return typedfail(p,"local-delta-argument");
            for(k=4;k<12;k+=4)if(!dis64layoutaddress(&child->frame,arg->offset+k,4,0,&part)||part!=offset+k)return typedfail(p,"local-delta-argument");p->code[at-p->first]=(Dis64Instruction){IMOVM,SRC(wire)|AXIMM|DST(AFP|AIND),12,src,offset};p->returns[at++-p->first]=64;continue;
        }
        if(c->loads && !spawning && kind==Tadt && typedwordlay(arg->ty)){
            if(!((in->op==IMOVW&&in->mm==Anone)||(in->op==IMOVM&&in->mm==Aimm&&in->m.offset==4)) || in->branch || in->dm!=Afpind || in->d.reg!=frame->d.reg || in->d.offset!=arg->offset || !dis64layoutaddress(&child->frame,arg->offset,4,0,&offset) || !typedoperand(in->sm,&in->s,Tint,1,0,p,&source,&sm,&actual) || actual!=Tint)return typedfail(p,"local-lay-argument");
            p->code[at-p->first]=(Dis64Instruction){IMOVW,SRC(sm)|DST(AFP|AIND),0,source,offset};p->returns[at++-p->first]=64;continue;
        }
        if(c->loads && !spawning && kind==Ttuple){
            Dis64Layout tuple={0};unsigned int j,src=0,dst=0,x,y;int valid=dis64layouttype(arg->ty,&tuple),actual;long part;unsigned int wire;
            if(!valid || (tuple.bytes!=8 && tuple.bytes!=16) || tuple.bytes!=tuple.legacybytes || tuple.count!=tuple.bytes/4 || in->op!=IMOVM || in->mm!=Aimm || in->m.offset!=tuple.bytes || in->branch || (in->sm!=Afp && in->sm!=Amp) || in->dm!=Afpind || in->d.reg!=frame->d.reg || in->d.offset!=arg->offset)valid=0;
            for(j=0;valid && j<tuple.count;j++){Addr a=in->s;a.reg+=4*j;a.offset=0;
                if(tuple.leaves[j].kind!=Tint || tuple.leaves[j].bytes!=4 || tuple.leaves[j].native!=4*j || tuple.leaves[j].legacy!=4*j || !dis64layoutaddress(&child->frame,arg->offset+4*j,4,0,&x) || x<64 || x>65535 || !typedoperand(in->sm,&a,Tint,0,0,p,&part,&wire,&actual) || actual!=Tint || part<0)valid=0;
                else {y=part;if(!j){src=y;dst=x;}else if(y!=src+4*j || x!=dst+4*j)valid=0;}
            }
            if(valid){p->code[at-p->first]=(Dis64Instruction){IMOVM,SRC(in->sm==Afp?AFP:AMP)|AXIMM|DST(AFP|AIND),tuple.bytes,src,dst};p->returns[at++-p->first]=64;}
            dis64layoutfree(&tuple);if(!valid)return typedfail(p,"local-word-tuple-argument");continue;
        }
        if(c->loads && !spawning && kind==Tadt){
            int prepared=0;
            if(in->op!=IMOVP && (in->op!=IMOVM || in->m.offset!=arg->ty->size || in->s.native64type&&in->s.native64type->kind==Ttuple)){if(!typedlocalgeometryliteral(c,p,child,frame,arg,end,&at))return 0;continue;}
            if(in->op==IMOVP){if(at+2>=end || !typedlocalscreenimageprep(in,c->instructions[at+1],p,at-p->first))return 0;in=c->instructions[++at];prepared=1;}
            if(!typedlocalgeometryarg(in,frame,arg,p,child,at-p->first))return 0;at++;
            if(prepared){unsigned int clear=at;Inst store=*in;store.op=IMOVP;store.sm=Afp;store.s.offset=0;if(!typedrootargclear(c,p,&store,end,&at) || clear==at)return typedfail(p,"local-screen-image-clear");}
            continue;
        }
        if(c->loads&&!spawning&&kind==Tstring&&in->op==IINDW&&typedstringarray(&in->s)){
            Inst*use=at+1<end?c->instructions[at+1]:nil;long array,index;unsigned int am,im;int actual;
            if(!use||!typedstringarraystable(c,in)||in->mm!=Afp||!in->m.decl||in->m.offset||in->branch||use->op!=IMOVP||use->sm!=Afpind||use->s.decl!=in->m.decl||use->s.reg!=in->m.reg||use->s.offset||use->mm!=Anone||use->branch||use->dm!=Afpind||use->d.reg!=frame->d.reg||use->d.offset!=arg->offset||!typedoperand(in->sm,&in->s,Tarray,0,0,p,&array,&am,&actual)||!typedoperand(in->dm,&in->d,Tint,1,0,p,&index,&im,&actual)||!dis64layoutaddress(&child->frame,arg->offset,4,1,&offset))return typedfail(p,"local-string-array-argument");
            p->code[at-p->first]=(Dis64Instruction){IINDL,SRC(am)|AXINF|DST(im),48,array,index};p->returns[at++-p->first]=74;p->code[at-p->first]=(Dis64Instruction){IMOVP,SRC(AFP|AIND)|DST(AFP|AIND),0,48,offset};p->returns[at++-p->first]=74;continue;
        }
        if(c->loads&&!spawning&&kind==Tstring&&in->op==IADDC){long middle;unsigned int mm;
            if(arg->next||in->branch||in->dm!=Afpind||in->d.reg!=frame->d.reg||in->d.offset!=arg->offset||!typedoperand(in->sm,&in->s,Tstring,0,0,p,&source,&sm,&actual)||!typedoperand(in->mm,&in->m,Tstring,0,1,p,&middle,&mm,&actual)||!dis64layoutaddress(&child->frame,arg->offset,4,1,&offset))return typedfail(p,"local-string-concat-argument");p->code[at-p->first]=(Dis64Instruction){IADDC,SRC(sm)|(mm==AFP?AXINF:AXINM)|DST(AFP|AIND),middle,source,offset};p->returns[at++-p->first]=75;continue;
        }
        if(c->loads && !spawning && kind==Tref && in->op==IINDW && typedrootarrayelement(&in->s)){
            if(!dis64layoutaddress(&child->frame,arg->offset,4,1,&offset) || !typedrefarrayarg(c,p,frame,arg,end,&at,offset))return 0;continue;
        }
        if(c->loads && (!spawning||typedclientreferencekind(kind)) && (in->sm==Afpind || in->sm==Ampind)){
            unsigned int view;int k=typedclientreferencekind(kind);
            if(!spawning && dis64layoutaddress(&child->frame,arg->offset,4,root,&view) &&
               typedrecordargument(in,frame,p,arg->ty,at-p->first,arg->offset,view,0)){at++;continue;}
            if(dis64layoutaddress(&child->frame,arg->offset,kind==Tbig||kind==Treal?8:4,k,&view) &&
               typedlibraryargument(in,frame,p,at-p->first,arg->offset,view,kind)){at++;continue;}
        }
        if(c->loads && kind==Treal && in->op==ICVTWF){
            if(spawning || in->branch || in->mm!=Anone || in->dm!=Afpind || in->d.reg!=frame->d.reg || in->d.offset!=arg->offset || !dis64layoutaddress(&child->frame,arg->offset,8,0,&offset) || !typedoperand(in->sm,&in->s,Tint,1,0,p,&source,&sm,&actual))return typedfail(p,"local-real-argument");
            p->code[at-p->first]=(Dis64Instruction){ICVTWF,SRC(sm)|DST(AFP|AIND),0,source,offset};p->returns[at++-p->first]=152;continue;
        }
        if(c->loads&&kind==Tint&&in->op==IADDW&&in->dm==Afp){Inst*use=at+1<end?c->instructions[at+1]:nil;if(!use||use->op!=IINDC||use->mm!=Afp||use->m.reg!=in->d.reg||use->m.decl!=in->d.decl||in->sm!=Aimm||in->s.offset!=1||in->mm!=Afp||in->branch||in->d.offset||!typedinst(in,end,p,p->code+at-p->first,p->returns+at-p->first))return typedfail(p,"local-string-index-prep");p->returns[at++-p->first]=46;in=use;}
        if(c->loads && kind==Tint && in->op==IINDC){
            unsigned int mm;long middle;
            if(spawning || in->branch || in->dm!=Afpind || in->d.reg!=frame->d.reg || in->d.offset!=arg->offset || !dis64layoutaddress(&child->frame,arg->offset,4,0,&offset) || !typedoperand(in->sm,&in->s,Tstring,0,0,p,&source,&sm,&actual) || !typedoperand(in->mm,&in->m,Tint,1,1,p,&middle,&mm,&actual))return typedfail(p,"local-string-index-argument");
            p->code[at-p->first]=(Dis64Instruction){IINDC,SRC(sm)|(mm==AIMM?AXIMM:mm==AFP?AXINF:AXINM)|DST(AFP|AIND),middle,source,offset};p->returns[at++-p->first]=137;continue;
        }
        if(c->loads && !spawning && kind==Tint && typedwordargop(in->op)){
            if(!dis64layoutaddress(&child->frame,arg->offset,4,0,&offset) || !typedwordargument(in,frame,p,at-p->first,arg->offset,offset))return 0;at++;continue;
        }
        if(c->loads && !spawning && kind==Tint && (in->op==ITAIL || in->op==IHEADP)){
            if(!dis64layoutaddress(&child->frame,arg->offset,4,0,&offset) || offset<64 || offset>65535 || !typedexternalinthead(c,p,frame,end,&at,arg->offset,offset))return typedfail(p,"local-int-head-argument");
            continue;
        }
        if(c->loads&&!spawning&&in->dm==Afpind&&((root&&in->op==IHEADP)||(kind==Tlist&&in->op==ITAIL))){
            if(!dis64layoutaddress(&child->frame,arg->offset,4,1,&offset)||
               !typedlistargument(in,frame,arg,p,at-p->first,offset))return 0;
            at++;continue;
        }
        if(c->loads&&!spawning&&root&&typedheadfieldstart(c,at,end)){
            if(!dis64layoutaddress(&child->frame,arg->offset,4,1,&offset)||
               !typedexternalrootpath(c,p,frame,end,&at,arg->offset,offset,arg->ty))return 0;
            continue;
        }
        if(c->loads && !spawning && kind==Tstring && in->op==ITAIL && in->dm==Afp){
            if(!dis64layoutaddress(&child->frame,arg->offset,4,1,&offset) ||
               offset>65535-8 || !typedexternalheadarg(c,p,frame,arg,end,&at,offset))
                return typedfail(p,"local-string-head-argument");
            continue;
        }
        if(c->loads&&!spawning&&kind==Tstring&&in->op==IHEADP){if(arg->next||!dis64layoutaddress(&child->frame,arg->offset,4,1,&offset)||!typedexternalstringexpr(c,p,frame,arg,end,&at,offset))return typedfail(p,"local-head-string-argument");continue;}
        if(c->loads&&kind==Tstring&&in->op==IMOVP&&in->dm==Afp&&typedlibraryrecord(typedfieldowner(&in->s))==12&&in->s.offset==16){unsigned int z;
            if((!spawning&&arg->next)||!dis64layoutaddress(&child->frame,arg->offset,4,1,&offset)||!typedexternalstringexpr(c,p,frame,arg,end,&at,offset))return typedfail(p,"local-client-string-expression");
            if(spawning)for(z=at;z<end;z++){Inst*q=c->instructions[z];if(q->op!=IMOVP||q->mm!=Anone||q->branch||((q->dm!=Afpind||q->d.reg!=frame->d.reg)&&q->dm!=Afp))return typedfail(p,"spawn-string-following-allocation");}continue;
        }
        if(spawning && (in->op!=op || (in->sm!=Afp && in->sm!=Amp && in->sm!=Aimm)))return typedfail(p,"spawn-argument");
        /* No later allocating argument may follow the pending String root. */
        if(c->loads && kind==Tstring && in->op==IMOVP && in->dm==Afp){
            Inst*slice,*convert,*clear;unsigned int view;
            if(arg!=c->functions[target]->ty->ids || arg->next || at+3>=end || !(slice=c->instructions[at+1]) || !(convert=c->instructions[at+2]) || !(clear=c->instructions[at+3]) ||
               in->mm!=Anone || in->branch || in->sm!=Afp || !typedbytearrayaddr(&in->s) || !typedbytearrayaddr(&in->d) ||
               slice->op!=ISLICEA || slice->dm!=Afp || slice->d.reg!=in->d.reg || slice->d.decl!=in->d.decl ||
               convert->op!=ICVTAC || convert->sm!=Afp || convert->s.reg!=in->d.reg || convert->s.decl!=in->d.decl ||
               convert->mm!=Anone || convert->branch || convert->dm!=Afpind || convert->d.reg!=frame->d.reg || convert->d.offset!=arg->offset ||
               clear->op!=IMOVP || clear->sm!=Amp || clear->dm!=Afp || clear->mm!=Anone || clear->branch ||
               clear->d.reg!=in->d.reg || clear->d.decl!=in->d.decl || !clear->s.decl || !clear->s.decl->ty || clear->s.decl->ty->kind!=Tany ||
               !typedinst(in,stop,p,p->code+at-p->first,p->returns+at-p->first) || !typedloadbytes(slice,p,at+1-p->first) || !typedclientarray(slice,p,at+1-p->first) ||
               !typedbytearrayaddr(&convert->s) || !typedoperand(convert->sm,&convert->s,Tarray,0,0,p,&source,&sm,&actual) ||
               !dis64layoutaddress(&child->frame,arg->offset,4,1,&view) || view>65535 ||
               !typedinst(clear,stop,p,p->code+at+3-p->first,p->returns+at+3-p->first))return typedfail(p,"local-slice-string-argument");
            p->code[at+2-p->first]=(Dis64Instruction){ICVTAC,SRC(sm)|DST(AFP|AIND),0,source,view};p->returns[at+2-p->first]=64;at+=4;continue;
        }
        if(c->loads && !spawning && kind==Tstring && in->op==ICVTWC){
            Inst*first=in,*concat,*clear;unsigned int mm;long middle;
            if(c->functions[target]->ty->tof->kind!=Tnone || arg!=c->functions[target]->ty->ids || arg->next || at+2>=end || in->dm!=Afp || !in->d.decl || in->d.decl->store!=Dlocal || !in->d.decl->sym || in->d.decl->sym->name[0]!='.' || in->d.decl->sym->name[1]!='t' ||
               !typedclientidstring(in,p,at-p->first) || !dis64layoutaddress(&child->frame,arg->offset,4,1,&offset) || offset!=64)return typedfail(p,"local-id-string-prefix");
            concat=c->instructions[at+1];clear=c->instructions[at+2];
            if(concat->op!=IADDC || concat->branch || concat->dm!=Afpind || concat->d.reg!=frame->d.reg || concat->d.offset!=arg->offset ||
               !((concat->sm==Afp && concat->s.decl==first->d.decl && concat->s.reg==first->d.reg && !concat->s.offset) || (concat->mm==Afp && concat->m.decl==first->d.decl && concat->m.reg==first->d.reg && !concat->m.offset)) ||
               !typedoperand(concat->sm,&concat->s,Tstring,0,0,p,&source,&sm,&actual) || actual!=Tstring || !typedoperand(concat->mm,&concat->m,Tstring,0,1,p,&middle,&mm,&actual) || actual!=Tstring ||
               clear->op!=IMOVP || clear->sm!=Amp || clear->dm!=Afp || clear->mm!=Anone || clear->branch || clear->d.decl!=first->d.decl || clear->d.reg!=first->d.reg || clear->d.offset || !clear->s.decl || !clear->s.decl->ty || clear->s.decl->ty->kind!=Tany ||
               !typedinst(clear,stop,p,p->code+at+2-p->first,p->returns+at+2-p->first))return typedfail(p,"local-id-string-argument");
            p->code[at+1-p->first]=(Dis64Instruction){IADDC,SRC(sm)|(mm==AFP?AXINF:AXINM)|DST(AFP|AIND),middle,source,offset};p->returns[at+1-p->first]=64;p->returns[at+2-p->first]=238;at+=3;continue;
        }
        if(c->loads && ((kind==Tstring && in->op==ICVTAC) || (kind==Tarray && arg->ty->tof && arg->ty->tof->kind==Tbyte && in->op==ICVTCA))){
            if(arg->next || in->mm!=Anone || in->branch || in->dm!=Afpind || in->d.reg!=frame->d.reg || in->d.offset!=arg->offset ||
               (in->op==ICVTAC && !typedbytearrayaddr(&in->s)) || !dis64layoutaddress(&child->frame,arg->offset,4,1,&offset) || offset>65535 ||
               !typedoperand(in->sm,&in->s,in->op==ICVTAC?Tarray:Tstring,0,0,p,&source,&sm,&actual))return typedfail(p,"local-byte-string-argument");
            p->code[at-p->first]=(Dis64Instruction){in->op,SRC(sm)|DST(AFP|AIND),0,source,offset};p->returns[at++-p->first]=64;continue;
        }
        if(c->loads && kind==Tstring && in->op==IADDC && (child==p || (arg==c->functions[target]->ty->ids && !arg->next))){
            unsigned int mm;long middle;
            if(in->dm!=Afpind || in->d.reg!=frame->d.reg || in->d.offset!=arg->offset || in->branch ||
               !dis64layoutaddress(&child->frame,arg->offset,4,1,&offset) || offset>65535 ||
               !typedoperand(in->sm,&in->s,Tstring,0,0,p,&source,&sm,&actual) || actual!=Tstring ||
               !typedoperand(in->mm,&in->m,Tstring,0,1,p,&middle,&mm,&actual) || actual!=Tstring)
                return typedfail(p,"self-concat-argument");
            p->code[at-p->first]=(Dis64Instruction){IADDC,SRC(sm)|(mm==AFP?AXINF:AXINM)|DST(AFP|AIND),middle,source,offset};
            p->returns[at++-p->first]=64;continue;
        }
        if(c->loads&&root&&in->op==IMOVP&&in->dm==Afp&&(in->sm==Afpind||in->sm==Ampind)){if(!dis64layoutaddress(&child->frame,arg->offset,4,1,&offset)||!typedexternalrootpath(c,p,frame,end,&at,arg->offset,offset,arg->ty))return typedfail(p,"local-root-path");continue;}
        if(c->loads && root && (in->sm==Afpind || in->sm==Ampind)){
            if(in->dm!=Afpind || in->d.reg!=frame->d.reg || in->d.offset!=arg->offset ||
               !dis64layoutaddress(&child->frame,arg->offset,4,1,&offset) || offset>65535 || !typedselectedfieldarg(in,p,at-p->first,offset,kind) || p->returns[at-p->first]==233)return typedfail(p,"multi-field-argument");
            at++;continue;
        }
        if((!root && kind!=Tbyte && kind!=Tint && kind!=Tfix && kind!=Tbig && kind!=Treal) ||
           in->op!=op || in->mm!=Anone || in->dm!=Afpind || in->d.reg!=frame->d.reg || in->d.offset!=arg->offset || in->branch!=nil ||
           !dis64layoutaddress(&child->frame,arg->offset,width,root,&offset) || offset>65535 ||
           !(kind==Tbyte && c->external>=2?typedclientbyteoperand(in->sm,&in->s,kind,p,&source,&sm):
             typedoperand(in->sm,&in->s,root?TypedRoot:kind,!root && width<8,0,p,&source,&sm,&actual)) ||
           (root && actual!=kind && actual!=Tany &&
            !(c->loads && kind==Tstring && typedcaughtstring(in,p))))return typedfail(p,"multi-argument");
        p->code[at-p->first]=(Dis64Instruction){op,SRC(sm)|DST(AFP|AIND),0,source,offset};p->returns[at++-p->first]=64;
        if(c->loads && root){unsigned int before=at;if(!typedrootargclear(c,p,in,end,&at))return 0;if(at!=before)p->returns[at-1-p->first]=182;}
    }
    if(child->resultkind!=Tnone){
        if(at+1!=end || (binding=c->instructions[at])==nil || binding->op!=ILEA || binding->mm!=Anone ||
           binding->dm!=Afpind || binding->d.reg!=frame->d.reg || binding->d.offset!=16 || binding->branch!=nil)return typedfail(p,"multi-result");
        kind=child->resultkind;root=kind==Tstring || kind==Tref || kind==Tarray || kind==Tlist || kind==Tchan || kind==Tmodule;
        contract=child->resultaggregate?child->resultaggregate:child->resultpair>=3?16:child->resulttriple==2?12:child->resultgeometry?(child->resultgeometry==8?96:112):child->resulttriple?72:child->resultpair?48:child->resultbytes | (root?16:0);
        if(child->resultaggregate&&binding->sm==Afp){if(!typedwmtuplebinding(c->functions[target],binding,p,&local,0))return typedfail(p,"local-aggregate-result-binding");p->code[at-p->first]=(Dis64Instruction){ILEA,SRC(AFP)|DST(AFP|AIND),0,local,32};p->returns[at-p->first]=(contract==80||contract==92||contract==116||contract==124)?85:128|contract;}
        else if(c->loads && typedimagearraybinding(c,p,frame,binding,c->functions[target]->ty->tof,pc,at)){
            local=p->code[at-p->first].source;
        }else if(c->loads&&child->resultgeometry&&binding->sm==Afpind&&binding->s.reg!=16){
            if(!typedwindowgeometrybinding(binding,c->functions[target]->ty->tof,p,at-p->first,&local,&contract))return typedfail(p,"local-window-geometry-result");
        }else if(c->loads&&contract==24&&!child->resultpair&&binding->sm==Afpind&&binding->s.offset==48&&typedlibraryrecord(typedfieldowner(&binding->s))==6&&typedwmref(c->functions[target]->ty->tof,"Draw","Wmcontext")){
            Addr a=binding->s;Type*t=typedfieldowner(&a);a.offset=0;a.native64type=t;if(!typedoperand(Afp,&a,TypedRoot,0,0,p,&source,&sm,&actual)||actual!=Tref)return typedfail(p,"client-context-result");
            local=source;p->code[at-p->first]=(Dis64Instruction){ILEA,SRC(AFP|AIND)|DST(AFP|AIND),24,source,32};p->returns[at-p->first]=93;
        }else if(c->loads&&root&&!child->resultpair&&typedrecordresult(binding,p,c->functions[target]->ty->tof,at-p->first,&local,1)){
            contract=24;
        }else if(binding->sm==Afp){
            if(child->resultgeometry){if(!typedgeometrybinding(binding,c->functions[target]->ty->tof,p,&local,&contract))return typedfail(p,"local-geometry-result");source=local;sm=AFP;}
            else if(child->resultpair>=3 || child->resulttriple==2){unsigned int k,next,n=child->resulttriple==2?3:2,oldstep=child->resultpair>=4?8:4;
                for(k=0;k<n;k++){if(!dis64layoutaddress(&p->frame,binding->s.reg+k*oldstep,k==0&&child->resultpair>=4?8:4,child->resultpair>=3&&k==1,&next) || (k && next!=local+(child->resulttriple==2?4:8)*k))return typedfail(p,"mixed-call-result");if(!k)local=next;}source=local;sm=AFP;
            }
            else if(child->resulttriple){if(!typedchannelbinding(c,p,binding,&local))return 0;source=local;sm=AFP;}
            else if(child->resultpair){
                if(!typedrootpairbinding(c->functions[target],binding,p,&offset,0))return typedfail(p,"pair-call-result");
                source=offset;sm=AFP;
            }else if(!typedoperand(Afp,&binding->s,root?TypedRoot:kind,0,0,p,&source,&sm,&actual) ||
               (root && actual!=kind && actual!=Tany))return typedfail(p,"multi-result");
            local=source;p->code[at-p->first]=(Dis64Instruction){ILEA,SRC(AFP)|DST(AFP|AIND),0,source,32};p->returns[at-p->first]=128|contract;
        }else if(c->loads && root && !child->resultpair && binding->sm==Amp){
            if(!binding->s.decl || binding->s.decl->store!=Dglobal || !typedoperand(Amp,&binding->s,TypedRoot,0,0,p,&source,&sm,&actual) || actual!=kind)return typedfail(p,"multi-global-result");
            local=source;p->code[at-p->first]=(Dis64Instruction){ILEA,SRC(AMP)|DST(AFP|AIND),0,source,32};p->returns[at-p->first]=128|contract;
        }else{
            if((child->resultgeometry&&child->resultgeometry!=p->resultgeometry) || binding->sm!=Afpind || binding->s.reg!=16 || binding->s.offset || !((child->resultgeometry&&child->resultgeometry==p->resultgeometry)||(kind==Tstring && (p->resultaggregate==80 || p->resultaggregate==124)) || (kind==p->resultkind && child->resultpair==p->resultpair && child->resulttriple==p->resulttriple && child->resultaggregate==p->resultaggregate)))return typedfail(p,"multi-inherit");
            if(child->resultpair && !tequal(p->resulttype,c->functions[target]->ty->tof))return typedfail(p,"mixed-inherit-type");
            inherit=1;local=32;p->code[at-p->first]=(Dis64Instruction){IMOVL,SRC(AFP)|DST(AFP|AIND),0,32,32};p->returns[at-p->first]=64;
        }
        at++;
    }
    if(at!=end)return typedfail(p,"multi-setup-length");
    p->code[end-p->first]=(Dis64Instruction){spawning?ISPAWN:ICALL,SRC(AFP)|DST(AIMM),0,40,call->d.offset};p->returns[end-p->first]=spawning?186:inherit?(kind==Tstring && (p->resultaggregate==80 || p->resultaggregate==124)?78:32):0;
    site=p->sites+p->nsites++;*site=(Dis64TypedSite){pc-p->first,end-p->first,call->d.offset,frame->s.offset,local,contract,inherit};
    *next=end+1;return 1;
}

#include "dis64-alt.h"
#include "dis64-library-records.h"
#include "dis64-record-fields.h"

/* One forward string catch in a void function. Translate the cleanup map
 * only after frame repacking has fixed the native offsets. */
static int
typedcatch(Dis64TypedBody *p, uchar *inside)
{
 Except *e,*found=nil;
 Label *label;
 unsigned int first,end,target,slot,i,bit,n;
 Desc *d;
 for(e=typedreportcatches;e;e=e->next){
  if(!e->p1->reach&&!e->p2->reach)continue;
  if(getpc(e->p1)<p->first||getpc(e->p1)>=p->first+p->count)continue;
  if(found)return typedfail(p,"catch-count");
  found=e;
 }
 if(!found||found->ne||!found->c||found->c->nlab!=1||
    found->c->iwild||!found->d||found->d->ty!=rtexception)
  return typedfail(p,"catch-shape");
 label=found->c->labs;
 if(!label->start||label->start!=label->stop||!label->start->decl||!label->start->decl->sym||!label->inst)
  return typedfail(p,"catch-label");
 first=getpc(found->p1)-p->first;end=getpc(found->p2)-p->first;target=label->inst->pc-p->first;
 if(first>=end||end>p->count||target<end||target>=p->count||
    (inside&&inside[target])||!dis64layoutaddress(&p->frame,found->d->offset,4,1,&slot))
  return typedfail(p,"catch-layout");
 d=found->desc;
 if(d){
  n=(p->frame.bytes+63)/64;
  p->catchcleanup=allocmem(n);
  memset(p->catchcleanup,0,n);
  if(d->nmap<0||d->nmap>(p->frame.legacybytes+31)/32||!d->map)return typedfail(p,"catch-cleanup");
  for(i=0;i<(unsigned int)d->nmap*8;i++)if(d->map[i/8]&(128>>(i%8))){
   if(!dis64layoutaddress(&p->frame,4*i,4,1,&bit))return typedfail(p,"catch-cleanup");
   bit/=8;p->catchcleanup[bit/8]|=128>>(bit%8);
  }
 }
 p->catchpattern=label->start->decl->sym;
 p->catchfirst=first;p->catchend=end;p->catchtarget=target;p->catchslot=slot;
 return 1;
}

static int
typedmultibody(TypedMulti *c,unsigned int index,unsigned int depth)
{
    Dis64TypedBody *p=c->bodies+index;Decl *fn=c->functions[index];
    unsigned int i,pc,stop=c->count,next,h=1,cost;uchar *inside=nil;Inst *in;
    if(depth>=64)return typedfail(p,"multi-depth");
    if(c->state[index]==1)return typedfail(p,"multi-cycle");
    if(c->state[index])return c->state[index]==2;
    c->state[index]=1;
    for(i=0;i<(unsigned int)c->nf;i++)if(c->functions[i]->pc->pc>fn->pc->pc && c->functions[i]->pc->pc<(int)stop)stop=c->functions[i]->pc->pc;
    i=typedlower(fn,c->globals,c->instructions[0],c->count,stop,p,c->loads);
    cost=p->frame.bytes+p->data.bytes+(p->frame.count+p->data.count)*sizeof(Dis64LayoutLeaf);
    if(cost>32*1024*1024-c->cost){typedfail(p,"multi-storage-budget");goto fail;}c->cost+=cost;
    /* A successful scalar lowering may retain an immutable byte in MP.
     * Command files serialize strings only, so replay through the byte folder. */
    if(i && c->external>=2)for(pc=0;pc<p->count;pc++)
        if((p->code[pc].op==IMOVB || p->code[pc].op==ICVTBW) && USRC(p->code[pc].mode)==AMP){i=0;typedfail(p,"unsupported-opcode");break;}
    if(i){if(fn->handler&&(!typedcatch(p,nil)||!typedresults(p)))goto fail;c->state[index]=2;c->height[index]=1;return 1;}
    if(p->reason==nil || (strcmp(p->reason,"unsupported-opcode") && !((c->external>=2 || c->loads) && p->code && !p->frame.reason && !p->data.reason)))goto fail;
    p->reason=nil;p->ntables=0;memset(p->code,0,p->count*sizeof(*p->code));memset(p->returns,0,p->count);
    if(c->loads){
        unsigned int before=p->frame.bytes;
        if(!typedwmtupleframe(c,fn,p,stop)||!typedstatframe(c,fn,p,stop))goto fail;
        if(p->frame.bytes-before>32*1024*1024-c->cost){typedfail(p,"multi-storage-budget");goto fail;}
        c->cost+=p->frame.bytes-before;
    }
    p->sites=allocmem((p->count/2+1)*sizeof(*p->sites));
    if(c->external)p->external=allocmem((p->count/2+1)*sizeof(*p->external));
    if(c->loads){p->variadic=allocmem((p->count/2+1)*sizeof(*p->variadic));memset(p->variadic,0,(p->count/2+1)*sizeof(*p->variadic));}
    inside=allocmem(p->count);memset(inside,0,p->count);
    for(pc=p->first;pc<stop;){
        in=c->instructions[pc];p->badpc=pc;
        if(in==nil){typedfail(p,"multi-instruction-order");goto fail;}
        if(c->loads && (in->op==ISLICEA||in->op==ISLICELA) &&
           (typedrawclass(&in->d)||typedrawfieldclass(&in->d,in->dm)||
            (in->op==ISLICELA&&typedrawfieldclass(&in->s,in->sm)))){
            if(!typedrawslice(in,p,pc-p->first))goto fail;pc++;
        }
        else if(c->loads && (in->op==INEWA||in->op==INEWAZ) &&
                (typedrawclass(&in->d)||typedrawfieldclass(&in->d,in->dm))){
            if(!typedrawnew(in,p,pc-p->first))goto fail;pc++;
        }
        else if(c->loads && (in->op==IINDX||in->op==IINDW||in->op==IINDL) &&
                typedrawfieldclass(&in->s,in->sm)){
            if(!typedrawfieldindex(c,p,in,pc,stop))goto fail;
            inside[pc+1-p->first]=1;pc+=2;
        }
        else if(c->loads && (in->op==IINDX||in->op==IINDW||in->op==IINDL) &&
                !(in->sm==Afp&&typedbuiltinarray(&in->s))&&typedrawclass(&in->s)){
            if(!typedrawindex(c,p,in,pc,stop))goto fail;pc++;
        }
        else if(c->loads && (typedrawborrow(p,&in->s,in->sm,pc)>=0 || typedrawborrow(p,&in->d,in->dm,pc)>=0)){if(!typedrawuse(p,in,pc,stop,-1))goto fail;pc++;}
        else if(c->loads&&p->resultaggregate&&in->dm==Afpind&&in->d.reg==16&&(in->op==IMOVP||in->op==IMOVW||in->op==IMOVM||in->op==IMOVMP)){if(!typedaggregatereturn(in,p,pc-p->first))goto fail;pc++;}
        else if(c->loads && in->op==IINDB){
            unsigned int gap=0;Inst*shift;
            if(pc+1>=stop)goto fail;shift=c->instructions[pc+1];
            if(shift->op==ISHRW){
                gap=1;
                if(pc+2>=stop || in->sm!=Afp || in->s.offset || shift->branch || shift->sm!=Aimm || shift->s.offset<0 || shift->s.offset>31 || shift->mm!=Afp || shift->dm!=Afp || !typedinst(shift,stop,p,p->code+pc+1-p->first,p->returns+pc+1-p->first))goto fail;
            }
            if(shift->op==IFRAME){unsigned int q=pc+1,calls=0,target=~0U,t,j;Inst*call;Dis64TypedBody*child;Decl*arg;
                gap=11;if(pc+1+gap>=stop||in->sm!=Afp||in->s.offset){typedfail(p,"byte-call-borrow-span");goto fail;}
                while(q<pc+1+gap){Inst*v=c->instructions[q];
                    if(v->op==IFRAME){unsigned int start=q;if(!typedmultisite(c,p,q,stop,depth,&next,&h)||next>pc+1+gap)goto fail;call=c->instructions[next-1];t=c->entries[call->d.offset];if(t>=(unsigned int)c->nf){typedfail(p,"byte-call-borrow-target");goto fail;}child=c->bodies+t;arg=c->functions[t]->ty->ids;
                        if(calls&&target!=t){typedfail(p,"byte-call-borrow-target");goto fail;}target=t;calls++;
                        if(child->resultkind!=Tint||child->resultbytes!=4||child->resultaggregate||child->nsites||child->ntables||!arg||arg->next||!arg->ty||arg->ty->kind!=Tint){typedfail(p,"byte-call-borrow-callee");goto fail;}
                        for(j=0;j<child->frame.count;j++)if(child->frame.leaves[j].kind!=Tint){typedfail(p,"byte-call-borrow-callee-roots");goto fail;}
                        for(j=0;j<child->count;j++){unsigned int op=child->code[j].op;if(op!=IRET&&op!=IMOVW&&op!=IADDW&&op!=ISUBW&&op!=IBEQW&&op!=IBNEW&&op!=IBLTW&&op!=IBLEW&&op!=IBGTW&&op!=IBGEW&&op!=IJMP){typedfail(p,"byte-call-borrow-callee-op");goto fail;}}
                        for(j=start+1;j<next;j++)inside[j-p->first]=1;q=next;
                    }else{if((v->op!=ISHLW&&v->op!=IADDW&&v->op!=IORW)||v->branch||v->dm!=Afp||!typedinst(v,stop,p,p->code+q-p->first,p->returns+q-p->first)){typedfail(p,"byte-call-borrow-gap");goto fail;}q++;}
                }if(calls!=2){typedfail(p,"byte-call-borrow-count");goto fail;}
            }
            if(!typedlibrarybyteindex(in,c->instructions[pc+1+gap],p,pc-p->first,gap))goto fail;
            for(i=1;i<=1+gap;i++)inside[pc+i-p->first]=1;pc+=2+gap;
        }else if(c->loads&&in->op==IMOVP&&(in->sm==Afpind||in->sm==Ampind)&&(in->dm==Afpind||in->dm==Ampind)&&typedrootfieldcopy(in,p,pc-p->first)){pc++;
        }else if(c->loads && (in->op==INEW||in->op==INEWZ||in->op==IMOVMP)&&typedwindowconstruct(in,p,pc-p->first)){pc++;
        }else if(c->loads&&in->op==IMOVW&&in->dm==Afp&&in->d.reg==RTemp&&in->d.offset==0&&
                 pc+1<stop&&typedrecordmask(in,c->instructions[pc+1],p)){
            inside[pc+1-p->first]=1;pc+=2;
        }else if(c->loads && typedlibraryinst(in,p,pc-p->first,stop)){pc++;
        }else if(c->loads && typedrecordinst(in,p,pc-p->first,stop)){pc++;
        }else if(c->loads && in->op==ISLICELA){if(!(typedrootarrayelement(&in->s)?typedrootarraycopy(in,p,pc-p->first):typedlibrarybytecopy(in,p,pc-p->first)))goto fail;pc++;
        }else if(in->op==IFRAME || (c->external && in->op==IMFRAME)){
            if(in->op==IMFRAME){if(!typedexternalsite(c,p,pc,stop,&next))goto fail;}
            else if(c->loads && in->native64args){if(!typedvariadicsite(c,p,pc,stop,&next))goto fail;}
            else if(!typedmultisite(c,p,pc,stop,depth,&next,&h))goto fail;
            if(pc>p->first && p->returns[pc-1-p->first]==131)inside[pc-p->first]=1;
            for(i=pc+1;i<next;i++)inside[i-p->first]=1;
            pc=next;
        }else if(c->loads && (in->op==IALT || in->op==INBALT || ((in->op==IMOVW || in->op==ILEA) && in->dm==Afp && in->d.reg>=0 && typedaltslot(&in->d,fn)))){
            if(!typedaltinst(in,fn,p,pc-p->first))goto fail;
            if(in->op==IALT || in->op==INBALT){unsigned int start=pc-p->first-2,n=p->code[pc-p->first].middle;while(start && n){start--;if(p->returns[start]==91 || p->returns[start]==189 || p->returns[start]==192 || p->returns[start]==193 || p->returns[start]==198)n--;}if(n){typedfail(p,"alt-setup-shape");goto fail;}for(i=start+1;i<=pc-p->first;i++)inside[i]=1;}
            pc++;
        }else if(c->loads && p->resultgeometry && in->op==IMOVM && in->dm==Afpind && in->d.reg==16){
            if(!typedlocalgeometryreturn(in,p,p->code+pc-p->first,p->returns+pc-p->first))goto fail;pc++;
        }else if(c->loads && in->op==IMOVM && (in->sm==Afpind||in->sm==Ampind) && (in->dm==Afpind||in->dm==Ampind) && typedwmref(typedfieldowner(&in->s),"Draw","Pointer")){
            if(!typedpointercopy(in,p,pc-p->first))goto fail;pc++;
        }else if(c->loads && in->op==IMOVM){
            if(!typedrecordwordcopy(in,p,pc-p->first)&&!typedlocalgeometry(in,p,p->code+pc-p->first,p->returns+pc-p->first))goto fail;pc++;
        }else if(c->loads && p->resultpair && in->op==IMOVMP && in->dm==Afpind && in->d.reg==16){
            Dis64Layout l={0};Decl*d=in->m.decl;unsigned int base=0;int valid;
            valid=in->sm==Afp && in->mm==Adesc && !in->d.offset && !in->branch && d && d->desc && d->desc->id==in->m.offset && tequal(d->ty,p->resulttype) && dis64layouttype(p->resulttype,&l) && d->desc->size==l.legacybytes && d->desc->nmap==1 && d->desc->map && d->desc->map[0]==(p->resultpair<3?0xc0:(128>>(l.leaves[1].legacy/4))) && dis64layoutbindresult(&p->frame,&l,in->s.reg,8,0,&base);
            dis64layoutfree(&l);if(!valid){typedfail(p,"mixed-result-copy");goto fail;}
            p->code[pc-p->first]=(Dis64Instruction){IMOVMP,SRC(AFP)|AXIMM|DST(AFP|AIND),((p->resultpair<3?48:16)<<16)|(16<<8),base,32};p->returns[pc++-p->first]=77;
        }else if(c->loads && p->resulttriple && in->op==IMOVMP){
            Desc*d;unsigned int base=0;
            for(d=descriptors;d && d->id!=in->m.offset;d=d->next);
            if(in->sm!=Afp || in->mm!=Adesc || in->dm!=Afpind || in->d.reg!=16 || in->d.offset || in->branch || !in->m.decl || in->m.decl->desc!=d || !typedchanneltriple(in->m.decl->ty) || !d || d->size!=12 || d->nmap!=1 || d->map[0]!=0xe0 || !typedchannelbinding(c,p,in,&base)){typedfail(p,"channel-result-copy");goto fail;}
            p->code[pc-p->first]=(Dis64Instruction){IMOVMP,SRC(AFP)|AXIMM|DST(AFP|AIND),0,base,32};p->returns[pc++-p->first]=171;
        }else if(c->loads && (in->op==IMOVP || in->op==ISLICEA) && typedreadreplyprep(c,in,p,pc,stop)){pc++;
        }else if(c->loads && in->op==IMOVMP){
            if(!typedrecordcopy(in,p,pc-p->first)&&!typedaltpaircopy(in,p,p->code+pc-p->first,p->returns+pc-p->first))goto fail;pc++;
        }else if(c->external>=2 && in->op==IMOVP && in->dm==Afpind && in->d.reg==16 && in->d.offset==0){
            if(!typedinst(in,stop,p,p->code+pc-p->first,p->returns+pc-p->first))goto fail;pc++;

        }else if(c->loads && (in->op==IINDL || in->op==IINDX) && in->sm==Afp && typedbuiltinarray(&in->s)){
            if(pc+1>=stop||!typedbuiltinarrayindex(in,c->instructions[pc+1],p,pc-p->first))goto fail;
            inside[pc+1-p->first]=1;pc+=2;
        }else if(c->loads && in->op==IINDW && (typedrootarrayelement(&in->s) && (in->dm==Afp || in->dm==Amp || in->dm==Aimm) && pc+1<stop && (c->instructions[pc+1]->op==IMOVP || c->instructions[pc+1]->op==IBEQW || c->instructions[pc+1]->op==IBNEW || c->instructions[pc+1]->op==IBEQC || c->instructions[pc+1]->op==IBNEC))){
            if(pc+1>=stop||!typedrootarrayindex(in,c->instructions[pc+1],p,pc-p->first))goto fail;inside[pc+1-p->first]=1;pc+=2;
        }else if(c->loads && in->op==IINDW && typedregistryarray(&in->s)){
            if(pc+1>=stop||!typedregistryindex(in,c->instructions[pc+1],p,pc-p->first,stop,fn->ty->tof))goto fail;inside[pc+1-p->first]=1;pc+=2;
        }else if(c->loads && in->op==IINDW && typedimagearrayaddr(&in->s) && pc+2<stop && c->instructions[pc+1]->op==IHEADP){
            if(!typedimagearraywindowindex(in,c->instructions[pc+1],c->instructions[pc+2],p,pc-p->first))goto fail;inside[pc+1-p->first]=inside[pc+2-p->first]=1;pc+=3;
        }else if(c->loads && in->op==ISLICEA && in->dm==Afpind && fn->ty->tof->kind==Tarray && fn->ty->tof->tof && typedmanagedkind(fn->ty->tof->tof->kind)){
            if(!typedrootarrayslice(in,p,pc-p->first,fn->ty->tof))goto fail;pc++;
        }else if(c->loads && (in->op==INEWA || in->op==INEWAZ) && typedrootarrayelement(&in->d)){
            if(!typedrootarraynew(in,p,pc-p->first))goto fail;pc++;
        }else if(c->loads && in->op==IINDW && typedimagearrayaddr(&in->s) && pc+1<stop && (c->instructions[pc+1]->op==IFRAME || c->instructions[pc+1]->op==IMFRAME)){
            if(!typedimagearrayresultindex(in,p,pc-p->first))goto fail;pc++;
        }else if(c->loads && in->op==IINDW && typedimagearrayaddr(&in->s)){
            if(pc+1>=stop || !typedrootarrayindex(in,c->instructions[pc+1],p,pc-p->first))goto fail;inside[pc+1-p->first]=1;pc+=2;
        }else if((c->external>=2 || c->loads) && (in->op==INEWA || in->op==INEWAZ || in->op==ISLICEA)){
            if((c->loads && !typedloadbytes(in,p,pc-p->first)) || !typedclientarray(in,p,pc-p->first))goto fail;pc++;
        }else if(c->external>=2 && (in->op==IINDB || in->op==IINDW)){
            if(pc+1>=stop || !typedclientindex(in,c->instructions[pc+1],p,pc-p->first))goto fail;
            inside[pc+1-p->first]=1;pc+=2;
        }else if(c->external>=2 && (in->op==IMOVB || in->op==ICVTBW || in->op==ICVTWB)){
            if(!typedclientbyte(in,p,pc-p->first))goto fail;pc++;
        }else if(c->loads && in->op==ICVTWC && (in->sm==Afpind || in->sm==Ampind) && typedwmref(typedfieldowner(&in->s),"Wmsrv","Client")){
            if(!typedclientidstring(in,p,pc-p->first))goto fail;pc++;
        }else if(c->loads && in->op==ICVTAC && in->dm==Afpind){
            long source;unsigned int sm;int actual;
            if(!typedloadbytes(in,p,pc-p->first))goto fail;
            if(p->resultkind!=Tstring || in->mm!=Anone || in->branch || in->d.reg!=16 || in->d.offset){typedfail(p,"load-byte-result");goto fail;}
            if(!typedoperand(in->sm,&in->s,Tarray,0,0,p,&source,&sm,&actual))goto fail;
            p->code[pc-p->first]=(Dis64Instruction){ICVTAC,SRC(sm)|DST(AFP|AIND),0,source,32};p->returns[pc-p->first]=1;pc++;
        }else if(c->loads && in->op==IADDC && in->dm==Afpind){
            long source,middle;unsigned int sm,mm;int actual;
            if(p->resultkind!=Tstring || in->d.reg!=16 || in->d.offset || in->branch ||
               !typedoperand(in->sm,&in->s,Tstring,0,0,p,&source,&sm,&actual) ||
               !typedoperand(in->mm,&in->m,Tstring,0,1,p,&middle,&mm,&actual))goto fail;
            p->code[pc-p->first]=(Dis64Instruction){IADDC,SRC(sm)|(mm==AFP?AXINF:AXINM)|DST(AFP|AIND),middle,source,32};p->returns[pc-p->first]=1;pc++;
        }else if(c->loads && in->op==ICVTCW && (in->dm==Afpind || in->dm==Ampind) && typedwmref(typedfieldowner(&in->d),"Draw","Pointer")){
            if(!typedpointerparse(in,p,pc-p->first))goto fail;pc++;
        }else if((c->external>=2 && (in->op==ICVTCW || in->op==ICVTCL || in->op==ICVTCF || in->op==ICVTWC || in->op==ICVTLC || in->op==ICVTFC || in->op==ICVTCA || in->op==ICVTAC)) || (c->loads && (in->op==ICVTCW || in->op==ICVTWC || in->op==ICVTCA || in->op==ICVTAC || (in->op==ICVTCF && p->resultpair==5 && in->dm==Afpind)))){
            if((c->loads && (in->op==ICVTCA || in->op==ICVTAC) && !typedloadbytes(in,p,pc-p->first)) || !typedclientcodec(in,p,pc-p->first))goto fail;pc++;
        }else if((c->external>=2 && (in->op==IADDC || in->op==IINSC || in->op==ISLICEC || in->op==IINDC || in->op==IBEQC || in->op==IBNEC || in->op==IBLTC || in->op==IBLEC || in->op==IBGTC || in->op==IBGEC)) ||
                 (c->loads && (in->op==IADDC || in->op==IINSC || in->op==ISLICEC || in->op==IINDC || in->op==IBEQC || in->op==IBNEC || in->op==IBLTC || in->op==IBLEC || in->op==IBGTC || in->op==IBGEC))){
            if(!typedclientstring(in,p,pc-p->first,stop))goto fail;pc++;
        }else if(c->loads && ((in->sm==Afpind||in->sm==Ampind)&&typedwindowowner(&in->s)&&in->s.native64type&&in->s.native64type->kind==Tint || in->op==IMOVW&&(in->dm==Afpind||in->dm==Ampind)&&typedwindowowner(&in->d))){
            if(!typedwindowword(in,p,pc-p->first,stop))goto fail;pc++;
        }else if(c->loads && (in->sm==Afpind || in->sm==Ampind) && (typeddrawowner(&in->s)==1 || (typedwmref(typedfieldowner(&in->s),"Wmsrv","Client") && in->s.native64type && in->s.native64type->kind==Tint && (in->op==IBEQW || in->op==IBNEW || in->op==IBLTW || in->op==IBLEW || in->op==IBGTW || in->op==IBGEW))) && (in->op==IMOVW || typedwordargop(in->op) || in->op==IBEQW || in->op==IBNEW || in->op==IBLTW || in->op==IBLEW || in->op==IBGTW || in->op==IBGEW)){
            if(!typedimageword(in,p,pc-p->first,stop))goto fail;pc++;
        }else if(c->loads && (in->op==IORW || in->op==IANDW || in->op==IXORW) && (in->dm==Afpind || in->dm==Ampind) && typedwmref(typedfieldowner(&in->d),"Wmsrv","Client")){
            if(!typedclientflagsor(in,p,pc-p->first))goto fail;pc++;
        }else if(c->loads && in->op==IANDW && (in->sm==Afpind || in->sm==Ampind) && typedwmref(typedfieldowner(&in->s),"Wmsrv","Client")){
            if(!typedclientflagsand(in,p,pc-p->first))goto fail;pc++;
        }else if(c->loads && in->op==IMOVW && (in->dm==Afpind || in->dm==Ampind) && typedwmref(typedfieldowner(&in->d),"Draw","Pointer")){
            if(!typedpointerstore(in,p,pc-p->first))goto fail;pc++;
        }else if(c->loads && (in->op==IMOVW || in->op==IBEQW || in->op==IBNEW) && (in->sm==Afpind || in->sm==Ampind) && typedwmref(typedfieldowner(&in->s),"Draw","Pointer")){
            if(!typedpointerword(in,p,pc-p->first,stop))goto fail;pc++;
        }else if(c->loads && in->op==IMOVP && (in->sm==Afpind || in->sm==Ampind) && (in->dm==Afpind || in->dm==Ampind)){
            if(!typedcontextcopy(in,p,pc-p->first))goto fail;pc++;
        }else if(c->loads && in->op==IMOVP && (in->sm==Afpind || in->sm==Ampind || in->dm==Afpind || in->dm==Ampind) &&
          typedwmsrvwindow(typedfieldowner((in->sm==Afpind || in->sm==Ampind)?&in->s:&in->d))){
            if(!typedwmsrvwindowfield(in,p,p->code+pc-p->first,p->returns+pc-p->first))goto fail;pc++;
        }else if(c->loads && (in->op==IMOVP || in->op==IMOVW) &&
          (in->sm==Afpind || in->sm==Ampind || in->dm==Afpind || in->dm==Ampind) &&
          typedwmref(typedfieldowner((in->sm==Afpind || in->sm==Ampind)?&in->s:&in->d),"Wmsrv","Client")){
            if(!typedclientfieldmove(in,p,p->code+pc-p->first,p->returns+pc-p->first))goto fail;pc++;
        }else if((c->external>=2 || c->loads) && in->op==IMOVP && (in->sm==Afpind || in->sm==Ampind || (in->dm==Afpind && in->d.reg!=16) || in->dm==Ampind)){
            Addr*field=(in->sm==Afpind || in->sm==Ampind)?&in->s:&in->d;
            int window=c->loads && typedwindowowner(field),draw=c->loads && typeddrawowner(field);
            if(!(window?typedwindowfield(in,p,pc-p->first,0,nil):draw?typeddrawfield(in,p,pc-p->first,0,nil):typedclientfield(in,p,pc-p->first,0)))goto fail;
            if(c->loads){p->returns[pc-p->first]=window?100:draw?113:96;p->code[pc-p->first].middle=p->contextfields[pc-p->first];}pc++;
        }else if(c->loads && (in->op==IBEQW || in->op==IBNEW) &&
          (in->sm==Afpind || in->sm==Ampind || in->mm==Afpind || in->mm==Ampind)){
            if(!typedloadcontextcompare(in,p,pc-p->first,stop))goto fail;pc++;
        }else if((c->external>=2 || c->loads) && (in->op==IBEQW || in->op==IBNEW || in->op==IBEQL || in->op==IBNEL) &&
          ((in->s.decl && in->s.decl->ty && typedclientreferencekind(in->s.decl->ty->kind)) ||
           (in->m.decl && in->m.decl->ty && typedclientreferencekind(in->m.decl->ty->kind)))){
            if(!(c->loads?typedloadcompare(in,p,pc-p->first,stop):typedclientcompare(in,p,pc-p->first,stop)))goto fail;pc++;
        }else if(c->loads && (in->op==ISEND || in->op==IRECV)){
            if(!typedclienttransfer(in,p,p->code+pc-p->first,p->returns+pc-p->first))goto fail;pc++;
        }else if(c->loads && in->op==INEWCM){
            Type*t=in->d.native64type?in->d.native64type:in->d.decl?in->d.decl->ty:nil;Inst copy=*in;
            if(!t||t->kind!=Tchan||!typedpointpayload(t->tof)||in->sm!=Aimm||in->s.offset!=8||(in->dm!=Afp&&in->dm!=Amp)) {typedfail(p,"point-channel-type");goto fail;}
            copy.op=INEWCL;copy.sm=Anone;if(!typedchanneloutput(&copy,fn,p,p->code+pc-p->first,p->returns+pc-p->first))goto fail;p->returns[pc-p->first]=55;pc++;
        }else if(c->loads && in->op==INEWCMP){
            if(!typedclientmappedchannel(in,fn,p,p->code+pc-p->first,p->returns+pc-p->first))goto fail;pc++;
        }else if(c->loads && (in->op==INEWCB || in->op==INEWCW || in->op==INEWCL || in->op==INEWCF || in->op==INEWCP)){
            if(!typedclientchannel(in,fn,p,p->code+pc-p->first,p->returns+pc-p->first))goto fail;pc++;
        }else if((c->external>=2 || c->loads) && (in->op==INEW || in->op==INEWZ)){
            if(c->loads && in->s.decl && in->s.decl->sym && !strcmp(in->s.decl->sym->name,"Pointer")){
                if(!typedpointernew(in,p,p->code+pc-p->first))goto fail;p->returns[pc-p->first]=242;pc++;continue;
            }
            if(!typedclientcontext(in,p,p->code+pc-p->first)){
                if(!c->loads||!typedrecordnew(in,p,pc-p->first)){
                    typedfail(p,"client-context-geometry");goto fail;
                }
                pc++;continue;
            }
            if(c->loads){p->code[pc-p->first].source=0;p->returns[pc-p->first]=UDST(p->code[pc-p->first].mode)==(AFP|AIND)?174:172;}pc++;
        }else if(c->loads && ((in->op==ICONSMP&&typedshellmappedpairlist(&in->d))||
                  ((in->op==IHEADMP||in->op==ITAIL)&&typedshellmappedpairlist(&in->s)))){
            if(!typedshellmappedpairlistop(in,p,p->code+pc-p->first,p->returns+pc-p->first))goto fail;pc++;
        }else if(c->loads && (in->op==ICONSM || in->op==IHEADM || (in->op==ITAIL && typedrectlisttype(&in->s)))){
            if(!typedrectlist(in,p,p->code+pc-p->first,p->returns+pc-p->first))goto fail;pc++;
        }else if(c->loads && ((in->op==ICONSP && typedshellrootlist(&in->d)) || ((in->op==IHEADP || in->op==ITAIL) && typedshellrootlist(&in->s)))){
            if(!typedshelllistop(in,p,p->code+pc-p->first,p->returns+pc-p->first))goto fail;pc++;
        }else if(c->loads && (in->op==ICONSP || in->op==IHEADP || in->op==ITAIL) && ((in->op!=ICONSP && typedpointerlisttype(&in->s)) || (in->op!=IHEADP && typedpointerlisttype(&in->d)))){
            if(!typedpointerlist(in,p,p->code+pc-p->first,p->returns+pc-p->first))goto fail;pc++;
        }else if(c->loads && (in->op==ICONSW || in->op==IHEADW || (in->op==ITAIL && typedintlisttype(&in->s)))){
            if(!typedclientintlist(in,fn,p,p->code+pc-p->first,p->returns+pc-p->first))goto fail;pc++;
        }else if(c->loads && (in->op==ICONSP || in->op==IHEADP || in->op==ITAIL) &&
          ((in->op!=ICONSP && typedwindowlisttype(&in->s)) || (in->op!=IHEADP && typedwindowlisttype(&in->d)))){
            if(!typedwindowlist(in,p,p->code+pc-p->first,p->returns+pc-p->first,fn->ty->tof))goto fail;pc++;
        }else if((c->external>=2 || c->loads) && (in->op==ICONSP || in->op==IHEADP || in->op==ITAIL)){
            if(!(c->loads?typedloadlist(in,fn,p,p->code+pc-p->first):typedclientlist(in,p,p->code+pc-p->first)))goto fail;
            if(UDST(p->code[pc-p->first].mode)==(AFP|AIND))p->returns[pc-p->first]=1;pc++;
        }else if(c->loads && in->op==IRAISE){
            if(!typedraise(in,p,p->code+pc-p->first))goto fail;pc++;
        }else if((c->external==2 || c->loads) && in->op==ILOAD){
            long source,destination;unsigned int sm,dm;int actual;
            if(in->mm!=Aldt || in->m.decl==nil || in->m.decl->offset<0 || in->m.decl->offset>63 ||
               (!c->loads && in->m.decl->offset!=0) || in->branch!=nil ||
               !typedoperand(in->sm,&in->s,Tstring,0,0,p,&source,&sm,&actual) || actual!=Tstring ||
               !typedoperand(in->dm,&in->d,TypedRoot,0,0,p,&destination,&dm,&actual) || actual!=Tmodule){typedfail(p,"load-roles");goto fail;}
            p->code[pc-p->first]=(Dis64Instruction){ILOAD,SRC(sm)|AXIMM|DST(dm),in->m.decl->offset,source,destination};pc++;
        }else{
            if(!typedinst(in,stop,p,p->code+pc-p->first,p->returns+pc-p->first))goto fail;
            pc++;
        }
    }
    for(pc=p->first;pc<stop;pc++){
        in=c->instructions[pc];
        if(in->op!=ICALL && in->op!=IMCALL && in->branch!=nil && inside[in->branch->pc-p->first] &&
           p->returns[pc-p->first]!=Native64TypedWordChoice){typedfail(p,"multi-setup-branch");goto fail;}
    }
    for(i=0;i<p->ntables;i++){
        Dis64TypedTable *t=p->tables+i;unsigned int j,target;
        for(j=0;j<(unsigned int)t->table->nlab+(t->op!=IGOTO || t->altdefault);j++){
            target=(j<(unsigned int)t->table->nlab?t->table->labs[j].inst:t->table->iwild)->pc-p->first;
            if(inside[target]){typedfail(p,"multi-table-setup-branch");goto fail;}
        }
    }
    if((c->external<2 && !c->loads && !p->nsites && !p->nexternal) ||
       (fn->handler&&!typedcatch(p,inside)) || !typedresults(p))goto fail;
    free(inside);p->tail=c->loads?5:c->external?4:3;c->state[index]=2;c->height[index]=h;return 1;
fail:
    free(inside);c->state[index]=3;return 0;
}

static int
typedmultilower(Decl *fn,Decl *globals,Decl **functions,int nf,Inst *input,unsigned int count,unsigned int stop,Dis64TypedBody *p,int external)
{
    TypedMulti c={0};Inst *in;unsigned int i,root=0;int found=0,ok=0;
    USED(stop);
    if(p->code!=nil || p->reason!=nil || p->sites!=nil || p->external!=nil || p->variadic!=nil)return typedfail(p,"nonempty-output");
    if(nf<1 || nf>4096 || !count || count>TypedInstructions || functions==nil || input==nil)return typedfail(p,"multi-limit");
    c.loads=external==4;c.external=c.loads?1:external;c.globals=globals;c.functions=functions;c.nf=nf;c.count=count;
    c.bodies=allocmem(nf*sizeof(*c.bodies));memset(c.bodies,0,nf*sizeof(*c.bodies));
    c.state=allocmem(nf);memset(c.state,0,nf);c.height=allocmem(nf*sizeof(*c.height));memset(c.height,0,nf*sizeof(*c.height));
    c.instructions=allocmem(count*sizeof(*c.instructions));memset(c.instructions,0,count*sizeof(*c.instructions));
    c.entries=allocmem(count*sizeof(*c.entries));for(i=0;i<count;i++)c.entries[i]=-1;
    for(i=0;i<(unsigned int)nf;i++){
        if(functions[i]==nil || functions[i]->pc==nil || functions[i]->pc->pc<0 || (unsigned int)functions[i]->pc->pc>=count || c.entries[functions[i]->pc->pc]>=0){typedfail(p,"multi-function-table");goto done;}
        c.entries[functions[i]->pc->pc]=i;if(functions[i]==fn){root=i;found=1;}
    }
    if(!found){typedfail(p,"multi-function-table");goto done;}
    for(in=input;in!=nil;in=in->next){
        if(in->op==INOOP)continue;
        if(in->pc<0 || (unsigned int)in->pc>=count || c.instructions[in->pc]!=nil){typedfail(p,"multi-instruction-table");goto done;}
        c.instructions[in->pc]=in;
    }
    if(c.instructions[0]==nil){typedfail(p,"multi-instruction-table");goto done;}
    ok=typedmultibody(&c,root,0);*p=c.bodies[root];memset(c.bodies+root,0,sizeof(*p));
    if(!ok && p->reason==nil)typedfail(p,"multi-body");
done:
    for(i=0;i<(unsigned int)nf;i++)dis64typedfree(c.bodies+i);
    free(c.bodies);free(c.state);free(c.height);free(c.instructions);free(c.entries);return ok;
}

int
dis64typedmulti(Decl *fn,Decl *globals,Decl **functions,int nf,Inst *input,unsigned int count,unsigned int stop,Dis64TypedBody *p)
{
    return typedmultilower(fn,globals,functions,nf,input,count,stop,p,0);
}
int
dis64typedexternal(Decl *fn,Decl *globals,Decl **functions,int nf,Inst *input,unsigned int count,unsigned int stop,Dis64TypedBody *p)
{
    return typedmultilower(fn,globals,functions,nf,input,count,stop,p,1);
}

int
dis64typedclient(Decl *fn,Decl *globals,Decl **functions,int nf,Inst *input,unsigned int count,unsigned int stop,Dis64TypedBody *p)
{
 return typedmultilower(fn,globals,functions,nf,input,count,stop,p,2);
}

int dis64typedcommand(Decl *fn,Decl *globals,Decl **functions,int nf,Inst *input,unsigned int count,unsigned int stop,Dis64TypedBody *p){return typedmultilower(fn,globals,functions,nf,input,count,stop,p,3);}

int
dis64typedloads(Decl *fn,Decl *globals,Decl **functions,int nf,Inst *input,unsigned int count,unsigned int stop,Dis64TypedBody *p)
{
    return typedmultilower(fn,globals,functions,nf,input,count,stop,p,4);
}
