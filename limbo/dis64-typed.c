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
typedinst(Inst *in, unsigned int stop, Dis64TypedBody *p, Dis64Instruction *out, unsigned char *result)
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
	}else if(in->dm==Afpind){
		if(p->resultkind==Tnone || in->d.reg!=16 || in->d.offset!=0 ||
		   (dk==TypedRoot ? !(p->resultkind==Tstring || p->resultkind==Tref || p->resultkind==Tarray || p->resultkind==Tlist || p->resultkind==Tchan || p->resultkind==Tmodule) :
		    (dk!=p->resultkind && !(dk==Tint && p->resultkind==Tfix))) ||
		   (binary && in->mm==Anone)) return typedfail(p,"result-destination");
		out->destination=32; dm=AFP|AIND; destkind=p->resultkind; *result=1;
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

/* Meet all reachable paths: a caller-owned result must be defined before RET. */
static int
typedresults(Dis64TypedBody *p)
{
	unsigned char *state;
	unsigned int *queue,head=0,tail=0,n,pc,edge[2],i,flow,joined;
	int ok=1;
	if(p->resultkind==Tnone) return 1;
	state=allocmem(p->count);memset(state,0,p->count);
	queue=allocmem((2*p->count+1)*sizeof(*queue));state[0]=1;queue[tail++]=0;
	while(head<tail){
		pc=queue[head++];flow=(p->returns[pc] && p->returns[pc]<=32)?3:state[pc];n=0;
		if(p->code[pc].op==IRET) continue;
		if(p->code[pc].op==IJMP)edge[n++]=p->code[pc].destination;
		else{
			if(p->code[pc].op==IBEQW || p->code[pc].op==IBNEW || p->code[pc].op==IBLTW || p->code[pc].op==IBLEW || p->code[pc].op==IBGTW || p->code[pc].op==IBGEW ||
			   p->code[pc].op==IBEQL || p->code[pc].op==IBNEL || p->code[pc].op==IBLTL || p->code[pc].op==IBLEL || p->code[pc].op==IBGTL || p->code[pc].op==IBGEL ||
			   p->code[pc].op==IBEQF || p->code[pc].op==IBNEF || p->code[pc].op==IBLTF || p->code[pc].op==IBLEF || p->code[pc].op==IBGTF || p->code[pc].op==IBGEF)edge[n++]=p->code[pc].destination;
			if(pc+1>=p->count){ok=typedfail(p,"result-fallthrough");break;}edge[n++]=pc+1;
		}
		for(i=0;i<n;i++){
			joined=state[edge[i]]?state[edge[i]]&flow:flow;
			if(joined!=state[edge[i]]){state[edge[i]]=joined;queue[tail++]=edge[i];}
		}
	}
	if(ok)for(pc=0;pc<p->count;pc++)if(p->code[pc].op==IRET && state[pc]==1){p->badpc=p->first+pc;ok=typedfail(p,"undefined-result");break;}
	free(state);free(queue);return ok;
}

int
dis64typedlower(Decl *fn, Decl *globals, Inst *input, unsigned int count, unsigned int stop, Dis64TypedBody *p)
{
	Inst *in;
	unsigned int at=0;
	if(p->code!=nil || p->reason!=nil) return typedfail(p,"nonempty-output");
	if(fn!=nil && fn->pc!=nil && fn->pc->pc>=0) p->first=fn->pc->pc;
	if(fn==nil || fn->ty==nil || fn->pc==nil || fn->ty->kind!=Tfn || fn->ty->varargs ||
	   fn->handler || fn->ty->polys!=nil || fn->ty->tof==nil || fn->pc->pc<0)
		return typedfail(p,"function-result-or-polymorphism");
	p->resultkind=fn->ty->tof->kind;
	switch(p->resultkind){
	case Tnone:break;
	case Tbyte:p->resultbytes=1;break;
	case Tint:case Tfix:p->resultbytes=4;break;
	case Tbig:case Treal:case Tstring:case Tref:case Tarray:case Tlist:case Tchan:case Tmodule:p->resultbytes=8;break;
	default:return typedfail(p,"aggregate-result");
	}
	p->first=fn->pc->pc;
	if(count>TypedInstructions || stop>count || stop<=p->first) return typedfail(p,"code-limit");
	if(!dis64layoutstorage(fn->ty->ids,fn->locals,fn->desc,1,&p->frame) ||
	   !dis64layoutstorage(globals,nil,descriptors,0,&p->data)) return typedfail(p,"storage-layout");
	p->count=stop-p->first; p->code=allocmem(p->count*sizeof(*p->code));
	p->returns=allocmem(p->count);memset(p->returns,0,p->count);
	for(in=input;in!=nil;in=in->next){
		if(in->op==INOOP || in->pc<p->first || in->pc>=stop) continue;
		p->badpc=in->pc;
		if(at>=p->count || in->pc!=p->first+at) return typedfail(p,"instruction-order");
		if(!typedinst(in,stop,p,p->code+at,p->returns+at)) return 0;
		at++;
	}
	if(at!=p->count) return typedfail(p,"instruction-count");
	return typedresults(p);
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
	free(p->external);free(p->sites);free(p->code);free(p->returns); dis64layoutfree(&p->frame); dis64layoutfree(&p->data); memset(p,0,sizeof(*p));
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

void
dis64typedreport(Decl *globals, Decl **functions, int nfunctions, Inst *first, long count)
{
	Dis64TypedBody p;
	Decl *fn;
	unsigned int stop,i;
	int n,j;
	char *enabled=getenv("LIMBO_DIS64_TYPED");
	char *tail=getenv("LIMBO_DIS64_TYPED_TAIL");
	char *local=getenv("LIMBO_DIS64_TYPED_LOCAL");
	char *external=getenv("LIMBO_DIS64_TYPED_EXTERNAL");
	char *multi=getenv("LIMBO_DIS64_TYPED_MULTI");
	if(enabled==nil || strcmp(enabled,"1")!=0) return;
	for(n=0;n<nfunctions;n++){
		fn=functions[n]; stop=count;
		for(j=0;j<nfunctions;j++) if(functions[j]->pc->pc>fn->pc->pc && functions[j]->pc->pc<stop) stop=functions[j]->pc->pc;
		memset(&p,0,sizeof(p));
		if(external!=nil && strcmp(external,"1")==0) dis64typedexternal(fn,globals,functions,nfunctions,first,count,stop,&p);
		else if(multi!=nil && strcmp(multi,"1")==0) dis64typedmulti(fn,globals,functions,nfunctions,first,count,stop,&p);
		else if(local!=nil && strcmp(local,"1")==0) dis64typedlocal(fn,globals,functions,nfunctions,first,count,stop,&p);
		else if(tail!=nil && strcmp(tail,"1")==0) dis64typedtail(fn,globals,functions,nfunctions,first,count,stop,&p);
		else dis64typedlower(fn,globals,first,count,stop,&p);
		fprint(2,"DIS64_TYPED {\"schema\":\"%s\",\"name\":",p.tail==4?"inferno-typed-external-v1":p.tail==3?"inferno-typed-multi-v1":p.tail==2?"inferno-typed-local-v1":p.tail?"inferno-typed-tail-v1":"inferno-typed-body-v2");typedquote(fn->sym);
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
                typedexportname(e->callee);fprint(2,",\"args\":");typedbindings(e->callee->ty->ids,1);fprint(2,"}");
            }
            fprint(2,"]");
        }
		fprint(2,",\"first\":%ud,\"accepted\":%s",p.first,p.reason==nil?"true":"false");
		if(p.reason!=nil) fprint(2,",\"reason\":\"%s\",\"pc\":%ud",p.reason,p.badpc);
		else{
            if((multi!=nil && strcmp(multi,"1")==0) || (external!=nil && strcmp(external,"1")==0)){
                fprint(2,",\"signature\":%ud,\"export_name\":",(unsigned int)sign(fn));typedexportname(fn);
            }
			fprint(2,",\"result\":[%d,%ud]",p.resultkind,p.resultbytes);
			fprint(2,",\"descriptor\":%d,\"data_descriptor\":%d,\"frame\":",fn->desc->id,descriptors->id);
			typedlayout(&p.frame); fprint(2,",\"data\":"); typedlayout(&p.data);
			fprint(2,",\"args\":");typedbindings(fn->ty->ids,1);
			fprint(2,",\"code\":[");
			for(i=0;i<p.count;i++) fprint(2,"%s[%ud,%ud,%ld,%ld,%ld,%ud]",i?",":"",p.code[i].op,p.code[i].mode,p.code[i].source,p.code[i].middle,p.code[i].destination,p.returns[i]>=32?p.returns[i]:p.returns[i]?(p.resultbytes | (p.resultkind==Tstring || p.resultkind==Tref || p.resultkind==Tlist || p.resultkind==Tarray || p.resultkind==Tchan || p.resultkind==Tmodule?16:0)):0);
			fprint(2,"],\"globals\":");typedbindings(globals,0);
		}
		fprint(2,"}\n"); dis64typedfree(&p);
	}
}
