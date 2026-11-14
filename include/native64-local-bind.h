#ifndef NATIVE64_LOCAL_BIND_H
#define NATIVE64_LOCAL_BIND_H
#include "native64-model.h"
/* Trusted VM frames/Types under synchronous exclusion. No file capability. */
static int
native64localtype(Type *t)
{
    unsigned int i,b;
    if(t==nil || t==H || t->ref<=0 || t->size<64 || t->size>1024*1024 || t->size%8 ||
       t->np<0 || t->np>(t->size+63)/64 || (t->np && t->map[0]))return 0;
    for(i=0;i<(unsigned int)t->np;i++)for(b=0;b<8;b++)
        if((t->map[i]&(0x80>>b)) && i*64+b*8+8>(unsigned int)t->size)return 0;
    return 1;
}

/* Called before vmdecode/JIT. Only an explicitly selected trusted owner can
 * reinterpret this private LEA as an eight-byte address publication. */
static int
native64localbind(void)
{
    VmFetch fetched;Inst instruction;Frame *pending;Type *caller,*callee;Stkext *extent;
    uintptr base,start,top,target,at,bit;unsigned int width,reference,contract;
    void *address,*destination;
    if(!vmfetch(&fetched) || fetched.op!=ILEA)return 0;
    if(fetched.module->cstep!=native64typedlea8step)return 0;
    memmove(&instruction,fetched.pc,sizeof(instruction));contract=instruction.reg;width=contract&15;reference=!!(contract&16);
    if(!native64model() || instruction.add!=(SRC(AFP)|DST(AFP|AIND)) || instruction.d.i.f!=40 || instruction.d.i.s!=32 ||
       (contract&~31) || (width!=1 && width!=4 && width!=8) || (reference && width!=8) || R.FP==nil || R.FP==H)error(exBounds);
    base=(uintptr)R.FP;
    if(base%8 || base>(uintptr)-1-sizeof(Frame))error(exBounds);
    memmove(&caller,R.FP+O(Frame,t),sizeof(caller));
    if(caller==nil){
        if(base<OA(Stkext,reg.tos.fu))error(exBounds);
        memmove(&caller,(void*)(base-OA(Stkext,reg.tos.fu)+O(Stkext,reg.TR)),sizeof(caller));
    }
    if(!native64localtype(caller) || base>(uintptr)-1-caller->size || instruction.s.ind<64 ||
       (unsigned int)instruction.s.ind>(unsigned int)caller->size || width>(unsigned int)caller->size-instruction.s.ind || instruction.s.ind%width)error(exBounds);
    for(at=instruction.s.ind;at<(unsigned int)instruction.s.ind+width;at++){
        bit=at/8;
        if(!!(bit/8<(unsigned int)caller->np && (caller->map[bit/8]&(0x80>>(bit%8))))!=reference)error(exBounds);
    }
    extent=(Stkext*)R.EX;top=(uintptr)R.SP;
    if(extent==nil || extent==H || (uintptr)extent%sizeof(void*) ||
       (uintptr)extent>(uintptr)-1-OA(Stkext,reg.tos.fu) || R.TS==nil || R.TS==H || top>(uintptr)R.TS)error(exBounds);
    start=(uintptr)extent+OA(Stkext,reg.tos.fu);
    memmove(&pending,R.FP+40,sizeof(pending));target=(uintptr)pending;
    if(target<start || target>top || top-target<64 || target%8)error(exBounds);
    if(base>=start && base<top){if(target!=base+caller->size)error(exBounds);}
    else if(target!=start || (uintptr)extent->reg.SP!=base+caller->size)error(exBounds);
    memmove(&callee,(uchar*)pending+O(Frame,t),sizeof(callee));
    if(callee==nil && target==start)callee=extent->reg.TR;
    if(!native64localtype(callee) || (uintptr)callee->size!=top-target || pending->mr!=nil)error(exBounds);
    address=(void*)(base+instruction.s.ind);destination=(void*)(target+32);
    vmdecode(&fetched);
    if(R.s!=address || R.d!=destination)error(exBounds);
    memmove(destination,&address,sizeof(address));
    return 1;
}
#endif
