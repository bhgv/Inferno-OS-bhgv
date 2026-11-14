#include "limbo.h"
#include "dis64-typed.h"
#include "dis64-call-chain.h"

/* Existing raw call-file policy, not a new wire profile or public opcode ID. */
enum { CallFileCode=256,CallFileBytes=1048576,CallFileObject=1048576 };
typedef struct CallFileBody CallFileBody;
struct CallFileBody {Decl *fn;Dis64TypedBody body;unsigned int first,count,*pcs,kind;};
typedef struct CallFileProof CallFileProof;
struct CallFileProof {
    Dis64CallInference inference;Dis64CallDecoded decoded[CallFileCode];
    Dis64InitWork scratch[CallFileCode];unsigned int queue[CallFileCode];
    Dis64BoundCall records[Dis64LocalFunctions];Dis64CallImage image;
};

static int
callfilekind(int kind)
{
    return kind==Tnone?Dis64Void:kind==Tint?Dis64Int:kind==Tbig?Dis64Big:kind==Treal?Dis64Real:-1;
}
static int
callfilename(Sym *s,uchar *bytes,DisStringPoolRecord *record)
{
    unsigned int i;
    if(s==nil || s->len<1 || s->len>255)return 0;
    dis64put32(bytes,s->len);
    for(i=0;i<(unsigned int)s->len;i++){if((uchar)s->name[i]>127)return 0;dis64put32(bytes+4+4*i,(uchar)s->name[i]);}
    *record=(DisStringPoolRecord){bytes,4+4*s->len};return 1;
}
static int
callfilestore(Dis64Instruction *in,unsigned int returned)
{
    return returned && returned<32 && in->op!=IMOVW && in->op!=IMOVL && in->op!=IMOVF;
}
static int
callfilebranch(unsigned int op)
{
    switch(op){
    case IJMP:case IBEQW:case IBNEW:case IBLTW:case IBLEW:case IBGTW:case IBGEW:
    case IBEQL:case IBNEL:case IBLTL:case IBLEL:case IBGTL:case IBGEL:
    case IBEQF:case IBNEF:case IBLTF:case IBLEF:case IBGTF:case IBGEF:return 1;
    }return 0;
}
/* Encode the existing base-then-secondary indirect destination framing. */
static int
callfileencode(uchar *out,unsigned int *at,Dis64Instruction *in,long base,long secondary)
{
    long values[4];unsigned int i,n=0,used;
    if(*at>CallFileBytes-18)return 0;
    out[(*at)++]=in->op;out[(*at)++]=in->mode;
    if(in->mode&ARM)values[n++]=in->middle;
    if(USRC(in->mode)!=AXXX)values[n++]=in->source;
    if(UDST(in->mode)!=(AFP|AIND)){if(UDST(in->mode)!=AXXX)values[n++]=in->destination;}
    else {values[n++]=base;values[n++]=secondary;}
    for(i=0;i<n;i++){used=disoperand(out+*at,4,values[i]);if(!used)return 0;*at+=used;}
    return 1;
}

