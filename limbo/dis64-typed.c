#include "limbo.h"
#include "dis64-typed.h"

enum { TypedRoot=-10, TypedInstructions=65536 };

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
	if(leaf==nil || (reference ? (leaf->oldbytes!=4 || leaf->bytes!=8 || leaf->kind<0 ||
	   (kind!=TypedRoot && leaf->kind!=kind && leaf->kind!=Tany)) :
	   (leaf->kind!=kind && !(kind==Tint && leaf->kind==Tfix) &&
	    !((kind==Tbig || kind==Treal) && (leaf->kind==Tbig || leaf->kind==Treal))))) return typedfail(p,"opcode-type");
	if(middle && offset>65535) return typedfail(p,"middle-offset");
	*value=offset; *wire=mode==Afp?AFP:AMP; *actual=leaf->kind; return 1;
}

static int
typedinst(Inst *in, unsigned int stop, Dis64TypedBody *p, Dis64Instruction *out)
{
	unsigned int sm,mm=AXXX,dm;
	int sk=Tint,mk=Tint,dk=Tint,sourcekind=0,destkind=0,middlekind=0;
	int binary=0,branch=0,immediate=1;
	memset(out,0,sizeof(*out)); out->op=in->op;
	switch(in->op){
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
	}else if(!typedoperand(in->dm,&in->d,dk,0,0,p,&out->destination,&dm,&destkind)) return 0;
	if(branch==2){out->mode=SRC(AXXX)|DST(dm);return 1;}
	if(!typedoperand(in->sm,&in->s,sk,immediate,0,p,&out->source,&sm,&sourcekind)) return 0;
	if(in->op==IMOVP && sourcekind!=Tany && sourcekind!=destkind) return typedfail(p,"reference-class");
	if(binary || branch){
		if(in->mm==Anone){if(branch) return typedfail(p,"missing-comparison");}
		else if(!typedoperand(in->mm,&in->m,mk,immediate,1,p,&out->middle,&mm,&middlekind)) return 0;
	}else if(in->mm!=Anone) return typedfail(p,"unexpected-middle");
	out->mode=SRC(sm)|DST(dm);
	if(mm!=AXXX) out->mode|=mm==AIMM?AXIMM:mm==AFP?AXINF:AXINM;
	return 1;
}

int
dis64typedlower(Decl *fn, Decl *globals, Inst *input, unsigned int count, unsigned int stop, Dis64TypedBody *p)
{
	Inst *in;
	unsigned int at=0;
	if(p->code!=nil || p->reason!=nil) return typedfail(p,"nonempty-output");
	if(fn!=nil && fn->pc!=nil && fn->pc->pc>=0) p->first=fn->pc->pc;
	if(fn==nil || fn->ty==nil || fn->pc==nil || fn->ty->kind!=Tfn || fn->ty->varargs ||
	   fn->handler || fn->ty->polys!=nil || fn->ty->tof==nil || fn->ty->tof->kind!=Tnone || fn->pc->pc<0)
		return typedfail(p,"function-result-or-polymorphism");
	p->first=fn->pc->pc;
	if(count>TypedInstructions || stop>count || stop<=p->first) return typedfail(p,"code-limit");
	if(!dis64layoutstorage(fn->ty->ids,fn->locals,fn->desc,1,&p->frame) ||
	   !dis64layoutstorage(globals,nil,descriptors,0,&p->data)) return typedfail(p,"storage-layout");
	p->count=stop-p->first; p->code=allocmem(p->count*sizeof(*p->code));
	for(in=input;in!=nil;in=in->next){
		if(in->op==INOOP || in->pc<p->first || in->pc>=stop) continue;
		p->badpc=in->pc;
		if(at>=p->count || in->pc!=p->first+at) return typedfail(p,"instruction-order");
		if(!typedinst(in,stop,p,p->code+at++)) return 0;
	}
	if(at!=p->count) return typedfail(p,"instruction-count");
	return 1;
}

void
dis64typedfree(Dis64TypedBody *p)
{
	free(p->code); dis64layoutfree(&p->frame); dis64layoutfree(&p->data); memset(p,0,sizeof(*p));
}

static void
typedlayout(Dis64Layout *p)
{
	unsigned int i,j,bits;
	fprint(2,"{\"legacy_bytes\":%ud,\"bytes\":%ud,\"map\":[",p->legacybytes,p->bytes);
	for(i=0;i<(p->bytes+63)/64;i++){
		bits=0;
		for(j=0;j<p->count;j++) if(p->leaves[j].kind>=0 && p->leaves[j].oldbytes==4 && p->leaves[j].bytes==8 && p->leaves[j].native/64==i)
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

void
dis64typedreport(Decl *globals, Decl **functions, int nfunctions, Inst *first, long count)
{
	Dis64TypedBody p;
	Decl *fn;
	unsigned int stop,i;
	int n,j;
	char *enabled=getenv("LIMBO_DIS64_TYPED");
	if(enabled==nil || strcmp(enabled,"1")!=0) return;
	for(n=0;n<nfunctions;n++){
		fn=functions[n]; stop=count;
		for(j=0;j<nfunctions;j++) if(functions[j]->pc->pc>fn->pc->pc && functions[j]->pc->pc<stop) stop=functions[j]->pc->pc;
		memset(&p,0,sizeof(p)); dis64typedlower(fn,globals,first,count,stop,&p);
		fprint(2,"DIS64_TYPED {\"schema\":\"inferno-typed-body-v1\",\"name\":");typedquote(fn->sym);
		fprint(2,",\"first\":%ud,\"accepted\":%s",p.first,p.reason==nil?"true":"false");
		if(p.reason!=nil) fprint(2,",\"reason\":\"%s\",\"pc\":%ud",p.reason,p.badpc);
		else{
			fprint(2,",\"descriptor\":%d,\"data_descriptor\":%d,\"frame\":",fn->desc->id,descriptors->id);
			typedlayout(&p.frame); fprint(2,",\"data\":"); typedlayout(&p.data);
			fprint(2,",\"args\":");typedbindings(fn->ty->ids,1);
			fprint(2,",\"code\":[");
			for(i=0;i<p.count;i++) fprint(2,"%s[%ud,%ud,%ld,%ld,%ld]",i?",":"",p.code[i].op,p.code[i].mode,p.code[i].source,p.code[i].middle,p.code[i].destination);
			fprint(2,"],\"globals\":");typedbindings(globals,0);
		}
		fprint(2,"}\n"); dis64typedfree(&p);
	}
}
