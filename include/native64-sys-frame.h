#ifndef INFERNO_NATIVE64_SYS_FRAME_H
#define INFERNO_NATIVE64_SYS_FRAME_H // Trusted active-VM bridge; not a serialized variadic calling convention.

#include "native64-sys-call.h"

typedef struct Native64SysFrameCall Native64SysFrameCall;
struct Native64SysFrameCall {
    Native64SysFrameRequest request; // Borrowed immutable call-site recipe captured before callbacks.
    Native64TailPlan plan;
    const LimboNative64Entry *fixed;
    uvlong epoch; // Detect guarded activation invalidation without claiming a scheduler lifetime lease.
    Modlink *module; // Exact current module; native only after MCALL.
    void *extent; // Current callee extent, stable while resolving this activation.
    REG *live;
    int pid;
    int entered; // Pending FRAME binding or entered MCALL result resolution.
    void *borrowed; // Stable result identity after explicit inherited preparation.
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

enum { Native64SysStackDepth = 64 };

typedef struct Native64SysStack Native64SysStack;
struct Native64SysStack {
    const Stkext *extent;
    uintptr stop, top;
    unsigned int depth;
};

/* Locate an exact frame boundary using only the live stack and saved extents.
 * The cursor never searches a younger extent after starting from an ancestor. */
static int
native64sysstackframe(Native64SysStack *stack, uintptr address, Frame *frame, Type **type)
{
    const Stkext *extent;
    uintptr base, first, cursor, end;
    Frame item;
    Type *t;
    int found;

    for(; stack->depth<Native64SysStackDepth; stack->depth++){
        extent=stack->extent;
        base=(uintptr)extent;
        if(extent==nil || extent==H || base%sizeof(void*) ||
           base>(uintptr)-1-OA(Stkext,reg.tos.fu))return 0;
        first=base+OA(Stkext,reg.tos.fu);
        if(stack->stop<first || stack->top<stack->stop || stack->top-base>Native64BindFrame)return 0;
        if(address>=first && address<stack->stop){
            found=0;
            for(cursor=first;cursor<stack->stop;cursor=end){
                if(stack->stop-cursor<sizeof(item))return 0;
                memmove(&item,(void*)cursor,sizeof(item));
                t=item.t;
                if(t==nil && cursor==first)t=extent->reg.TR;
                if(!native64sysframetype(t) || (unsigned int)t->size>stack->stop-cursor)return 0;
                end=cursor+t->size;
                if(cursor==address){
                    *frame=item;
                    *type=t;
                    found=1;
                }
            }
            return found;
        }
        stack->stop=(uintptr)extent->reg.SP;
        stack->top=(uintptr)extent->reg.TS;
        stack->extent=(const Stkext*)extent->reg.EX;
        if(stack->extent==extent)return 0;
    }
    return 0;
}

/* The return cell must belong to the active caller chain, including when
 * FRAME grew a new extent between the result owner and this Sys call. */
static int
native64sysancestor(Native64SysStack stack, uintptr caller, const Frame *frame,
    uintptr base, unsigned int span, unsigned int bias, unsigned int contract,
    unsigned int width, int reference)
{
    Frame parent=*frame;
    Type *type;
    uintptr ancestor, end;
    unsigned int depth, previous, slot, k;

    if(base%width)return 0;
    for(depth=0;depth<Native64SysStackDepth;depth++){
        ancestor=(uintptr)parent.fp;
        previous=stack.depth;
        if(!native64sysstackframe(&stack,ancestor,&parent,&type) ||
           (stack.depth==previous && ancestor>=caller))return 0;
        end=ancestor+type->size;
        if(base>=ancestor+64 && base<=end && span<=end-base){
            if(contract)
                for(k=0;k<span;k+=8){
                    slot=(base+k-ancestor)/8;
                    if(!!(type->map[slot/8]&(128>>(slot%8)))!=(contract!=80||!k))return 0;
                }
            slot=(base+bias-ancestor)/8;
            return !!(type->map[slot/8]&(128>>(slot%8)))==reference;
        }
        caller=ancestor;
    }
    return 0;
}

/* Re-find both exact frame boundaries and the declared result cell after every allocating callback.
 * REG/extent/Type storage is trusted live VM memory; numeric bounds do not authenticate C pointers.
 * The call-site recipe owns its Type and slot identities through this synchronous native activation. */
static void*
native64sysframecell(void *argument)
{
    Native64SysFrameCall *call = argument;
    Native64SysFrameRequest *request = &call->request;
    uintptr callee, caller, result, base;
    Frame frame, active;
    Native64SysStack stack;
    Type *type;
    void *address;
    unsigned int bytes, slot, span, bias;

    if(request->prog != currun() || request->prog->state != Pready || request->prog->kill != nil ||
       request->prog->pid <= 0 || request->prog->pid != call->pid ||
       request->prog->native64live != call->live || (call->live != nil && call->live != &R) || request->prog->native64holds != 0 || request->prog->native64epoch != call->epoch || call->epoch == ~(uvlong)0 ||
       (request->prog->flags & Pkilled) || R.M != call->module ||
       (call->entered && (call->module->prog != nil || R.FP != request->frame)) ||
       R.EX != call->extent || R.EX == nil || R.EX == H || R.SP == nil || R.TS == nil)
        return nil;
    stack=(Native64SysStack){(Stkext*)R.EX,(uintptr)R.SP,(uintptr)R.TS,0};
    callee=(uintptr)request->frame;
    if(!native64sysstackframe(&stack,callee,&active,&type) || stack.depth!=0 ||
       type!=request->type || type->size!=stack.stop-callee)return nil;
    caller = call->entered ? (uintptr)active.fp : (uintptr)R.FP;
    if((call->entered ? active.mr==nil || active.mr==H : active.mr!=nil) ||
       !native64sysstackframe(&stack,caller,&frame,&type) || type!=request->caller_type ||
       (!stack.depth && caller>=callee))return nil;
    if(!native64sysframemap(request->type, call->fixed, request->slots, &call->plan)) return nil;
    bytes = call->operation >= 2 ? sizeof(WORD) : sizeof(void*);
    if(request->inherited){
        span=request->result_contract==124?24:request->result_contract?16:bytes;
        bias=request->result_offset-32;
        if(!native64sysresultshape(request->result_contract,request->result_offset,call->operation,1))return nil;
        memmove(&address,(void*)(caller+32),sizeof(address));base=(uintptr)address;
        if(base>(uintptr)-1-bias)return nil;result=base+bias;
        if((call->borrowed && (void*)result!=call->borrowed) ||
           !native64sysancestor(stack,caller,&frame,base,span,bias,request->result_contract,
                                bytes,call->operation<2))return nil;
    }else{
        if(request->result_offset<64 || request->result_offset%bytes ||
           request->result_offset>(unsigned int)request->caller_type->size || bytes>(unsigned int)request->caller_type->size-request->result_offset)return nil;
        slot=request->result_offset/8;
        if(!!(request->caller_type->map[slot/8]&(128>>(slot%8)))!=(call->operation<2))return nil;
        result=caller+request->result_offset;
    }
    if(!call->entered) return (void*)result;
    memmove(&address, (uchar*)request->frame+32, sizeof(address));
    return (uintptr)address == result ? (void*)result : nil;
}

/* Capture one trusted call-site recipe for a pending or entered activation. */
static int
native64sysframeprepare(Native64SysFrameCall *call, const Native64SysFrameRequest *request,
    int operation, int entered)
{
    const LimboNative64Prefix *prefix;void *result;
    if(!native64model() || request == nil || request->prog == nil || request->prog == H || request->prog != currun() ||
       (request->inherited!=0 && request->inherited!=1) || !native64sysresultshape(request->result_contract,request->result_offset,operation,request->inherited) || request->initial <= UTFmax+2 || request->limit < request->initial ||
       request->frame == nil || request->frame == H || (uintptr)request->frame%8 != 0 ||
       R.M == nil || R.M == H || (entered && R.M->prog != nil) || operation < Native64SysSprint || operation > Native64SysFprint)
        return 0;
    prefix = operation == 0 ? &limbo_native64_Sys_sprint_prefix :
        operation == 1 ? &limbo_native64_Sys_aprint_prefix :
        operation == 2 ? &limbo_native64_Sys_print_prefix : &limbo_native64_Sys_fprint_prefix;
    memset(call, 0, sizeof(*call));
    call->entered = entered;
    call->request = *request;
    call->fixed = &prefix->fixed;
    call->operation = operation;
    call->module = R.M;
    call->extent = R.EX;
    call->epoch = request->prog->native64epoch;
    call->pid = request->prog->pid;
    call->live = request->prog->native64live;
    if(!native64tailplan(&prefix->fixed, request->slots, request->count, &call->plan) ||
       !(result=native64sysframecell(call))) return 0;
    if(request->inherited)call->borrowed=result;
    return 1;
}

/* Bind an eight-byte result address only after complete pending-frame/caller checks.
 * No allocation/callback can invalidate the checked address before this single write. */
static int
native64sysframebind(const Native64SysFrameRequest *request, int operation)
{
    Native64SysFrameCall call;
    void *result;
    if(!native64sysframeprepare(&call, request, operation, 0)) return 0;
    result = native64sysframecell(&call);
    if(result == nil) return 0;
    memmove((uchar*)call.request.frame+32, &result, sizeof(result));
    return 1;
}

/* Borrow only an exact current VM frame, then let the existing gateways snapshot and root its values.
 * Slot kinds come from trusted compiler/C metadata, never inferred from maps or the format string. */
static int
native64sysframe(const Native64SysFrameRequest *request, int operation)
{
    Native64SysFrameCall call;
    Native64SysWriteRequest output = {0};
    String *pattern;
    void *fd;
    unsigned int offset;

    if(!native64sysframeprepare(&call, request, operation, 1)) return 0;
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
