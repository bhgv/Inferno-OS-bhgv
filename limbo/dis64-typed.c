#include "native64-array-layout.h"
#include "limbo.h"
#include "native64-record-layout.h"
#include "dis64-typed.h"

enum { TypedRoot=-10, TypedInstructions=65536 };

/* The checked compiler report can consult classic exception tables. */
static Except *typedreportcatches;

static int
typedfail(Dis64TypedBody *p, char *reason)
{
	if(p->reason==nil) p->reason=reason;
	return 0;
}

/* Width and role both follow the opcode. A descriptor bit alone never proves a scalar. */
static int
typedoperand(int mode, Addr *a, int kind, int immediate, int middle,
	Dis64TypedBody *p, long *value, unsigned int *wire, int *actual)
{
	Dis64Layout *layout;
	Dis64LayoutLeaf *leaf=nil;
	unsigned int i,offset,width=kind==Tbyte?1:kind==Tbig || kind==Treal?8:4;
	int reference=kind==TypedRoot || kind==Tstring || kind==Tlist || kind==Tarray;
    int role;Type *use;
	if(mode==Aimm){
		if(!immediate || reference || width==8 || a->offset < -536870912L || a->offset>536870911L ||
		   (middle && (a->offset < -32768 || a->offset>32767)) ||
		   (kind==Tbyte && (a->offset<0 || a->offset>255))) return typedfail(p,"immediate-role");
		*value=a->offset; *wire=AIMM; *actual=kind; return 1;
	}
	if((mode!=Afp && mode!=Amp) || a->reg<0 || a->reg>Dis64LayoutBytes)
		return typedfail(p,"indirect-or-reserved-operand");
	layout=mode==Afp?&p->frame:&p->data;
	if(!dis64layoutaddress(layout,a->reg,width,reference,&offset)) return typedfail(p,"typed-span");
	for(i=0;i<layout->count;i++) if(a->reg>=layout->leaves[i].legacy && a->reg-layout->leaves[i].legacy<layout->leaves[i].oldbytes){leaf=layout->leaves+i;break;}
    role=leaf?leaf->kind:-1;
    /* Compiler word temporaries share managed geometry across nominal classes.
     * Only their original per-use type can refine an already proven root. */
    use=a->native64type;
    if(p->sourceroles && reference && leaf && leaf->oldbytes==4 && leaf->bytes==8 && role>=0 && use && use->size==4 &&
       (use->kind==Tstring || use->kind==Tref || use->kind==Tlist || use->kind==Tarray || use->kind==Tmodule || use->kind==Tchan || use->kind==Tany) &&
       a->decl && a->decl->store==Dlocal && a->decl->sym && a->decl->sym->name[0]=='.' && a->decl->sym->name[1]=='t' &&
       a->reg==a->decl->offset && !a->offset)role=use->kind;
	if(leaf==nil || (reference ? (leaf->oldbytes!=4 || leaf->bytes!=8 || leaf->kind<0 ||
	   (kind!=TypedRoot && role!=kind && role!=Tany)) :
	   (leaf->kind!=kind && !(kind==Tint && leaf->kind==Tfix) &&
	    !((kind==Tbig || kind==Treal) && (leaf->kind==Tbig || leaf->kind==Treal))))) return typedfail(p,"opcode-type");
	if(middle && offset>65535) return typedfail(p,"middle-offset");
	*value=offset; *wire=mode==Afp?AFP:AMP; *actual=role; return 1;
}

static int
typedtextcmp(Sym *a,Sym *b)
{
    int order=memcmp(a->name,b->name,a->len<b->len?a->len:b->len);
    return order?order:a->len<b->len?-1:a->len>b->len;
}

static Type *typedfieldowner(Addr*);
static int typedrecordlayoutoperand(int,Addr*,int,Dis64TypedBody*,long*,unsigned int*);
static int typedrecordregister(Dis64TypedBody*,unsigned int,unsigned int,Type*);

static int
typedtable(Inst *in,unsigned int stop,Dis64TypedBody *p,Dis64Instruction *out,uchar *role)
{
    Decl *d=in->d.decl;Case *c;Label *l;unsigned int base,sm,i,rows=0;long value;int actual,kind;unsigned int altdefault=0;
    kind=in->op==ICASE?Tcase:in->op==ICASEL?Tcasel:in->op==ICASEC?Tcasec:Tgoto;
    if(p->tables==nil || p->ntables==256 || in->dm!=Amp || in->mm!=Anone || in->branch!=nil || d==nil || d->store!=Dglobal ||
       d->sym==nil || d->sym->name==nil || d->sym->name[0]!='.' || d->init==nil || d->init->op!=Oname || d->init->decl!=d ||
       d->ty==nil || d->ty->kind!=kind || (c=d->ty->cse)==nil ||
       in->d.reg!=d->offset+(in->op==IGOTO?4:0) || c->nlab<0 || c->nlab>4096 ||
       (c->nlab && c->labs==nil) || (in->op==IGOTO && !c->nlab) ||
       !dis64layoutaddress(&p->data,d->offset,4,0,&base))return typedfail(p,"table-shape");
    if(in->op!=IGOTO && (c->iwild==nil || c->iwild->pc<p->first || c->iwild->pc>=stop))return typedfail(p,"table-default-owner");
    for(i=0;i<p->ntables;i++)rows+=p->tables[i].table->nlab;
    if((unsigned int)c->nlab>65536-rows)return typedfail(p,"table-row-budget");
    if(in->op==IGOTO && p->sourceroles && in->pc>p->first && p->returns[in->pc-p->first-1]==191 && p->code[in->pc-p->first-1].op==INBALT){
        if(c->nlab!=p->code[in->pc-p->first-1].middle || !c->iwild || c->iwild->pc<p->first || c->iwild->pc>=stop)return typedfail(p,"alt-default-owner");altdefault=1;
    }
    for(i=0;i<(unsigned int)c->nlab;i++){
        l=c->labs+i;
        if(in->op==IGOTO && p->sourceroles && in->pc>p->first && p->returns[in->pc-p->first-1]==191 && (altdefault || (p->code[in->pc-p->first-1].op==IALT && c->iwild==nil)) &&
           c->nlab==p->code[in->pc-p->first-1].middle &&
           l->inst && l->inst->pc>=p->first && l->inst->pc<stop)continue;
        if(in->op==ICASEC){
            Sym *a,*b;
            if(l->start==nil || l->stop==nil || l->start->op!=Oconst || l->stop->op!=Oconst ||
               l->start->ty==nil || l->start->ty->kind!=Tstring || l->stop->ty==nil || l->stop->ty->kind!=Tstring ||
               l->start->decl==nil || l->stop->decl==nil || (a=l->start->decl->sym)==nil || (b=l->stop->decl->sym)==nil ||
               a->name==nil || b->name==nil || a->len<0 || b->len<0 || a->len>4096 || b->len>4096 || typedtextcmp(a,b)>0 ||
               (i && typedtextcmp(c->labs[i-1].stop->decl->sym,a)>=0) || l->inst==nil || l->inst->pc<p->first || l->inst->pc>=stop)
                return typedfail(p,"string-table-row-owner-or-order");
            continue;
        }
        if(l->start==nil || l->stop==nil || l->start->op!=Oconst || l->stop->op!=Oconst ||
           l->start->val>l->stop->val || l->inst==nil || l->inst->pc<p->first || l->inst->pc>=stop ||
           (i && c->labs[i-1].stop->val>=l->start->val) ||
           (in->op!=ICASEL && (l->start->val<(-2147483647LL-1) || l->stop->val>2147483647LL)) ||
           (in->op==IGOTO && (l->start->val!=l->stop->val || (i && l->start->val!=c->labs[i-1].start->val+1))))
            return typedfail(p,"table-row-owner-or-order");
    }
    if(p->sourceroles&&(in->op==ICASE||in->op==IGOTO||in->op==ICASEC)&&(in->sm==Afpind||in->sm==Ampind)){
        if((in->op==ICASEC&&(!in->s.native64type||in->s.native64type->kind!=Tstring))||
           !typedrecordlayoutoperand(in->sm,&in->s,in->op==ICASEC,p,&value,&sm)||
           !typedrecordregister(p,in->pc-p->first,0,typedfieldowner(&in->s)->tof))
            return typedfail(p,"table-record-selector");
        *role=Native64TypedRecord;
    }else if(!typedoperand(in->sm,&in->s,in->op==ICASEL?Tbig:in->op==ICASEC?Tstring:Tint,0,0,p,&value,&sm,&actual))return 0;
    if(in->op==IGOTO && in->pc>p->first && p->returns[in->pc-p->first-1]==191 && (sm!=AFP || value!=p->code[in->pc-p->first-1].destination))return typedfail(p,"alt-selection-table");
    p->tables[p->ntables++]=(Dis64TypedTable){in->pc-p->first,base,in->op,c,altdefault};
    *out=(Dis64Instruction){in->op,SRC(sm)|DST(AMP),0,value,base+(in->op==IGOTO?4:0)};return 1;
}

