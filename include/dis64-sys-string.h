/* Private fixed Sys String/Word/FD file trial; no public signature/tag assignment. */
#ifndef DIS64_SYS_STRING_H
#define DIS64_SYS_STRING_H
#include "dis64-managed-signature.h"
#include "dis64-sys-string-flow.h"

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

/* Only the selected generated fixed entries belong to this trial. */
static const char*
dis64sysstringmethod(const DisStringPoolView *pool, unsigned int symbol)
{
    unsigned int i;
    for(i=0; i<12; i++) if(dis64sysstringname(pool,symbol,dis64sysfixednames[i])) return dis64sysfixednames[i];
    return 0;
}

/* Fixed private result/argument trees, checked independently of consistency hashes. */
static int
dis64sysstringtable(const unsigned char *data, unsigned int bytes, unsigned int exports,
    const DisStringPoolView *pool, const Dis64TypeTable *types, unsigned int instructions,
    unsigned int limit, unsigned int parameters, Dis64Bindings *result, const void *context)
{
    Dis64Binding binding;
    unsigned int i, count, at=4, used, seen=0, signaturebytes;
    unsigned char signature[40];
    int kind;
    (void)types; (void)instructions; (void)context;
    if(exports || parameters == 0 || bytes < 4 || !dis64sysstringname(pool,0,"$Sys")) return 0;
    count = dis64u32(data);
    if(count == 0 || count > 12 || count > limit) return 0;
    for(i=0; i<count; i++){
        if(at > bytes || !dis64bindingrecord(data+at,bytes-at,0,&binding,&used) ||
           binding.module != 0 || binding.symbol != (i ? 3+i : 1)) return 0;
        kind = dis64sysfixedkind(dis64sysstringmethod(pool,binding.symbol));
        if(kind < 0) return 0;
        signaturebytes=dis64sysfixedsignature(kind,signature);
        if(binding.signature.bytes != signaturebytes || memcmp(binding.signature.data,signature,signaturebytes)) return 0;
        if(seen & (1U<<kind)) return 0;
        seen |= 1U<<kind; at += used;
    }
    if(at != bytes) return 0;
    *result = (Dis64Bindings){data,bytes,count,0,parameters};
    return 1;
}

static int
dis64sysstringcode(const unsigned char *data, unsigned int bytes, unsigned int limit, Dis64CodeView *result)
{
    Dis64CallDecoded decoded;
    unsigned int i, at = 4, used, count;
    if(bytes < 4) return 0;
    count = dis64u32(data);
    if(count < 5 || count > Dis64ClockInstructions || count > limit) return 0;
    for(i=0; i<count; i++){
        if(at > bytes || !dis64callread(data+at,bytes-at,&decoded,&used)) return 0;
        at += used;
    }
    if(at != bytes) return 0;
    *result = (Dis64CodeView){data,bytes,count};
    return 1;
}

/* Check complete fixed call groups and their mapped argument/result roots.
 * Word and FD facts are checked separately after typed projection. */
