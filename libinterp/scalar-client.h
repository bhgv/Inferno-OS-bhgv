/* Included after dis64runtimeinst. All proof/constructor storage remains native-owned. */
#include "dis64-scalar-client.h"
#include "dis64-import-owner.h"
typedef struct Dis64ClientBuild Dis64ClientBuild;
struct Dis64ClientBuild {Dis64ScalarClientView *view;Module *module;char *path;int jit,rooted;};
static int
dis64clientconstruct(void *argument)
{
 Dis64ClientBuild *b=argument;Dis64ScalarClientView *v=b->view;Module*m;Dis64Type type;DisRuneView name;Dis64ImportOwner *owner;
 unsigned int i,j,rune,roots=0,ascii;uchar *at;Heap*h;String*path;ulong bytes;
 b->module=m=mallocz(sizeof(*m),1);if(!m)return 0;m->ref=1;m->origmp=H;m->rt=DONTCOMPILE|Dis64ScalarClient;m->mptype=1;
 m->ntype=v->types.count;m->type=mallocz(m->ntype*sizeof(Type*),1);if(!m->type)return 0;
 for(i=0;i<(unsigned int)m->ntype;i++){if(!dis64typeget(&v->types,i,&type))return 0;if(i==0)for(j=0;j<type.mapbytes;j++)roots|=type.map[j];/* The proved all-zero byte map has no native pointer entries. */m->type[i]=dtype(freeheap,type.bytes,(uchar*)type.map,((v->bytearrays && i==v->bytetype) || (v->intarrays && i==v->inttype))?0:type.mapbytes);if(!m->type[i])return 0;}
 m->origmp=H2D(uchar*,heapz(m->type[1]));ptradd(D2H(m->origmp));b->rooted=1;
 if(Tstring.ref<=0 || Tstring.ref==0x7fffffff || !disstringpoolget(&v->strings,1,&name))return 0;
 h=nheap(sizeof(String)+name.count);h->t=&Tstring;Tstring.ref++;path=H2D(String*,h);path->tmp=nil;path->len=path->max=name.count;
 for(i=0;i<name.count;i++){disruneat(&name,i,&rune);path->Sascii[i]=rune;}path->Sascii[name.count]=0;
 memmove(m->origmp+16,&path,sizeof(path));Setmark(D2H(path));
 for(i=0;i<v->literals;i++){
  if(!disstringpoolget(&v->strings,3+v->imports.count+i,&name))return 0;
  ascii=1;for(j=0;j<name.count;j++){if(!disruneat(&name,j,&rune))return 0;if(rune>127)ascii=0;}
  path=ascii?newstring(name.count):newrunes(name.count);memmove(m->origmp+24+8*i,&path,sizeof(path));Setmark(D2H(path));
  for(j=0;j<name.count;j++){if(!disruneat(&name,j,&rune))return 0;if(ascii)path->Sascii[j]=rune;else memmove(path->Srune+j,&rune,sizeof(rune));}
 }
 if(v->module.profile){
 bytes=sizeof(*owner)+v->requests.bytes+v->names.bytes;owner=mallocz(bytes,1);if(!owner)return 0;m->native64imports=owner;
 owner->count=1;owner->jit=b->jit;at=(uchar*)(owner+1);
 owner->tables[0]=(Native64ImportRequest){at,at+v->requests.bytes,v->requests.bytes,v->names.bytes};
 memmove(at,v->envelope.data+v->requests.offset,v->requests.bytes);memmove(at+v->requests.bytes,v->envelope.data+v->names.offset,v->names.bytes);
 m->native64importload=read64ownedimport;
 }
 m->nprog=v->count;m->prog=mallocz(m->nprog*sizeof(Inst),1);if(!m->prog)return 0;
 for(i=0;i<v->count;i++){
  Dis64CallDecoded *d=v->decoded+i;if(!dis64runtimeinstruction(&d->instruction))return 0;
  dis64runtimeinst(m->prog+i,&d->instruction,m->prog);
  if(roots && d->instruction.op==ILEA)m->prog[i].reg=8;
  if(USRC(d->instruction.mode)==(AFP|AIND)){m->prog[i].s.i.f=d->instruction.source;m->prog[i].s.i.s=d->secondary;}
  if(UDST(d->instruction.mode)==(AFP|AIND)){m->prog[i].d.i.f=d->instruction.destination;m->prog[i].d.i.s=d->secondary;}
 }
 if(!disstringpoolget(&v->strings,0,&name))return 0;
 m->name=mallocz(name.count+1,1);if(!m->name)return 0;
 for(j=0;j<name.count;j++){disruneat(&name,j,&rune);m->name[j]=rune;}
 m->path=strdup(b->path);if(!m->path)return 0;m->entry=v->module.profile?m->prog:nil;m->entryt=v->module.profile?m->type[0]:nil;m->cstep=roots?native64typedlea8step:native64lea8step;
 if(!v->module.profile){Dis64Binding binding;
  if(!dis64clientbindingget(&v->imports,0,&binding))return 0;
  m->ext=mallocz(2*sizeof(Link),1);if(!m->ext)return 0;
  m->ext[0].name=strdup("init");if(!m->ext[0].name)return 0;
  m->ext[0].sig=dis64u32(binding.signature.data+4);m->ext[0].frame=m->type[0];m->ext[0].u.pc=m->prog;
 }
 if(b->jit && !compile(m,m->nprog,nil))return 0;return 1;
}
/* All descriptor/code/root roles come from the same independently checked file.
 * Executable callers publish through parsemod under exclusion; the entryless
 * library is registered here before its constructor pin is released. */
static Module*
dis64clientpreparemode(char *path,uchar *image,ulong bytes,int jit,int library)
{
 Dis64ClientBuild b={0};int ok;
 b.view=mallocz(sizeof(*b.view),1);if(!b.view)return nil;
 if(!(library?dis64commandcheck(image,bytes,b.view):dis64clientcheck(image,bytes,b.view))){free(b.view);kwerrstr("unsupported Dis64 scalar importing command");return nil;}
 b.path=path;b.jit=jit;ok=kprotect(dis64clientconstruct,&b);
 /* Publish a complete entryless module before dropping its source-MP pin.
  * Normal linkmod then clones MP; normal unload removes this registry entry. */
 if(ok && library){b.module->link=modules;modules=b.module;}
 if(b.rooted)ptrdel(D2H(b.module->origmp));
 if(!ok && b.module){destroy(b.module->origmp);destroylinks(b.module);freemod(b.module);b.module=nil;}
 free(b.view);return b.module;
}

static Module*dis64clientprepare(char *path,uchar *image,ulong bytes,int jit){return dis64clientpreparemode(path,image,bytes,jit,0);}