/* Managed references share storage width, but keep their declared type identity. */
static int
typedmanagedkind(int kind)
{return kind==Tstring || kind==Tlist || kind==Tarray || kind==Tref || kind==Tchan || kind==Tmodule;}

/* A scalar followed by one managed root. Match the declaration and complete
 * layout; the contract describes storage, independently of the callee name. */
static unsigned int
typedmixedtuple(Type *t, Dis64Layout *l)
{
 Decl *a,*b;int root;
 if(!t || t->kind!=Ttuple || !(a=t->ids) || !(b=a->next) || b->next || !a->ty || !b->ty)return 0;
 root=b->ty->kind;
 if(!typedmanagedkind(root))return 0;
 if(a->ty->kind!=Tint && a->ty->kind!=Tbig && a->ty->kind!=Treal)return 0;
 if(!dis64layouttype(t,l) || l->count!=2 || l->bytes!=16 || l->alignment!=8 ||
    l->legacybytes!=(a->ty->kind==Tint?8:16) ||
    l->leaves[0].legacy || l->leaves[0].native || l->leaves[0].kind!=a->ty->kind ||
    l->leaves[0].bytes!=(a->ty->kind==Tint?4:8) ||
    l->leaves[1].legacy!=(a->ty->kind==Tint?4:8) || l->leaves[1].native!=8 ||
    l->leaves[1].oldbytes!=4 || l->leaves[1].bytes!=8 || l->leaves[1].kind!=root)return 0;
 return 16;
}

/* Pair classification shares mixed layout validation with imported calls. */
static int
typedpairtype(Type *t)
{
 Decl *a;Dis64Layout l={0};int kind=0,ok=0;
 if(!t || t->kind!=Ttuple || !(a=t->ids) || !a->ty || !a->next || a->next->next || !a->next->ty)return 0;
 if(typedmixedtuple(t,&l)){kind=a->ty->kind==Tint?3:a->ty->kind==Tbig?4:5;dis64layoutfree(&l);}
 else{dis64layoutfree(&l);
  if(typedmanagedkind(a->ty->kind) && typedmanagedkind(a->next->ty->kind))kind=a->ty->kind==Tstring && a->next->ty->kind==Tstring?2:1;
 }
 if(!kind || !dis64layouttype(t,&l))goto done;
 ok=(l.legacybytes==(kind>=4?16:8)) && l.bytes==16 && l.count==2 && l.leaves[0].legacy==0 && l.leaves[0].native==0 && l.leaves[0].bytes==(kind==3?4:8) && l.leaves[1].legacy==(kind>=4?8:4) && l.leaves[1].native==8 && l.leaves[1].bytes==8;
 done:dis64layoutfree(&l);return ok?kind:0;
}

static int
typedwordtriple(Type*t)
{Decl*d;Dis64Layout l={0};unsigned int n=0;int ok=0;if(!t || t->kind!=Ttuple)return 0;for(d=t->ids;d;d=d->next)if(!d->ty || d->ty->kind!=Tint || d->offset!=4*n++)return 0;if(n==3 && dis64layouttype(t,&l) && l.legacybytes==12 && l.bytes==12 && l.count==3)ok=1;dis64layoutfree(&l);return ok;}

/* Three channel handles, regardless of their independently typed payloads. */
static int
typedchanneltriple(Type*t)
{
 Decl*d;Dis64Layout l={0};unsigned int i=0;int ok=0;
 if(!t || t->kind!=Ttuple)return 0;
 for(d=t->ids;d;d=d->next){if(!d->ty || d->ty->kind!=Tchan)return 0;i++;}
 if(i!=3 || !dis64layouttype(t,&l) || l.legacybytes!=12 || l.bytes!=24 || l.alignment!=8 || l.count!=3)goto done;
 for(i=0;i<3;i++)if(l.leaves[i].legacy!=4*i || l.leaves[i].native!=8*i || l.leaves[i].oldbytes!=4 || l.leaves[i].bytes!=8 || l.leaves[i].kind!=Tchan)goto done;
 ok=1;
 done:dis64layoutfree(&l);return ok;
}

static int
typedcatchstringslot(Inst *in, Dis64TypedBody *p)
{
 Except *e;
 Label *label;
 Addr *a=&in->s;
 if(in->sm!=Afp||!a->decl||a->decl->ty!=rtexception||a->offset||
    a->reg!=a->decl->offset)return 0;
 for(e=typedreportcatches;e;e=e->next){
  if(e->d!=a->decl||e->ne||!e->c||e->c->nlab!=1||e->c->iwild||
     (!e->p1->reach&&!e->p2->reach)||getpc(e->p1)<p->first||getpc(e->p2)>in->pc)
   continue;
  label=e->c->labs;
  if(label->start&&label->start==label->stop&&label->start->ty==tstring&&
     label->inst&&label->inst->pc>=getpc(e->p2)&&label->inst->pc<=in->pc)return 1;
 }
 return 0;
}

static int
typedraise(Inst *in,Dis64TypedBody *p,Dis64Instruction *out)
{
 unsigned int sm;long source;int actual;
 if(in->mm!=Anone || in->dm!=Anone || in->branch!=nil ||
    !typedoperand(in->sm,&in->s,TypedRoot,0,0,p,&source,&sm,&actual) ||
    (actual!=Tstring&&!typedcatchstringslot(in,p)))
  return typedfail(p,"raise-string-roles");
 *out=(Dis64Instruction){IRAISE,SRC(sm)|DST(AXXX),0,source,0};return 1;
}

/* Reused compiler tuple temporaries retain storage classes from an earlier use.
 * Only an exact two/three-root scratch group can change its component classes. */
static int
typedroottemporary(Addr*a,int sourcekind)
{Type*t;Dis64Layout l={0};unsigned int i;int ok=0;
 if(!a->decl||a->decl->store!=Dlocal||!a->decl->sym||a->decl->sym->name[0]!='.'||(a->decl->sym->name[1]!='t'&&a->decl->sym->name[1]!='b')||a->offset||a->reg<a->decl->offset||(a->reg-a->decl->offset)%4)return 0;
 if(!a->native64type||a->native64type->size!=4||a->native64type->kind!=sourcekind)return 0;
 t=a->native64type&&a->native64type->kind==Ttuple?a->native64type:a->decl->ty;
 if(!t||t->kind!=Ttuple||!dis64layouttype(t,&l)||(l.count!=2&&l.count!=3)||l.legacybytes!=4*l.count||l.bytes!=8*l.count||l.alignment!=8||(unsigned int)(a->reg-a->decl->offset)>=l.legacybytes)goto done;
 for(i=0;i<l.count;i++)if(l.leaves[i].legacy!=4*i||l.leaves[i].native!=8*i||l.leaves[i].oldbytes!=4||l.leaves[i].bytes!=8||l.leaves[i].kind<0)goto done;ok=1;
 done:dis64layoutfree(&l);return ok;}

