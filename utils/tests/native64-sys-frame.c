/* Real MFRAME/MCALL owners, with bounded current-extent and scheduler transports. */
#include "native64-sys-frame.h"
#include "native64-sys-sites.h"
#include "callsites-native64.h"
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
static int vmownedlive;
static Module *vmowner;
static int vmsiteowned(int mode)
{
    return mode == 0 || mode == 4 || mode == 10 || mode == 14 || mode == 21 || mode == 22 || mode == 23;
}

/* A separately installed table is valid metadata but cannot operate on another caller's frames. */
static int vmsitesforeign(Module *module, Prog *prog, void *frame, int entered)
{
    Module foreign = *module;
    Native64SysSites *table = module->native64sysframes;
    int status, before;
    foreign.native64sysframes = nil; foreign.native64sysframesfree = nil;
    check(Sys_setframes64(&foreign, &table->sites[0].spec, 1));
    before = contextattempt;
    status = entered ? Sys_callsite64(&foreign, 0, prog, frame) : Sys_bindsite64(&foreign, 0, prog, frame);
    check(contextattempt == before);
    check(Sys_clearframes64(&foreign));
    return status;
}

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
    if(vmsiteowned(vmmode)) {
        if(vmmode == 0 || vmmode == 14) check(!vmsitesforeign(vmowner, &process, frame, 1));
        vmstatus = Sys_callsite64(vmowner, 0, &process, frame);
    }
    else switch(vmoperation) {
    case 0: vmstatus = Sys_sprintframe64(&vmrequest); break;
    case 1: vmstatus = Sys_aprintframe64(&vmrequest); break;
    case 2: vmstatus = Sys_printframe64(&vmrequest); break;
    default: vmstatus = Sys_fprintframe64(&vmrequest); break;
    }
    type->map[1] = map; type->size = size; process.state = state;
    ((Frame*)frame)->t = actual; ((Frame*)frame)->fp = caller; process.native64holds = 0; process.flags &= ~Pkilled;
}

static int vmframe(void *context) { if(context) frame(); else mframe(); return 1; }
static int vmcall(void *context) { USED(context); mcall(); return 1; }

/* Refused pending bindings preserve all eight result-address bytes and reserved storage. */
static void vmbindrefusals(Native64SysFrameRequest *request, int operation)
{
    Native64SysFrameRequest candidate;
    REG registers = R;
    Frame *pending = request->frame;
    Modlink *mr = pending->mr;
    uvlong guard = 0x12345678deadbeefULL, observed, epoch = process.native64epoch;
    uchar reserved[24], map = request->type->map[1];
    int size = request->type->size, probe, before = contextattempt, owners = contextlive;
    memset(reserved, 0xa5, sizeof(reserved));
    memmove((uchar*)pending+40, reserved, sizeof(reserved));
    for(probe=0; probe<32; probe++) {
        candidate = *request;
        memmove((uchar*)pending+32, &guard, sizeof(guard));
        switch(probe) {
        case 0: candidate.frame = (void*)1; break;
        case 1: candidate.frame = R.FP; break;
        case 2: candidate.caller_type = &Tbyte; break;
        case 3: candidate.type = &Tbyte; break;
        case 4: candidate.result_offset = 32; break;
        case 5: candidate.result_offset++; break;
        case 6: candidate.result_offset = operation < 2 ? offsetof(F_Roots_bridge, count) : offsetof(F_Roots_bridge, stringresult); break;
        case 7: candidate.count = Native64BindEntries+1; break;
        case 8: candidate.slots = nil; break;
        case 9: request->type->map[1] ^= 0x20; break;
        case 10: request->type->size += 8; break;
        case 11: process.state = Palt; break;
        case 12: process.kill = "pending cancellation"; break;
        case 13: process.flags |= Pkilled; break;
        case 14: process.native64holds = 1; break;
        case 15: process.native64epoch = ~(uvlong)0; break;
        case 16: pending->mr = H; break;
        case 17: R.SP = (uchar*)R.SP-8; break;
        case 18: R.FP = request->frame; break;
        case 19: R.EX = nil; break;
        case 20: R.M = nil; break;
        case 21: candidate.prog = (Prog*)1; break;
        case 22: process.native64live = (REG*)1; break;
        case 23: R.TS = (uchar*)R.SP-1; break;
        case 24: candidate.frame = nil; break;
        case 25: R.FP = R.EX; break;
        case 26: candidate.limit = 0; break;
        case 27: candidate.result_offset = request->caller_type->size; break;
        case 28: candidate.prog = nil; break;
        }
        check(!Sys_bindframe64(probe == 29 ? nil : &candidate,
            probe == 30 ? (Native64SysFrameOperation)-1 : probe == 31 ? (Native64SysFrameOperation)4 : (Native64SysFrameOperation)operation));
        R = registers; request->type->size = size; request->type->map[1] = map;
        pending->mr = mr; process.state = Pready; process.kill = nil; process.flags &= ~Pkilled;
        process.native64holds = 0; process.native64epoch = epoch; process.native64live = nil;
        memmove(&observed, (uchar*)pending+32, sizeof(observed));
        check(observed == guard && memcmp((uchar*)pending+40, reserved, sizeof(reserved)) == 0);
        check(contextattempt == before && contextlive == owners && writes == 0 && errdepth == 0);
    }
}

