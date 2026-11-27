#include "dis64-file-strings.h"
/* Selected ordinary Command.init producer; fixed opaque-ref/ListString contract. */
static int
dis64commandparams(Decl *fn)
{
 Decl *a,*b;
 if(!fn || !fn->sym || !fn->ty || fn->ty->kind!=Tfn || fn->ty->varargs || fn->ty->polys)return 0;
 a=fn->ty->ids;b=a?a->next:nil;
 return a && b && !b->next && a->ty->kind==Tref && a->ty->tof->kind==Tadt && b->ty->kind==Tlist && b->ty->tof->kind==Tstring;
}
static int
dis64commandargs(Decl *fn){return dis64commandparams(fn) && fn->ty->tof->kind==Tnone;}
static int
dis64commandhelper(Decl *fn)
{
 Type *t;Decl *arg;unsigned int n=0;
 if(!fn || !fn->sym || !fn->ty || fn->ty->kind!=Tfn || fn->ty->varargs || fn->ty->polys || !fn->ty->tof)return 0;
 arg=fn->ty->ids;
 if(arg && arg->next && arg->ty->kind==Tref && arg->ty->tof->kind==Tadt && arg->next->ty->kind==Tlist && arg->next->ty->tof->kind==Tstring)arg=arg->next->next;
 for(;arg;arg=arg->next){
  t=arg->ty;
  if(n++==8 || !(t->kind==Tbyte || t->kind==Tint || t->kind==Tbig || t->kind==Treal || t->kind==Tstring ||
    (t->kind==Tlist && t->tof->kind==Tstring) || (t->kind==Tref && t->tof->kind==Tadt) ||
    (t->kind==Tarray && (t->tof->kind==Tbyte || t->tof->kind==Tint))))return 0;
 }
 t=fn->ty->tof;
 return t->kind==Tnone || t->kind==Tbyte || t->kind==Tint || t->kind==Tbig || t->kind==Treal || t->kind==Tstring || (t->kind==Tlist && t->tof->kind==Tstring) ||
  (t->kind==Tref && t->tof->kind==Tadt) || (t->kind==Tarray && (t->tof->kind==Tbyte || t->tof->kind==Tint));
}
static int
dis64commandshape(Decl *fn){return dis64commandargs(fn) && !strcmp(fn->sym->name,"init");}
/* Existing Dis indirect-source operand order; selected field proof owns its role. */
static int
dis64clientencode(uchar *out,unsigned int *at,Dis64Instruction *in,long base,long secondary,long field)
{
 long values[4];unsigned int i,n,count=0;
 if(USRC(in->mode)!=(AFP|AIND))return callfileencode(out,at,in,base,secondary);
 if(*at>CallFileBytes-18 || !(((in->op==IMOVP || in->op==IMOVB || in->op==ICVTBW || in->op==IMOVW) && in->mode==(SRC(AFP|AIND)|DST(AFP))) ||
  ((in->op==IBEQL || in->op==IBNEL) && in->mode==(SRC(AFP|AIND)|AXINM|DST(AIMM)))))return 0;
 out[(*at)++]=in->op;out[(*at)++]=in->mode;if(in->mode&ARM)values[count++]=in->middle;
 values[count++]=in->source;values[count++]=field;values[count++]=in->destination;
 for(i=0;i<count;i++){n=disoperand(out+*at,4,values[i]);if(!n)return 0;*at+=n;}return 1;
}
#include "dis64-command-local.h"
int
dis64commandfile(Decl *globals,Decl **functions,int nf,Inst *instructions,long count,long databytes)
{
 FileStrings literals;Dis64TypedBody body={0};Dis64ScalarClientView *proof=nil;Dis64Type types[5];Dis64ModuleInfo module={0};Dis64Payload sections[5];
 DisStringPoolRecord names[4+Dis64ManagedLiterals];DisStringPoolView pool;Decl *fn;Dis64LayoutLeaf *leaf;Dis64Instruction in;
 uchar *code=nil,*output=nil,*typedata=nil,*strings=nil,*runes=nil,*maps=nil;uchar metadata[24],exports[36];
 unsigned int sizes[5],i,at=4,bytes,ntypes;char *failure="unsupported ordinary init library",*detail=nil;
 if(nf<1 || !dis64commandshape(functions[0]))return 0;
 if(nf>1)return dis64commandlocalfile(globals,functions,nf,instructions,count,databytes);
 fn=functions[0];
 if(nf!=1 || fn->pc->pc || count<1 || count>128 || !filestringscollect(&literals,globals,databytes))goto done;
 failure="unsupported init body/layout";
 if(!dis64typedcommand(fn,globals,functions,nf,instructions,count,count,&body) || body.nsites || body.nexternal || body.frame.bytes>4080 || !filestringsplan(&literals,&body,0))goto done;
 failure="unsupported init data/local kinds";
 for(i=0;i<body.frame.count;i++){
  leaf=body.frame.leaves+i;
  if(leaf->kind!=Tbyte && leaf->kind!=Tint && leaf->kind!=Tbig && leaf->kind!=Treal && leaf->kind!=Tref && leaf->kind!=Tchan && leaf->kind!=Tarray && leaf->kind!=Tstring && leaf->kind!=Tlist && leaf->kind!=Tany && leaf->kind!=-1)goto done;
 }
 code=allocmem(CallFileBytes);output=allocmem(CallFileBytes);typedata=allocmem(CallFileBytes);strings=allocmem(CallFileBytes);runes=allocmem((4+Dis64ManagedLiterals)*(4+4*1023));maps=allocmem(72);memset(maps,0,72);proof=allocmem(sizeof(*proof));
 {char *texts[]={"Command","command.dis64","Command","init"};unsigned int j,n;
  for(i=0;i<4;i++){n=strlen(texts[i]);dis64put32(runes+i*(4+4*1023),n);for(j=0;j<n;j++)dis64put32(runes+i*(4+4*1023)+4+4*j,texts[i][j]);names[i]=(DisStringPoolRecord){runes+i*(4+4*1023),4+4*n};}
 }
 if(!callfilename(impdecl->sym,runes,names))goto done;
 if(!filestringpool(&literals,runes,names,4))goto done;
 failure="unsupported init operand encoding";
 for(i=0;i<body.count;i++){
  in=body.code[i];
  if((in.op==IBEQL || in.op==IBNEL) && (in.mode&ARM)==AXINM){if(!literals.nilvalue || in.middle!=literals.niloffset)goto done;in.middle=8;}
  if(USRC(in.mode)==AMP && !filestringaddress(&literals,in.source,&in.source))goto done;
  if((in.op==IADDC || in.op==IBEQC || in.op==IBNEC || in.op==IBLTC || in.op==IBLEC || in.op==IBGTC || in.op==IBGEC) && (in.mode&ARM)==AXINM)
   if(!filestringaddress(&literals,in.middle,&in.middle))goto done;
  if(in.op==INEW || in.op==INEWZ)in.source=literals.literals?2:1;
  if(in.op==INEWAZ)in.middle=(literals.literals?3:2)+(in.middle?literals.bytearrays:0);
  if(!dis64clientencode(code,&at,&in,in.destination,body.contextfields?body.contextfields[i]:0,body.contextfields?body.contextfields[i]:0))goto done;
 }
 dis64put32(code,body.count);sizes[1]=at;
 for(i=0;i<body.frame.count;i++){
  leaf=body.frame.leaves+i;if(leaf->kind==Tarray || leaf->kind==Tref || leaf->kind==Tchan || leaf->kind==Tstring || leaf->kind==Tlist || leaf->kind==Tany)maps[leaf->native/64]|=0x80>>((leaf->native/8)%8);
 }
 types[0]=(Dis64Type){body.frame.bytes+16,8,maps,(body.frame.bytes+79)/64};ntypes=filestringtypes(&literals,types,maps);module.databytes=24+8*literals.literals;module.datatype=1;
 sizes[3]=dis64commandbindingwrite(exports,1,sign(fn));
 if(!dis64modulewrite(metadata,sizeof(metadata),&module,&sizes[0]) || !dis64typeswrite(typedata,CallFileBytes,types,ntypes,&sizes[2]) ||
    !disstringpoolwrite(strings,CallFileBytes,names,4+literals.literals,&sizes[4]) || !disstringpoolcheck(strings,sizes[4],65536,&pool))goto done;
 sections[0]=(Dis64Payload){Dis64Module,Dis64Required,metadata,sizes[0]};sections[1]=(Dis64Payload){Dis64Code,Dis64Required,code,sizes[1]};sections[2]=(Dis64Payload){Dis64Types,Dis64Required,typedata,sizes[2]};
 sections[3]=(Dis64Payload){Dis64Exports,Dis64Required,exports,sizes[3]};sections[4]=(Dis64Payload){Dis64Strings,Dis64Required,strings,sizes[4]};
 failure="independent init proof refused output";
 if(!dis64write(output,CallFileBytes,nil,0,sections,5,&bytes) || !dis64commandcheck(output,bytes,proof) || Bwrite(bout,output,bytes)!=bytes)goto done;failure=nil;
 done:detail=body.reason;dis64typedfree(&body);free(code);free(output);free(typedata);free(strings);free(runes);free(maps);free(proof);
 if(failure)error(impdecl->src.start,"Dis64 command: %s (%s)",failure,detail?detail:"");return 1;
}
