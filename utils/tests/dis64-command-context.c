/* A real generated Draw_Context owner with nil display/screen/channel fields.
 * This witnesses pointer identity/lifetime, not window/device functionality. */
static int commandcontext,commandfields,commandcontextnomark=0;
static void *commandfieldowner,*commandfieldvalues[3];
static void
commandcontextseed(const Dis64ScalarClientView *view,uchar *root)
{
 unsigned int i;long offset=-1;void *old,*value;Heap *h;Type *type;uchar map[1]={0xe0};
 require(limbo_native64_Command_layout());
 require(offsetof(Draw_Context,display)==0 && offsetof(Draw_Context,screen)==8 && offsetof(Draw_Context,wm)==16);
 require(sizeof(Draw_Context)==24 && offsetof(F_Command_init,ctxt)==64 && offsetof(F_Command_init,argv)==72);
 for(i=0;i+1<view->count;i++)if(view->decoded[i].instruction.op==IMFRAME){
  require(view->decoded[i+1].instruction.op==IMOVP && USRC(view->decoded[i+1].instruction.mode)==AFP);offset=view->decoded[i+1].instruction.source;break;
 }
 require(offset>=64);memmove(&old,root+offset,sizeof(old));require(old==H);
 type=dtype(freeheap,sizeof(Draw_Context),map,1);require(type!=nil);if(commandcontextnomark)type->mark=noptrs;h=heapz(type);freetype(type);value=H2D(void*,h);memmove(root+offset,&value,sizeof(value));
 /* Three opaque native ownership witnesses, not GUI or channel resources.
  * The admitted guest can only copy/clear/test these references. */
 if(commandfields){
  commandfieldowner=value;
  for(i=0;i<3;i++){void *child;Type *leaf=dtype(freeheap,8,nil,0);require(leaf!=nil);Heap *owner=heapz(leaf);freetype(leaf);child=H2D(void*,owner);commandfieldvalues[i]=child;memmove((uchar*)value+8*i,&child,sizeof(child));Setmark(owner);}
 }

}

static void
commandfieldcheck(void)
{
 unsigned int i;void *value;
 if(!commandfields)return;
 for(i=0;i<3;i++){
  memmove(&value,(uchar*)commandfieldowner+8*i,sizeof(value));
  if(value!=commandfieldvalues[i]){fprintf(stderr,"Context field identity mismatch\n");exit(10);}
 }
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