static void vmsitesmonitor(int freeing, void *pointer, ulong bytes)
{
    contextmonitor(freeing, pointer, bytes);
    if(vmownedlive && !freeing) check(!Sys_clearframes64(vmowner));
}

static void vmsitesinstall(Module *module, Native64SysFrameRequest *request, int operation)
{
    Native64SysFrameSpec spec = {0}, bad, pair[2];
    Module *detached;
    int i, before, owners, callerrefs = request->caller_type->ref, calleerefs = request->type->ref;
    int stringrefs = Tstring.ref;
    spec.operation = operation; spec.type = request->type; spec.caller_type = request->caller_type;
    spec.result_offset = request->result_offset; spec.slots = request->slots; spec.count = request->count;
    spec.initial = request->initial; spec.limit = request->limit;
    before = contextattempt; owners = contextlive;
    for(i=0; i<8; i++) {
        bad = spec;
        if(i == 0) bad.operation = (Native64SysFrameOperation)4;
        if(i == 1) bad.type = &Tbyte;
        if(i == 2) bad.caller_type = &Tbyte;
        if(i == 3) bad.result_offset = 32;
        if(i == 4) bad.result_offset++;
        if(i == 5) bad.count = 4097;
        if(i == 6) bad.slots = nil;
        check(!Sys_setframes64(module, &bad, i == 7 ? 0 : 1));
        check(contextattempt == before && contextlive == owners && module->native64sysframes == nil);
    }
    for(i=1; i<=2; i++) {
        contextfailure = contextattempt+i;
        check(!Sys_setframes64(module, &spec, 1));
        check(contextlive == owners && module->native64sysframes == nil && module->native64sysframesfree == nil);
        check(request->caller_type->ref == callerrefs && request->type->ref == calleerefs && Tstring.ref == stringrefs);
    }
    contextfailure = 0;
    pair[0] = pair[1] = spec;
    pair[1].count = 4097;
    before = contextattempt;
    check(!Sys_setframes64(module, pair, 2) && contextattempt == before);
    pair[1] = spec;
    contextfailure = contextattempt+3; // Fail after the first record has acquired its Type owners.
    check(!Sys_setframes64(module, pair, 2));
    contextfailure = 0;
    contextsitethrow = contextattempt+3;
    check(!Sys_setframes64(module, pair, 2));
    contextsitethrow = 0;
    check(errdepth == 0);
    check(contextlive == owners && module->native64sysframes == nil);
    check(request->caller_type->ref == callerrefs && request->type->ref == calleerefs && Tstring.ref == stringrefs);
    Tstring.ref = 0x7ffffffe;
    check(!Sys_setframes64(module, pair, 2)); // Duplicate reference reservations must not overflow.
    Tstring.ref = stringrefs;
    check(contextlive == owners && module->native64sysframes == nil);
    check(request->caller_type->ref == callerrefs && request->type->ref == calleerefs);
    Tstring.ref = 0x7fffffff;
    check(!Sys_setframes64(module, &spec, 1));
    Tstring.ref = stringrefs;
    check(contextlive == owners && module->native64sysframes == nil);
    check(request->caller_type->ref == callerrefs && request->type->ref == calleerefs);
    detached = mallocz(sizeof(Module), 1); check(detached != nil);
    detached->ref = 1; detached->ntype = 2;
    detached->type = mallocz(2*sizeof(Type*), 0); check(detached->type != nil);
    detached->type[0] = request->caller_type; detached->type[1] = request->type;
    request->caller_type->ref++; request->type->ref++;
    check(Sys_setframes64(detached, &spec, 1));
    native64_owned_freemod(detached); // Verbatim production metadata teardown, including its callback.
    check(contextlive == owners && request->caller_type->ref == callerrefs && request->type->ref == calleerefs && Tstring.ref == stringrefs);
    check(Sys_setframes64(module, &spec, 1));
    check(!Sys_setframes64(module, &spec, 1)); // No overwriting a live metadata owner.
    memset(&spec, 0xa5, sizeof(spec)); // Installation must not borrow this source record.
}