static int
dis64sysstringview(const Dis64StorageImage *image, Dis64ManagedLiteralsView *literals)
{
    Dis64Type frame;
    Dis64CallDecoded decoded;
    Dis64Instruction in, flow[Dis64ClockInstructions];
    unsigned char boundary[Dis64ClockInstructions];
    DisRuneView name;
    unsigned int i, at = 4, used, calls = 0, phase = 0, pending = 0, rune, words = 0, ordinal = 0, seen = 0, parameter = 0;
    int kinds[12];
    int extended, pairroots=0, fdroots=0;
    long fdargument=96, nilargument=0;
    if(image->module.profile != 1 || image->module.entry != 0 || image->module.name != 2 ||
       (image->imports.count == 0 || image->imports.count > 12) || image->exports.count != 0 || image->initializers.count != 0 || image->data.align != 8 ||
       image->types.count != 2 || image->module.frametype != 0 || image->module.datatype != 1 ||
       !dis64typeget(&image->types,0,&frame) || (frame.bytes != 80 && frame.bytes != 112) || frame.align != 8 ||
       frame.mapbytes != 2 || frame.map[0] != 0 || (frame.map[1] != 0x80 && frame.map[1] != 0xc0 && frame.map[1] != 0x88 && frame.map[1] != 0xc8) ||
       !dis64sysstringname(&image->strings,3,"$Sys") ||
       !disstringpoolget(&image->strings,2,&name) || name.count == 0 || name.count > 255) return 0;
    extended = frame.bytes == 112;
    for(i=0; i<image->imports.count; i++){
        kinds[i] = dis64sysfixedkind(dis64sysstringmethod(&image->strings,i ? 3+i : 1));
        if(kinds[i] == 6 || kinds[i] == 7) pairroots=1;
        if(kinds[i] >= 8) fdroots=1;
        if(kinds[i] < 0 || (!extended && kinds[i] >= 3)) return 0;
    }
    if(frame.map[1] != ((pairroots ? 0xc0 : 0x80)|(fdroots ? 8 : 0)) || ((pairroots || fdroots) && !extended)) return 0;
    for(i=0; i<name.count; i++)
        if(!disruneat(&name,i,&rune) || rune == 0 || rune > 127) return 0;
    for(i=0; i<image->code.count; i++){
        if(at > image->code.bytes || !dis64callread(image->code.data+at,image->code.bytes-at,&decoded,&used)) return 0;
        at += used; in = decoded.instruction; flow[i] = in; boundary[i] = phase == 1;
        if(in.op == ILOAD && (i == 0 || (phase == 1 && (!pending || extended)))){
            if(in.mode != (SRC(AMP)|AXIMM|DST(AMP)) || in.middle != 0 || in.source != 16 || in.destination != 0) return 0;
            if(++words > Dis64ClockWords) return 0;
            phase = 1; pending = 1; continue;
        }
        if(i == image->code.count-1){
            if(in.op != IEXIT || in.mode != (SRC(AXXX)|DST(AXXX)) || phase != 1 || (pending && !extended) || calls == 0) return 0;
            break;
        }
        if(phase == 1 && fdroots && in.op == IMOVP && in.mode == (SRC(AMP)|DST(AFP)) && in.source == 8 && in.destination == 96){
            if(++words > Dis64ClockWords) return 0;
            flow[i]=(Dis64Instruction){IMOVW,SRC(AIMM)|DST(AFP),0,0,96};continue;
        }
        if(in.op == IMFRAME && phase == 1){
            if(in.mode != (SRC(AMP)|AXIMM|DST(AFP)) || in.source != 0 || in.destination != 40 ||
               in.middle < 0 || (unsigned long)in.middle >= image->imports.count || (kinds[in.middle] < 3 || kinds[in.middle] == 6 || kinds[in.middle] == 7 || kinds[in.middle] >= 10)) return 0;
            ordinal = in.middle; seen |= 1U<<ordinal; parameter=0; fdargument=96;
            phase = dis64sysfixedargs(kinds[ordinal]) ? 6 : 4; continue;
        }
        if(phase == 6 && kinds[ordinal] == 9){
            if(in.op != IMOVP || in.mode != (SRC(AFP)|DST(AFP|AIND)) || in.source != fdargument ||
               in.destination != 40 || decoded.secondary != 64) return 0;
            flow[i]=(Dis64Instruction){IMOVW,SRC(fdargument == 96 ? AFP : AIMM)|DST(AFP|AIND),0,fdargument == 96 ? 96 : 0,40};
            phase=4;continue;
        }
        if(phase == 6 && dis64sysfixedstring(kinds[ordinal],parameter)){
            if(in.op != IMOVP || in.mode != (SRC(AFP)|DST(AFP|AIND)) || in.source != 64+8*parameter ||
               in.destination != 40 || decoded.secondary != 64+8*parameter) return 0;
            flow[i] = (Dis64Instruction){IMOVW,SRC(AIMM)|DST(AFP|AIND),0,0,40};
            if(++parameter == dis64sysfixedargs(kinds[ordinal])) phase=4;
            continue;
        }
        if(phase == 6){
            if(in.op != IMOVW || in.destination != 40 || decoded.secondary != dis64sysfixedoffset(kinds[ordinal],parameter) ||
               (in.mode != (SRC(AFP)|DST(AFP|AIND)) && in.mode != (SRC(AIMM)|DST(AFP|AIND)))) return 0;
            if(USRC(in.mode) == AFP){
                flow[i].source = dis64sysstringcell(in.source,0); if(flow[i].source < 0) return 0;
            }
            if(++parameter == dis64sysfixedargs(kinds[ordinal])) phase = 4;
            continue;
        }
        if(phase == 1 && (dis64clockwordop(in.op) || dis64clockbranch(in.op))){
            if(!extended || ++words > Dis64ClockWords || !dis64sysstringword(&in,0) ||
               !dis64instructioncheck(&in,image->code.count,0,80)) return 0;
            flow[i] = in; continue;
        }
        switch(phase){
        case 1:
            if(in.op != IMOVP || in.mode != (SRC(AMP)|DST(AFP)) || in.destination != 64 ||
               (in.source != 8 && (in.source < 24 || in.source%8 || (unsigned long)in.source >= image->data.bytes))) return 0;
            flow[i] = (Dis64Instruction){ILOAD,0,0,0,0};
            nilargument=in.source == 8; phase = 2; break;
        case 2:
            if(pairroots && in.op == IMOVP){
                if(in.mode != (SRC(AMP)|DST(AFP)) || in.destination != 72 ||
                   (in.source != 8 && (in.source < 24 || in.source%8 || (unsigned long)in.source >= image->data.bytes))) return 0;
                flow[i] = (Dis64Instruction){ILOAD,0,0,0,0}; phase=7; break;
            }
            if(in.op != IMFRAME || in.mode != (SRC(AMP)|AXIMM|DST(AFP)) || in.source != 0 || in.middle < 0 || (unsigned long)in.middle >= image->imports.count || in.destination != 40) return 0;
            ordinal = in.middle; if(kinds[ordinal] >= 3 && (kinds[ordinal] < 10 && (kinds[ordinal] != 9 || !nilargument))) return 0; seen |= 1U<<ordinal;
            fdargument=64; parameter=0; phase = kinds[ordinal] == 9 || kinds[ordinal] >= 10 ? 6 : 3; break;
        case 7:
            if(in.op != IMFRAME || in.mode != (SRC(AMP)|AXIMM|DST(AFP)) || in.source != 0 ||
               in.middle < 0 || (unsigned long)in.middle >= image->imports.count || in.destination != 40 || (kinds[in.middle] != 6 && kinds[in.middle] != 7)) return 0;
            ordinal=in.middle; seen |= 1U<<ordinal; parameter=0; phase=6; break;
        case 3:
            if(in.op != IMOVP || in.mode != (SRC(AFP)|DST(AFP|AIND)) || in.source != 64 || in.destination != 40 || decoded.secondary != 64) return 0;
            flow[i] = (Dis64Instruction){IMOVW,SRC(AIMM)|DST(AFP|AIND),0,0,40};
            phase = 4; break;
        case 4:
            if(in.op != ILEA || in.mode != (SRC(AFP)|DST(AFP|AIND)) || in.source != (dis64sysfixedfdresult(kinds[ordinal]) ? 96 : (long)frame.bytes-8) || in.destination != 40 || decoded.secondary != 32) return 0;
            phase = 5; break;
        case 5:
            if(in.op != IMCALL || in.mode != (SRC(AFP)|AXIMM|DST(AMP)) || in.source != 40 || in.middle != ordinal || in.destination != 0 || ++calls > Dis64ManagedCalls) return 0;
            if(dis64sysfixedfdresult(kinds[ordinal])) flow[i].middle=-1;
            phase = 1; pending = 0; break;
        default: return 0;
        }
    }
    for(i=0; i<image->code.count; i++) if(dis64clockbranch(flow[i].op)){
        if(flow[i].destination <= 0 || (unsigned long)flow[i].destination >= image->code.count ||
           !boundary[flow[i].destination]) return 0;
    }
    if(!dis64clockflow(flow,image->code.count)) return 0;
    if(at != image->code.bytes || image->strings.count != 3+image->imports.count+(image->data.bytes-24)/8 || seen != (1U<<image->imports.count)-1) return 0;
    return dis64managedliteralview(image,literals);
}
#endif
