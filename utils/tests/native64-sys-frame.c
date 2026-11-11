/* Real MFRAME/MCALL owners, with bounded current-extent and scheduler transports. */
#include "native64-sys-frame.h"
char exModule[] = "module";
REG R;
Prog *currun(void) { return &process; }
void extend(void) { abort(); } // Growth across extents is deliberately refused by this first bridge.
void unextend(Frame *frame) { USED(frame); abort(); }
void Roots_bridge(void *frame) { USED(frame); abort(); }

VM_FRAME_IMPLEMENTATION

static Native64SysFrameRequest vmrequest;
static int vmoperation, vmmode, vmstatus, vmcalls;
static Type *vmtype;
static void vmcheck(int ok, int line)
{
    if(!ok) fprintf(stderr, "VM frame predicate: operation=%d mode=%d line=%d status=%d\n", vmoperation, vmmode, line, vmstatus);
    check(ok);
}
#undef check
#define check(test) vmcheck(!!(test), __LINE__)



static int vmhook;
static void vmmonitor(int freeing, void *pointer, ulong bytes)
{
    contextmonitor(freeing, pointer, bytes);
    if(!freeing && vmhook) {
        vmhook = 0;
        if(vmmode == 21) process.kill = "VM frame cancelled";
        if(vmmode == 23) process.native64epoch++;
        if(vmmode == 22) { void *bad = (void*)1; memmove((uchar*)vmrequest.frame+32, &bad, sizeof(bad)); }
    }
}
static void vmwritechange(void)
{
    void *bad = (void*)1;
    if(vmmode == 21) process.kill = "VM frame cancelled";
    else memmove((uchar*)vmrequest.frame+32, &bad, sizeof(bad));
}

/* The compiled trusted call-site recipe supplies kinds/Types; no format or map inference. */
static void vmcallback(void *frame)
{
    Type *type = vmtype;
    uchar map = type->map[1];
    int size = type->size, state = process.state;
    Type *actual = ((Frame*)frame)->t;
    uchar *caller = ((Frame*)frame)->fp;
    vmrequest.frame = frame;
    check(D2H(R.M)->ref == 2);
    if(vmmode == 1) type->map[1] ^= 0x20;
    if(vmmode == 2) type->size += 8;
    if(vmmode == 9) process.state = Palt;
    if(vmmode == 19) ((Frame*)frame)->t = nil;
    if(vmmode == 24) process.native64holds = 1;
    if(vmmode == 25) process.flags |= Pkilled;
    if(vmmode == 27) vmrequest.frame = (void*)1;
    if(vmmode == 31) ((Frame*)frame)->fp = R.EX;
    vmcalls++;
    switch(vmoperation) {
    case 0: vmstatus = Sys_sprintframe64(&vmrequest); break;
    case 1: vmstatus = Sys_aprintframe64(&vmrequest); break;
    case 2: vmstatus = Sys_printframe64(&vmrequest); break;
    default: vmstatus = Sys_fprintframe64(&vmrequest); break;
    }
    type->map[1] = map; type->size = size; process.state = state;
    ((Frame*)frame)->t = actual; ((Frame*)frame)->fp = caller; process.native64holds = 0; process.flags &= ~Pkilled;
}

static int vmframe(void *context) { USED(context); mframe(); return 1; }
static int vmcall(void *context) { USED(context); mcall(); return 1; }

