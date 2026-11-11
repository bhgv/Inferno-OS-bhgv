#ifndef INFERNO_NATIVE64_SYS_FRAME_H
#define INFERNO_NATIVE64_SYS_FRAME_H // Trusted active-VM bridge; not a serialized variadic calling convention.

#include "native64-sys-call.h"

typedef struct Native64SysFrameCall Native64SysFrameCall;
struct Native64SysFrameCall {
    Native64SysFrameRequest request; // Borrowed immutable call-site recipe captured before callbacks.
    Native64TailPlan plan;
    const LimboNative64Entry *fixed;
    uvlong epoch; // Detect guarded activation invalidation without claiming a scheduler lifetime lease.
    Modlink *module; // Exact active native module retained by MCALL.
    void *extent; // This first bridge accepts caller and callee in one current extent.
    REG *live;
    int pid;
    int operation; // Private selector: sprint/aprint/print/fprint; never serialized.
};

/* Validate complete trusted runtime descriptors without accepting compressed or partial maps. */
static int
native64sysframetype(Type *type)
{
    DisAbi abi;
    if(type == nil || type == H || (uintptr)type%sizeof(void*) != 0 ||
       (uintptr)type > (uintptr)-1-O(Type, map) || type->ref <= 0 ||
       type->size < 64 || type->size > Native64BindFrame || type->size%8 != 0 ||
       type->np != (type->size+63)/64 || (uintptr)type+O(Type, map) > (uintptr)-1-type->np || type->map[0] != 0)
        return 0;
    return disabinit(&abi, 8) && disabmap(&abi, type->size, type->map, type->np);
}

/* Check an already rooted readable frame Type against fixed+typed-tail ownership before reading payload. */
static int
native64sysframemap(Type *type, const LimboNative64Entry *fixed,
    const Native64TailSlot *slots, const Native64TailPlan *plan)
{
    unsigned int i, bit, offset, index, expected;
    if(!native64sysframetype(type) || type->size != plan->size || type->np != plan->map_bytes)
        return 0;
    for(i=0; i<plan->map_bytes; i++) {
        expected = 0;
        for(bit=0; bit<8; bit++) {
            offset = i*64+bit*8;
            if(offset < plan->tail) {
                if(fixed->map[i] & (0x80>>bit)) expected |= 0x80>>bit;
            } else if(offset < plan->size) {
                index = (offset-plan->tail)/8;
                if(slots[index].kind == Native64TailRef) expected |= 0x80>>bit;
            }
        }
        if(type->map[i] != expected) return 0;
    }
    return 1;
}

/* Re-find both exact frame boundaries and the declared result cell after every allocating callback.
 * REG/extent/Type storage is trusted live VM memory; numeric bounds do not authenticate C pointers.
 * The call-site recipe owns its Type and slot identities through this synchronous native activation. */
