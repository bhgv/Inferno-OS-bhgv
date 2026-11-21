#ifndef NATIVE64_LOCAL_BIND_H
#define NATIVE64_LOCAL_BIND_H
#include "native64-model.h"
#include "native64-fileio-layout.h"
#include "native64-stat-result.h"
#include "native64-wm-tuple.h"
#include "native64-geometry-result.h"
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

#include "native64-raw-array-runtime.h"

/* Called before vmdecode/JIT. Only an explicitly selected trusted owner can
 * reinterpret this private LEA as an eight-byte address publication. */
static int
native64localbind(void)
{
    VmFetch fetched;Inst instruction;Frame *pending;Type *caller,*callee,*source;Stkext *extent;
    uintptr base,start,top,target,at,bit,sourcebase;unsigned int width,reference,contract,retbias,altaddress,windowaddress,fieldaddress,arrayaddress,sourcewidth,sourceroot;long offset;
    void *owner;Type *ownertype;
    void *address,*destination;
    if(!vmfetch(&fetched))return 0;
    if(fetched.module->cstep!=native64typedlea8step)return 0;
    if(native64rawarraystep(&fetched))return 1;
    if(fetched.module->native64sysstep!=nil){
        if(fetched.module->native64sysstep==(int(*)(void))H)error(exCompile);
        if(fetched.module->native64sysstep())return 1;
    }
    if(fetched.op==ICASEC && fetched.module->native64casec!=nil){
        if(fetched.module->native64casec==(int(*)(void))H)error(exCompile);
        return fetched.module->native64casec();
    }
    if(fetched.op!=ILEA)return 0;
    memmove(&instruction,fetched.pc,sizeof(instruction));contract=instruction.reg;retbias=!!(contract&4096);if(retbias)contract&=~4096;windowaddress=!!(contract&2048);if(windowaddress)contract&=~2048;arrayaddress=!!(contract&1024);if(arrayaddress)contract&=~1024;fieldaddress=!!(contract&512);if(fieldaddress)contract&=~512;altaddress=!!(contract&256);if(altaddress)contract&=~256;width=native64wmtuplewidth(contract)?native64wmtuplewidth(contract):native64geometryresultwidth(contract)?native64geometryresultwidth(contract):native64fileiowidth(contract)?native64fileiowidth(contract):contract==72?24:contract==88?88:contract==48?16:contract&15;reference=!native64wmtuplewidth(contract) && !native64geometryresultwidth(contract) && (contract==72 || !!(contract&16));
    if(retbias&&(windowaddress||arrayaddress||fieldaddress||altaddress||contract!=96||instruction.add!=(SRC(AFP|AIND)|DST(AFP|AIND))||instruction.s.i.f!=32||instruction.s.i.s!=8))error(exBounds);
    if(fieldaddress && (altaddress || (contract!=24 && contract!=4 && contract!=96 && contract!=112) || instruction.add!=(SRC(AFP|AIND)|DST(AFP|AIND)) || (windowaddress?(contract!=24||!native64windowrootfield(instruction.s.i.s)):(contract==96||contract==112)?!native64windowgeometryfield(instruction.s.i.s,width):contract==4?instruction.s.i.s!=12:(instruction.s.i.s!=24&&instruction.s.i.s!=88))))error(exBounds);
    if(windowaddress&&(!fieldaddress||altaddress||arrayaddress))error(exBounds);
    if(arrayaddress && (fieldaddress || altaddress || contract!=24 || instruction.add!=(SRC(AFP)|DST(AFP|AIND))))error(exBounds);
    sourcewidth=(fieldaddress||arrayaddress||retbias)?8:width;sourceroot=(fieldaddress||arrayaddress)?1:reference;
    offset=(fieldaddress||retbias)?instruction.s.i.f:instruction.s.ind;
    if(!native64model() || (altaddress?((instruction.add!= (SRC(AFP)|DST(AFP)) && instruction.add!=(SRC(AMP)|DST(AFP))) || (contract!=4 && contract!=24 && contract!=48 && contract!=72 && contract!=104 && contract!=120 && contract!=136)):
        ((instruction.add!=(SRC(AFP)|DST(AFP|AIND)) && !((contract==24 || native64geometryresultwidth(contract)) && instruction.add==(SRC(AMP)|DST(AFP|AIND))) && !fieldaddress && !retbias) || instruction.d.i.f!=40 || instruction.d.i.s!=32)) ||
       (!native64wmtuplewidth(contract) && !native64geometryresultwidth(contract) && !native64fileiowidth(contract) && contract!=48 && contract!=72 && contract!=88 && ((contract&~31) || (width!=1 && width!=4 && width!=8) || (reference && width!=8))) || R.FP==nil || R.FP==H)error(exBounds);
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
    if(offset<(retbias?32:USRC(instruction.add)==AMP?0:64) ||
       (unsigned int)offset>(unsigned int)source->size || sourcewidth>(unsigned int)source->size-offset || offset%(fieldaddress?8:native64geometryresultwidth(contract)?4:(contract==48 || contract==72 || contract==88 || native64wmtuplewidth(contract) || native64fileiowidth(contract))?8:width))error(exBounds);
    for(at=offset;at<(unsigned int)offset+sourcewidth;at++){
        bit=at/8;
        if(!!(bit/8<(unsigned int)source->np && (source->map[bit/8]&(0x80>>(bit%8))))!=(native64wmtuplewidth(contract)?native64wmtupleroot(contract,at-offset):native64fileiowidth(contract)?native64fileioroot(contract,at-offset):contract==88?(at>=(unsigned int)offset+8 && at<(unsigned int)offset+40):sourceroot))error(exBounds);
    }
    if(altaddress){
        extent=(Stkext*)R.EX;top=(uintptr)R.SP;
        if(extent==nil || extent==H || (uintptr)extent%sizeof(void*) || (uintptr)extent>(uintptr)-1-OA(Stkext,reg.tos.fu) ||
           R.TS==nil || R.TS==H || top>(uintptr)R.TS)error(exBounds);
        start=(uintptr)extent+OA(Stkext,reg.tos.fu);
        if(base<start || base>top || (uintptr)caller->size>top-base || base+caller->size!=top)error(exBounds);
        if(instruction.d.ind<64 || instruction.d.ind%8 || (unsigned int)instruction.d.ind>(unsigned int)caller->size || 8>(unsigned int)caller->size-instruction.d.ind)error(exBounds);
        bit=instruction.d.ind/8;if(bit/8<(unsigned int)caller->np && (caller->map[bit/8]&(0x80>>(bit%8))))error(exBounds);
        address=(void*)(sourcebase+offset);destination=R.FP+instruction.d.ind;
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
    address=(void*)(sourcebase+offset);
    if(retbias){memmove(&address,address,sizeof(address));if(address==nil||address==H||(uintptr)address%4||(uintptr)address>(uintptr)-1-16)error(exBounds);address=(uchar*)address+8;}
    /* The local mapped owner stays in this caller frame across the import.
     * A global owner could be replaced while the callee is suspended. */
    if(fieldaddress){
        memmove(&owner,address,sizeof(owner));if(owner==H)error(exNilref);
        if(owner==nil || (uintptr)owner%8 || (uintptr)owner>(uintptr)-1-((windowaddress||native64geometryresultwidth(contract))?128:instruction.s.i.s==88?96:32) || D2H(owner)->ref==0)error(exBounds);
        ownertype=D2H(owner)->t;
        if(windowaddress||contract==96||contract==112){if(ownertype==nil||ownertype==H||ownertype->ref<=0||ownertype->size!=128||ownertype->np!=2||ownertype->map[0]!=0x92||ownertype->map[1]!=0xc4)error(exBounds);address=(uchar*)owner+instruction.s.i.s;}
        else if(contract==4){
            if(ownertype==nil || ownertype==H || ownertype->ref<=0 || ownertype->size!=16 || ownertype->np<0 || ownertype->np>1 || (ownertype->np && ownertype->map[0]))error(exBounds);
            address=(uchar*)owner+12;
        }else if(instruction.s.i.s==88){
            if(ownertype==nil||ownertype==H||ownertype->ref<=0||ownertype->size!=96||ownertype->np!=2||ownertype->map[0]!=0xf7||ownertype->map[1]!=0x90)error(exBounds);
            address=(uchar*)owner+88;
        }else{
            if(ownertype==nil || ownertype==H || ownertype->ref<=0 || ownertype->size!=32 || ownertype->np!=1 || ownertype->map[0]!=0x90)error(exBounds);
            address=(uchar*)owner+24;
        }
    }
    if(arrayaddress){
        Array*a;Type*t;uintptr data,slot,span;
        memmove(&owner,address,sizeof(owner));if(owner==H)error(exNilref);
        if(owner==nil || (uintptr)owner%8 || (uintptr)owner>(uintptr)-1-sizeof(Array) || D2H(owner)->ref==0 || D2H(owner)->t!=&Tarray)error(exBounds);
        a=owner;t=a->t;if(t==nil || t==H || t->ref<=0 || t->size!=8 || t->np!=1 || t->map[0]!=0x80 || a->len<0 || (uvlong)a->len>0x7fffffff/8)error(exBounds);
        data=(uintptr)a->data;span=(uintptr)a->len*8;memmove(&address,R.FP+48,sizeof(address));slot=(uintptr)address;
        if(!data || a->data==H || data>(uintptr)-1-span || slot<data || slot-data>=span || (slot-data)%8)error(exBounds);
    }
    destination=(void*)(target+32);
    vmdecode(&fetched);
    if(R.s!=(arrayaddress?(void*)(sourcebase+offset):address) || R.d!=destination)error(exBounds);
    memmove(destination,&address,sizeof(address));
    return 1;
}
#endif
