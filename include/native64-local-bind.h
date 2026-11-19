#ifndef NATIVE64_LOCAL_BIND_H
#define NATIVE64_LOCAL_BIND_H
#include "native64-model.h"
#include "native64-stat-result.h"
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
    VmFetch fetched;Inst instruction;Frame *pending;Type *caller,*callee,*source;Stkext *extent;
    uintptr base,start,top,target,at,bit,sourcebase;unsigned int width,reference,contract,altaddress;
    void *address,*destination;
    if(!vmfetch(&fetched))return 0;
    if(fetched.module->cstep!=native64typedlea8step)return 0;
    if(fetched.module->native64sysstep!=nil){
        if(fetched.module->native64sysstep==(int(*)(void))H)error(exCompile);
        if(fetched.module->native64sysstep())return 1;
    }
    if(fetched.op==ICASEC && fetched.module->native64casec!=nil){
        if(fetched.module->native64casec==(int(*)(void))H)error(exCompile);
        return fetched.module->native64casec();
    }
    if(fetched.op!=ILEA)return 0;
    memmove(&instruction,fetched.pc,sizeof(instruction));contract=instruction.reg;altaddress=!!(contract&256);if(altaddress)contract&=~256;width=contract==72?24:contract==88?88:contract==48?16:contract&15;reference=contract==72 || !!(contract&16);
    if(!native64model() || (altaddress?((instruction.add!= (SRC(AFP)|DST(AFP)) && instruction.add!=(SRC(AMP)|DST(AFP))) || (contract!=4 && contract!=24 && contract!=48)):
        ((instruction.add!=(SRC(AFP)|DST(AFP|AIND)) && !(contract==24 && instruction.add==(SRC(AMP)|DST(AFP|AIND)))) || instruction.d.i.f!=40 || instruction.d.i.s!=32)) ||
       (contract!=48 && contract!=72 && contract!=88 && ((contract&~31) || (width!=1 && width!=4 && width!=8) || (reference && width!=8))) || R.FP==nil || R.FP==H)error(exBounds);
    base=(uintptr)R.FP;
    if(base%8 || base>(uintptr)-1-sizeof(Frame))error(exBounds);
    memmove(&caller,R.FP+O(Frame,t),sizeof(caller));
    if(caller==nil){
        if(base<OA(Stkext,reg.tos.fu))error(exBounds);
        memmove(&caller,(void*)(base-OA(Stkext,reg.tos.fu)+O(Stkext,reg.TR)),sizeof(caller));
    }
    if(!native64localtype(caller) || base>(uintptr)-1-caller->size)error(exBounds);
    source=caller;sourcebase=base;
    if(USRC(instruction.add)==AMP){
        if(fetched.module->ntype<=0 || fetched.module->type==nil || fetched.module->type==H || fetched.module->mptype<0 || fetched.module->mptype>=fetched.module->ntype)error(exBounds);
        source=fetched.module->type[fetched.module->mptype];sourcebase=(uintptr)R.MP;
        if(source==nil || source==H || source->ref<=0 || source->size<8 || source->size>1024*1024 || source->np<0 || source->np>(source->size+63)/64 || !sourcebase || R.MP==H || sourcebase%8 || sourcebase>(uintptr)-1-source->size)error(exBounds);
    }
    if(instruction.s.ind<(USRC(instruction.add)==AMP?0:64) ||
       (unsigned int)instruction.s.ind>(unsigned int)source->size || width>(unsigned int)source->size-instruction.s.ind || instruction.s.ind%((contract==48 || contract==72 || contract==88)?8:width))error(exBounds);
    for(at=instruction.s.ind;at<(unsigned int)instruction.s.ind+width;at++){
        bit=at/8;
        if(!!(bit/8<(unsigned int)source->np && (source->map[bit/8]&(0x80>>(bit%8))))!=(contract==88?(at>=(unsigned int)instruction.s.ind+8 && at<(unsigned int)instruction.s.ind+40):reference))error(exBounds);
    }
    if(altaddress){
        extent=(Stkext*)R.EX;top=(uintptr)R.SP;
        if(extent==nil || extent==H || (uintptr)extent%sizeof(void*) || (uintptr)extent>(uintptr)-1-OA(Stkext,reg.tos.fu) ||
           R.TS==nil || R.TS==H || top>(uintptr)R.TS)error(exBounds);
        start=(uintptr)extent+OA(Stkext,reg.tos.fu);
        if(base<start || base>top || (uintptr)caller->size>top-base || base+caller->size!=top)error(exBounds);
        if(instruction.d.ind<64 || instruction.d.ind%8 || (unsigned int)instruction.d.ind>(unsigned int)caller->size || 8>(unsigned int)caller->size-instruction.d.ind)error(exBounds);
        bit=instruction.d.ind/8;if(bit/8<(unsigned int)caller->np && (caller->map[bit/8]&(0x80>>(bit%8))))error(exBounds);
        address=(void*)(sourcebase+instruction.s.ind);destination=R.FP+instruction.d.ind;
        vmdecode(&fetched);if(R.s!=address || R.d!=destination)error(exBounds);
        memmove(destination,&address,sizeof(address));return 1;
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
    address=(void*)(sourcebase+instruction.s.ind);destination=(void*)(target+32);
    vmdecode(&fetched);
    if(R.s!=address || R.d!=destination)error(exBounds);
    memmove(destination,&address,sizeof(address));
    return 1;
}
#endif
