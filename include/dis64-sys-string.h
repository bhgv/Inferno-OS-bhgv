/* Private String-to-Word Sys file trial; no public signature/tag assignment. */
#ifndef DIS64_SYS_STRING_H
#define DIS64_SYS_STRING_H
#include "dis64-managed-signature.h"

static int
dis64sysstringname(const DisStringPoolView *pool, unsigned int index, const char *text)
{
    DisRuneView name;
    unsigned int i, rune;
    if(!disstringpoolget(pool,index,&name)) return 0;
    for(i=0; i<name.count; i++)
        if(text[i] == 0 || !disruneat(&name,i,&rune) || rune != (unsigned char)text[i]) return 0;
    return text[i] == 0;
}

/* One complete private structural record; never pass String tags to the scalar reader. */
static int
dis64sysstringtable(const unsigned char *data, unsigned int bytes, unsigned int exports,
    const DisStringPoolView *pool, const Dis64TypeTable *types, unsigned int instructions,
    unsigned int limit, unsigned int parameters, Dis64Bindings *result, const void *context)
{
    Dis64Binding binding;
    unsigned int used;
    (void)types; (void)instructions; (void)context;
    if(exports || limit == 0 || parameters == 0 || bytes != 32 || dis64u32(data) != 1 ||
       !dis64bindingrecord(data+4,bytes-4,0,&binding,&used) || used != bytes-4 ||
       binding.module != 0 || binding.symbol != 1 || binding.signature.bytes != 16 ||
       dis64u32(binding.signature.data) != Dis64TrialVersion ||
       dis64u32(binding.signature.data+4) != 1 ||
       dis64u32(binding.signature.data+8) != Dis64Int ||
       dis64u32(binding.signature.data+12) != Dis64TrialString ||
       !dis64sysstringname(pool,0,"$Sys") || !dis64sysstringname(pool,1,"werrstr")) return 0;
    *result = (Dis64Bindings){data,bytes,1,0,parameters};
    return 1;
}

static int
dis64sysstringcode(const unsigned char *data, unsigned int bytes, unsigned int limit, Dis64CodeView *result)
{
    Dis64CallDecoded decoded;
    unsigned int i, at = 4, used, count;
    if(bytes < 4) return 0;
    count = dis64u32(data);
    if(count < 7 || count > 6*Dis64ManagedCalls+1 || count > limit) return 0;
    for(i=0; i<count; i++){
        if(at > bytes || !dis64callread(data+at,bytes-at,&decoded,&used)) return 0;
        at += used;
    }
    if(at != bytes) return 0;
    *result = (Dis64CodeView){data,bytes,count};
    return 1;
}

/* Every call is preceded by a mapped String source and one complete pending frame.
 * No control-flow edge or arbitrary pointer/result store is admitted. */
static int
dis64sysstringview(const Dis64StorageImage *image, Dis64ManagedLiteralsView *literals)
{
    Dis64Type frame;
    Dis64CallDecoded decoded;
    Dis64Instruction in;
    DisRuneView name;
    unsigned int i, at = 4, used, calls = 0, phase = 0, pending = 0, rune;
    if(image->module.profile != 1 || image->module.entry != 0 || image->module.name != 2 ||
       image->imports.count != 1 || image->exports.count != 0 || image->initializers.count != 0 || image->data.align != 8 ||
       image->types.count != 2 || image->module.frametype != 0 || image->module.datatype != 1 ||
       !dis64typeget(&image->types,0,&frame) || frame.bytes != 80 || frame.align != 8 ||
       frame.mapbytes != 2 || frame.map[0] != 0 || frame.map[1] != 0x80 ||
       !dis64sysstringname(&image->strings,3,"$Sys") ||
       !disstringpoolget(&image->strings,2,&name) || name.count == 0 || name.count > 255) return 0;
    for(i=0; i<name.count; i++)
        if(!disruneat(&name,i,&rune) || rune == 0 || rune > 127) return 0;
    for(i=0; i<image->code.count; i++){
        if(at > image->code.bytes || !dis64callread(image->code.data+at,image->code.bytes-at,&decoded,&used)) return 0;
        at += used; in = decoded.instruction;
        if(in.op == ILOAD && (i == 0 || (phase == 1 && !pending))){
            if(in.mode != (SRC(AMP)|AXIMM|DST(AMP)) || in.middle != 0 || in.source != 16 || in.destination != 0) return 0;
            phase = 1; pending = 1; continue;
        }
        if(i == image->code.count-1){
            if(in.op != IEXIT || in.mode != (SRC(AXXX)|DST(AXXX)) || phase != 1 || pending || calls == 0) return 0;
            break;
        }
        switch(phase){
        case 1:
            if(in.op != IMOVP || in.mode != (SRC(AMP)|DST(AFP)) || in.destination != 64 ||
               (in.source != 8 && (in.source < 24 || in.source%8 || (unsigned long)in.source >= image->data.bytes))) return 0;
            phase = 2; break;
        case 2:
            if(in.op != IMFRAME || in.mode != (SRC(AMP)|AXIMM|DST(AFP)) || in.source != 0 || in.middle != 0 || in.destination != 40) return 0;
            phase = 3; break;
        case 3:
            if(in.op != IMOVP || in.mode != (SRC(AFP)|DST(AFP|AIND)) || in.source != 64 || in.destination != 40 || decoded.secondary != 64) return 0;
            phase = 4; break;
        case 4:
            if(in.op != ILEA || in.mode != (SRC(AFP)|DST(AFP|AIND)) || in.source != 72 || in.destination != 40 || decoded.secondary != 32) return 0;
            phase = 5; break;
        case 5:
            if(in.op != IMCALL || in.mode != (SRC(AFP)|AXIMM|DST(AMP)) || in.source != 40 || in.middle != 0 || in.destination != 0 || ++calls > Dis64ManagedCalls) return 0;
            phase = 1; pending = 0; break;
        default: return 0;
        }
    }
    if(at != image->code.bytes || image->strings.count != 4+(image->data.bytes-24)/8) return 0;
    return dis64managedliteralview(image,literals);
}
#endif