void
dis64callsfile(Decl *entry,Decl *globals,Decl **functions,int nf,Inst *instructions,long count)
{
    CallFileBody *b=nil,*f;CallFileProof *proof=nil;Dis64Type *types=nil;Dis64ModuleInfo module={0};Dis64Payload sections[5];
    DisStringPoolRecord names[Dis64LocalFunctions+1];Dis64Instruction in,copy;Dis64TypedSite *site;
    Decl *fn,*arg,*export;uchar *maps=nil,*runes=nil,*strings=nil,*typedata=nil,*code=nil,*exports=nil,*image=nil;
    unsigned int i,j,k,n=0,stop,total=0,at,used,records,parameters[Dis64LocalParameters],np,signaturebytes,exportat=4;
    unsigned int bytes[5]={0},kind,tail,base,secondary;int root=-1,found;char *failure="unsupported source shape for scalar call file";
    if(entry==nil || globals!=nil || nf<2 || nf>Dis64LocalFunctions || count<1 || count>CallFileCode)goto done;
    b=allocmem(nf*sizeof(*b));memset(b,0,nf*sizeof(*b));
    for(i=0;i<(unsigned int)nf;i++)if(functions[i]==entry)root=i;
    if(root<0)goto done;
    b[0].fn=entry;for(i=0;i<(unsigned int)nf;i++)if((int)i!=root)b[++n].fn=functions[i];
    /* The existing catalog covers all functions and exposes only declared exports. */
    for(i=0;i<(unsigned int)nf;i++){
        f=b+i;fn=f->fn;found=0;
        for(export=impdecl->ty->tof->ids;export!=nil;export=export->next)
            if(export->store==Dfn && export->pc==fn->pc && export->sym==fn->sym)found++;
        if(found!=1 || (fn->dot!=nil && fn->dot->ty->kind==Tadt) || fn->ty->kind!=Tfn ||
           (f->kind=callfilekind(fn->ty->tof->kind))>(unsigned int)Dis64Real)goto done;
        np=0;for(arg=fn->ty->ids;arg!=nil;arg=arg->next)
            if(++np>Dis64LocalParameters || callfilekind(arg->ty->kind)<1)goto done;
        if(i==0 && (np || f->kind!=Dis64Void))goto done;
        stop=count;for(j=0;j<(unsigned int)nf;j++)if(functions[j]->pc->pc>fn->pc->pc && (unsigned int)functions[j]->pc->pc<stop)stop=functions[j]->pc->pc;
        if(!dis64typedmulti(fn,globals,functions,nf,instructions,count,stop,&f->body) || f->body.nexternal ||
           f->body.frame.bytes>CallFileObject-16 || f->body.data.bytes)goto done;
        for(j=0;j<f->body.frame.count;j++)if(f->body.frame.leaves[j].kind!=Tint && f->body.frame.leaves[j].kind!=Tbig &&
           f->body.frame.leaves[j].kind!=Treal && f->body.frame.leaves[j].kind!=-1)goto done;
        f->pcs=allocmem((f->body.count+1)*sizeof(*f->pcs));f->first=total;
        for(j=0;j<f->body.count;j++){
            f->pcs[j]=total++;in=f->body.code[j];
            if((i==0 || f->body.nsites) && callfilebranch(in.op))goto done;
            if(callfilestore(&in,f->body.returns[j]))total++;
            if(in.op==ICALL){
                for(k=0;k<f->body.nsites;k++)if(f->body.sites[k].end==j)break;
                if(k==f->body.nsites)goto done;
                if(f->body.sites[k].contract)total+=f->body.sites[k].inherited?2:1;
            }
            if(total>CallFileCode)goto done;
        }
        f->pcs[j]=total;f->count=total-f->first;
    }
    if(!b[0].body.nsites)goto done;
    types=allocmem(nf*sizeof(*types));maps=allocmem(nf*((CallFileObject+63)/64));memset(maps,0,nf*((CallFileObject+63)/64));
    runes=allocmem((nf+1)*(4+4*255));strings=allocmem(CallFileBytes);typedata=allocmem(CallFileBytes);
    code=allocmem(CallFileBytes);exports=allocmem(CallFileBytes);image=allocmem(CallFileBytes);proof=allocmem(sizeof(*proof));
    if(!callfilename(impdecl->sym,runes,names))goto done;
    at=4;
    for(i=0;i<(unsigned int)nf;i++){
        f=b+i;tail=f->body.frame.bytes+8;types[i]=(Dis64Type){f->body.frame.bytes+16,8,maps+i*((CallFileObject+63)/64),(f->body.frame.bytes+79)/64};
        if(!callfilename(f->fn->sym,runes+(i+1)*(4+4*255),names+i+1))goto done;
        np=0;for(arg=f->fn->ty->ids;arg!=nil;arg=arg->next)parameters[np++]=callfilekind(arg->ty->kind);
        signaturebytes=12+4*np;
        dis64put32(exports+exportat,i+1);dis64put32(exports+exportat+4,f->first);dis64put32(exports+exportat+8,i);
        dis64put32(exports+exportat+12,signaturebytes);
        if(!dis64signaturewrite(exports+exportat+16,signaturebytes,f->kind,parameters,np,&used))goto done;
        exportat+=16+signaturebytes;
        for(j=0;j<f->body.count;j++){
            in=f->body.code[j];base=secondary=0;
            if(in.op==IFRAME || in.op==ICALL){
                for(k=0;k<f->body.nsites;k++)if((in.op==IFRAME?f->body.sites[k].first:f->body.sites[k].end)==j)break;
                if(k==f->body.nsites)goto done;site=f->body.sites+k;
                for(k=1;k<(unsigned int)nf;k++)if(b[k].body.first==site->target)break;
                if(k==(unsigned int)nf)goto done;
                if(in.op==IFRAME)in.source=k;else in.destination=b[k].first;
            }
            if(callfilebranch(in.op)){
                if(in.destination<0 || (unsigned long)in.destination>=f->body.count)goto done;
                in.destination=f->pcs[in.destination];
            }
            for(k=0;k<f->body.nsites;k++)if(f->body.sites[k].inherited && f->body.sites[k].end==j+1){in.op=ILEA;in.source=tail;break;}
            if(UDST(in.mode)==(AFP|AIND)){
                if(f->body.returns[j]>=64){base=40;secondary=in.destination;if(in.op==ILEA)in.source=tail;}
                else {base=32;secondary=0;}
            }
            if(i==0 && in.op==IRET)in.op=IEXIT;
            if(callfilestore(&in,f->body.returns[j])){
                in.mode=(in.mode&~7U)|DST(AFP);in.destination=f->body.frame.bytes;
                if(!callfileencode(code,&at,&in,0,0))goto done;
                kind=f->kind;copy=(Dis64Instruction){kind==Dis64Big?IMOVL:kind==Dis64Real?IMOVF:IMOVW,SRC(AFP)|DST(AFP|AIND),0,f->body.frame.bytes,32};
                if(!callfileencode(code,&at,&copy,32,0))goto done;
            }else if(!callfileencode(code,&at,&in,base,secondary))goto done;
            if(in.op==ICALL){
                for(k=0;k<f->body.nsites;k++)if(f->body.sites[k].end==j)break;site=f->body.sites+k;
                if(site->contract){
                    copy=(Dis64Instruction){IMOVW,SRC(AFP)|DST(AFP),0,tail,site->inherited?f->body.frame.bytes:site->resultoffset};
                    /* Eight-byte result kind comes from the callee, never binding opcode. */
                    for(k=1;k<(unsigned int)nf;k++)if(b[k].body.first==site->target)break;
                    copy.op=b[k].kind==Dis64Big?IMOVL:b[k].kind==Dis64Real?IMOVF:IMOVW;
                    if(!callfileencode(code,&at,&copy,0,0))goto done;
                    if(site->inherited){copy.source=f->body.frame.bytes;copy.mode=SRC(AFP)|DST(AFP|AIND);copy.destination=32;
                        if(!callfileencode(code,&at,&copy,32,0))goto done;}
                }
            }
        }
    }
    dis64put32(code,total);dis64put32(exports,nf);module.profile=1;
    if(!dis64modulewrite(image,24,&module,&bytes[0]))goto done;
    /* Reuse no mutable output as a payload when writing the complete container. */
    {uchar metadata[24];memmove(metadata,image,bytes[0]);
        bytes[1]=at;bytes[3]=exportat;
        if(!dis64typeswrite(typedata,CallFileBytes,types,nf,&bytes[2]) ||
           !disstringpoolwrite(strings,CallFileBytes,names,nf+1,&bytes[4]))goto done;
        sections[0]=(Dis64Payload){Dis64Module,Dis64Required,metadata,bytes[0]};sections[1]=(Dis64Payload){Dis64Code,Dis64Required,code,bytes[1]};
        sections[2]=(Dis64Payload){Dis64Types,Dis64Required,typedata,bytes[2]};sections[3]=(Dis64Payload){Dis64Exports,Dis64Required,exports,bytes[3]};
        sections[4]=(Dis64Payload){Dis64Strings,Dis64Required,strings,bytes[4]};
        if(!dis64write(image,CallFileBytes,nil,0,sections,5,&used))goto done;
    }
    failure="raw scalar call file proof refused the complete generated image";
    if(!dis64callchaininfercheck(image,used,CallFileObject,&proof->inference,proof->decoded,proof->scratch,proof->queue,CallFileCode,
       Dis64CallLimit,proof->records,Dis64LocalFunctions,&records,&proof->image) || records!=(unsigned int)nf)goto done;
    if(Bwrite(bout,image,used)!=used){failure="cannot write complete scalar call image";goto done;}
    failure=nil;
done:
    if(b){for(i=0;i<(unsigned int)nf;i++){dis64typedfree(&b[i].body);free(b[i].pcs);}free(b);}
    free(types);free(maps);free(runes);free(strings);free(typedata);free(code);free(exports);free(image);free(proof);
    if(failure)error(entry?entry->src.start:impdecl->src.start,"Dis64 calls: %s",failure);
}
