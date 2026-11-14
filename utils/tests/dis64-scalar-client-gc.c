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
 }
}
static void
clientcollect(Prog *process,Module *client,uchar *root)
{
 Type *type=client->type[0];void *value;unsigned int i,bit;
 clientgraphcount=0;
 for(i=0;i<(unsigned int)type->np;i++)for(bit=0;bit<8;bit++)if(type->map[i]&(0x80>>bit)){
  memmove(&value,root+64*i+8*bit,sizeof(value));clientobserve(value);
 }
 numericcollect(process);
 for(i=0;i<clientgraphcount;i++)if(numericowner(clientgraph[i])->magic!=MAGIC_A){
  fprintf(stderr,"client GC graph retention mismatch\n");exit(10);
 }
}