/* Source values still come from C, but each unique typed source is installed once.
 * Every generated operation then reads the compiler-selected native bank offset. */
static void vmstores(int phase, int aliased, int operation, unsigned int count, void *pending, String *pattern, String *text, void *fd)
{
    const Native64GeneratedStore *program = nil, *store;
    const Native64GeneratedBank *bank = nil;
    Native64Result input = {H, 0};
    Type *type;
    REG saved = R;
    uchar filled[4098];
    unsigned int i, length = count+(operation == 3 ? 2 : 1), sourceoffset, width;
    WORD number;
    void *reference, *previous;
    ulong refs;
    uvlong value;
    int baseline = contextlive;
#define STOREPROGRAM(OP) case OP: program = count == 150 ? callstores_##OP##_150 : callstores_##OP##_2; bank = count == 150 ? &callstores_##OP##_150_bank : &callstores_##OP##_2_bank; break
    switch(operation) { STOREPROGRAM(0); STOREPROGRAM(1); STOREPROGRAM(2); STOREPROGRAM(3); default: abort(); }
#undef STOREPROGRAM
    if(phase == 1 && operation == 0 && count == 2) { program = callliteralstores; bank = &callliteralstores_bank; }
    if(aliased) { program = callaliasstores; bank = &callaliasstores_bank; }
    check(bank->count <= nelem(filled)); memset(filled, 0, sizeof(filled));
    process.R = R; // Publish the pending frame before source-bank allocations/collection.
    if(fd != H) ptradd(D2H(fd));
    type = dtype(freeheap, bank->bytes, (uchar*)bank->map, bank->mapbytes); check(type != nil);
    check(native64resultadopt(&input, H2D(void*, heapz(type))));
    for(i=0; i<length; i++) {
        store = &program[i]; if(store->immediate) continue;
        check(store->cell >= 0 && (unsigned int)store->cell < bank->count);
        if(filled[store->cell]) continue;
        filled[store->cell] = 1; sourceoffset = store->source_offset;
        check(sourceoffset == 64+(unsigned int)store->cell*8 && sourceoffset+8 <= bank->bytes);
        R.d = (uchar*)input.value+sourceoffset;
        if(store->op == IMOVP) {
            reference = store->fixed ? operation == 3 && store->ordinal == 0 ? fd : pattern : text;
            memmove(&previous, R.d, sizeof(previous)); check(previous == H);
            R.s = &reference; movp();
        } else { number = -1; R.s = &number; movw(); }
        epochs(1);
    }
    for(i=0; i<bank->count; i++) check(filled[i]);
    for(i=0; i<length; i++) {
        store = &program[i]; R.d = (uchar*)pending+store->destination;
        number = (WORD)store->source;
        if(store->immediate) R.s = &number;
        else {
            sourceoffset = store->source_offset; // Compiler-selected source relocation.
            width = store->op == ICVTWL || store->op == ICVTWF ? sizeof(WORD) : 8;
            check(sourceoffset >= 64 && sourceoffset <= bank->bytes && width <= bank->bytes-sourceoffset);
            R.s = (uchar*)input.value+sourceoffset;
        }
        if(store->op == IMOVP) {
            memmove(&reference, R.s, sizeof(reference));
            memmove(&previous, R.d, sizeof(previous)); check(previous == H);
            refs = reference == H ? 0 : D2H(reference)->ref;
        }
        epochs(1);
        switch(store->op) {
        case ICVTWL: cvtwl(); break; // Canonical eight-byte integer argument.
        case IMOVP: movp(); break; // Managed retain/mark before result publication.
        default: abort();
        }
        if(store->op == IMOVP) check(reference == H || D2H(reference)->ref == refs+1);
        else { memmove(&value, R.d, sizeof(value)); check(value == ~0ULL); }
    }
    native64resultclear(&input); epochs(6); freetype(type);
    if(fd != H) ptrdel(D2H(fd));
    check(contextlive == baseline);
    R = saved; process.R = R;
}