static int
typedinst(Inst *in, unsigned int stop, Dis64TypedBody *p, Dis64Instruction *out, unsigned char *result)
{
	unsigned int sm,mm=AXXX,dm;
	int sk=Tint,mk=Tint,dk=Tint,sourcekind=0,destkind=0,middlekind=0;
	int binary=0,branch=0,immediate=1;
	memset(out,0,sizeof(*out)); out->op=in->op;
	switch(in->op){
	case ICASE:case ICASEL:case IGOTO:case ICASEC:return typedtable(in,stop,p,out,result);
	case IEXIT:
        if(!p->sourceroles)return typedfail(p,"unsupported-opcode");
        /* Process termination has no result or fallthrough edge. */
    case IRET: case INOP:
		if(in->sm!=Anone || in->mm!=Anone || in->dm!=Anone || in->branch!=nil) return typedfail(p,"terminal-operands");
		out->mode=SRC(AXXX)|DST(AXXX); return 1;
	case IJMP:
		if(in->sm!=Anone || in->mm!=Anone) return typedfail(p,"jump-operands");
		branch=2; break;
	case IMOVB: sk=dk=Tbyte; break;
	case IMOVW: break;
	case IMOVL: sk=dk=Tbig; immediate=0; break;
	case IMOVF: sk=dk=Treal; immediate=0; break;
	case IMOVP: sk=dk=TypedRoot; immediate=0; break;
	case ICVTBW: sk=Tbyte; break;
	case ICVTWB: dk=Tbyte; break;
	case ICVTWL: dk=Tbig; break;
	case ICVTLW: sk=Tbig; immediate=0; break;
	case ICVTWF: dk=Treal; break;
	case ICVTFW: sk=Treal; immediate=0; break;
	case ICVTLF: sk=Tbig; dk=Treal; immediate=0; break;
	case ICVTFL: sk=Treal; dk=Tbig; immediate=0; break;
	case INEGF: sk=dk=Treal; immediate=0; break;
	case ILENC: sk=Tstring; immediate=0; break;
	case ILENL: sk=Tlist; immediate=0; break;
	case ILENA: sk=Tarray; immediate=0; break;
	case IADDW: case ISUBW: case IMULW: case IDIVW: case IMODW:
	case IANDW: case IORW: case IXORW: case ISHLW: case ISHRW:
		binary=1; break;
	case IADDL: case ISUBL: case IMULL: case IDIVL: case IMODL:
	case IANDL: case IORL: case IXORL:
		sk=mk=dk=Tbig; binary=1; immediate=0; break;
	case ISHLL: case ISHRL:
		mk=dk=Tbig; binary=1; break;
	case IADDF: case ISUBF: case IMULF: case IDIVF:
		sk=mk=dk=Treal; binary=1; immediate=0; break;
	case IBEQW: case IBNEW: case IBLTW: case IBLEW: case IBGTW: case IBGEW:
		branch=1; break;
	case IBEQL: case IBNEL: case IBLTL: case IBLEL: case IBGTL: case IBGEL:
		sk=mk=Tbig; branch=1; immediate=0; break;
	case IBEQF: case IBNEF: case IBLTF: case IBLEF: case IBGTF: case IBGEF:
		sk=mk=Treal; branch=1; immediate=0; break;
	default: return typedfail(p,"unsupported-opcode");
	}
	if(!branch && in->branch!=nil) return typedfail(p,"hidden-branch");
	if(branch){
		if(in->dm!=Apc || in->branch==nil || in->branch->pc<p->first || in->branch->pc>=stop || in->d.offset!=in->branch->pc)
			return typedfail(p,"branch-owner");
		out->destination=in->branch->pc-p->first; dm=AIMM;
	}else if(in->dm==Afpind && p->resulttriple){
        if(in->op!=(p->resulttriple==2?IMOVW:IMOVP) || in->mm!=Anone || in->d.reg!=16 || in->d.offset<0 || in->d.offset>8 || in->d.offset%4)return typedfail(p,"channel-result-destination");
        out->destination=32+(p->resulttriple==2?1:2)*in->d.offset;dm=AFP|AIND;destkind=p->resulttriple==2?Tint:Tchan;*result=2<<(in->d.offset/4);
    }else if(in->dm==Afpind && p->resultpair){
        int second=in->d.offset!=0,firstkind=p->resultpair==3?Tint:p->resultpair==4?Tbig:Treal;
        if(in->d.reg!=16 || (in->d.offset && in->d.offset!=(p->resultpair>=4?8:4)) || (p->resultpair<3 || second ? in->op!=IMOVP || in->mm!=Anone : dk!=firstkind))return typedfail(p,"pair-result-destination");
        out->destination=second?40:32;dm=AFP|AIND;destkind=p->resultpair>=3?(second?p->resulttype->ids->next->ty->kind:firstkind):(second?p->resulttype->ids->next->ty->kind:p->resulttype->ids->ty->kind);*result=second?16:8;
	}else if(in->dm==Afpind){
		if(p->resultkind==Tnone || in->d.reg!=16 || in->d.offset!=0 ||
		   (dk==TypedRoot ? !(p->resultkind==Tstring || p->resultkind==Tref || p->resultkind==Tarray || p->resultkind==Tlist || p->resultkind==Tchan || p->resultkind==Tmodule) :
		    (dk!=p->resultkind && !(dk==Tint && p->resultkind==Tfix))) ||
		   (binary && in->mm==Anone)) return typedfail(p,"result-destination");
		out->destination=32; dm=AFP|AIND; destkind=p->resultkind; *result=1;
	}else if(!typedoperand(in->dm,&in->d,dk,0,0,p,&out->destination,&dm,&destkind)) return 0;
	if(branch==2){out->mode=SRC(AXXX)|DST(dm);return 1;}
	if(!typedoperand(in->sm,&in->s,sk,immediate,0,p,&out->source,&sm,&sourcekind)) return 0;
	if(in->op==IMOVP && sourcekind!=Tany && sourcekind!=destkind && !(p->sourceroles && in->dm==Afp && typedroottemporary(&in->d,sourcekind))) return typedfail(p,"reference-class");
	if(binary || branch){
		if(in->mm==Anone){if(branch) return typedfail(p,"missing-comparison");}
		else if(!typedoperand(in->mm,&in->m,mk,immediate,1,p,&out->middle,&mm,&middlekind)) return 0;
	}else if(in->mm!=Anone) return typedfail(p,"unexpected-middle");
	out->mode=SRC(sm)|DST(dm);
	if(mm!=AXXX) out->mode|=mm==AIMM?AXIMM:mm==AFP?AXINF:AXINM;
	return 1;
}

static int typedwmref(Type*,char*,char*);
static int typedwordgeometry(Type*,Dis64Layout*);
/* Closed flattened aggregate ABIs used by Wmlib and Winplace. */
static unsigned int
typedaggregatecontract(Type*t,Dis64Layout*l)
{Decl*d;unsigned int i,c=0;if(t&&t->kind==Tadt)return typedwordgeometry(t,l)?(l->bytes==16?112:96):0;if(!t||t->kind!=Ttuple||!dis64layouttype(t,l))return 0;d=t->ids;
 if(l->count==2&&d&&d->next&&!d->next->next&&l->legacybytes==8&&l->bytes==16){if(d->ty->kind==Tstring&&d->next->ty->kind==Tint)c=80;else if(typedwmref(d->ty,"Draw","Context")&&d->next->ty->kind==Tstring)c=48;else if(typedwmref(d->ty,"Tk","Toplevel")&&d->next->ty->kind==Tchan&&d->next->ty->tof&&d->next->ty->tof->kind==Tstring)c=116;}
 if(l->count==3&&l->legacybytes==12){if(d&&d->next&&d->next->next&&!d->next->next->next&&d->ty->kind==Tstring&&typedwmref(d->next->ty,"Draw","Image")&&d->next->next->ty->kind==Tstring&&l->bytes==24)c=124;else if(!typedwordtriple(t)&&l->bytes==12)c=12;}
 if(l->count==3&&l->legacybytes==12&&l->bytes==24&&!c){Decl*v;int valid=1;for(v=d;v;v=v->next){Dis64Layout rect={0};if(!v->ty||v->ty->kind!=Tlist||!typedwordgeometry(v->ty->tof,&rect)||rect.bytes!=16)valid=0;dis64layoutfree(&rect);}if(valid)c=72;}
 if(l->count==5&&l->legacybytes==20&&l->bytes==20)c=92;
 if(!c)return 0;for(i=0;i<l->count;i++){Dis64LayoutLeaf*v=l->leaves+i;int root=c==48||c==116||c==72||c==124||(c==80&&!i),kind=c==116?(i?Tchan:Tref):c==72?Tlist:c==48?(i?Tstring:Tref):c==124?(i==1?Tref:Tstring):c==80&&!i?Tstring:Tint;unsigned int off=c==48||c==116||c==72||c==124?8*i:c==80?(i?8:0):4*i;if(v->legacy!=4*i||v->native!=off||v->oldbytes!=4||v->bytes!=(root?8:4)||v->kind!=kind)return 0;}return c;}
