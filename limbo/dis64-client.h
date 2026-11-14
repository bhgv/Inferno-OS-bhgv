/* Included with scalar call-file helpers; trusted final IR -> separately proved bytes. */
#include "dis64-scalar-client.h"
void
dis64clientfile(Decl *entry,Decl *globals,Decl *ldts,Decl **functions,int nf,Inst *instructions,long count,long databytes)
{
 Dis64TypedBody body={0};Dis64ScalarClientView *proof=nil;Dis64Binding bindings[64];DisStringPoolRecord names[67];Dis64Type types[2];Dis64ModuleInfo module={0};
 Dis64Payload sections[5];Decl *path,*fn,*arg,*imports[64];Dis64Instruction in,copy;Dis64TypedExternal *site;Dis64LayoutLeaf *leaf;
 uchar *runes=nil,*strings=nil,*requests=nil,*signatures=nil,*code=nil,*output=nil,*typedata=nil,*maps=nil;uchar metadata[24];DisStringPoolView pool;
 unsigned int i,j,n=0,np,params[64],used,sizes[5],at=4,total=0,moduleoffset=0,pathoffset=0,loads=0,tail,bytes,k;
 char *failure="unsupported importing scalar command";
 if(!entry || nf!=1 || functions[0]!=entry || entry->ty->ids || entry->ty->tof->kind!=Tnone || entry->pc->pc ||
    !globals || globals->next || databytes!=4 || !ldts || ldts->next || !ldts->init || !ldts->init->decl || count>128 || count<4)goto done;
 path=globals;if(path->ty->kind!=Tstring || !path->init || path->init->op!=Oconst || !path->init->decl || !path->init->decl->sym)goto done;
 if(!dis64typedclient(entry,globals,functions,nf,instructions,count,count,&body) || body.nsites || !body.nexternal || body.frame.bytes>4080)goto done;
 for(i=0;i<body.frame.count;i++){
  leaf=body.frame.leaves+i;
  if(leaf->kind==Tmodule){if(loads++)goto done;moduleoffset=leaf->native;}
  else if(leaf->kind!=Tint && leaf->kind!=Tbig && leaf->kind!=Treal && leaf->kind!=-1)goto done;
 }
 if(loads!=1 || body.data.count!=1 || body.data.leaves[0].kind!=Tstring)goto done;
 pathoffset=body.data.leaves[0].native;loads=0;
 for(fn=ldts->init->decl->ty->ids;fn;fn=fn->next){if(n==64 || fn->offset!=(long)n || fn->ty->kind!=Tfn || fn->ty->varargs || fn->ty->polys || !fn->dot || !fn->dot->sym)goto done;imports[n++]=fn;}
 if(!n || n!=(unsigned int)ldts->init->val)goto done;
 runes=allocmem(67*(4+4*1023));strings=allocmem(CallFileBytes);requests=allocmem(CallFileBytes);signatures=allocmem(64*(12+64*4));
 code=allocmem(CallFileBytes);output=allocmem(CallFileBytes);typedata=allocmem(CallFileBytes);maps=allocmem(65);memset(maps,0,65);proof=allocmem(sizeof(*proof));
 if(!callfilename(impdecl->sym,runes,names) || !callfilename(imports[0]->dot->sym,runes+2*(4+4*1023),names+2))goto done;
 {Sym *p=path->init->decl->sym;if(p->len<1 || p->len>1023 || p->name[0]=='$')goto done;
  dis64put32(runes+4+4*1023,p->len);for(i=0;i<(unsigned int)p->len;i++){if(!p->name[i] || (uchar)p->name[i]>127)goto done;dis64put32(runes+4+4*1023+4+4*i,(uchar)p->name[i]);}
  names[1]=(DisStringPoolRecord){runes+4+4*1023,4+4*p->len};
 }
 for(i=0;i<n;i++){
  fn=imports[i];if(strcmp(fn->dot->sym->name,imports[0]->dot->sym->name) || !callfilename(fn->sym,runes+(i+3)*(4+4*1023),names+i+3))goto done;
  np=0;for(arg=fn->ty->ids;arg;arg=arg->next){int kind=callfilekind(arg->ty->kind);if(np==64 || kind<1)goto done;params[np++]=kind;}
  k=callfilekind(fn->ty->tof->kind);if(k>Dis64Real)goto done;
  if(!dis64signaturewrite(signatures+i*(12+64*4),12+64*4,k,params,np,&used))goto done;
  bindings[i]=(Dis64Binding){0};bindings[i].module=2;bindings[i].symbol=i+3;
  if(!dis64signaturecheck(signatures+i*(12+64*4),used,64,&bindings[i].signature))goto done;
 }
 tail=body.frame.bytes+8;
 for(i=0;i<body.count;i++){
  in=body.code[i];
  if(in.op==ILOAD){if(in.mode!=(SRC(AMP)|AXIMM|DST(AFP)) || in.source!=pathoffset || in.destination!=moduleoffset || in.middle)goto done;in.mode=SRC(AMP)|AXIMM|DST(AMP);in.source=16;in.destination=0;loads++;}
  else if(in.op==IMFRAME || in.op==IMCALL){
   for(j=0;j<body.nexternal;j++)if((in.op==IMFRAME?body.external[j].first:body.external[j].end)==i)break;
   if(j==body.nexternal)goto done;site=body.external+j;
   if(site->modulemode!=AFP || site->moduleoffset!=moduleoffset || site->binding>=n || site->inherited)goto done;
   if(in.op==IMFRAME){in.mode=SRC(AMP)|AXIMM|DST(AFP);in.source=0;}else{in.mode=SRC(AFP)|AXIMM|DST(AMP);in.destination=0;}
  }else if(in.op==ILEA){if(body.returns[i]<128 || UDST(in.mode)!=(AFP|AIND))goto done;in.source=tail;}
  else if(in.op==IRET){if(i!=body.count-1)goto done;in.op=IEXIT;}
  if(UDST(in.mode)==(AFP|AIND)){if(body.returns[i]<64)goto done;if(!callfileencode(code,&at,&in,40,in.destination))goto done;}
  else if(!callfileencode(code,&at,&in,0,0))goto done;
  total++;
  if(in.op==IMCALL && site->contract){
   k=bindings[site->binding].signature.result;
   copy=(Dis64Instruction){k==Dis64Int?IMOVW:k==Dis64Big?IMOVL:IMOVF,SRC(AFP)|DST(AFP),0,tail,site->resultoffset};
   if(!callfileencode(code,&at,&copy,0,0))goto done;total++;
  }
 }
 if(!loads || total>Dis64ClientCode)goto done;
 dis64put32(code,total);sizes[1]=at;maps[64]=0xe0;
 types[0]=(Dis64Type){body.frame.bytes+16,8,maps,(body.frame.bytes+79)/64};types[1]=(Dis64Type){24,8,maps+64,1};
 module.profile=1;module.databytes=24;module.datatype=1;
 if(!dis64modulewrite(metadata,sizeof(metadata),&module,&sizes[0]) || !dis64typeswrite(typedata,CallFileBytes,types,2,&sizes[2]) ||
    !disstringpoolwrite(strings,CallFileBytes,names,n+3,&sizes[4]) || !disstringpoolcheck(strings,sizes[4],65536,&pool) ||
    !dis64bindingswrite(requests,CallFileBytes,0,bindings,n,&pool,0,0,64,&sizes[3]))goto done;
 sections[0]=(Dis64Payload){Dis64Module,Dis64Required,metadata,sizes[0]};sections[1]=(Dis64Payload){Dis64Code,Dis64Required,code,sizes[1]};
 sections[2]=(Dis64Payload){Dis64Types,Dis64Required,typedata,sizes[2]};sections[3]=(Dis64Payload){Dis64Imports,Dis64Required,requests,sizes[3]};sections[4]=(Dis64Payload){Dis64Strings,Dis64Required,strings,sizes[4]};
 if(!dis64write(output,CallFileBytes,nil,0,sections,5,&bytes) || !dis64clientcheck(output,bytes,proof))goto done;
 if(Bwrite(bout,output,bytes)!=bytes)goto done;failure=nil;
done:
 dis64typedfree(&body);free(runes);free(strings);free(requests);free(signatures);free(code);free(output);free(typedata);free(maps);free(proof);
 if(failure)error(impdecl->src.start,"Dis64 client: %s",failure);
}
