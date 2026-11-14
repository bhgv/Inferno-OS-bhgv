/* Observe owned graphs independently before actual collection; never resume after
 * a missing-root control has retired an observed object. Arena storage stays valid. */
static Heap *clientgraph[64];
static unsigned int clientgraphcount;
static void
clientobserve(void *value)
{
 Heap *h;List *list;void *head;unsigned int i;
 if(value==H)return;
 require(value!=nil);h=D2H(value);require(numericowner(h)->magic==MAGIC_A);
 for(i=0;i<clientgraphcount;i++)if(clientgraph[i]==h)return;
 require(clientgraphcount<64);clientgraph[clientgraphcount++]=h;
 if(h->t==&Tlist){
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
 void *value;unsigned int i,bit,frames=0;USED(client);USED(root);
 clientgraphcount=0;
 while(extent!=nil){
  fp=extent->reg.tos.fu;
  while(fp!=top){
   require(++frames<128 && (uintptr)fp<(uintptr)top && (uintptr)top-(uintptr)fp>=sizeof(Frame));
   memmove(&frame,fp,sizeof(frame));type=frame.t?frame.t:extent->reg.TR;
   require(native64localtype(type) && (uintptr)type->size<=(uintptr)top-(uintptr)fp);
   for(i=0;i<(unsigned int)type->np;i++)for(bit=0;bit<8;bit++)if(type->map[i]&(0x80>>bit)){
    memmove(&value,fp+64*i+8*bit,sizeof(value));clientobserve(value);
   }
   fp+=type->size;
  }
  top=extent->reg.SP;extent=(Stkext*)extent->reg.EX;
 }
 numericcollect(process);
 for(i=0;i<clientgraphcount;i++)if(numericowner(clientgraph[i])->magic!=MAGIC_A){
  fprintf(stderr,"client GC graph retention mismatch\n");exit(10);
 }
}