static int
typedaggregatereturn(Inst*in,Dis64TypedBody*p,unsigned int pc)
{Dis64Layout l={0};Dis64Layout*space;unsigned int c=p->resultaggregate,i,k,width,off,base,wire=0;long source=0;int actual,ok=0;
 if(!c||in->dm!=Afpind||in->d.reg!=16||in->d.offset<0||in->branch||!typedaggregatecontract(p->resulttype,&l))goto done;
 for(i=0;i<l.count;i++)if(l.leaves[i].legacy==(unsigned int)in->d.offset)break;if(i==l.count)goto done;off=l.leaves[i].native;width=in->op==IMOVP?8:in->op==IMOVW?4:in->op==IMOVM?in->m.offset:in->op==IMOVMP?l.bytes:0;if(!width||width>l.bytes-off)goto done;
 if(in->op==IMOVP||in->op==IMOVW){if(in->mm!=Anone||width!=l.leaves[i].bytes||!typedoperand(in->sm,&in->s,width==8?TypedRoot:Tint,width==4,0,p,&source,&wire,&actual)||(width==8&&actual!=l.leaves[i].kind&&actual!=Tany))goto done;}
 else if(in->op==IMOVM){if(in->mm!=Aimm||(in->sm!=Afp&&in->sm!=Amp)||width%4)goto done;space=in->sm==Afp?&p->frame:&p->data;for(k=0;k<width;k+=4){unsigned int j,n;if(i+k/4>=l.count||l.leaves[i+k/4].kind!=Tint||l.leaves[i+k/4].native!=off+k||!dis64layoutaddress(space,in->s.reg+k,4,0,&n)||(k&&n!=base+k))goto done;for(j=0;j<space->count;j++)if(space->leaves[j].legacy==(unsigned int)in->s.reg+k&&space->leaves[j].kind==Tint)break;if(j==space->count)goto done;if(!k)base=n;}source=base;wire=in->sm==Afp?AFP:AMP;}
 else if(in->op==IMOVMP){Decl*d=in->m.decl;if((c!=48&&c!=116&&c!=72&&c!=124)||off||in->sm!=Afp||in->mm!=Adesc||!d||!d->desc||d->desc->id!=in->m.offset||d->desc->size!=l.legacybytes||d->desc->nmap!=1||d->desc->map[0]!=((c==48||c==116)?0xc0:0xe0))goto done;for(k=0;k<l.count;k++){unsigned int n;if(!dis64layoutaddress(&p->frame,in->s.reg+4*k,4,1,&n)||(k&&n!=base+8*k))goto done;if(!k)base=n;}source=base;wire=AFP;}
 else goto done;p->code[pc]=(Dis64Instruction){in->op,SRC(wire)|((in->op==IMOVM||in->op==IMOVMP)?AXIMM:0)|DST(AFP|AIND),(c<<16)|(width<<8)|off,source,32};p->returns[pc]=77;ok=1;
 done:dis64layoutfree(&l);return ok?1:typedfail(p,"aggregate-return-layout");}

/* Meet all reachable paths: a caller-owned result must be defined before RET. */
static int
typedresults(Dis64TypedBody *p)
{
	unsigned char *state;
	unsigned int *queue,head=0,tail=0,n,pc,edge[2],i,flow,joined;
	int ok=1;
	if(p->resultkind==Tnone) return 1;
	state=allocmem(p->count);memset(state,0,p->count);
	queue=allocmem(((p->resultaggregate?32:p->resulttriple?4:p->resultpair?3:2)*p->count+2)*sizeof(*queue));state[0]=1;queue[tail++]=0;
	if(p->catchpattern){
		if(p->catchtarget>=p->count){free(state);free(queue);return typedfail(p,"catch-result-target");}
		state[p->catchtarget]=1;queue[tail++]=p->catchtarget;
	}
	while(head<tail){
		pc=queue[head++];flow=state[pc];
        if(p->resultaggregate){unsigned int c=p->resultaggregate,n=c==92?5:c==112?4:(c==12||c==72||c==124)?3:2;if(p->returns[pc]==45)flow|=8;if(p->returns[pc]==32||p->returns[pc]==47)flow=(1U<<(n+1))-1;if(p->returns[pc]==78)flow|=p->code[pc].op==IMCALL&&pc&&p->returns[pc-1]==152?(2U<<(p->code[pc-1].middle/8)):c==112?24:2;if(p->returns[pc]==77){unsigned int off=p->code[pc].middle&255,bytes=(p->code[pc].middle>>8)&255,first=c==80?(off?1:0):off/((c==48||c==116||c==72||c==124)?8:4),count=p->code[pc].op==IMOVP?1:bytes/((c==48||c==116||c==72||c==124)?8:4);while(count--)flow|=2U<<(first++);}}
        else if(p->resulttriple){if(p->returns[pc]==2 || p->returns[pc]==4 || p->returns[pc]==8)flow|=p->returns[pc];if(p->returns[pc]==32 || p->returns[pc]==171)flow=15;}
        else if(p->resultpair){if(p->returns[pc]==77)flow=7;if(p->returns[pc]==8)flow|=2;if(p->returns[pc]==16||(p->resultpair>=3&&p->returns[pc]==78))flow|=4;if(p->returns[pc]==32)flow=7;}
        else if(((p->returns[pc]==65 || p->returns[pc]==72) && (p->code[pc].op==IMOVW || p->code[pc].op==IMOVP) && p->code[pc].destination==32 && UDST(p->code[pc].mode)==(AFP|AIND)) || (p->returns[pc] && p->returns[pc]<=32) || ((p->returns[pc]==109 || (p->returns[pc]==111 && p->code[pc].op==IMOVP)) || p->returns[pc]==174 || p->returns[pc]==185 || p->returns[pc]==246 || p->returns[pc]==247 || (p->returns[pc]==158 && p->code[pc].destination==32 && UDST(p->code[pc].mode)==(AFP|AIND)) || ((p->returns[pc]==154 || (p->returns[pc]==153 && !(p->code[pc].middle&0xff0000) && p->code[pc].destination==32)) && UDST(p->code[pc].mode)==(AFP|AIND))))flow=3;
        if(p->returns[pc]==Native64TypedRecordPair)flow|=2;
        if(!p->resultaggregate&&!p->resultpair&&!p->resulttriple&&p->code[pc].destination==32&&
           UDST(p->code[pc].mode)==(AFP|AIND)&&
           (p->returns[pc]==Native64TypedRecordNew||p->returns[pc]==Native64TypedRecord))flow=3;
        n=0;
		if(p->code[pc].op==IRET || p->code[pc].op==IRAISE || p->code[pc].op==IEXIT) continue;
        if(p->code[pc].op==ICASE || p->code[pc].op==ICASEL || p->code[pc].op==IGOTO || p->code[pc].op==ICASEC){
            Dis64TypedTable *t=nil;unsigned int j,target;
            for(j=0;j<p->ntables;j++)if(p->tables[j].pc==pc){t=p->tables+j;break;}
            if(t==nil){ok=typedfail(p,"missing-table-flow");break;}
            for(j=0;j<(unsigned int)t->table->nlab+(t->op!=IGOTO || t->altdefault);j++){
                target=(j<(unsigned int)t->table->nlab?t->table->labs[j].inst:t->table->iwild)->pc-p->first;
                joined=state[target]?state[target]&flow:flow;
                if(joined!=state[target]){state[target]=joined;queue[tail++]=target;}
            }
            continue;
        }
		if(p->code[pc].op==IJMP)edge[n++]=p->code[pc].destination;
		else{
			if(p->code[pc].op==IBEQW || p->code[pc].op==IBNEW || p->code[pc].op==IBLTW || p->code[pc].op==IBLEW || p->code[pc].op==IBGTW || p->code[pc].op==IBGEW ||
			   p->code[pc].op==IBEQL || p->code[pc].op==IBNEL || p->code[pc].op==IBLTL || p->code[pc].op==IBLEL || p->code[pc].op==IBGTL || p->code[pc].op==IBGEL ||
			   p->code[pc].op==IBEQF || p->code[pc].op==IBNEF || p->code[pc].op==IBLTF || p->code[pc].op==IBLEF || p->code[pc].op==IBGTF || p->code[pc].op==IBGEF ||
               p->code[pc].op==IBEQC || p->code[pc].op==IBNEC || p->code[pc].op==IBLTC || p->code[pc].op==IBLEC || p->code[pc].op==IBGTC || p->code[pc].op==IBGEC)edge[n++]=p->code[pc].destination;
			if(pc+1>=p->count){ok=typedfail(p,"result-fallthrough");break;}edge[n++]=pc+1;
		}
		for(i=0;i<n;i++){
			joined=state[edge[i]]?state[edge[i]]&flow:flow;
			if(joined!=state[edge[i]]){state[edge[i]]=joined;queue[tail++]=edge[i];}
		}
	}
	if(ok)for(pc=0;pc<p->count;pc++)if(p->code[pc].op==IRET && (p->resultaggregate?(state[pc]!=0 && state[pc]!=((1U<<((p->resultaggregate==92?5:p->resultaggregate==112?4:(p->resultaggregate==12||p->resultaggregate==72||p->resultaggregate==124)?3:2)+1))-1)):p->resulttriple?(state[pc]!=0 && state[pc]!=15):p->resultpair?(state[pc]!=0 && state[pc]!=7):state[pc]==1)){p->badpc=p->first+pc;ok=typedfail(p,"undefined-result");break;}
	free(state);free(queue);return ok;
}



