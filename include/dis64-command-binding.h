/* Private fixed Command.init trial. Not a public managed signature assignment. */
#ifndef DIS64_COMMAND_BINDING_H
#define DIS64_COMMAND_BINDING_H
#include "dis64-bindings.h"
#define Dis64CommandSignature 0xface0186U
/* The ordinary Limbo .m hash is a consistency token, not authentication. The
 * fixed shape is opaque ADT-ref,ListString -> void; no ADT field access follows. */
static int
dis64commandsignature(Dis64Signature *s)
{
 if(!s || !s->data || s->bytes!=16 || dis64u32(s->data)!=Dis64CommandSignature || dis64u32(s->data+8)!=2 || dis64u32(s->data+12))return 0;
 s->result=0;s->parameters=2;return 1;
}
static int
dis64commandname(const DisStringPoolView *pool,unsigned int index,const char *name)
{
 DisRuneView r;unsigned int i,c;if(!disstringpoolget(pool,index,&r))return 0;
 for(i=0;i<r.count;i++)if(!name[i] || !disruneat(&r,i,&c) || c!=(unsigned char)name[i])return 0;
 return !name[r.count];
}
static int
dis64clientbindingget(const Dis64Bindings *v,unsigned int index,Dis64Binding *b)
{
 unsigned int used;Dis64Binding candidate;
 if(!b)return 0;
 if(v && v->data && v->bytes>=4 && v->exports<=1 && v->count==1 && index==0 &&
    dis64bindingrecord(v->data+4,v->bytes-4,v->exports,&candidate,&used) && used==v->bytes-4 && dis64commandsignature(&candidate.signature)){
  *b=candidate;return 1;
 }
 return dis64bindingget(v,index,b);
}
static int
dis64clientbindingscheck(const unsigned char *data,unsigned int bytes,unsigned int exports,
 const DisStringPoolView *pool,const Dis64TypeTable *types,unsigned int instructions,
 unsigned int limit,unsigned int parameters,Dis64Bindings *out)
{
 Dis64Binding b;unsigned int used;
 if(data && out && bytes>=4 && dis64u32(data)==1 && exports<=1 && limit && parameters>=2 &&
    dis64bindingrecord(data+4,bytes-4,exports,&b,&used) && used==bytes-4 && dis64commandsignature(&b.signature)){
  if(!dis64commandname(pool,2,"Command") || !dis64commandname(pool,3,"init") || b.symbol!=3 ||
     (exports?(b.entry || b.frame):b.module!=2))return 0;
  *out=(Dis64Bindings){data,bytes,1,exports,parameters};return 1;
 }
 return dis64bindingscheck(data,bytes,exports,pool,types,instructions,limit,parameters,out);
}
static unsigned int
dis64commandbindingwrite(unsigned char *out,unsigned int exports,unsigned int hash)
{
 unsigned int at=exports?20:16;
 dis64put32(out,1);dis64put32(out+4,exports?3:2);dis64put32(out+8,exports?0:3);
 if(exports)dis64put32(out+12,0);dis64put32(out+at-4,16);
 dis64put32(out+at,Dis64CommandSignature);dis64put32(out+at+4,hash);dis64put32(out+at+8,2);dis64put32(out+at+12,0);return at+16;
}
#endif
