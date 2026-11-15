/* Observe owned graphs independently before actual collection; never resume after
 * a missing-root control has retired an observed object. Arena storage stays valid. */
static Heap *clientgraph[256];
static unsigned int clientgraphcount;
static int clientindexpause;
static unsigned int clientindexpauses;
static Type *clientlocaltypes[8];
static unsigned int clientlocalargcount[8],clientlocalargoffset[8][8];
static unsigned int clientlocalcount,clientlocalpauses,clientlocalvalues[8];
static void *clientresulttargets[64];
static unsigned int clientresultcount,clientresultpauses;
static void
clientlocalremember(Module *module,const uchar *data,unsigned int bytes)
{
 Dis64ScalarClientView view;clientlocalcount=clientresultcount=0;
 if(dis64commandcheck(data,bytes,&view))for(unsigned int i=0;i<view.localcount;i++){
  unsigned int offset=view.localprefix[i]?80:64;clientlocalargcount[i]=0;
  for(unsigned int j=0;j<view.localargc[i];j++){
   unsigned int kind=view.localargs[i][j],width=dis64clientwidth(kind);offset=(offset+width-1)&~(width-1);
   if(dis64clientisroot(kind))clientlocalargoffset[i][clientlocalargcount[i]++]=offset;
   offset+=width;
  }
  clientlocaltypes[clientlocalcount]=module->type[view.localtype[i]];clientlocalvalues[clientlocalcount++]=view.localresult[i];
 }
}
static void
clientobserve(void *value)
{
 Heap *h;List *list;void *head;unsigned int i;
 if(value==H)return;
 require(value!=nil);h=D2H(value);require(numericowner(h)->magic==MAGIC_A);
 for(i=0;i<clientgraphcount;i++)if(clientgraph[i]==h)return;
 require(clientgraphcount<256);clientgraph[clientgraphcount++]=h;
 if(h->t==&Tmodlink){
  Modlink *link=value;clientobserve(link->MP);clientobserve(link->m->origmp);
 }else if(h->t==&Tarray){
  Array *array=value;require((array->t->size==1 || array->t->size==4) && array->t->np==0);clientobserve(array->root);
 }else if(h->t==&Tlist){
  list=value;clientobserve(list->tail);
  require(list->t==&Tptr);memmove(&head,list->data,sizeof(head));clientobserve(head);
 }else{
  for(i=0;i<(unsigned int)h->t->np*8;i++)if(h->t->map[i/8]&(0x80>>(i%8))){
   require(8*i+8<=(unsigned int)h->t->size);memmove(&head,(uchar*)value+8*i,sizeof(head));clientobserve(head);
  }
 }
}
static void
clientcollect(Prog *process,Module *client,uchar *root)
{
 Stkext *extent=(Stkext*)process->R.EX;uchar *top=process->R.SP,*fp;Type *type;Frame frame;
 void *value,*returnaddress=nil,*rootaddresses[1024],*frameaddresses[128];Type *frametypes[128];unsigned int nframeaddresses=0,resultkind=0;unsigned int i,j,bit,frames=0,naddresses=0,wantresult=0;USED(client);USED(root);
 clientindexpause=0;
 if(process->R.M!=H && process->R.M!=nil){
  Module *m=process->R.M->m;uintptr pc=(uintptr)process->R.PC,base=(uintptr)m->prog;
  if(pc>base && pc<=base+m->nprog*sizeof(Inst) && (pc-base)%sizeof(Inst)==0){
   Inst *previous=process->R.PC-1;
   if((previous->op==IINDB || previous->op==IINDW)){
    Array *array;uchar *address;WORD index;
    require(USRC(previous->add)==AFP && (previous->add&ARM)==AXINF);
    memmove(&array,process->R.FP+previous->s.ind,sizeof(array));
    if(UDST(previous->add)==AIMM)memmove(&index,&previous->d,sizeof(index));
    else memmove(&index,process->R.FP+previous->d.ind,sizeof(index));
    memmove(&address,process->R.FP+previous->reg,sizeof(address));
    require(array!=H && array!=nil && index>=0 && index<array->len && address==array->data+(previous->op==IINDB?1:4)*index);
    {Frame active;memmove(&active,process->R.FP,sizeof(active));Type *current=active.t?active.t:((Stkext*)process->R.EX)->reg.TR;require(current && previous->reg==current->size-16);}
    clientindexpause=1;clientindexpauses++;
   }
  }
 }
 clientgraphcount=0;clientobserve(process->R.M);clientobserve(client->origmp);
 while(extent!=nil){
  fp=extent->reg.tos.fu;
  while(fp!=top){
   require(++frames<128 && (uintptr)fp<(uintptr)top && (uintptr)top-(uintptr)fp>=sizeof(Frame));
   memmove(&frame,fp,sizeof(frame));if(frame.mr && frame.mr!=H)clientobserve(frame.mr);type=frame.t?frame.t:extent->reg.TR;
   require(native64localtype(type) && (uintptr)type->size<=(uintptr)top-(uintptr)fp);
   if(fp==process->R.FP)for(i=0;i<clientlocalcount;i++)if(type==clientlocaltypes[i]){
    clientlocalpauses++;
    if(clientlocalvalues[i]){wantresult=1;resultkind=clientlocalvalues[i];memmove(&returnaddress,fp+32,sizeof(returnaddress));}
   }
   if(fp!=process->R.FP){require(nframeaddresses<128);frameaddresses[nframeaddresses]=fp;frametypes[nframeaddresses++]=type;}
   for(i=0;i<(unsigned int)type->np;i++)for(bit=0;bit<8;bit++)if(type->map[i]&(0x80>>bit)){
    if(fp!=process->R.FP){require(naddresses<1024);rootaddresses[naddresses++]=fp+64*i+8*bit;}
    memmove(&value,fp+64*i+8*bit,sizeof(value));clientobserve(value);
   }
   fp+=type->size;
  }
  top=extent->reg.SP;extent=(Stkext*)extent->reg.EX;
 }
 if(wantresult){
  require(returnaddress!=nil && returnaddress!=H);
  require((uintptr)returnaddress>0xffffffffULL);clientresultpauses++;
  if(dis64clientisroot(resultkind)){
   for(j=0;j<naddresses && returnaddress!=rootaddresses[j];j++);require(j<naddresses);
   for(j=0;j<clientresultcount && returnaddress!=clientresulttargets[j];j++);
   if(j==clientresultcount){require(clientresultcount<64);clientresulttargets[clientresultcount++]=returnaddress;}
  }else{
   unsigned int width=dis64clientwidth(resultkind),offset,byte,found=0;
   require(width==1 || width==4 || width==8);
   for(j=0;j<nframeaddresses;j++){
    uintptr base=(uintptr)frameaddresses[j],address=(uintptr)returnaddress;type=frametypes[j];
    if(address<base+64 || address>base+type->size-16 || width>base+type->size-16-address)continue;
    offset=address-base;if(offset%width)continue;
    for(byte=offset;byte<offset+width;byte++)require(byte/64>=(unsigned int)type->np || !(type->map[byte/64]&(0x80>>((byte/8)%8))));
    found=1;break;
   }
   require(found);
  }
 }
 numericcollect(process);
 for(i=0;i<clientgraphcount;i++)if(numericowner(clientgraph[i])->magic!=MAGIC_A){
  fprintf(stderr,"client GC graph retention mismatch\n");exit(10);
 }
}

