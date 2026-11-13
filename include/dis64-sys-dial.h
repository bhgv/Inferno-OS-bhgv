#ifndef DIS64_SYS_DIAL_H
#define DIS64_SYS_DIAL_H
#include "dis64-sys-string.h"

#include "dis64-sys-dial-schema.h"

static int
dis64sysdialtable(const unsigned char *data,unsigned int bytes,unsigned int exports,
    const DisStringPoolView *pool,const Dis64TypeTable *types,unsigned int instructions,
    unsigned int limit,unsigned int parameters,Dis64Bindings *result,const void *context)
{
    Dis64Binding b;
    unsigned char signature[72];
    unsigned int used,n=dis64sysdialsignature(signature);
    (void)types;(void)instructions;(void)parameters;(void)context;
    if(exports || limit < 1 || bytes < 4 || dis64u32(data)!=1 ||
       !dis64bindingrecord(data+4,bytes-4,0,&b,&used) || used!=bytes-4 ||
       b.module!=0 || b.symbol!=1 || b.signature.bytes!=n || memcmp(b.signature.data,signature,n) ||
       !dis64sysstringname(pool,0,"$Sys") || !dis64sysstringname(pool,1,"dial"))return 0;
    *result=(Dis64Bindings){data,bytes,1,0,2};return 1;
}

/* Every call owns two String arguments and a disjoint 32-byte result region. */
static int
dis64sysdialview(const Dis64StorageImage *image,Dis64ManagedLiteralsView *literals)
{
    Dis64Type frame;
    Dis64CallDecoded d;
    Dis64Instruction in;
    unsigned int i,at=4,used,phase=0,calls=0;
    DisRuneView name;
    if(image->module.profile!=1 || image->module.entry!=0 || image->module.name!=2 ||
       image->module.frametype!=0 || image->module.datatype!=1 || image->types.count!=2 ||
       image->exports.count || image->imports.count!=1 || image->initializers.count ||
       image->data.align!=8 || image->strings.count!=4+(image->data.bytes-24)/8 ||
       !dis64typeget(&image->types,0,&frame) || frame.bytes!=112 || frame.align!=8 ||
       frame.mapbytes!=2 || frame.map[0]!=0 || frame.map[1]!=0xdc ||
       !dis64sysstringname(&image->strings,3,"$Sys") ||
       !disstringpoolget(&image->strings,2,&name) || !name.count || name.count>255)return 0;
    for(i=0;i<name.count;i++){unsigned int rune;if(!disruneat(&name,i,&rune)||!rune||rune>127)return 0;}
    for(i=0;i<image->code.count;i++){
        if(!dis64callread(image->code.data+at,image->code.bytes-at,&d,&used))return 0;
        at+=used;in=d.instruction;
        if(i==image->code.count-1){if(in.op!=IEXIT || in.mode!=(SRC(AXXX)|DST(AXXX)) || phase!=1 || !calls)return 0;break;}
        if(in.op==ILOAD){
            if((phase!=0 && phase!=1) || in.mode!=(SRC(AMP)|AXIMM|DST(AMP)) || in.source!=16 || in.middle || in.destination)return 0;
            phase=1;continue;
        }
        if(phase==1 || phase==2){
            if(in.op!=IMOVP || in.mode!=(SRC(AMP)|DST(AFP)) || in.destination!=(phase==1?64:72) ||
               (in.source!=8 && (in.source<24 || in.source%8 || (unsigned long)in.source>=image->data.bytes)))return 0;
        }else if(phase==3){
            if(in.op!=IMFRAME || in.mode!=(SRC(AMP)|AXIMM|DST(AFP)) || in.source || in.middle || in.destination!=40)return 0;
        }else if(phase==4 || phase==5){
            if(in.op!=IMOVP || in.mode!=(SRC(AFP)|DST(AFP|AIND)) || in.source!=(phase==4?64:72) || in.destination!=40 || d.secondary!=in.source)return 0;
        }else if(phase==6){
            if(in.op!=ILEA || in.mode!=(SRC(AFP)|DST(AFP|AIND)) || in.source!=80 || in.destination!=40 || d.secondary!=32)return 0;
        }else if(phase==7){
            if(in.op!=IMCALL || in.mode!=(SRC(AFP)|AXIMM|DST(AMP)) || in.source!=40 || in.middle || in.destination || ++calls>Dis64ManagedCalls)return 0;
            phase=1;continue;
        }else return 0;
        phase++;
    }
    return at==image->code.bytes && dis64managedliteralview(image,literals);
}
#endif
