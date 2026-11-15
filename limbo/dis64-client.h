/* Included with scalar call-file helpers; trusted final IR -> separately proved bytes. */
#include "dis64-scalar-client.h"
#include "dis64-command.h"
void
dis64clientfile(Decl *entry,Decl *globals,Decl *ldts,Decl **functions,int nf,Inst *instructions,long count,long databytes)
{
 FileStrings literals;Dis64TypedBody body={0};Dis64ScalarClientView *proof=nil;Dis64Binding bindings[64];DisStringPoolRecord names[67+Dis64ManagedLiterals];Dis64Type types[3];Dis64ModuleInfo module={0};
 Dis64Payload sections[5];Decl *path=nil,*nilvalue=nil,*fn,*arg,*imports[64];Dis64Instruction in,copy;Dis64TypedExternal *site;Dis64LayoutLeaf *leaf;
 uchar *runes=nil,*strings=nil,*requests=nil,*signatures=nil,*code=nil,*output=nil,*typedata=nil,*maps=nil;uchar metadata[24];DisStringPoolView pool;
 unsigned int i,j,n=0,np,params[64],used,sizes[5],at=4,total=0,moduleoffset=0,pathoffset=0,niloffset=0,loads=0,tail,bytes,k,pcs[129],command=0,ntypes;
 char *failure="unsupported importing scalar command";
 if(!entry || nf!=1 || functions[0]!=entry || entry->ty->ids || entry->ty->tof->kind!=Tnone || entry->pc->pc ||
    !globals || !filestringscollect(&literals,globals,databytes) || !ldts || ldts->next || !ldts->init || !ldts->init->decl || count>128 || count<4)goto done;
 if(!dis64typedclient(entry,globals,functions,nf,instructions,count,count,&body)){failure=body.reason?body.reason:failure;goto done;}
 if(body.nsites || !body.nexternal || body.frame.bytes>4080 || !filestringsplan(&literals,&body,1))goto done;
 path=literals.decls[literals.path];pathoffset=literals.offsets[literals.path];nilvalue=literals.nilvalue;niloffset=literals.niloffset;
 for(i=0;i<body.frame.count;i++){
  leaf=body.frame.leaves+i;
  if(leaf->kind==Tmodule){if(loads++)goto done;moduleoffset=leaf->native;}
  else if(leaf->kind!=Tint && leaf->kind!=Tbig && leaf->kind!=Treal && leaf->kind!=Tstring && leaf->kind!=Tlist && leaf->kind!=Tany && leaf->kind!=Tref && leaf->kind!=Tchan && leaf->kind!=-1)goto done;
 }
 if(loads!=1)goto done;
 loads=0;
 if(body.count>128)goto done;
 for(i=0;i<body.count;i++){
  pcs[i]=total++;
  if(body.code[i].op==IMCALL)for(j=0;j<body.nexternal;j++)if(body.external[j].end==i && body.external[j].contract)total++;
 }
 pcs[body.count]=total;
 for(fn=ldts->init->decl->ty->ids;fn;fn=fn->next){if(n==64 || fn->offset!=(long)n || fn->ty->kind!=Tfn || fn->ty->varargs || fn->ty->polys || !fn->dot || !fn->dot->sym)goto done;imports[n++]=fn;}
 if(!n || n!=(unsigned int)ldts->init->val)goto done;
 runes=allocmem((67+Dis64ManagedLiterals)*(4+4*1023));strings=allocmem(CallFileBytes);requests=allocmem(CallFileBytes);signatures=allocmem(64*(12+64*4));
 code=allocmem(CallFileBytes);output=allocmem(CallFileBytes);typedata=allocmem(CallFileBytes);maps=allocmem(72);memset(maps,0,72);proof=allocmem(sizeof(*proof));
 if(!callfilename(impdecl->sym,runes,names) || !callfilename(imports[0]->dot->sym,runes+2*(4+4*1023),names+2))goto done;
 {Sym *p=path->init->decl->sym;if(p->len<1 || p->len>1023 || p->name[0]=='$')goto done;
  dis64put32(runes+4+4*1023,p->len);for(i=0;i<(unsigned int)p->len;i++){if(!p->name[i] || (uchar)p->name[i]>127)goto done;dis64put32(runes+4+4*1023+4+4*i,(uchar)p->name[i]);}
  names[1]=(DisStringPoolRecord){runes+4+4*1023,4+4*p->len};
 }
 for(i=0;i<n;i++){
  fn=imports[i];if(strcmp(fn->dot->sym->name,imports[0]->dot->sym->name) || !callfilename(fn->sym,runes+(i+3)*(4+4*1023),names+i+3))goto done;
  if(dis64commandshape(fn)){
   if(n!=1 || strcmp(imports[0]->dot->sym->name,"Command"))goto done;command=1;
   sizes[3]=dis64commandbindingwrite(requests,0,sign(fn));
   bindings[i]=(Dis64Binding){0};bindings[i].signature.data=requests+16;bindings[i].signature.bytes=16;
   if(!dis64commandsignature(&bindings[i].signature))goto done;
  }else{
  np=0;for(arg=fn->ty->ids;arg;arg=arg->next){int kind=callfilekind(arg->ty->kind);if(np==64 || kind<1)goto done;params[np++]=kind;}
  k=callfilekind(fn->ty->tof->kind);if(k>Dis64Real)goto done;
  if(!dis64signaturewrite(signatures+i*(12+64*4),12+64*4,k,params,np,&used))goto done;
  bindings[i]=(Dis64Binding){0};bindings[i].module=2;bindings[i].symbol=i+3;
  if(!dis64signaturecheck(signatures+i*(12+64*4),used,64,&bindings[i].signature))goto done;
  }
 }
 if(!filestringpool(&literals,runes,names,n+3))goto done;
 tail=body.frame.bytes+8;
 for(i=0;i<body.count;i++){
  in=body.code[i];
  if(dis64clientbranch(in.op)){
   if(in.destination<0 || (unsigned long)in.destination>=body.count)goto done;
   in.destination=pcs[in.destination];
   if((in.op==IBEQL || in.op==IBNEL) && (in.mode&ARM)==AXINM){if(!nilvalue || (in.mode!=(SRC(AFP)|AXINM|DST(AIMM)) && in.mode!=(SRC(AFP|AIND)|AXINM|DST(AIMM))) || in.middle!=niloffset)goto done;if(USRC(in.mode)==AFP && in.source==moduleoffset){in.mode=SRC(AMP)|AXINM|DST(AIMM);in.source=0;}in.middle=8;}
  }
  if(in.op==IINDC || in.op==IBEQC || in.op==IBNEC || in.op==IBLTC || in.op==IBLEC || in.op==IBGTC || in.op==IBGEC || in.op==IMOVP || in.op==ILENC || in.op==ILENL || in.op==ICONSP || in.op==IHEADP || in.op==ITAIL){
   if(USRC(in.mode)==AMP){if(!filestringaddress(&literals,in.source,&in.source))goto done;}
  }
  if((in.op==IBEQC || in.op==IBNEC || in.op==IBLTC || in.op==IBLEC || in.op==IBGTC || in.op==IBGEC) && (in.mode&ARM)==AXINM)
   if(!filestringaddress(&literals,in.middle,&in.middle))goto done;
  if(in.op==INEW || in.op==INEWZ)in.source=literals.literals?2:1;
  if(in.op==ILOAD){if(in.mode!=(SRC(AMP)|AXIMM|DST(AFP)) || in.source!=pathoffset || in.destination!=moduleoffset || in.middle)goto done;in.mode=SRC(AMP)|AXIMM|DST(AMP);in.source=16;in.destination=0;loads++;}
  else if(in.op==IMFRAME || in.op==IMCALL){
   for(j=0;j<body.nexternal;j++)if((in.op==IMFRAME?body.external[j].first:body.external[j].end)==i)break;
   if(j==body.nexternal)goto done;site=body.external+j;
   if(site->modulemode!=AFP || site->moduleoffset!=moduleoffset || site->binding>=n || site->inherited)goto done;
   if(in.op==IMFRAME){in.mode=SRC(AMP)|AXIMM|DST(AFP);in.source=0;}else{in.mode=SRC(AFP)|AXIMM|DST(AMP);in.destination=0;}
  }else if(in.op==ILEA){if(body.returns[i]<128 || UDST(in.mode)!=(AFP|AIND))goto done;in.source=tail;}
  else if(in.op==IRET){in.op=IEXIT;}
  if(UDST(in.mode)==(AFP|AIND)){if(body.returns[i]>=64){if(!callfileencode(code,&at,&in,40,in.destination))goto done;}else if(!body.contextfields || !dis64clientencode(code,&at,&in,in.destination,body.contextfields[i],0))goto done;}
  else if(!dis64clientencode(code,&at,&in,0,0,body.contextfields?body.contextfields[i]:0))goto done;
  if(in.op==IMCALL && site->contract){
   k=bindings[site->binding].signature.result;
   copy=(Dis64Instruction){k==Dis64Int?IMOVW:k==Dis64Big?IMOVL:IMOVF,SRC(AFP)|DST(AFP),0,tail,site->resultoffset};
   if(!callfileencode(code,&at,&copy,0,0))goto done;
  }
 }
 if(!loads || total>Dis64ClientCode)goto done;
 dis64put32(code,total);sizes[1]=at;
 for(i=0;i<body.frame.count;i++){
  leaf=body.frame.leaves+i;
  if(leaf->kind==Tstring || leaf->kind==Tlist || leaf->kind==Tany || leaf->kind==Tref || leaf->kind==Tchan)maps[leaf->native/64]|=0x80>>((leaf->native/8)%8);
 }
 types[0]=(Dis64Type){body.frame.bytes+16,8,maps,(body.frame.bytes+79)/64};ntypes=filestringtypes(&literals,types,maps);
 module.profile=1;module.databytes=24+8*literals.literals;module.datatype=1;
 if(!dis64modulewrite(metadata,sizeof(metadata),&module,&sizes[0]) || !dis64typeswrite(typedata,CallFileBytes,types,ntypes,&sizes[2]) ||
    !disstringpoolwrite(strings,CallFileBytes,names,n+3+literals.literals,&sizes[4]) || !disstringpoolcheck(strings,sizes[4],65536,&pool) ||
    (!command && !dis64bindingswrite(requests,CallFileBytes,0,bindings,n,&pool,0,0,64,&sizes[3])))goto done;
 sections[0]=(Dis64Payload){Dis64Module,Dis64Required,metadata,sizes[0]};sections[1]=(Dis64Payload){Dis64Code,Dis64Required,code,sizes[1]};
 sections[2]=(Dis64Payload){Dis64Types,Dis64Required,typedata,sizes[2]};sections[3]=(Dis64Payload){Dis64Imports,Dis64Required,requests,sizes[3]};sections[4]=(Dis64Payload){Dis64Strings,Dis64Required,strings,sizes[4]};
 failure="independent importing-client proof refused output";
 if(!dis64write(output,CallFileBytes,nil,0,sections,5,&bytes))goto done;
 if(!dis64clientcheck(output,bytes,proof))goto done;
 if(Bwrite(bout,output,bytes)!=bytes)goto done;failure=nil;
done:
 dis64typedfree(&body);free(runes);free(strings);free(requests);free(signatures);free(code);free(output);free(typedata);free(maps);free(proof);
 if(failure)error(impdecl->src.start,"Dis64 client: %s",failure);
}
