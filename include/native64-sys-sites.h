#ifndef INFERNO_NATIVE64_SYS_SITES_H
#define INFERNO_NATIVE64_SYS_SITES_H // Trusted module-owned recipes, never a serialized call table.

/* The active caller Modlink/frame owns Module lifetime. All metadata is immutable
 * under VM exclusion; no API here authenticates arbitrary C pointers or file PCs. */
typedef struct Native64SysSite Native64SysSite;
struct Native64SysSite {
    Native64SysFrameSpec spec;
    Native64TailSlot *slots;
    unsigned int typed;
    int callerheld, calleeheld;
};
typedef struct Native64SysSites Native64SysSites;
struct Native64SysSites {
    Module *owner; // Exact non-owning backpointer; the module owns this table.
    unsigned int count, busy;
    Native64SysSite sites[1];
};
typedef struct Native64SysSitesBuild Native64SysSitesBuild;
struct Native64SysSitesBuild {
    Module *module;
    const Native64SysFrameSpec *specs;
    unsigned int count;
    Native64SysSites *table;
};

static int
native64sysspec(Module *module, const Native64SysFrameSpec *spec)
{
    const LimboNative64Prefix *prefix;
    Native64TailPlan plan;
    unsigned int i, caller = 0, callee = 0, bytes, slot;
    if(module == nil || module == H || module->ref <= 0 || module->ntype <= 0 || module->ntype > 65536 ||
       module->type == nil || module->type == H || spec == nil ||
       spec->operation < Native64SysSprint || spec->operation > Native64SysFprint ||
       spec->initial <= UTFmax+2 || spec->limit < spec->initial)
        return 0;
    for(i=0; i<(unsigned int)module->ntype; i++) {
        caller |= module->type[i] == spec->caller_type;
        callee |= module->type[i] == spec->type;
    }
    if(!caller || !callee || !native64sysframetype(spec->caller_type)) return 0;
    prefix = spec->operation == Native64SysSprint ? &limbo_native64_Sys_sprint_prefix :
        spec->operation == Native64SysAprint ? &limbo_native64_Sys_aprint_prefix :
        spec->operation == Native64SysPrint ? &limbo_native64_Sys_print_prefix : &limbo_native64_Sys_fprint_prefix;
    if(!native64tailplan(&prefix->fixed, spec->slots, spec->count, &plan) ||
       !native64sysframemap(spec->type, &prefix->fixed, spec->slots, &plan)) return 0;
    bytes = spec->operation >= Native64SysPrint ? sizeof(WORD) : sizeof(void*);
    if(spec->result_offset < 64 || spec->result_offset%bytes != 0 ||
       spec->result_offset > (unsigned int)spec->caller_type->size ||
       bytes > (unsigned int)spec->caller_type->size-spec->result_offset) return 0;
    slot = spec->result_offset/8;
    return !!(spec->caller_type->map[slot/8] & (0x80>>(slot%8))) == (spec->operation < Native64SysPrint);
}

static void
native64syssitesdiscard(Native64SysSites *table)
{
    unsigned int i, j;
    if(table == nil) return;
    for(i=0; i<table->count; i++) {
        Native64SysSite *site = &table->sites[i];
        for(j=0; j<site->typed; j++)
            if(site->slots[j].kind == Native64TailRef) freetype(site->slots[j].type);
        if(site->callerheld) freetype(site->spec.caller_type);
        if(site->calleeheld) freetype(site->spec.type);
        free(site->slots);
    }
    free(table);
}

/* Only a trusted constructor installs this callback; detach before nonthrowing retirement. */
static void
native64syssitesretire(Module *module)
{
    Native64SysSites *table = module->native64sysframes;
    if(table != nil && (table->owner != module || table->busy)) abort(); // A live frame must retain its owning module.
    module->native64sysframes = nil;
    module->native64sysframesfree = nil;
    native64syssitesdiscard(table);
}

static int
native64syssitesclear(Module *module)
{
    Native64SysSites *table;
    if(module == nil || module == H) return 0;
    if(module->native64sysframesfree == nil) return module->native64sysframes == nil;
    if(module->native64sysframesfree != native64syssitesretire) return 0;
    table = module->native64sysframes;
    if(table == nil || table->owner != module || table->busy) return 0;
    native64syssitesretire(module);
    return 1;
}