/* Active omission keeps the established module/nil/path prefix marked but
 * deliberately loses only the additional literal roots. Maps stay observable. */
static void
clientliteralmpmark(Type *type,void *mp)
{
 Type prefix={0};USED(type);prefix.size=24;prefix.np=1;prefix.map[0]=0xe0;markheap(&prefix,mp);
}
/* Negative control: preserve every mapped root except a live byte-array owner. */
static void
clientcodecframemark(Type *type,void *frame)
{
 Type one={0};void *value;unsigned int i;
 one.size=8;one.np=1;one.map[0]=0x80;
 for(i=0;i<(unsigned int)type->np*8;i++)if(type->map[i/8]&(0x80>>(i%8))){
  memmove(&value,(uchar*)frame+8*i,sizeof(value));
  if(value!=H && value!=nil && D2H(value)->t==&Tarray)continue;
  markheap(&one,&value);
 }
}

/* Omit the owner only while suspended after INDB, before its byte consumer. */
static void
clientindexframemark(Type *type,void *frame)
{
 if(clientindexpause)clientcodecframemark(type,frame);
 else markheap(type,frame);
}

/* Lose only a recorded caller result root; callee/private/argument maps remain. */
static void
clientresultframemark(Type *type,void *frame)
{
 Type one={0};void *value;unsigned int i,j;one.size=8;one.np=1;one.map[0]=0x80;
 for(i=0;i<(unsigned int)type->np*8;i++)if(type->map[i/8]&(0x80>>(i%8))){
  void *slot=(uchar*)frame+8*i;
  for(j=0;j<clientresultcount && slot!=clientresulttargets[j];j++);
  if(j<clientresultcount)continue;
  memmove(&value,slot,sizeof(value));markheap(&one,&value);
 }
}

/* Omit only the inferred managed parameter roots, preserving all other locals. */
static void
clientargumentframemark(Type *type,void *frame)
{
 Type one={0};unsigned int i,j,k,skip;one.size=8;one.np=1;one.map[0]=0x80;
 for(i=0;i<(unsigned int)type->np*8;i++)if(type->map[i/8]&(0x80>>(i%8))){
  skip=0;for(j=0;j<clientlocalcount;j++)if(type==clientlocaltypes[j])
   for(k=0;k<clientlocalargcount[j];k++)skip|=8*i==clientlocalargoffset[j][k];
  if(!skip)markheap(&one,(uchar*)frame+8*i);
 }
}
