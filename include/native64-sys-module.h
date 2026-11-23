#ifndef INFERNO_NATIVE64_SYS_MODULE_H
#define INFERNO_NATIVE64_SYS_MODULE_H
/* Explicit trusted source builder. Include after the coherent Sys gateways and
 * native64-typed-module.h. No serialized variadic records or registry expansion. */
typedef struct Native64ModuleVariadic Native64ModuleVariadic;
struct Native64ModuleVariadic {
    unsigned int function,first,end,binding,modulemode,moduleoffset,resultoffset,contract,operation,count,bytes;
    const uchar *map;unsigned int mapbytes;
    const Native64TailSlot *tail; /* nil retains the earlier all-String tail. */
    unsigned int inherited;
};

static int
native64sysmoduleblocked(unsigned int pc,const Native64ModuleVariadic *v)
{
    return pc>v->first && pc<=v->end;
}

static int
native64sysmodulecheck(const Native64ModuleSpec*s,const Native64ModuleVariadic*v,unsigned int n)
{
    unsigned int code,links,scratch,imports,i,j,k,at,bit,total=0,fields,seen,sm,bytes,fixed,kind,reference,store,width,fd=~0U,lastroot=~0U;
    const Native64ModuleFunction*f;const Native64TypedStep*step;const uchar*map;Inst probe;
    if(!n || n>256 || v==nil || v==H || !native64modulecheck(s,&code,&links,&scratch,&imports))return 0;
    for(i=0;i<n;i++){
        const Native64ModuleVariadic*p=v+i;
        if(p->function>=s->nfunctions || (p->operation!=Native64SysSprint && p->operation!=Native64SysAprint && p->operation!=Native64SysPrint && p->operation!=Native64SysFprint) ||
           p->count>256 || p->count>4096-total || p->contract!=(p->operation<Native64SysPrint?24:4) || p->binding>32767 ||
           (p->modulemode!=AFP && p->modulemode!=AMP))return 0;
        total+=p->count;fixed=p->operation!=Native64SysFprint?1:2;fields=p->count+fixed;f=s->functions+p->function;
        if(p->bytes!=64+8*fields || !native64modulemap(p->bytes,p->map,p->mapbytes,1) ||
           p->first>=f->count || p->end>=f->count-1 || p->end<p->first+fields+2)return 0;
        if(p->tail==H || (p->tail && (uintptr)p->tail>(uintptr)-1-p->count*sizeof(*p->tail)))return 0;
        for(j=0;j<p->count;j++)if(p->tail){const Native64TailSlot*t=p->tail+j;
            if((t->kind!=Native64TailInt && t->kind!=Native64TailFix && t->kind!=Native64TailBig && t->kind!=Native64TailReal && t->kind!=Native64TailRef) ||
               t->offset!=8*j || t->bytes!=8 || (t->kind==Native64TailRef?t->type!=&Tstring:t->type!=nil))return 0;
        }
        for(at=0;at<p->bytes;at+=8){bit=at/8;
            reference=at>=64 && (at<64+8*fixed || !p->tail || p->tail[(at-64)/8-fixed].kind==Native64TailRef);
            if(!!(p->map[bit/8]&(128>>(bit%8)))!=reference)return 0;
        }
        for(j=0;j<i;j++)if(v[j].function==p->function && !(p->end<v[j].first || v[j].end<p->first))return 0;
        for(j=0;j<f->nsites;j++)if(!(p->end<f->sites[j].first || f->sites[j].end<p->first))return 0;
        bytes=p->modulemode==AFP?f->bytes:s->databytes;map=p->modulemode==AFP?f->map:s->datamap;bit=p->moduleoffset/8;
        if(p->moduleoffset%8 || (p->modulemode==AFP && p->moduleoffset<64) || p->moduleoffset>bytes || 8>bytes-p->moduleoffset || !(map[bit/8]&(128>>(bit%8))))return 0;
        step=f->code+p->first;
        if(step->op!=IFRAME || step->mode!=(SRC(AIMM)|DST(AFP)) || step->m || step->d!=40 || step->returned)return 0;
        step=f->code+p->end;
        if(step->op!=IMCALL || step->mode!=(SRC(AFP)|AXIMM|DST(p->modulemode)) || step->m!=p->binding || step->s!=40 || step->d!=p->moduleoffset || step->returned!=(p->inherited?(f->resultcontract==48||f->resultcontract==80||f->resultcontract==124?78:32):0))return 0;
        step=f->code+p->end-1;
        bytes=p->contract==24?8:4;bit=p->resultoffset/8;
        if(p->inherited>1 || step->op!=(p->inherited?IMOVL:ILEA) || step->mode!=(SRC(AFP)|DST(AFP|AIND)) || step->m!=(p->inherited?p->resultoffset-32:0) ||
           step->s!=(p->inherited?32:p->resultoffset) || step->d!=32 || step->returned!=(128|p->contract))return 0;
        if(p->inherited){if(!native64sysresultshape((f->resultcontract==48||f->resultcontract==80||f->resultcontract==124)?f->resultcontract:0,p->resultoffset,p->operation,1))return 0;}
        else if(p->resultoffset<64 || p->resultoffset%bytes || p->resultoffset>f->bytes || bytes>f->bytes-p->resultoffset ||
                !!(f->map[bit/8]&(128>>(bit%8)))!=(p->contract==24))return 0;
        seen=0;fd=~0U;lastroot=~0U;
        for(at=p->first+1;at<p->end-1;at++){
            step=f->code+at;sm=USRC(step->mode);
            if(step->returned==Native64TypedRecord){
                const Native64TypedStep *word=step+1,*clear=step+2;
                if(seen<fixed||seen>=fields||!p->tail||p->tail[seen-fixed].kind!=Native64TailInt||
                   at+2>=p->end-1||step->op!=IMOVP||!(sm&AIND)||UDST(step->mode)!=AFP||
                   !native64typedrecord(step,f->bytes,s->databytes)||
                   word->op!=ICVTWL||USRC(word->mode)!=(AFP|AIND)||(word->s&65535)!=step->d||word->d!=64+8*seen||
                   !native64recordarglayout(word,f->bytes,s->databytes,p->bytes,p->map,p->mapbytes)||
                   clear->returned!=223||clear->op!=IMOVP||clear->mode!=(SRC(AMP)|DST(AFP))||clear->m||clear->d!=step->d||
                   !native64typedrootoperand(AMP,clear->s,f->bytes,s->databytes)||
                   !native64rawarraymap(s,f,AMP,clear->s,1)||!native64modulereadonlynil(s,clear->s))return 0;
                at+=2;seen++;lastroot=~0U;continue;
            }
            if(step->returned==Native64TypedRecordVararg){
                if(seen>=fields||step->d!=64+8*seen||
                   !native64recordarglayout(step,f->bytes,s->databytes,p->bytes,p->map,p->mapbytes)||
                   !native64rawarraymap(s,f,sm,step->s,1))return 0;
                kind=seen<fixed||!p->tail?Native64TailRef:p->tail[seen-fixed].kind;
                if(step->op==IMOVP?kind!=Native64TailRef:kind!=Native64TailInt)return 0;
                seen++;lastroot=~0U;continue;
            }
            if(step->returned==153){const Native64TypedStep*b=step+1,*z=step+2;unsigned int root;
                if(seen<fixed||seen>=fields||!p->tail||p->tail[seen-fixed].kind!=Native64TailRef||at+2>=p->end-1||step->op!=IMOVP||step->mode!=(SRC(AFP|AIND)|DST(AFP))||step->m!=(12L<<24)||((unsigned long)step->s>>16)!=24||!native64typedlibrary(step,f->bytes,s->databytes))return 0;
                root=(step->s&65535)/8;if(!(f->map[root/8]&(128>>(root%8))))return 0;root=step->d/8;if(!(f->map[root/8]&(128>>(root%8))))return 0;
                if(b->op!=IADDC||b->returned!=57||b->mode!=(SRC(AMP)|AXINF|DST(AFP|AIND))||b->m!=step->d||b->d!=64+8*seen||!native64typedrootoperand(AMP,b->s,f->bytes,s->databytes))return 0;root=b->s/8;if(!(s->datamap[root/8]&(128>>(root%8))))return 0;
                if(z->op!=IMOVP||z->returned!=223||z->mode!=(SRC(AMP)|DST(AFP))||z->m||z->d!=step->d||!native64typedrootoperand(AMP,z->s,f->bytes,s->databytes)||!native64modulereadonlynil(s,z->s))return 0;root=z->s/8;if(!(s->datamap[root/8]&(128>>(root%8))))return 0;
                at+=2;seen++;lastroot=~0U;continue;
            }
            if(step->returned==95){
                if(seen<fixed || seen>=fields || !p->tail || p->tail[seen-fixed].kind!=Native64TailInt ||
                   step->op!=ICVTWL || step->mode!=(SRC(sm)|DST(AFP|AIND)) || (sm!=(AFP|AIND) && sm!=(AMP|AIND)) ||
                   (step->m!=32 && step->m!=72 && step->m!=76 && step->m!=80) ||
                   !native64typedrootoperand(sm&~AIND,step->s,f->bytes,s->databytes) || step->d!=64+8*seen)return 0;
                map=(sm&~AIND)==AFP?f->map:s->datamap;bit=(unsigned long)step->s/8;if(!(map[bit/8]&(128>>(bit%8))))return 0;
                lastroot=~0U;seen++;continue;
            }
            if(step->returned==53){if(p->operation!=Native64SysFprint||seen||!native64sysfdarg(step,f->bytes,s->databytes,p->bytes,p->map,p->mapbytes))return 0;seen++;lastroot=~0U;continue;}
            if(step->returned==59){const Native64TypedStep*b,*z;long tmp;unsigned int x;if(p->operation!=Native64SysFprint||seen||at+2>=p->end-1||step->op!=IMOVP||step->mode!=(SRC(AFP|AIND)|DST(AFP))||step->m!=48||!native64typedrootoperand(AFP,step->s,f->bytes,s->databytes)||!native64typedrootoperand(AFP,step->d,f->bytes,s->databytes))return 0;tmp=step->d;b=step+1;z=step+2;
             if(b->returned!=59||b->op!=IMOVP||b->mode!=(SRC(AFP|AIND)|DST(AFP|AIND))||b->m!=40||b->s!=tmp||b->d!=64||z->returned!=59||z->op!=IMOVP||z->mode!=(SRC(AMP)|DST(AFP))||z->m||z->d!=tmp||!native64typedrootoperand(AMP,z->s,f->bytes,s->databytes))return 0;
             for(x=0;x<2;x++){bit=(x?tmp:step->s)/8;if(!(f->map[bit/8]&(128>>(bit%8))))return 0;}bit=z->s/8;if(!(s->datamap[bit/8]&(128>>(bit%8)))||!native64modulereadonlynil(s,z->s))return 0;
             at+=2;seen++;lastroot=~0U;continue;
            }
            if(step->returned==58){if(seen<fixed||seen>=fields||!p->tail||p->tail[seen-fixed].kind!=Native64TailRef||step->op!=IHEADP||step->mode!=(SRC(sm)|DST(AFP|AIND))||step->m||step->d!=64+8*seen||!native64typedrootoperand(sm,step->s,f->bytes,s->databytes))return 0;map=sm==AFP?f->map:s->datamap;bit=step->s/8;if(!(map[bit/8]&(128>>(bit%8))))return 0;seen++;lastroot=~0U;continue;}
            if(step->m)return 0;
            if(step->returned==222){
                if(seen<fixed || seen>=fields || (p->tail && p->tail[seen-fixed].kind!=Native64TailRef) ||
                   step->op!=IMOVP || step->mode!=(SRC(sm)|DST(AFP|AIND)) || (sm!=(AFP|AIND) && sm!=(AMP|AIND)) ||
                   !native64typedrootoperand(sm&~AIND,step->s,f->bytes,s->databytes) || step->d!=64+8*seen)return 0;
                map=(sm&~AIND)==AFP?f->map:s->datamap;bit=step->s/8;if(!(map[bit/8]&(128>>(bit%8))))return 0;
                lastroot=~0U;seen++;
            }else if(step->returned==223){
                if(lastroot==~0U || step->op!=IMOVP || step->mode!=(SRC(AMP)|DST(AFP)) || step->d!=lastroot ||
                   !native64typedrootoperand(AMP,step->s,f->bytes,s->databytes) || !(s->datamap[step->s/64]&(128>>((step->s/8)%8))) ||
                   !native64typedrootoperand(AFP,step->d,f->bytes,s->databytes) || !(f->map[step->d/64]&(128>>((step->d/8)%8))))return 0;
                for(j=0;j<s->nstrings;j++)if(s->strings[j].offset==(unsigned int)step->s)return 0;
                for(j=at+1;j<p->end-1;j++)if(USRC(f->code[j].mode)==AFP && f->code[j].s==step->d)return 0;
                lastroot=~0U;
            }else if(step->returned==64){
                if(seen==fields)return 0;
                kind=seen<fixed || !p->tail?Native64TailRef:p->tail[seen-fixed].kind;
                reference=kind==Native64TailRef;width=kind==Native64TailInt || kind==Native64TailFix?4:8;
                store=reference?IMOVP:kind==Native64TailReal?IMOVF:width==4?ICVTWL:IMOVL;
                if(step->op!=store || step->mode!=(SRC(sm)|DST(AFP|AIND)) || step->d!=64+8*seen)return 0;
                if(sm==AIMM){if(reference || width!=4 || step->s< -536870912L || step->s>536870911L)return 0;}
                else{
                    bytes=sm==AFP?f->bytes:s->databytes;map=sm==AFP?f->map:s->datamap;bit=step->s/8;
                    if((sm!=AFP && sm!=AMP) || step->s<0 || (sm==AFP && step->s<64) || step->s%width ||
                       (unsigned long)step->s>bytes || width>bytes-step->s || !!(map[bit/8]&(128>>(bit%8)))!=reference)return 0;
                }
                lastroot=reference && sm==AFP && (p->operation!=Native64SysFprint || seen!=0)?step->s:~0U;
                if(!seen && p->operation==Native64SysFprint && sm==AFP)fd=step->s;
                seen++;
            }else{
                if(step->op!=IMOVP || sm!=AMP || !native64typedrootoperand(sm,step->s,f->bytes,s->databytes) ||
                   !(s->datamap[(step->s/8)/8]&(128>>((step->s/8)%8))) || step->returned || seen!=1 || p->operation!=Native64SysFprint || fd==~0U || step->mode!=(SRC(AMP)|DST(AFP)) ||
                   step->d!=fd || !native64typedcode(&probe,1,step,1,f->bytes,s->databytes))return 0;
                bit=fd/8;if(!(f->map[bit/8]&(128>>(bit%8))))return 0;
                for(j=0;j<s->nstrings;j++)if(s->strings[j].offset==(unsigned int)step->s)return 0;
                for(j=at+1;j<p->end-1;j++)if(USRC(f->code[j].mode)==AFP && f->code[j].s==(long)fd)return 0;
                fd=~0U;
            }
        }
        if(seen!=fields)return 0;
        for(j=0;j<f->count;j++)if(native64typedbranch(f->code[j].op) && native64sysmoduleblocked(f->code[j].d,p))return 0;
        for(j=0;j<f->ncases;j++){
            const Native64ModuleCase*t=f->cases+j;
            if(t->op!=IGOTO && native64sysmoduleblocked(t->fallback,p))return 0;
            for(k=0;k<t->count;k++)if(native64sysmoduleblocked(t->rows[k].target,p))return 0;
        }
    }
    for(i=0;i<s->nfunctions;i++)for(j=0;j<s->functions[i].count;j++)if(s->functions[i].code[j].returned==53 || s->functions[i].code[j].returned==57 || s->functions[i].code[j].returned==58 || s->functions[i].code[j].returned==59 || s->functions[i].code[j].returned==95 || s->functions[i].code[j].returned==222 || s->functions[i].code[j].returned==223 || s->functions[i].code[j].returned==Native64TypedRecordVararg){
        seen=0;for(k=0;k<n;k++)if(v[k].function==i && v[k].first<j && j<v[k].end-1)seen++;if(seen!=1)return 0;
    }
    for(i=0;i<s->nfunctions;i++){f=s->functions+i;for(j=6;j<f->count;j++)if(f->code[j].returned==195&&native64altaprintprep(s,f,j,f->code+j)){
        seen=0;for(k=0;k<n;k++)if(v[k].function==i&&v[k].first==j-6&&v[k].end==j-2&&v[k].operation==Native64SysAprint&&v[k].count==1&&!v[k].inherited&&v[k].contract==24&&v[k].resultoffset==(unsigned int)f->code[j].s)seen++;if(seen!=1)return 0;
    }}
    return 1;
}