static int
native64syssitesconstruct(void *argument)
{
    Native64SysSitesBuild *build = argument;
    Native64SysSite *site;
    Type *type;
    unsigned int i, j;
    build->table = mallocz(sizeof(Native64SysSites)+(build->count-1)*sizeof(Native64SysSite), 1);
    if(build->table == nil) return 0;
    build->table->owner = build->module;
    build->table->count = build->count;
    for(i=0; i<build->count; i++) {
        site = &build->table->sites[i];
        site->spec = build->specs[i];
        if(site->spec.count != 0) {
            site->slots = mallocz(site->spec.count*sizeof(*site->slots), 0);
            if(site->slots == nil) return 0;
            memmove(site->slots, site->spec.slots, site->spec.count*sizeof(*site->slots));
        }
        site->spec.slots = site->slots;
        if(!native64sysspec(build->module, &site->spec)) return 0;
        type = site->spec.caller_type;
        if(type->ref <= 0 || type->ref == 0x7fffffff) return 0;
        type->ref++; site->callerheld = 1;
        type = site->spec.type;
        if(type->ref <= 0 || type->ref == 0x7fffffff) return 0;
        type->ref++; site->calleeheld = 1;
        for(j=0; j<site->spec.count; j++) {
            if(site->slots[j].kind == Native64TailRef) {
                type = site->slots[j].type;
                if(type->ref <= 0 || type->ref == 0x7fffffff) return 0;
                type->ref++;
            }
            site->typed = j+1;
        }
    }
    return 1;
}

/* Validate the entire proposed table before any allocation; publish only a complete owner. */
static int
native64syssitesinstall(Module *module, const Native64SysFrameSpec *specs, unsigned int count)
{
    Native64SysSitesBuild build;
    unsigned int i, slots = 0;
    if(!native64model() || module == nil || module == H || specs == nil || count == 0 || count > 256 ||
       module->native64sysframes != nil || module->native64sysframesfree != nil) return 0;
    for(i=0; i<count; i++) {
        if(specs[i].count > 4096-slots || !native64sysspec(module, &specs[i])) return 0;
        slots += specs[i].count;
    }
    memset(&build, 0, sizeof(build));
    build.module = module; build.specs = specs; build.count = count;
    if(!kprotect(native64syssitesconstruct, &build)) {
        native64syssitesdiscard(build.table);
        return 0;
    }
    if(module->native64sysframes != nil || module->native64sysframesfree != nil) {
        native64syssitesdiscard(build.table); return 0;
    }
    for(i=0; i<count; i++)
        if(!native64sysspec(module, &build.table->sites[i].spec)) {
            native64syssitesdiscard(build.table); return 0;
        }
    module->native64sysframes = build.table;
    module->native64sysframesfree = native64syssitesretire;
    return 1;
}

typedef struct Native64SysSiteRun Native64SysSiteRun;
struct Native64SysSiteRun {
    Native64SysFrameRequest request;
    int operation, entered;
};
static int
native64syssitesinvoke(void *argument)
{
    Native64SysSiteRun *run = argument;
    return run->entered ? native64sysframe(&run->request, run->operation) : native64sysframebind(&run->request, run->operation);
}

static int
native64syssitesrun(Module *module, unsigned int index, Prog *prog, void *frame, int entered)
{
    Native64SysSites *table;
    Native64SysSite *site;
    Native64SysSiteRun run;
    Native64SysFrameCall call;
    Frame active;
    int result;
    if(module == nil || module == H || module->ref <= 0 ||
       module->native64sysframesfree != native64syssitesretire || module->native64sysframes == nil) return 0;
    table = module->native64sysframes;
    if(table->owner != module || index >= table->count || table->busy) return 0;
    site = &table->sites[index];
    memset(&run, 0, sizeof(run));
    run.request.prog = prog; run.request.frame = frame;
    run.request.type = site->spec.type; run.request.caller_type = site->spec.caller_type;
    run.request.result_offset = site->spec.result_offset; run.request.slots = site->slots;
    run.request.count = site->spec.count; run.request.initial = site->spec.initial; run.request.limit = site->spec.limit;
    run.operation = site->spec.operation; run.entered = entered;
    if(!native64sysframeprepare(&call, &run.request, run.operation, entered)) return 0;
    if(entered) {
        memmove(&active, frame, sizeof(active));
        if(((Modlink*)active.mr)->m != module) return 0; // MCALL retains this exact caller owner.
    } else if(R.M->m != module) return 0;
    table->busy = 1;
    result = kprotect(native64syssitesinvoke, &run);
    table->busy = 0;
    return result;
}
#endif