static void vmcase(int phase, int operation, int mode)
{
    const LimboNative64Prefix *prefix;
    Native64TailSlot slots[150];
    Native64TailPlan plan;
    Type *caller_type, *callee_type, *fdtype = nil;
    F_Roots_bridge *caller;
    Frame *pending;
    String *pattern, *text, *old = H;
    Sys_FD *fd = H;
    Array *oldarray = H;
    void *result, *bad;
    uchar map[32], *beforefp;
    uvlong bits;
    WORD ordinal = 0;
    Inst next = {0};
    unsigned int count = mode == 14 ? 150 : 2, i, offset;
    int baseline, before, expected, completed;

    contextstart(phase, Pready);
    process.pid = 99;
    nativewriteobserver = nil; vmhook = 0;
    vmoperation = operation; vmmode = mode; vmstatus = -1; vmcalls = 0;
    writemode = mode == 10 ? 4 : mode == 17 ? 15 : 0;
    writefprint = operation == 3;
    releasedvm = releases = acquires = writes = writeresolves = movedword = fdretired = 0;
    nativecount = -99; memset(nativebytes, 0x5a, sizeof(nativebytes));
    caller_type = native64frametype(&limbo_native64_Roots_bridge_entry);
    check(caller_type != nil);
    caller = (F_Roots_bridge*)stackframes(caller_type, 1);
    caller->count = 77; caller->canary = 88;
    pattern = newstring(5); memmove(pattern->Sascii, "%d|%s", 6); ptradd(D2H(pattern));
    text = newstring(2); memmove(text->Sascii, "ok", 3); caller->text = text;
    if(operation == 0) {
        old = newstring(3); memmove(old->Sascii, "old", 4); caller->stringresult = old;
    } else if(operation == 1) caller->arrayresult = oldarray = mem2array("old", 3);
    prefix = operation == 0 ? &limbo_native64_Sys_sprint_prefix : operation == 1 ? &limbo_native64_Sys_aprint_prefix :
        operation == 2 ? &limbo_native64_Sys_print_prefix : &limbo_native64_Sys_fprint_prefix;
    for(i=0; i<count; i++) slots[i] = (Native64TailSlot){Native64TailInt, i*8, 8, nil};
    slots[1].kind = Native64TailRef; slots[1].type = &Tstring;
    if(count == 150) {
        slots[70].kind = slots[149].kind = Native64TailRef;
        slots[70].type = slots[149].type = &Tstring;
    }
    check(native64tailplan(&prefix->fixed, slots, count, &plan));
    check(plan.map_bytes <= sizeof(map) && native64tailmap(&prefix->fixed, slots, count, map, sizeof(map)));
    callee_type = dtype(freeheap, plan.size, map, plan.map_bytes); check(callee_type != nil);
    if(operation == 3) {
        uchar empty[] = {0};
        TFD = fdtype = dtype(freeheap, sizeof(Sys_FD), empty, 1); check(fdtype != nil);
        if(mode != 17) { fd = H2D(Sys_FD*, heapz(fdtype)); fd->fd = 7; }
    }
    rootlink->prog = nil; rootlink->nlinks = 1;
    rootlink->links[0].frame = callee_type; rootlink->links[0].u.runt = vmcallback;
    R = process.R; R.FP = (uchar*)caller; R.TS = stackspace.bytes+sizeof(stackspace.bytes); R.PC = &next;
    R.s = &rootlink; R.m = &ordinal; R.d = &pending;
    beforefp = R.FP;
    check(kprotect(vmframe, nil) && pending != nil && pending->t == callee_type);
    offset = operation == 3 ? offsetof(F_Sys_fprint, s) : offsetof(F_Sys_sprint, s);
    native64argumentstore((uchar*)pending+offset, pattern, &Tstring);
    if(operation == 3) {
        memmove((uchar*)pending+64, &fd, sizeof(fd)); // Transfer the sole FD owner into the mapped VM frame.
        if(fd != H) Setmark(D2H(fd));
    }
    for(i=0; i<count; i++) {
        if(slots[i].kind == Native64TailRef)
            native64argumentstore((uchar*)pending+plan.tail+i*8, i == 70 ? H : text, &Tstring);
        else { bits = i == 0 ? (uvlong)(vlong)-1 : 0; memmove((uchar*)pending+plan.tail+i*8, &bits, 8); }
    }
    result = operation == 0 ? (void*)&caller->stringresult : operation == 1 ? (void*)&caller->arrayresult : (void*)&caller->count;
    memmove((uchar*)pending+32, &result, sizeof(result));
    memset(&vmrequest, 0, sizeof(vmrequest));
    vmtype = callee_type; vmrequest.prog = &process; vmrequest.type = callee_type; vmrequest.caller_type = caller_type;
    vmrequest.slots = slots; vmrequest.count = count; vmrequest.initial = 8; vmrequest.limit = 128;
    vmrequest.result_offset = (uchar*)result-(uchar*)caller;
    if(mode == 3) vmrequest.count = Native64BindEntries+1;
    if(mode == 4) { bits = 0xffffffffULL; memmove((uchar*)pending+plan.tail, &bits, 8); }
    if(mode == 5) slots[1].type = &Tarray;
    if(mode == 6) { bad = (void*)1; memmove((uchar*)pending+32, &bad, sizeof(bad)); }
    if(mode == 7) vmrequest.caller_type = &Tbyte;
    if(mode == 8) vmrequest.type = &Tbyte;
    if(mode == 11) vmrequest.prog = nil;
    if(mode == 12) vmrequest.result_offset = 32;
    if(mode == 13) vmrequest.initial = 0;
    if(mode == 15) vmrequest.slots = nil;
    if(mode == 16) slots[0].kind = Native64TailReal;
    if(mode == 18) caller->regs.t = nil; // Ordinary root-frame descriptor comes from extent->reg.TR.
    if(mode == 20) vmrequest.result_offset++;
    if(mode == 23 || ((mode == 21 || mode == 22) && operation < 2)) { heapmonitor = vmmonitor; vmhook = 1; }
    if(mode == 26) vmrequest.prog = (Prog*)1;
    if(mode == 28) vmrequest.result_offset += 4;
    if(mode == 29) vmrequest.limit = 8;
    if(mode == 30) {
        vmrequest.result_offset = operation < 2 ? offsetof(F_Roots_bridge, count) : offsetof(F_Roots_bridge, stringresult);
        bad = (uchar*)caller+vmrequest.result_offset;
        memmove((uchar*)pending+32, &bad, sizeof(bad));
    }
    if((mode == 21 || mode == 22) && operation >= 2) nativewriteobserver = vmwritechange;
    baseline = contextlive; before = contextattempt;
    R.s = &pending; R.m = &ordinal; R.d = &rootlink;
    allocationcollections = 1;
    completed = kprotect(vmcall, nil);
    check(completed == (mode != 21));
    if(mode == 21) { check(strcmp(up->env->errstr, "VM frame cancelled") == 0); process.kill = nil; }
    nativewriteobserver = nil;
    allocationcollections = 0;
    check(D2H(rootlink)->ref == 1);
    check(vmcalls == 1 && R.FP == beforefp && R.M == rootlink && R.SP == (uchar*)pending);
    check(contextlive == baseline && errdepth == 0 && D2H(text)->ref == 1 && D2H(pattern)->ref == 1);
    expected = mode == 0 || mode == 14 || mode == 17 || mode == 18 || (mode == 10 && operation < 2);
    check(vmstatus == expected);
    if(mode == 1 || mode == 2 || mode == 3 || mode == 4 || mode == 5 || mode == 6 || mode == 7 || mode == 8 ||
       mode == 9 || mode == 11 || mode == 12 || mode == 13 || mode == 15 || mode == 19 || mode == 20 || mode == 24 || mode == 25 || mode == 26 || mode == 27 || mode == 28 || mode == 30 || mode == 31) check(contextattempt == before);
    if(operation < 2 && expected) {
        if(operation == 0) {
            String *value = caller->stringresult;
            check(value != old && value->len == 5 && memcmp(value->Sascii, "-1|ok", 6) == 0);
        } else check(caller->arrayresult->len == 5 && memcmp(caller->arrayresult->data, "-1|ok", 5) == 0);
    } else if(operation == 0) check(caller->stringresult == old);
    else if(operation == 1) check(caller->arrayresult == oldarray);
    if(operation >= 2) {
        check(writes == (mode == 0 || mode == 10 || mode == 14 || mode == 17 || mode == 18 || mode == 21 || mode == 22));
        check(releases == writes && acquires == writes && !releasedvm);
        check(caller->count == (expected ? operation == 3 && mode == 17 ? -1 : 5 : 77));
    }
    check(caller->canary == 88);
    ptrdel(D2H(pattern)); destroy(pattern);
    freeptrs(caller, caller_type); initmem(caller_type, caller);
    process.R = R; process.R.EX = process.R.SP = process.R.FP = nil;
    rootlink->nlinks = 0; rootlink->links[0].frame = nil; rootlink->links[0].u.runt = nil;
    memset(&R, 0, sizeof(R));
    epochs(6); freetype(caller_type); freetype(callee_type); freetype(fdtype); TFD = nil; clearrootcache();
    check(contextlive == 0 && Tstring.ref == 1 && Tarray.ref == 1 && Tbyte.ref == 1 && errdepth == 0);
    heapmonitor = nil;
}

int main(int argc, char **argv)
{
    int phase, operation, mode, cases = 0;
    native64_syswrite_main(argc, argv);
    for(phase=0; phase<3; phase++) for(operation=0; operation<4; operation++) for(mode=0; mode<32; mode++) {
        vmcase(phase, operation, mode); cases++;
    }
    printf("PASS typed Sys VM frames: %d real MFRAME/MCALL/GC cases, four gateways, 150-slot frames, refusal and result/cleanup checks\n", cases);
    return 0;
}