/* The mixed numeric producer selects CVTWL/CVTWF. Guard bytes expose partial stores. */
static void vmstoreconversions(void)
{
    static const WORD numbers[] = {(-2147483647-1), -1, 0, 2147483647};
    static const uvlong integers[] = {0xffffffff80000000ULL, ~0ULL, 0, 0x7fffffffULL};
    static const uvlong reals[] = {0xc1e0000000000000ULL, 0xbff0000000000000ULL, 0, 0x41dfffffffc00000ULL};
    uchar source[128], destination[128];
    const Native64GeneratedStore *store;
    REG saved = R;
    unsigned int i, n, alignment, j;
    uvlong bits;
    for(i=0; i<nelem(callmixedstores); i++) {
        store = &callmixedstores[i]; if(store->fixed || store->op == IMOVP) continue;
        check(store->op == ICVTWL || store->op == ICVTWF);
        for(n=0; n<nelem(numbers); n++) for(alignment=0; alignment<8; alignment++) {
            memset(source, 0x5a, sizeof(source)); memset(destination, 0xa5, sizeof(destination));
            memmove(source+store->source_offset+alignment, &numbers[n], sizeof(WORD));
            R.s = source+store->source_offset+alignment; R.d = destination+store->destination;
            if(store->op == ICVTWL) cvtwl(); else cvtwf();
            memmove(&bits, R.d, sizeof(bits)); check(bits == (store->op == ICVTWL ? integers[n] : reals[n]));
            for(j=0; j<sizeof(destination); j++)
                if(j < store->destination || j >= store->destination+8) check(destination[j] == 0xa5);
        }
    }
    R = saved;
}

/* Compiler-selected MOVL/MOVF preserve signed64 and IEEE bits, including NaN payloads. */
static void vmstoremoves(void)
{
    static const uvlong patterns[] = {0, 1, ~0ULL, 0x8000000000000000ULL,
        0x7ff0000000000000ULL, 0xfff0000000000000ULL, 0x7ff8123456789abcULL, 0x7ff0123456789abcULL};
    uchar source[128], destination[136];
    const Native64GeneratedStore *store;
    REG saved = R;
    unsigned int i, n, from, to, j, offset;
    uvlong bits;
    for(i=0; i<nelem(callwidestores); i++) {
        store = &callwidestores[i]; if(store->fixed) continue;
        check(store->op == IMOVL || store->op == IMOVF);
        for(n=0; n<nelem(patterns); n++) for(from=0; from<8; from++) for(to=0; to<8; to++) {
            memset(source, 0x5a, sizeof(source)); memset(destination, 0xa5, sizeof(destination));
            memmove(source+store->source_offset+from, &patterns[n], 8);
            offset = store->destination+to; R.s = source+store->source_offset+from; R.d = destination+offset;
            if(store->op == IMOVL) movl(); else movf();
            memmove(&bits, R.d, 8); check(bits == patterns[n]);
            for(j=0; j<sizeof(destination); j++)
                if(j < offset || j >= offset+8) check(destination[j] == 0xa5);
        }
    }
    R = saved;
}