static int
typedlower(Decl *fn, Decl *globals, Inst *input, unsigned int count, unsigned int stop, Dis64TypedBody *p,int loads)
{
	Inst *in;
	unsigned int at=0;
    p->sourceroles=loads;
	if(p->code!=nil || p->reason!=nil) return typedfail(p,"nonempty-output");
	if(fn!=nil && fn->pc!=nil && fn->pc->pc>=0) p->first=fn->pc->pc;
	if(fn==nil || fn->ty==nil || fn->pc==nil || fn->ty->kind!=Tfn || fn->ty->varargs ||
	   (fn->handler && !(loads && typedreportcatches)) || fn->ty->polys!=nil || fn->ty->tof==nil || fn->pc->pc<0)
		return typedfail(p,"function-result-or-polymorphism");
	p->resultkind=fn->ty->tof->kind;p->resulttype=fn->ty->tof;
	switch(p->resultkind){
	case Tnone:break;
	case Tbyte:p->resultbytes=1;break;
	case Tint:case Tfix:p->resultbytes=4;break;
	case Tbig:case Treal:case Tstring:case Tref:case Tarray:case Tlist:case Tchan:case Tmodule:p->resultbytes=8;break;
    case Tadt:{Dis64Layout layout={0};int ok=loads && typedwordgeometry(fn->ty->tof,&layout);if(ok){p->resultgeometry=p->resultbytes=layout.bytes;for(in=input;in;in=in->next)if(in->pc>=(int)p->first&&in->pc<(int)stop&&(in->op==IMOVW||(in->op==IMOVM&&(unsigned int)in->m.offset<layout.bytes))&&in->dm==Afpind&&in->d.reg==16){p->resultaggregate=layout.bytes==16?112:96;break;}}dis64layoutfree(&layout);if(!ok)return typedfail(p,"aggregate-result");break;}
	case Ttuple:{Dis64Layout l={0};if(loads&&(p->resultaggregate=typedaggregatecontract(fn->ty->tof,&l)))p->resultbytes=l.bytes;dis64layoutfree(&l);if(p->resultaggregate)break;}if(loads && typedwordtriple(fn->ty->tof)){p->resultbytes=12;p->resulttriple=2;}else if(loads && typedchanneltriple(fn->ty->tof)){p->resultbytes=24;p->resulttriple=1;}else if(loads && typedpairtype(fn->ty->tof)){p->resultbytes=16;p->resultpair=typedpairtype(fn->ty->tof);}else return typedfail(p,"aggregate-result");break;
	default:return typedfail(p,"aggregate-result");
	}
	p->first=fn->pc->pc;
	if(count>TypedInstructions || stop>count || stop<=p->first) return typedfail(p,"code-limit");
	if(!dis64layoutstorage(fn->ty->ids,fn->locals,fn->desc,1,&p->frame) ||
	   !dis64layoutstorage(globals,nil,descriptors,0,&p->data)) return typedfail(p,"storage-layout");
	p->count=stop-p->first; p->code=allocmem(p->count*sizeof(*p->code));
    p->tables=allocmem((p->count<256?p->count:256)*sizeof(*p->tables));
	p->returns=allocmem(p->count);memset(p->returns,0,p->count);
	for(in=input;in!=nil;in=in->next){
		if(in->op==INOOP || in->pc<p->first || in->pc>=stop) continue;
		p->badpc=in->pc;
		if(at>=p->count || in->pc!=p->first+at) return typedfail(p,"instruction-order");
		if(in->op==IRAISE && loads){if(!typedraise(in,p,p->code+at))return 0;}
		else if(!typedinst(in,stop,p,p->code+at,p->returns+at)) return 0;
		at++;
	}
	if(at!=p->count) return typedfail(p,"instruction-count");
	return fn->handler?1:typedresults(p);
}

int
dis64typedlower(Decl *fn,Decl *globals,Inst *input,unsigned int count,unsigned int stop,Dis64TypedBody *p)
{
 return typedlower(fn,globals,input,count,stop,p,0);
}

/* Canonical forwarding calls: one prepared frame, all declared arguments,
 * inherited result address, CALL and RET. No branch may enter setup. */
