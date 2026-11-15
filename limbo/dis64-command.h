/* Selected ordinary Command.init producer; fixed opaque-ref/ListString contract. */
static int
dis64commandshape(Decl *fn)
{
 Decl *a,*b;
 if(!fn || !fn->sym || strcmp(fn->sym->name,"init") || !fn->ty || fn->ty->kind!=Tfn || fn->ty->varargs || fn->ty->polys || fn->ty->tof->kind!=Tnone)return 0;
 a=fn->ty->ids;b=a?a->next:nil;
 return a && b && !b->next && a->ty->kind==Tref && a->ty->tof->kind==Tadt && b->ty->kind==Tlist && b->ty->tof->kind==Tstring;
}
/* Existing Dis indirect-source operand order; selected field proof owns its role. */
static int
dis64clientencode(uchar *out,unsigned int *at,Dis64Instruction *in,long base,long secondary,long field)
{
 long values[4];unsigned int i,n,count=0;
 if(USRC(in->mode)!=(AFP|AIND))return callfileencode(out,at,in,base,secondary);
 if(*at>CallFileBytes-18 || !((in->op==IMOVP && in->mode==(SRC(AFP|AIND)|DST(AFP))) ||
  ((in->op==IBEQL || in->op==IBNEL) && in->mode==(SRC(AFP|AIND)|AXINM|DST(AIMM)))))return 0;
 out[(*at)++]=in->op;out[(*at)++]=in->mode;if(in->mode&ARM)values[count++]=in->middle;
 values[count++]=in->source;values[count++]=field;values[count++]=in->destination;
 for(i=0;i<count;i++){n=disoperand(out+*at,4,values[i]);if(!n)return 0;*at+=n;}return 1;
}
int
dis64commandfile(Decl *globals,Decl **functions,int nf,Inst *instructions,long count,long databytes)
{
 Dis64TypedBody body={0};Dis64ScalarClientView *proof=nil;Dis64Type types[2];Dis64ModuleInfo module={0};Dis64Payload sections[5];
 DisStringPoolRecord names[4];DisStringPoolView pool;Decl *fn,*g,*nilvalue=nil;Dis64LayoutLeaf *leaf;Dis64Instruction in;
 uchar *code=nil,*output=nil,*typedata=nil,*strings=nil,*runes=nil,*maps=nil;uchar metadata[24],exports[36];
 unsigned int sizes[5],i,at=4,bytes,niloffset=0;char *failure="unsupported ordinary init library",*detail=nil;
 if(strcmp(impdecl->sym->name,"Command") || nf<1 || !dis64commandshape(functions[0]))return 0;
 fn=functions[0];
 if(nf!=1 || fn->pc->pc || count<1 || count>128 || databytes!=0 && databytes!=4)goto done;
 for(g=globals;g;g=g->next){if(nilvalue || g->ty->kind!=Tany || g->init || !g->sym || strcmp(g->sym->name,"nil"))goto done;nilvalue=g;}
 failure="unsupported init body/layout";
 if(databytes!=4*(nilvalue!=nil) || !dis64typedcommand(fn,globals,functions,nf,instructions,count,count,&body) || body.nsites || body.nexternal || body.frame.bytes>4080)goto done;
 failure="unsupported init data/local kinds";
 if(body.data.count!=(nilvalue!=nil))goto done;
 if(nilvalue){if(body.data.leaves[0].kind!=Tany)goto done;niloffset=body.data.leaves[0].native;}
 for(i=0;i<body.frame.count;i++){
  leaf=body.frame.leaves+i;
  if(leaf->kind!=Tint && leaf->kind!=Tbig && leaf->kind!=Treal && leaf->kind!=Tref && leaf->kind!=Tchan && leaf->kind!=Tstring && leaf->kind!=Tlist && leaf->kind!=Tany && leaf->kind!=-1)goto done;
 }
 code=allocmem(CallFileBytes);output=allocmem(CallFileBytes);typedata=allocmem(CallFileBytes);strings=allocmem(CallFileBytes);runes=allocmem(4*(4+4*255));maps=allocmem(65);memset(maps,0,65);proof=allocmem(sizeof(*proof));
 {char *texts[]={"Command","command.dis64","Command","init"};unsigned int j,n;
  for(i=0;i<4;i++){n=strlen(texts[i]);dis64put32(runes+i*(4+4*255),n);for(j=0;j<n;j++)dis64put32(runes+i*(4+4*255)+4+4*j,texts[i][j]);names[i]=(DisStringPoolRecord){runes+i*(4+4*255),4+4*n};}
 }
 failure="unsupported init operand encoding";
 for(i=0;i<body.count;i++){
  in=body.code[i];
  if((in.op==IBEQL || in.op==IBNEL) && (in.mode&ARM)==AXINM){if(!nilvalue || in.middle!=niloffset)goto done;in.middle=8;}
  if(USRC(in.mode)==AMP){if(!nilvalue || in.source!=niloffset)goto done;in.source=8;}
  if(!dis64clientencode(code,&at,&in,in.destination,body.contextfields?body.contextfields[i]:0,body.contextfields?body.contextfields[i]:0))goto done;
 }
 dis64put32(code,body.count);sizes[1]=at;maps[64]=0xe0;
 for(i=0;i<body.frame.count;i++){
  leaf=body.frame.leaves+i;if(leaf->kind==Tref || leaf->kind==Tchan || leaf->kind==Tstring || leaf->kind==Tlist || leaf->kind==Tany)maps[leaf->native/64]|=0x80>>((leaf->native/8)%8);
 }
 types[0]=(Dis64Type){body.frame.bytes+16,8,maps,(body.frame.bytes+79)/64};types[1]=(Dis64Type){24,8,maps+64,1};module.databytes=24;module.datatype=1;
 sizes[3]=dis64commandbindingwrite(exports,1,sign(fn));
 if(!dis64modulewrite(metadata,sizeof(metadata),&module,&sizes[0]) || !dis64typeswrite(typedata,CallFileBytes,types,2,&sizes[2]) ||
    !disstringpoolwrite(strings,CallFileBytes,names,4,&sizes[4]) || !disstringpoolcheck(strings,sizes[4],65536,&pool))goto done;
 sections[0]=(Dis64Payload){Dis64Module,Dis64Required,metadata,sizes[0]};sections[1]=(Dis64Payload){Dis64Code,Dis64Required,code,sizes[1]};sections[2]=(Dis64Payload){Dis64Types,Dis64Required,typedata,sizes[2]};
 sections[3]=(Dis64Payload){Dis64Exports,Dis64Required,exports,sizes[3]};sections[4]=(Dis64Payload){Dis64Strings,Dis64Required,strings,sizes[4]};
 failure="independent init proof refused output";
 if(!dis64write(output,CallFileBytes,nil,0,sections,5,&bytes) || !dis64commandcheck(output,bytes,proof) || Bwrite(bout,output,bytes)!=bytes)goto done;failure=nil;
 done:detail=body.reason;dis64typedfree(&body);free(code);free(output);free(typedata);free(strings);free(runes);free(maps);free(proof);
 if(failure)error(impdecl->src.start,"Dis64 command: %s (%s)",failure,detail?detail:"");return 1;
}