/* The same source Big cell feeds MOVL and CVTLF without changing its native address. */
static void vmstorelongconversions(void)
{
    static const LONG values[] = {(-0x7fffffffffffffffLL-1), -17, 0, 0x20000000000000LL};
    static const uvlong expected[] = {0xc3e0000000000000ULL, 0xc031000000000000ULL, 0, 0x4340000000000000ULL};
    uchar source[128], destination[128];
    const Native64GeneratedStore *store;
    REG saved = R;
    unsigned int i, n, alignment, j;
    uvlong bits;
    for(i=0; i<nelem(callcastaliasstores); i++) {
        store = &callcastaliasstores[i]; if(store->op != ICVTLF) continue;
        for(n=0; n<nelem(values); n++) for(alignment=0; alignment<8; alignment++) {
            memset(source, 0x5a, sizeof(source)); memset(destination, 0xa5, sizeof(destination));
            memmove(source+store->source_offset+alignment, &values[n], 8);
            R.s = source+store->source_offset+alignment; R.d = destination+store->destination;
            cvtlf(); memmove(&bits, R.d, 8); check(bits == expected[n]);
            for(j=0; j<sizeof(destination); j++)
                if(j < store->destination || j >= store->destination+8) check(destination[j] == 0xa5);
        }
    }
    R = saved;
}

static void vmcase(int phase, int operation, int mode, int useframe)
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
    Type *table[2], **savedtypes;
    Native64TailSlot *source;
    const uchar *sourcemap;
    UWORD sourcesignature;
    WORD typeindex = 1;
    int savedcount;
    unsigned int count = mode == 14 ? 150 : 2;
    int baseline, before, expected, completed;
    int aliased = phase == 2 && operation == 0 && mode == 0;

    contextstart(phase, Pready);
    sourcebankpool = 1; sourcebankallocs = sourcebankfrees = sourcebanklive = 0;
    process.pid = 99;
    nativewriteobserver = nil; vmhook = 0; vmownedlive = 0; vmowner = rootlink->m; vmowner->ref = 1;
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
#define CALLSITE(OP) case OP: source = count == 150 ? callslots_##OP##_150 : callslots_##OP##_2; sourcemap = count == 150 ? callmap_##OP##_150 : callmap_##OP##_2; sourcesignature = count == 150 ? callsignature_##OP##_150 : callsignature_##OP##_2; break
    switch(operation) { CALLSITE(0); CALLSITE(1); CALLSITE(2); CALLSITE(3); default: abort(); }