typedef struct Native64SysModuleBuild Native64SysModuleBuild;
struct Native64SysModuleBuild {
    const Native64ModuleSpec *spec;const Native64ModuleVariadic *variadic;unsigned int count;int jit;
    Native64ModuleOwner owner;
    Native64ModuleFunction *functions;
    Native64TypedStep *code;
    Native64SysProgramSpec *programs;
    Native64TailSlot *slots;
};
static int
native64sysmodulemake(void*argument)
{
    Native64SysModuleBuild*b=argument;Native64ModuleSpec spec=*b->spec;Module*m;Type**types;
    unsigned int i,j,k,total=0,pc=0,offset,ordinal,slot=0,checked=0;Inst*out;const Native64TypedStep*step;
    for(i=0;i<spec.nfunctions;i++)total+=spec.functions[i].count;
    b->functions=mallocz(spec.nfunctions*sizeof(*b->functions),0);if(b->functions==nil)error(exNomem);
    b->code=mallocz(total*sizeof(*b->code),0);if(b->code==nil)error(exNomem);
    for(i=0;i<spec.nfunctions;i++){
        b->functions[i]=spec.functions[i];b->functions[i].code=b->code+pc;
        memmove(b->code+pc,spec.functions[i].code,spec.functions[i].count*sizeof(*b->code));
        for(j=0;j<b->count;j++)if(b->variadic[j].function==i){
            const Native64ModuleVariadic*v=b->variadic+j;const Native64ModuleFunction*f=spec.functions+i;unsigned int alt=v->end+2<f->count&&native64altaprintprep(&spec,f,v->end+2,f->code+v->end+2);
            /* Root-preserving inert preparation keeps the ALT island checkable.
             * The unpublished range is replaced by the validated Sys gateway. */
            for(k=v->first;k<=v->end;k++)b->code[pc+k]=alt?(Native64TypedStep){IMOVP,SRC(AFP)|DST(AFP),v->resultoffset,0,v->resultoffset,0}:(Native64TypedStep){IEXIT,SRC(AXXX)|DST(AXXX),0,0,0,0};
        }
        pc+=spec.functions[i].count;
    }
    spec.functions=b->functions;
    if(!native64typedmodule(&spec,0,&b->owner))error(exInval);
    m=b->owner.module;ordinal=m->ntype;
    /* Program gateways require caller/callee in one current extent. Reserve
     * the distinct-function source path plus its largest native callee before
     * publishing the entry, instead of growing at the first variadic FRAME. */
    total=0;pc=0;
    for(i=0;i<b->spec->nfunctions;i++)total+=b->spec->functions[i].bytes;
    for(i=0;i<b->count;i++)if(pc<b->variadic[i].bytes)pc=b->variadic[i].bytes;
    for(i=0;i<b->spec->ntables;i++)for(j=0;j<b->spec->tables[i].count;j++)
        if(pc<b->spec->tables[i].imports[j].bytes)pc=b->spec->tables[i].imports[j].bytes;
    /* FRAME grows the extent when its size exactly equals the free space. */
    if(total>Native64BindFrame || pc>Native64BindFrame-total ||
       OA(Stkext,reg.tos.fu)+8>Native64BindFrame-total-pc)error(exBounds);
    m->ss=OA(Stkext,reg.tos.fu)+total+pc+8;

    types=mallocz((m->ntype+b->count)*sizeof(*types),1);if(types==nil)error(exNomem);
    memmove(types,m->type,m->ntype*sizeof(*types));free(m->type);m->type=types;m->ntype+=b->count;
    b->programs=mallocz(b->count*sizeof(*b->programs),1);if(b->programs==nil)error(exNomem);
    total=0;for(i=0;i<b->count;i++)total+=b->variadic[i].count;
    if(total){b->slots=mallocz(total*sizeof(*b->slots),1);if(b->slots==nil)error(exNomem);}
    for(i=0;i<b->count;i++){
        const Native64ModuleVariadic*v=b->variadic+i;Native64SysProgramSpec*p=b->programs+i;
        const Native64ModuleFunction*f=b->spec->functions+v->function;
        offset=0;for(j=0;j<v->function;j++)offset+=b->spec->functions[j].count;
        m->type[ordinal+i]=dtype(freeheap,v->bytes,(uchar*)v->map,v->mapbytes);if(m->type[ordinal+i]==nil)error(exNomem);
        for(j=v->first;j<=v->end;j++){
            step=f->code+j;out=m->prog+offset+j;memset(out,0,sizeof(*out));out->op=step->op;out->add=step->mode;
            if(j==v->first){out->s.imm=ordinal+i;out->d.ind=40;}
            else if(j==v->end){out->reg=v->binding;out->s.ind=40;out->d.ind=v->moduleoffset;}
            else if(step->returned==Native64TypedRecordVararg){native64recordargcode(out,step);checked=1;}
            else if(step->returned==Native64TypedRecord){native64recordcode(out,step,nil);checked=1;}
            else if(step->returned==153){if(!native64typedcode(out,1,step,1,f->bytes,b->spec->databytes))error(exInval);}
            else if(step->returned==57&&step->op==IADDC){out->s.imm=step->s;out->reg=step->m;out->d.i.f=40;out->d.i.s=step->d;}
            else if(step->returned==58){out->s.ind=step->s;out->d.i.f=40;out->d.i.s=step->d;}
            else if(step->returned==53){out->s.i.f=step->s&65535;out->s.i.s=(unsigned long)step->s>>16;out->d.i.f=40;out->d.i.s=64;}
            else if(step->returned==59){if(USRC(step->mode)==AMP){Native64TypedStep copy=*step;copy.returned=0;if(!native64typedcode(out,1,&copy,1,f->bytes,b->spec->databytes))error(exInval);}else{out->s.i.f=step->s;out->s.i.s=step->m;if(UDST(step->mode)==AFP)out->d.ind=step->d;else{out->d.i.f=40;out->d.i.s=64;}}}
            else if(step->returned==95){out->s.i.f=step->s;out->s.i.s=step->m;out->d.i.f=40;out->d.i.s=step->d;}
            else if(step->returned==222){out->s.i.f=step->s;out->s.i.s=0;out->d.i.f=40;out->d.i.s=step->d;}
            else if(step->returned==223){Native64TypedStep copy=*step;copy.returned=0;if(!native64typedcode(out,1,&copy,1,f->bytes,b->spec->databytes))error(exInval);}
            else if(!step->returned){if(!native64typedcode(out,1,step,1,f->bytes,b->spec->databytes))error(exInval);}
            else{out->s.imm=step->s;out->d.i.f=40;out->d.i.s=step->d;if(j==v->end-1)out->reg=v->contract;}
        }
        p->bindpc=offset+v->end-1;p->callpc=offset+v->end;
        p->frame=(Native64SysFrameSpec){v->operation,m->type[ordinal+i],m->type[v->function+1],v->resultoffset,v->count?b->slots+slot:nil,v->count,8,4096,v->inherited,v->inherited&&(f->resultcontract==48||f->resultcontract==80||f->resultcontract==124)?f->resultcontract:0};
        for(j=0;j<v->count;j++)b->slots[slot++]=v->tail?v->tail[j]:(Native64TailSlot){Native64TailRef,8*j,8,&Tstring};
    }
    if(checked)native64modulecheckedsteps(m,b->spec);
    if(!Sys_setprogram64(m,b->programs,b->count))error(exInval);
    if(b->jit && !compile(m,m->nprog,nil))error(exCompile);
    return 1;
}

/* The base constructor sees only inert EXIT placeholders for the private call
 * ranges. The module stays unpublished; complete native sites replace those
 * placeholders and acquire their owners before any optional JIT is created. */
static int
native64sysmodule(const Native64ModuleSpec*s,const Native64ModuleVariadic*v,unsigned int n,int jit,Native64ModuleOwner*out)
{
    Native64SysModuleBuild b={0};REG saved=R;int ok;
    if(out==nil || out->module || out->rooted || out->self || (jit!=0 && jit!=1) || !native64sysmodulecheck(s,v,n))return 0;
    b.spec=s;b.variadic=v;b.count=n;b.jit=jit;ok=kprotect(native64sysmodulemake,&b);R=saved;
    free(b.functions);free(b.code);free(b.programs);free(b.slots);
    if(!ok){if(b.owner.module)native64typedmoduleclear(&b.owner);R=saved;return 0;}
    *out=b.owner;out->self=out;return 1;
}
#endif