static int
typedtail(Decl *fn,Decl *globals,Decl **functions,int nf,Inst *input,unsigned int count,unsigned int stop,Dis64TypedBody *p,Decl **active,unsigned int depth,int local)
{
    Inst *in,*frame,*call,*binding,*last;
    Decl *callee=nil,*arg;
    Dis64TypedBody child={0};
    unsigned int i,n=0,index=0,sm,dm,offset,width,end;
    int kind,actual,root,op,ok=0;
    long source;
    for(i=0;i<depth;i++)if(active[i]==fn)return typedfail(p,"tail-cycle");
    if(depth==64 || nf<1 || nf>4096)return typedfail(p,"tail-limit");
    active[depth]=fn;
    if(dis64typedlower(fn,globals,input,count,stop,p))return 1;
    if(p->reason==nil || strcmp(p->reason,"unsupported-opcode") || p->badpc!=p->first)return 0;
    frame=nil;call=nil;binding=nil;last=nil;
    for(in=input;in!=nil;in=in->next){
        if(in->op==INOOP || in->pc<p->first || in->pc>=stop)continue;
        if(n==0)frame=in;
        if(!local && in->pc==(int)stop-3)binding=in;
        if(!local && in->pc==(int)stop-2)call=in;
        if(in->pc==(int)stop-1)last=in;
        n++;
    }
    if(local && frame!=nil){
        binding=frame;
        for(in=frame->next;in!=nil && in->pc<(int)stop;in=in->next){
            if(in->op==INOOP)continue;
            if(in->op==ICALL){call=in;break;}
            if(in->dm!=Afpind)break;
            binding=in;
        }
    }
    if(frame==nil || frame->op!=IFRAME || frame->sm!=Adesc || frame->mm!=Anone || frame->dm!=Afp || frame->branch!=nil ||
       frame->d.reg<32 || frame->d.reg>fn->desc->size-4 ||
       call==nil || call->op!=ICALL || call->sm!=Afp || call->s.reg!=frame->d.reg || call->mm!=Anone || call->dm!=Apc ||
       last==nil || last->op!=IRET || last->sm!=Anone || last->mm!=Anone || last->dm!=Anone || last->branch!=nil ||
       binding==nil || n!=p->count || p->resultkind==Tnone)return 0;
    p->reason=nil;
    for(i=0;i<(unsigned int)nf;i++)if(functions[i]!=nil && functions[i]->pc!=nil && functions[i]->pc->pc==call->d.offset){
        if(callee!=nil)return typedfail(p,"tail-target");callee=functions[i];
    }
    if(callee==nil || callee->desc==nil || callee->desc->id!=frame->s.offset ||
       callee->ty==nil || callee->ty->tof==nil || callee->ty->tof->kind!=p->resultkind ||
       (call->branch!=nil && call->branch!=callee->pc))return typedfail(p,"tail-target");
    end=count;
    for(i=0;i<(unsigned int)nf;i++)if(functions[i]->pc->pc>callee->pc->pc && functions[i]->pc->pc<(int)end)end=functions[i]->pc->pc;
    if(!typedtail(callee,globals,functions,nf,input,count,end,&child,active,depth+1,local)){
        typedfail(p,"tail-callee");goto done;
    }
    /* A successful normal callee return defines the inherited caller result.
     * The canonical wrapper has no alternate path to its only RET. */
    memset(p->code,0,p->count*sizeof(*p->code));memset(p->returns,0,p->count);
    p->code[index++]=(Dis64Instruction){IFRAME,SRC(AIMM)|DST(AFP),0,frame->s.offset,40};
    in=frame->next;
    for(arg=callee->ty->ids;arg!=nil;arg=arg->next){
        while(in!=nil && in->op==INOOP)in=in->next;
        kind=arg->ty->kind;root=kind==Tstring || kind==Tref || kind==Tarray || kind==Tlist || kind==Tchan || kind==Tmodule;
        width=kind==Tbyte?1:kind==Tbig || kind==Treal?8:4;
        op=root?IMOVP:kind==Tbyte?IMOVB:kind==Tbig?IMOVL:kind==Treal?IMOVF:IMOVW;
        if((!root && kind!=Tbyte && kind!=Tint && kind!=Tfix && kind!=Tbig && kind!=Treal) ||
           in==nil || in==binding || in->op!=op || in->mm!=Anone || in->dm!=Afpind || in->d.reg!=frame->d.reg ||
           in->d.offset!=arg->offset || in->branch!=nil || index>=(unsigned int)(call->pc-p->first)-1 ||
           !dis64layoutaddress(&child.frame,arg->offset,width,root,&offset) || offset>65535 ||
           !typedoperand(in->sm,&in->s,root?TypedRoot:kind,!root && width<8,0,p,&source,&sm,&actual) ||
           (root && actual!=kind && actual!=Tany)){
            typedfail(p,"tail-argument");goto done;
        }
        dm=AFP|AIND;p->code[index]=(Dis64Instruction){op,SRC(sm)|DST(dm),0,source,offset};p->returns[index++]=64;
        in=in->next;
    }
    while(in!=nil && in->op==INOOP)in=in->next;
    if(local && in==binding && binding->op==ILEA && binding->sm==Afp){
        kind=p->resultkind;root=kind==Tstring || kind==Tref || kind==Tarray || kind==Tlist || kind==Tchan || kind==Tmodule;
        if(binding->mm!=Anone || binding->dm!=Afpind || binding->d.reg!=frame->d.reg || binding->d.offset!=16 || binding->branch!=nil ||
           !typedoperand(Afp,&binding->s,root?TypedRoot:kind,0,0,p,&source,&sm,&actual) ||
           (root && actual!=kind && actual!=Tany) || index!=(unsigned int)(call->pc-p->first)-1){typedfail(p,"local-result");goto done;}
        p->resultoffset=source;p->resultcontract=p->resultbytes | (root?16:0);p->callend=call->pc-p->first;
        p->code[index]=(Dis64Instruction){ILEA,SRC(AFP)|DST(AFP|AIND),0,source,32};p->returns[index++]=128|p->resultcontract;
        p->code[index++]=(Dis64Instruction){ICALL,SRC(AFP)|DST(AIMM),0,40,call->d.offset};
        for(in=call->next;in!=nil && in->pc<(int)stop;in=in->next){
            if(in->op==INOOP)continue;
            if(index>=p->count || in->pc!=(int)(p->first+index)){typedfail(p,"local-instruction-order");goto done;}
            if(!typedinst(in,stop,p,p->code+index,p->returns+index))goto done;
            if(in->branch!=nil && in->branch->pc<=call->pc){typedfail(p,"local-setup-branch");goto done;}
            index++;
        }
        if(index!=p->count){typedfail(p,"local-instruction-count");goto done;}
        if(!typedresults(p))goto done;
        p->tail=2;p->target=callee->pc->pc;p->targetdesc=callee->desc->id;ok=1;goto done;
    }
    if(in!=binding || binding->op!=ILEA || binding->sm!=Afpind || binding->s.reg!=16 || binding->s.offset!=0 ||
       binding->mm!=Anone || binding->dm!=Afpind || binding->d.reg!=frame->d.reg || binding->d.offset!=16 || binding->branch!=nil ||
       index!=p->count-3){typedfail(p,"tail-result");goto done;}
    p->code[index]=(Dis64Instruction){IMOVL,SRC(AFP)|DST(AFP|AIND),0,32,32};p->returns[index++]=64;
    p->code[index++]=(Dis64Instruction){ICALL,SRC(AFP)|DST(AIMM),0,40,call->d.offset};
    p->code[index++]=(Dis64Instruction){IRET,SRC(AXXX)|DST(AXXX),0,0,0};
    p->tail=1;p->target=callee->pc->pc;p->targetdesc=callee->desc->id;ok=1;
done:
    dis64typedfree(&child);return ok;
}

int
dis64typedtail(Decl *fn,Decl *globals,Decl **functions,int nf,Inst *input,unsigned int count,unsigned int stop,Dis64TypedBody *p)
{
    Decl *active[64];
    return typedtail(fn,globals,functions,nf,input,count,stop,p,active,0,0);
}

int
dis64typedlocal(Decl *fn,Decl *globals,Decl **functions,int nf,Inst *input,unsigned int count,unsigned int stop,Dis64TypedBody *p)
{
    Decl *active[64];
    return typedtail(fn,globals,functions,nf,input,count,stop,p,active,0,1);
}

#include "dis64-multi.h"

void
dis64typedfree(Dis64TypedBody *p)
{
	unsigned int i;
	free(p->catchcleanup);
    for(i=0;i<p->nvariadic;i++)dis64layoutfree(&p->variadic[i].frame);free(p->variadic);
	free(p->tables);free(p->recordowners);free(p->contextfields);free(p->external);free(p->sites);free(p->code);free(p->returns); dis64layoutfree(&p->frame); dis64layoutfree(&p->data); memset(p,0,sizeof(*p));
}

static void
typedlayout(Dis64Layout *p)
{
	unsigned int i,j,bits;
	fprint(2,"{\"legacy_bytes\":%ud,\"bytes\":%ud,\"map\":[",p->legacybytes,p->bytes);
	for(i=0;i<(p->bytes+63)/64;i++){
		bits=0;
		for(j=0;j<p->count;j++) if(p->leaves[j].kind>=0 && p->leaves[j].kind!=Tint && p->leaves[j].kind!=Tfix && p->leaves[j].oldbytes==4 && p->leaves[j].bytes==8 && p->leaves[j].native/64==i)
			bits|=0x80>>((p->leaves[j].native/8)%8);
		fprint(2,"%s%ud",i?",":"",bits);
	}
	fprint(2,"],\"leaves\":[");
	for(i=0;i<p->count;i++) fprint(2,"%s[%ud,%ud,%ud,%ud,%d]",i?",":"",p->leaves[i].legacy,p->leaves[i].native,p->leaves[i].oldbytes,p->leaves[i].bytes,p->leaves[i].kind);
	fprint(2,"]}");
}

static void
typedquote(Sym *s)
{
	int i;
	uchar c;
	fprint(2,"\"");
	for(i=0;i<s->len;i++){
		c=s->name[i];
		if(c=='"' || c=='\\') fprint(2,"\\%c",c);
		else if(c<32) fprint(2,"\\u%04x",c);
		else write(2,&c,1);
	}
	fprint(2,"\"");
}