static void*
native64sysframecell(void *argument)
{
    Native64SysFrameCall *call = argument;
    Native64SysFrameRequest *request = &call->request;
    uintptr first, stop, top, cursor, end, callee, caller, result;
    Frame frame, active;
    Stkext *extent;
    Type *type;
    void *address;
    unsigned int bytes, slot;
    int foundcallee = 0, foundcaller = 0;

    if(request->prog != currun() || request->prog->state != Pready || request->prog->kill != nil ||
       request->prog->pid <= 0 || request->prog->pid != call->pid ||
       request->prog->native64live != call->live || (call->live != nil && call->live != &R) || request->prog->native64holds != 0 || request->prog->native64epoch != call->epoch || call->epoch == ~(uvlong)0 ||
       (request->prog->flags & Pkilled) || R.M != call->module || call->module->prog != nil || R.FP != request->frame ||
       R.EX != call->extent || R.EX == nil || R.EX == H || R.SP == nil || R.TS == nil)
        return nil;
    extent = (Stkext*)R.EX;
    first = (uintptr)R.EX;
    if(first%sizeof(void*) != 0 || first > (uintptr)-1-OA(Stkext, reg.tos.fu)) return nil;
    first += OA(Stkext, reg.tos.fu);
    stop = (uintptr)R.SP; top = (uintptr)R.TS; callee = (uintptr)R.FP;
    if(stop < first || top < stop || top-(uintptr)R.EX > Native64BindFrame ||
       callee < first || callee > stop || stop-callee < sizeof(Frame)) return nil;
    memmove(&active, (void*)callee, sizeof(active));
    caller = (uintptr)active.fp;
    if(caller < first || caller >= callee || active.mr == nil || active.mr == H) return nil;
    for(cursor=first; cursor<stop; cursor=end) {
        if(stop-cursor < sizeof(Frame)) return nil;
        memmove(&frame, (void*)cursor, sizeof(frame));
        type = frame.t == nil ? extent->reg.TR : frame.t;
        if(!native64sysframetype(type) || (unsigned int)type->size > stop-cursor) return nil;
        end = cursor+type->size;
        if(cursor == callee) {
            if(type != request->type || end != stop) return nil;
            foundcallee = 1;
        }
        if(cursor == caller) {
            if(type != request->caller_type) return nil;
            foundcaller = 1;
        }
    }
    if(!native64sysframemap(request->type, call->fixed, request->slots, &call->plan)) return nil;
    bytes = call->operation >= 2 ? sizeof(WORD) : sizeof(void*);
    if(!foundcallee || !foundcaller || request->result_offset < 64 || request->result_offset%bytes != 0 ||
       request->result_offset > (unsigned int)request->caller_type->size ||
       bytes > (unsigned int)request->caller_type->size-request->result_offset) return nil;
    slot = request->result_offset/8;
    if(!!(request->caller_type->map[slot/8] & (0x80>>(slot%8))) != (call->operation < 2)) return nil;
    result = caller+request->result_offset;
    memmove(&address, (uchar*)request->frame+32, sizeof(address));
    return (uintptr)address == result ? (void*)result : nil;
}

/* Borrow only an exact current VM frame, then let the existing gateways snapshot and root its values.
 * Slot kinds come from trusted compiler/C metadata, never inferred from maps or the format string. */
static int
native64sysframe(const Native64SysFrameRequest *request, int operation)
{
    Native64SysFrameCall call;
    Native64SysWriteRequest output = {0};
    const LimboNative64Prefix *prefix;
    String *pattern;
    void *fd;
    unsigned int offset;

    if(!native64model() || request == nil || request->prog == nil || request->prog == H || request->prog != currun() ||
       request->initial <= UTFmax+2 || request->limit < request->initial ||
       request->frame == nil || request->frame == H || (uintptr)request->frame%8 != 0 ||
       R.M == nil || R.M == H || R.M->prog != nil || operation < 0 || operation > 3)
        return 0;
    prefix = operation == 0 ? &limbo_native64_Sys_sprint_prefix :
        operation == 1 ? &limbo_native64_Sys_aprint_prefix :
        operation == 2 ? &limbo_native64_Sys_print_prefix : &limbo_native64_Sys_fprint_prefix;
    memset(&call, 0, sizeof(call));
    call.request = *request;
    call.fixed = &prefix->fixed;
    call.operation = operation;
    call.module = R.M;
    call.extent = R.EX;
    call.epoch = request->prog->native64epoch;
    call.pid = request->prog->pid;
    call.live = request->prog->native64live;
    if(!native64tailplan(&prefix->fixed, request->slots, request->count, &call.plan) ||
       !native64sysframecell(&call)) return 0;
    offset = operation == 3 ? offsetof(F_Sys_fprint, s) : offsetof(F_Sys_sprint, s);
    memmove(&pattern, (uchar*)request->frame+offset, sizeof(pattern));
    fd = H;
    if(operation == 3) memmove(&fd, (uchar*)request->frame+offsetof(F_Sys_fprint, fd), sizeof(fd));
    output.format.prog = request->prog; output.format.pattern = pattern;
    output.format.slots = request->slots; output.format.count = request->count;
    output.format.values = (const uvlong*)((uchar*)request->frame+call.plan.tail);
    output.format.value_bytes = request->count*8;
    output.format.resolve = native64sysframecell; output.format.argument = &call;
    output.format.initial = request->initial; output.format.limit = request->limit;
    output.fd = fd;
    request->prog->R = R; // Publish the native activation before constructor/formatter collection.
    switch(operation) {
    case 0: return Sys_sprint64(&output.format);
    case 1: return Sys_aprint64(&output.format);
    case 2: return Sys_print64(&output);
    default: return Sys_fprint64(&output);
    }
}
#endif
