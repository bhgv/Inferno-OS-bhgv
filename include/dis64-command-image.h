/* Entryless Command implementation with outgoing imports and a separate init export. */
static int
dis64commandimportcheck(const unsigned char *data,unsigned int bytes,Dis64ScalarClientView *out)
{
 Dis64ScalarClientView v={0};Dis64Section s,module={0},types={0};Dis64Type frame,mp;Dis64Binding b;
 DisRuneView name;unsigned int i,j,c,seen=0,used;
 if(!out || bytes>Dis64CallImageBytes || !dis64check(data,bytes,0,6,0,0,&v.envelope) || v.envelope.sections!=6)return 0;
 for(i=0;i<6;i++){
  if(!dis64section(&v.envelope,i,&s) || s.kind>7 || (seen&(1U<<s.kind)))return 0;seen|=1U<<s.kind;
  switch(s.kind){case Dis64Module:module=s;break;case Dis64Code:v.code=s;break;case Dis64Types:types=s;break;case Dis64Imports:v.requests=s;break;case Dis64Exports:v.definitions=s;break;case Dis64Strings:v.names=s;break;default:return 0;}
 }
 v.importing=1;
 if(!dis64modulecheck(data+module.offset,module.bytes,&v.module) || v.module.profile || v.module.name || v.module.datatype!=1 ||
    !dis64typescheck(data+types.offset,types.bytes,13,4096,&v.types) || !dis64typeget(&v.types,0,&frame) || !dis64typeget(&v.types,1,&mp) ||
    frame.bytes<96 || frame.align!=8 || mp.align!=8 ||
    !disstringpoolcheck(data+v.names.offset,v.names.bytes,65536,&v.strings) ||
    !dis64clientbindingscheck(data+v.requests.offset,v.requests.bytes,0,&v.strings,0,0,64,64,&v.imports) || !v.imports.count ||
    v.requests.bytes>Dis64LocalBytes || v.names.bytes>Dis64LocalBytes || !dis64clientdatacheck(&v) || v.code.bytes<4)return 0;
 for(i=0;i<v.imports.count+4;i++){
  if(!disstringpoolget(&v.strings,i,&name) || !name.count || name.count>(i==1?1023U:255U))return 0;
  for(j=0;j<name.count;j++)if(!disruneat(&name,j,&c) || !c || c>127 || (i==1 && !j && c=='$'))return 0;
 }
 for(i=0;i<v.imports.count;i++){
  if(!dis64clientbindingget(&v.imports,i,&b) || b.module!=2 || b.symbol!=i+3 || b.signature.result>Dis64Real)return 0;
  if(!dis64commandsignature(&b.signature))for(j=0;j<b.signature.parameters;j++)if(dis64u32(b.signature.data+12+4*j)>Dis64Real)return 0;
 }
 if(v.definitions.bytes!=36 || dis64u32(data+v.definitions.offset)!=1 ||
    !dis64bindingrecord(data+v.definitions.offset+4,32,1,&b,&used) || used!=32 || b.entry || b.frame ||
    b.symbol!=v.imports.count+3 || !dis64commandname(&v.strings,b.symbol,"init") || !dis64commandsignature(&b.signature))return 0;
 v.exports=(Dis64Bindings){data+v.definitions.offset,v.definitions.bytes,1,1,2};
 v.frame=frame.bytes;v.count=dis64u32(data+v.code.offset);
 if(!v.count || v.count>Dis64ClientCode || !(v.localcount?dis64clientlocalprove(&v):dis64clientprove(&v,1)))return 0;
 *out=v;return 1;
}
/* Fixed Command.init interface; the implementation name is independent. */
static int
dis64commandcheck(const unsigned char *data,unsigned int bytes,Dis64ScalarClientView *out)
{
 Dis64ScalarClientView v={0};Dis64Section s,module={0},types={0};Dis64Type frame,mp;Dis64Binding b;
 DisRuneView name;unsigned int i,j,c,seen=0;
 if(data && bytes>=32 && dis64u32(data+28)==6)return dis64commandimportcheck(data,bytes,out);
 if(!out || bytes>Dis64CallImageBytes || !dis64check(data,bytes,0,5,0,0,&v.envelope) || v.envelope.sections!=5)return 0;
 for(i=0;i<5;i++){
  if(!dis64section(&v.envelope,i,&s) || s.kind>7 || (seen&(1U<<s.kind)))return 0;seen|=1U<<s.kind;
  switch(s.kind){case Dis64Module:module=s;break;case Dis64Code:v.code=s;break;case Dis64Types:types=s;break;case Dis64Exports:v.requests=s;break;case Dis64Strings:v.names=s;break;default:return 0;}
 }
 if(!dis64modulecheck(data+module.offset,module.bytes,&v.module) || v.module.profile || v.module.name || v.module.datatype!=1 ||
    !dis64typescheck(data+types.offset,types.bytes,13,4096,&v.types) || !dis64typeget(&v.types,0,&frame) || !dis64typeget(&v.types,1,&mp) ||
    frame.bytes<96 || frame.align!=8 || mp.align!=8 ||
    !disstringpoolcheck(data+v.names.offset,v.names.bytes,65536,&v.strings) ||
    !dis64clientbindingscheck(data+v.requests.offset,v.requests.bytes,1,&v.strings,&v.types,0,1,2,&v.imports) ||
    !dis64clientbindingget(&v.imports,0,&b) || !dis64commandsignature(&b.signature) || !dis64clientdatacheck(&v) || v.code.bytes<4)return 0;
 for(i=0;i<4;i++){
  if(!disstringpoolget(&v.strings,i,&name) || !name.count || name.count>255)return 0;
  for(j=0;j<name.count;j++)if(!disruneat(&name,j,&c) || !c || c>127)return 0;
 }
 v.frame=frame.bytes;v.count=dis64u32(data+v.code.offset);if(!v.count || v.count>Dis64ClientCode || !(v.localcount?dis64clientlocalprove(&v):dis64clientprove(&v,1)))return 0;
 *out=v;return 1;
}
/* Match the fixed request interface and complete signature bytes, not the
 * implementation label: e.g. load Command may select a module named Wm. */
static int
dis64commandmatch(const Dis64ScalarClientView *v,const unsigned char *requests,unsigned int requestbytes,
 const unsigned char *names,unsigned int namebytes)
{
 DisStringPoolView pool;Dis64Bindings table;Dis64Binding a,b;
 if(!disstringpoolcheck(names,namebytes,65536,&pool) ||
    !dis64clientbindingscheck(requests,requestbytes,0,&pool,0,0,1,2,&table) ||
    !dis64clientbindingget(&table,0,&a) || !dis64commandsignature(&a.signature) ||
    !dis64clientbindingget(v->importing?&v->exports:&v->imports,0,&b) || a.signature.bytes!=b.signature.bytes)return 0;
 return !memcmp(a.signature.data,b.signature.data,a.signature.bytes);
}