static void
typedexportname(Decl *fn)
{
    Sym *parts[2];int n,j;uchar ch;
    parts[0]=fn->dot!=nil && fn->dot->ty->kind==Tadt?fn->dot->sym:nil;parts[1]=fn->sym;
    fprint(2,"\"");
    for(n=0;n<2;n++)if(parts[n]!=nil){
        for(j=0;j<parts[n]->len;j++){
            ch=parts[n]->name[j];
            if(ch=='"' || ch=='\\')fprint(2,"\\%c",ch);
            else if(ch<32)fprint(2,"\\u%04x",ch);
            else write(2,&ch,1);
        }
        if(n==0)fprint(2,".");
    }
    fprint(2,"\"");
}

/* Preserve every argument, including aggregate and zero-size values. A first
 * scalar/reference read cannot stand in for the whole argument's geometry. */
static void
typedbindings(Decl *decls, int frame)
{
	Decl *d;
	Dis64Layout type;
	unsigned int cursor=frame?64:0,offset,n=0;
	fprint(2,"[");
	for(d=decls;d!=nil;d=d->next){
		if(d->store!=(frame?Darg:Dglobal)) continue;
		memset(&type,0,sizeof(type));
		if(!dis64layouttype(d->ty,&type)) fatal("typed binding differs from checked storage");
		offset=(cursor+type.alignment-1)&~(type.alignment-1);cursor=offset+type.bytes;
		if(frame) fprint(2,"%s[%ud,%ud,%ud,%ud]",n++?",":"",offset,d->ty->kind,type.legacybytes,type.bytes);
		else if(d->sym!=nil && d->sym->len>0 && d->sym->name[0]!='.'){
			fprint(2,"%s[",n++?",":"");typedquote(d->sym);
			fprint(2,",%ud,%ud,%ud,%ud]",offset,type.legacybytes,type.bytes,d->ty->kind);
		}
		dis64layoutfree(&type);
	}
	fprint(2,"]");
}

static void
typedaggregatebindings(Decl*decls)
{
 Decl*d;Dis64Layout layout={0};unsigned int i=0,n=0;
 fprint(2,"[");
 for(d=decls;d;d=d->next,i++)if(d->ty && d->ty->kind==Tadt && (typedwordchans(d->ty,&layout) || (dis64layoutfree(&layout),typedwordgeometry(d->ty,&layout)) || (dis64layoutfree(&layout),typedpointerlayout(d->ty,&layout)))){
  fprint(2,"%s{\"index\":%ud,\"layout\":",n++?",":"",i);typedlayout(&layout);if(d->ty->decl&&d->ty->decl->sym&&!strcmp(d->ty->decl->sym->name,"Chans"))fprint(2,",\"nominal\":\"Draw.Chans\"");if(d->ty->decl && d->ty->decl->sym && !strcmp(d->ty->decl->sym->name,"Pointer"))fprint(2,",\"nominal\":\"Draw.Pointer\"");fprint(2,"}");dis64layoutfree(&layout);
 }else dis64layoutfree(&layout);
 fprint(2,"]");
}

/* Diagnostic hashes must not sign an interface already narrowed by the
 * compiler. Walk readonly with bounded recursion; report unavailable rather
 * than inventing a legacy hash or mutating module linkage to compute one. */
static int
typedsignable(Type *t,Type **active,unsigned int depth,unsigned int *budget)
{
 Decl *d;unsigned int i;
 if(!*budget || depth==64)return 0;
 --*budget;
 if(t==nil)return 1;
 for(i=0;i<depth;i++)if(active[i]==t)return 1;
 active[depth]=t;
 switch(t->kind){
 case Tnone:case Tbig:case Tbyte:case Treal:case Tint:case Tstring:case Tfix:case Tpoly:return 1;
 case Tref:case Tlist:case Tarray:case Tchan:return typedsignable(t->tof,active,depth+1,budget);
 case Tmodule:
  if(!t->tof || !t->tof->linkall)return 0;
  for(d=t->tof->ids;d;d=d->next){if(!*budget)return 0;--*budget;if(!d->tag && !typedsignable(d->ty,active,depth+1,budget))return 0;}
  return 1;
 case Tfn:case Ttuple:case Tadt:case Tadtpick:
  for(d=t->ids;d;d=d->next){if(!*budget)return 0;--*budget;if(d->store==Dlocal || (storespace[d->store] && !typedsignable(d->ty,active,depth+1,budget)))return 0;}
  for(d=t->polys;d;d=d->next){if(!*budget)return 0;--*budget;if(d->store==Dlocal || (storespace[d->store] && !typedsignable(d->ty,active,depth+1,budget)))return 0;}
  for(d=t->tags;d;d=d->next){if(!*budget)return 0;--*budget;if(!typedsignable(d->ty,active,depth+1,budget))return 0;}
  if(t->kind==Tfn)return typedsignable(t->tof,active,depth+1,budget);
  if(t->kind==Tadtpick)return t->decl && t->decl->dot && typedsignable(t->decl->dot->ty,active,depth+1,budget);
  return 1;
 }
 return 0;
}
static void
typedsignature(Decl *fn)
{
 Type *active[64];unsigned int budget=4096;
 if(fn->ty->sig || typedsignable(fn->ty,active,0,&budget))fprint(2,"%ud",(unsigned int)sign(fn));
 else fprint(2,"null,\"signature_available\":false");
}

