/* Append checked local frames after the existing MP/Context/primitive records.
 * Internal helpers use scalar parameters and an optional managed prefix. */
static int
dis64commandlocalfile(Decl *globals,Decl **functions,int nf,Inst *instructions,long count,long databytes)
{
 FileStrings literals;Dis64TypedBody bodies[9]={{0}},*body;Dis64ScalarClientView *proof=nil;
 Dis64Type types[13];Dis64ModuleInfo module={0};Dis64Payload sections[5];Dis64Instruction in;
 DisStringPoolRecord names[4+Dis64ManagedLiterals];DisStringPoolView pool;Dis64LayoutLeaf *leaf;
 uchar *code=nil,*output=nil,*typedata=nil,*strings=nil,*runes=nil,*maps=nil;
 uchar metadata[24],exports[36];unsigned int sizes[5],i,j,k,n,at=4,bytes,ntypes,frameids[9],cursor=0;
 char *failure="unsupported local init library",*detail=nil;
 if(nf<2 || nf>9 || count<1 || count>128 || !filestringscollect(&literals,globals,databytes))goto done;
 for(j=0;j<(unsigned int)nf;j++){
  body=bodies+j;
  if(!(j?dis64commandhelper(functions[j]):dis64commandargs(functions[j])) || !functions[j]->pc || functions[j]->pc->pc!=(long)cursor ||
     !dis64typedcommand(functions[j],globals,functions,nf,instructions,count,count,body) || body->nexternal ||
     body->frame.bytes>4080 || body->first!=cursor || !filestringsplan(&literals,body,0))goto done;
  for(i=0;i<body->nsites;i++)if(body->sites[i].contract && body->sites[i].contract!=1 && body->sites[i].contract!=4 && body->sites[i].contract!=8 && body->sites[i].contract!=24)goto done;
  for(i=0;i<body->frame.count;i++){
   leaf=body->frame.leaves+i;
   if(leaf->kind!=Tbyte && leaf->kind!=Tint && leaf->kind!=Tbig && leaf->kind!=Treal && leaf->kind!=Tref && leaf->kind!=Tchan && leaf->kind!=Tarray && leaf->kind!=Tstring && leaf->kind!=Tlist && leaf->kind!=Tany && leaf->kind!=-1)goto done;
  }
  cursor+=body->count;
 }
 if(cursor!=(unsigned long)count)goto done;
 code=allocmem(CallFileBytes);output=allocmem(CallFileBytes);typedata=allocmem(CallFileBytes);strings=allocmem(CallFileBytes);
 runes=allocmem((4+Dis64ManagedLiterals)*(4+4*1023));maps=allocmem(72+8*64);memset(maps,0,72+8*64);proof=allocmem(sizeof(*proof));
 {char *texts[]={"Command","command.dis64","Command","init"};
  for(i=0;i<4;i++){n=strlen(texts[i]);dis64put32(runes+i*(4+4*1023),n);for(k=0;k<n;k++)dis64put32(runes+i*(4+4*1023)+4+4*k,texts[i][k]);names[i]=(DisStringPoolRecord){runes+i*(4+4*1023),4+4*n};}
 }
 if(!filestringpool(&literals,runes,names,4))goto done;
 ntypes=filestringtypes(&literals,types,maps);
 for(j=0;j<(unsigned int)nf;j++){
  uchar *map=j?maps+72+64*(j-1):maps;body=bodies+j;
  for(i=0;i<body->frame.count;i++){
   leaf=body->frame.leaves+i;
   if(leaf->kind==Tarray || leaf->kind==Tref || leaf->kind==Tchan || leaf->kind==Tstring || leaf->kind==Tlist || leaf->kind==Tany)map[leaf->native/64]|=0x80>>((leaf->native/8)%8);
  }
  frameids[j]=j?ntypes++:0;types[frameids[j]]=(Dis64Type){body->frame.bytes+16,8,map,(body->frame.bytes+79)/64};
 }
 for(j=0;j<(unsigned int)nf;j++){
  body=bodies+j;
  for(i=0;i<body->count;i++){
   in=body->code[i];
   if(dis64clientbranch(in.op))in.destination+=body->first;
   if((in.op==IBEQL || in.op==IBNEL) && (in.mode&ARM)==AXINM){if(!literals.nilvalue || in.middle!=literals.niloffset)goto done;in.middle=8;}
   if(USRC(in.mode)==AMP && !filestringaddress(&literals,in.source,&in.source))goto done;
   if((in.op==IADDC || in.op==IBEQC || in.op==IBNEC || in.op==IBLTC || in.op==IBLEC || in.op==IBGTC || in.op==IBGEC) && (in.mode&ARM)==AXINM)
    if(!filestringaddress(&literals,in.middle,&in.middle))goto done;
   if(in.op==INEW || in.op==INEWZ)in.source=literals.literals?2:1;
   if(in.op==INEWAZ)in.middle=(literals.literals?3:2)+(in.middle?literals.bytearrays:0);
   if(in.op==IFRAME){
    for(k=0;k<body->nsites;k++)if(body->sites[k].first==i)break;
    if(k==body->nsites)goto done;
    for(n=1;n<(unsigned int)nf;n++)if(bodies[n].first==body->sites[k].target)break;
    if(n==(unsigned int)nf)goto done;in.source=frameids[n];
   }
   if(body->returns[i]>=64){if(!callfileencode(code,&at,&in,40,in.destination))goto done;}
   else if(!dis64clientencode(code,&at,&in,in.destination,body->contextfields?body->contextfields[i]:0,body->contextfields?body->contextfields[i]:0))goto done;
  }
 }
 dis64put32(code,count);sizes[1]=at;module.databytes=24+8*literals.literals;module.datatype=1;
 sizes[3]=dis64commandbindingwrite(exports,1,sign(functions[0]));
 if(!dis64modulewrite(metadata,sizeof(metadata),&module,&sizes[0]) || !dis64typeswrite(typedata,CallFileBytes,types,ntypes,&sizes[2]) ||
    !disstringpoolwrite(strings,CallFileBytes,names,4+literals.literals,&sizes[4]) || !disstringpoolcheck(strings,sizes[4],65536,&pool))goto done;
 sections[0]=(Dis64Payload){Dis64Module,Dis64Required,metadata,sizes[0]};sections[1]=(Dis64Payload){Dis64Code,Dis64Required,code,sizes[1]};sections[2]=(Dis64Payload){Dis64Types,Dis64Required,typedata,sizes[2]};
 sections[3]=(Dis64Payload){Dis64Exports,Dis64Required,exports,sizes[3]};sections[4]=(Dis64Payload){Dis64Strings,Dis64Required,strings,sizes[4]};
 failure="independent local init proof refused output";
 if(!dis64write(output,CallFileBytes,nil,0,sections,5,&bytes) || !dis64commandcheck(output,bytes,proof) || Bwrite(bout,output,bytes)!=bytes)goto done;failure=nil;
 done:
 for(i=0;i<9;i++){if(bodies[i].reason)detail=bodies[i].reason;dis64typedfree(bodies+i);}
 free(code);free(output);free(typedata);free(strings);free(runes);free(maps);free(proof);
 if(failure)error(impdecl->src.start,"Dis64 command: %s (%s)",failure,detail?detail:"");return 1;
}