#undef CALLSITE
    memmove(slots, source, count*sizeof(*slots));
    check(sourcesignature == prefix->fixed.legacy_signature);
    check(native64tailplan(&prefix->fixed, slots, count, &plan));
    check(plan.map_bytes <= sizeof(map) && native64tailmap(&prefix->fixed, slots, count, map, sizeof(map)));
    check(memcmp(map, sourcemap, plan.map_bytes) == 0);
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
    savedtypes = rootlink->m->type; savedcount = rootlink->m->ntype;
    table[0] = caller_type; table[1] = callee_type; rootlink->m->type = table; rootlink->m->ntype = 2;
    if(useframe) R.s = &typeindex;
    beforefp = R.FP;
    check(kprotect(vmframe, useframe ? &typeindex : nil) && pending != nil && pending->t == callee_type);
    vmstores(phase, aliased, operation, count, pending, pattern, text, fd);
    if(operation == 3 && fd != H) destroy(fd); // Drop the copied C source owner, retaining the frame owner.
    result = operation == 0 ? (void*)&caller->stringresult : operation == 1 ? (void*)&caller->arrayresult : (void*)&caller->count;
    memset(&vmrequest, 0, sizeof(vmrequest));
    vmtype = callee_type; vmrequest.prog = &process; vmrequest.type = callee_type; vmrequest.caller_type = caller_type;
    vmrequest.slots = slots; vmrequest.count = count; vmrequest.initial = 8; vmrequest.limit = 128;
    vmrequest.result_offset = (uchar*)result-(uchar*)caller;
    vmrequest.frame = pending;
    vmbindrefusals(&vmrequest, operation);
    caller->regs.t = nil; // Binding also supports the root-extent Type fallback.
    rootlink->prog = &next; // A bytecode caller may bind before entering the native target.
    if(vmsiteowned(mode)) {
        vmsitesinstall(vmowner, &vmrequest, operation);
        memset(slots, 0xa5, count*sizeof(*slots)); // All later metadata comes from the module-owned copy.
        check(!vmsitesforeign(vmowner, &process, pending, 0));
        check(Sys_bindsite64(vmowner, 0, &process, pending));
        check(!Sys_bindsite64(vmowner, 1, &process, pending));
    } else check(Sys_bindframe64(&vmrequest, (Native64SysFrameOperation)operation));
    rootlink->prog = nil; caller->regs.t = caller_type;
    memmove(&bad, (uchar*)pending+32, sizeof(bad));
    check(bad == result); // Also proves all eight bytes replace the nonzero upper guard.
#ifndef __BIONIC__
    check((uintptr)result > 0xffffffffU); // GNU PIE fixtures exercise a genuinely high result cell.
#endif

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
    if(mode == 0 || mode == 14) { vmownedlive = 1; heapmonitor = vmsitesmonitor; }
    completed = kprotect(vmcall, nil);
    vmownedlive = 0;
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
            check(value != old && value->len == (aliased ? 8 : 5) && memcmp(value->Sascii, aliased ? "-1|%d|%s" : "-1|ok", aliased ? 9 : 6) == 0);
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
    check(Sys_clearframes64(vmowner) && Sys_clearframes64(vmowner));
    rootlink->m->type = savedtypes; rootlink->m->ntype = savedcount;
    rootlink->nlinks = 0; rootlink->links[0].frame = nil; rootlink->links[0].u.runt = nil;
    memset(&R, 0, sizeof(R));
    epochs(6); freetype(caller_type); freetype(callee_type); freetype(fdtype); TFD = nil; clearrootcache();
    check(contextlive == 0 && Tstring.ref == 1 && Tarray.ref == 1 && Tbyte.ref == 1 && errdepth == 0);
    check(sourcebanklive == 0 && sourcebankallocs == sourcebankfrees);
    heapmonitor = nil; sourcebankpool = 0;
}

int main(int argc, char **argv)
{
    int phase, operation, mode, useframe, cases = 0;
    native64_syswrite_main(argc, argv);
    vmstoreconversions();
    vmstoremoves();
    vmstorelongconversions();
    for(useframe=0; useframe<2; useframe++) for(phase=0; phase<3; phase++) for(operation=0; operation<4; operation++) for(mode=0; mode<32; mode++) {
        vmcase(phase, operation, mode, useframe); cases++;
    }
    printf("PASS typed Sys VM frames: %d real FRAME/MFRAME/MCALL/GC cases with source-derived tail plans, four gateways, relocated source banks/GC/aliases, compiler-selected canonical stores, owned site copies/rollback/busy retirement, checked eight-byte binding/32 refusals, 150-slot frames, refusal and result/cleanup checks\n", cases);
    return 0;
}