void
dis64typedreport(Decl *globals, Decl **functions, int nfunctions, Inst *first, long count, Except *catches)
{
	Dis64TypedBody p;
	Decl *fn;
	unsigned int stop,i;
	int n,j;
	char *enabled=getenv("LIMBO_DIS64_TYPED");
	char *tail=getenv("LIMBO_DIS64_TYPED_TAIL");
	char *local=getenv("LIMBO_DIS64_TYPED_LOCAL");
	char *external=getenv("LIMBO_DIS64_TYPED_EXTERNAL");
	char *loads=getenv("LIMBO_DIS64_TYPED_LOAD");
	char *multi=getenv("LIMBO_DIS64_TYPED_MULTI");
	if(enabled==nil || strcmp(enabled,"1")!=0) return;
	if(loads!=nil && !strcmp(loads,"1"))typedreportcatches=catches;
	for(n=0;n<nfunctions;n++){
		fn=functions[n]; stop=count;
		for(j=0;j<nfunctions;j++) if(functions[j]->pc->pc>fn->pc->pc && functions[j]->pc->pc<stop) stop=functions[j]->pc->pc;
		memset(&p,0,sizeof(p));
		if(loads!=nil && strcmp(loads,"1")==0) dis64typedloads(fn,globals,functions,nfunctions,first,count,stop,&p);
		else if(external!=nil && strcmp(external,"1")==0) dis64typedexternal(fn,globals,functions,nfunctions,first,count,stop,&p);
		else if(multi!=nil && strcmp(multi,"1")==0) dis64typedmulti(fn,globals,functions,nfunctions,first,count,stop,&p);
		else if(local!=nil && strcmp(local,"1")==0) dis64typedlocal(fn,globals,functions,nfunctions,first,count,stop,&p);
		else if(tail!=nil && strcmp(tail,"1")==0) dis64typedtail(fn,globals,functions,nfunctions,first,count,stop,&p);
		else dis64typedlower(fn,globals,first,count,stop,&p);
		fprint(2,"DIS64_TYPED {\"schema\":\"%s\",\"name\":",p.tail==5?"inferno-typed-load-v1":p.tail==4?"inferno-typed-external-v1":p.tail==3?"inferno-typed-multi-v1":p.tail==2?"inferno-typed-local-v1":p.tail?"inferno-typed-tail-v1":"inferno-typed-body-v2");typedquote(fn->sym);
		if(p.tail==1 || p.tail==2) fprint(2,",\"tail\":[%ud,%ud]",p.target,p.targetdesc);
		if(p.tail==2) fprint(2,",\"local\":[%ud,%ud,%ud]",p.callend,p.resultoffset,p.resultcontract);
        if(p.nsites && p.reason==nil){
            fprint(2,",\"calls\":[");
            for(i=0;i<p.nsites;i++)fprint(2,"%s[%ud,%ud,%ud,%ud,%ud,%ud,%ud]",i?",":"",p.sites[i].first,p.sites[i].end,p.sites[i].target,p.sites[i].descriptor,p.sites[i].resultoffset,p.sites[i].contract,p.sites[i].inherited);
            fprint(2,"]");
        }
        if(p.nexternal && p.reason==nil){
            fprint(2,",\"externals\":[");
            for(i=0;i<p.nexternal;i++){
                Dis64TypedExternal *e=p.external+i;
                fprint(2,"%s{\"first\":%ud,\"end\":%ud,\"binding\":%ud,\"module\":[%ud,%ud],\"resultoffset\":%ud,\"contract\":%ud,\"inherited\":%ud,\"signature\":%ud,\"name\":",i?",":"",e->first,e->end,e->binding,e->modulemode,e->moduleoffset,e->resultoffset,e->contract,e->inherited,(unsigned int)sign(e->callee));
                typedexportname(e->callee);fprint(2,",\"args\":");typedbindings(e->callee->ty->ids,1);fprint(2,",\"aggregate_args\":");typedaggregatebindings(e->callee->ty->ids);if(e->contract==48)fprint(2,",\"result_pair\":[%d,%d]",e->callee->ty->tof->ids->ty->kind,e->callee->ty->tof->ids->next->ty->kind);if(e->contract==96 || e->contract==112)fprint(2,",\"result_geometry\":%ud",e->contract==96?8:16);if(typedchanneltriple(e->callee->ty->tof))fprint(2,",\"result_channels\":[%d,%d,%d]",Tchan,Tchan,Tchan);fprint(2,"}");
            }
            fprint(2,"]");
        }
        if(p.nvariadic && p.reason==nil){
            fprint(2,",\"variadics\":[");
            for(i=0;i<p.nvariadic;i++){
                Dis64TypedVariadic *v=p.variadic+i;
                fprint(2,"%s{\"first\":%ud,\"end\":%ud,\"binding\":%ud,\"module\":[%ud,%ud],\"resultoffset\":%ud,\"contract\":%ud,\"operation\":%ud,\"count\":%ud,\"signature\":%ud,\"name\":",
                    i?",":"",v->first,v->end,v->binding,v->modulemode,v->moduleoffset,v->resultoffset,v->contract,v->operation,v->count,(unsigned int)sign(v->callee));
                typedexportname(v->callee);fprint(2,",\"inherited\":%ud,\"frame\":",v->inherited);typedlayout(&v->frame);fprint(2,"}");
            }
            fprint(2,"]");
        }
		fprint(2,",\"first\":%ud,\"accepted\":%s",p.first,p.reason==nil?"true":"false");
		if(p.reason!=nil) fprint(2,",\"reason\":\"%s\",\"pc\":%ud",p.reason,p.badpc);
		else{
            if(p.ntables){
                unsigned int k;Dis64TypedTable *t;Label *l;
                fprint(2,",\"tables\":[");
                for(i=0;i<p.ntables;i++){
                    t=p.tables+i;
                    fprint(2,"%s{\"pc\":%ud,\"offset\":%ud,\"op\":%d,\"fallback\":%ld,\"rows\":[",i?",":"",t->pc,t->offset,t->op,t->op==IGOTO?-1L:t->table->iwild->pc-p.first);
                    for(k=0;k<(unsigned int)t->table->nlab;k++){
                        l=t->table->labs+k;
                        if(t->op==ICASEC){
                            fprint(2,"%s[",k?",":"");typedquote(l->start->decl->sym);fprint(2,",");
                            if(l->start==l->stop)fprint(2,"null");else typedquote(l->stop->decl->sym);
                            fprint(2,",%ld]",l->inst->pc-p.first);
                        }else fprint(2,"%s[%lld,%lld,%ld]",k?",":"",(t->op==IGOTO && t->pc && p.returns[t->pc-1]==191)?(vlong)k:l->start->val,(t->op==IGOTO && t->pc && p.returns[t->pc-1]==191)?(vlong)k:l->stop->val,l->inst->pc-p.first);
                    }
                    if(t->altdefault)fprint(2,"%s[%d,%d,%ld]",t->table->nlab?",":"",t->table->nlab,t->table->nlab,t->table->iwild->pc-p.first);
                    fprint(2,"]}");
                }
                fprint(2,"]");
            }
            if((multi!=nil && strcmp(multi,"1")==0) || (external!=nil && strcmp(external,"1")==0) || (loads!=nil && strcmp(loads,"1")==0)){
                fprint(2,",\"signature\":");typedsignature(fn);fprint(2,",\"export_name\":");typedexportname(fn);
            }
            if(p.catchpattern){
                fprint(2,",\"catches\":[{\"first\":%ud,\"end\":%ud,\"target\":%ud,\"slot\":%ud,\"pattern\":",p.catchfirst,p.catchend,p.catchtarget,p.catchslot);
                typedquote(p.catchpattern);fprint(2,",\"cleanup\":[");
                if(p.catchcleanup)for(i=0;i<(p.frame.bytes+63)/64;i++)fprint(2,"%s%ud",i?",":"",(unsigned int)p.catchcleanup[i]);
                fprint(2,"]}]");
            }
			fprint(2,",\"result\":[%d,%ud]",p.resultkind,p.resultbytes);if(p.resultpair && p.resultpair<3)fprint(2,",\"result_pair\":[%d,%d]",p.resulttype->ids->ty->kind,p.resulttype->ids->next->ty->kind);if(p.resultpair>=3)fprint(2,",\"result_mixed\":[%d,%d]",p.resultpair==3?Tint:p.resultpair==4?Tbig:Treal,p.resulttype->ids->next->ty->kind);if(p.resulttriple==2)fprint(2,",\"result_words\":[%d,%d,%d]",Tint,Tint,Tint);if(p.resulttriple==1)fprint(2,",\"result_channels\":[%d,%d,%d]",Tchan,Tchan,Tchan);if(p.resultaggregate)fprint(2,",\"result_aggregate\":%d",p.resultaggregate);if(p.resultgeometry)fprint(2,",\"result_geometry\":%d",p.resultgeometry);
			fprint(2,",\"descriptor\":%d,\"data_descriptor\":%d,\"frame\":",fn->desc->id,descriptors->id);
			typedlayout(&p.frame); fprint(2,",\"data\":"); typedlayout(&p.data);
            if(p.nrecordtypes){
                unsigned int side,n=0;
                fprint(2,",\"record_types\":[");for(i=0;i<p.nrecordtypes;i++){if(i)fprint(2,",");typedrecordtree(p.recordtypes[i]);}fprint(2,"],\"record_owners\":[");
                for(i=0;i<p.count;i++)for(side=0;side<3;side++)if(p.recordowners[3*i+side])fprint(2,"%s[%ud,%ud,%ud]",n++?",":"",i,side,(unsigned int)p.recordowners[3*i+side]-1);
                fprint(2,"]");
            }
			fprint(2,",\"args\":");typedbindings(fn->ty->ids,1);
			fprint(2,",\"code\":[");
			for(i=0;i<p.count;i++) fprint(2,"%s[%ud,%ud,%ld,%ld,%ld,%ud]",i?",":"",p.code[i].op,p.code[i].mode,p.code[i].source,p.code[i].middle,p.code[i].destination,p.resulttriple==2 && p.returns[i]==2?145:p.resulttriple==2 && p.returns[i]==4?146:p.resulttriple==2 && p.returns[i]==8?147:p.resulttriple && p.returns[i]==2?168:p.resulttriple && p.returns[i]==4?169:p.resulttriple && p.returns[i]==8?170:p.resultpair>=3 && p.returns[i]==8?146+p.resultpair:p.resultpair && p.returns[i]==8?40:p.resultpair && p.returns[i]==16?56:p.returns[i]>=32?p.returns[i]:p.returns[i]?(p.resultbytes | (p.resultkind==Tstring || p.resultkind==Tref || p.resultkind==Tlist || p.resultkind==Tarray || p.resultkind==Tchan || p.resultkind==Tmodule?16:0)):0);
			fprint(2,"],\"globals\":");typedbindings(globals,0);
		}
		fprint(2,"}\n"); dis64typedfree(&p);
	}
	typedreportcatches=nil;
}
