/* A real generated Draw_Context owner with nil display/screen/channel fields.
 * This witnesses pointer identity/lifetime, not window/device functionality. */
static int commandcontext;
static void
commandcontextseed(const Dis64ScalarClientView *view,uchar *root)
{
 unsigned int i;long offset=-1;void *old,*value;Heap *h;Type *type;uchar map[1]={0xe0};
 require(limbo_native64_Command_layout());
 require(sizeof(Draw_Context)==24 && offsetof(F_Command_init,ctxt)==64 && offsetof(F_Command_init,argv)==72);
 for(i=0;i+1<view->count;i++)if(view->decoded[i].instruction.op==IMFRAME){
  require(view->decoded[i+1].instruction.op==IMOVP && USRC(view->decoded[i+1].instruction.mode)==AFP);offset=view->decoded[i+1].instruction.source;break;
 }
 require(offset>=64);memmove(&old,root+offset,sizeof(old));require(old==H);
 type=dtype(freeheap,sizeof(Draw_Context),map,1);require(type!=nil);h=heapz(type);freetype(type);value=H2D(void*,h);memmove(root+offset,&value,sizeof(value));
}

static void clientempty(void);
/* Every provider read/constructor/link allocation failure must retire the source
 * MP, its clone, exported names, code, maps, pins and normal registry entry. */
static void
commandproviderfaults(const Dis64ScalarClientView *view)
{
 Modlink *link;unsigned int total,fault;int jit;
 for(jit=0;jit<2;jit++){
  allocations=0;link=read64calllibrary("library.dis64",jit,view->envelope.data+view->requests.offset,view->requests.bytes,
   view->envelope.data+view->names.offset,view->names.bytes,0);
  require(link && link!=H && !link->m->entry && !link->m->entryt && link->m->ext && link->nlinks==1 &&
   link->MP!=H && link->m->origmp!=H && link->MP!=link->m->origmp && modules==link->m);
  total=allocations;ptrdel(D2H(link));destroy(link);clientempty();require(Tmodlink.ref==1);
  for(fault=1;fault<=total;fault++){
   allocations=0;failat=fault;link=read64calllibrary("library.dis64",jit,view->envelope.data+view->requests.offset,view->requests.bytes,
    view->envelope.data+view->names.offset,view->names.bytes,0);failat=0;
   require(link==nil);clientempty();require(Tmodlink.ref==1);
  }
 }
}
