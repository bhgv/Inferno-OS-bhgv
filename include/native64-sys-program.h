#ifndef INFERNO_NATIVE64_SYS_PROGRAM_H
#define INFERNO_NATIVE64_SYS_PROGRAM_H
#include "vmfetch.h"

/* A caller owns immutable original Dis instructions for the complete activation.
 * These trusted recipes bind existing native frames; they grant no file ABI. */
static int native64sysprogrambind(void);

static int
native64sysprogramrecord(Module *module, const Native64SysProgramSpec *spec)
{
    const Inst *bind, *call;
    Type *type;
    unsigned int bit, mode, offset, width;
    if(!native64sysspec(module, &spec->frame) || module->cstep!=native64typedlea8step || module->compiled ||
       module->prog==nil || module->prog==H || module->nprog<=0 || module->nprog>65536 ||
       (uintptr)module->prog>(uintptr)-1-(uintptr)module->nprog*sizeof(Inst) ||
       spec->bindpc>=(unsigned int)module->nprog || spec->callpc!=spec->bindpc+1 ||
       spec->callpc>=(unsigned int)module->nprog-1)return 0;
    bind=module->prog+spec->bindpc;call=module->prog+spec->callpc;
    width=spec->frame.operation<Native64SysPrint?8:4;
    if(bind->op!=(spec->frame.inherited?IMOVL:ILEA) || bind->add!=(SRC(AFP)|DST(AFP|AIND)) ||
       bind->reg!=(width==8?24:4) || bind->s.ind!=(int)(spec->frame.inherited?32:spec->frame.result_offset) ||
       bind->d.i.f!=40 || bind->d.i.s!=32 || call->op!=IMCALL ||
       call->s.ind!=40 || call->reg<0 || call->reg>32767)return 0;
    mode=UDST(call->add);offset=call->d.ind;
    if((mode!=AFP && mode!=AMP) || call->add!=(SRC(AFP)|AXIMM|DST(mode)))return 0;
    type=mode==AFP?spec->frame.caller_type:module->type[0];
    if(!native64sysframetype(spec->frame.caller_type) || type==nil || type==H ||
       type->ref<=0 || type->size<0 || type->np<0 || offset%8 ||
       (mode==AFP && offset<64) || offset>(unsigned int)type->size ||
       8>(unsigned int)type->size-offset)return 0;
    bit=offset/8;
    return bit/8<(unsigned int)type->np && (type->map[bit/8]&(128>>(bit%8)));
}

/* One shared imported trampoline keeps the generated fixed-prefix Type while
 * each FRAME owns its complete typed tail. Exact per-site frames remain valid. */
static int
native64sysprogramentryframe(Type *type, const Native64SysSite *site)
{
    const LimboNative64Entry *prefix;
    if(type==site->spec.type)return 1;
    if(!native64sysframetype(type))return 0;
    prefix=site->spec.operation==Native64SysSprint?&limbo_native64_Sys_sprint_prefix.fixed:
        site->spec.operation==Native64SysAprint?&limbo_native64_Sys_aprint_prefix.fixed:
        site->spec.operation==Native64SysPrint?&limbo_native64_Sys_print_prefix.fixed:&limbo_native64_Sys_fprint_prefix.fixed;
    return type->size==(int)prefix->frame_size && type->np==(int)prefix->map_bytes && !memcmp(type->map,prefix->map,type->np);
}

/* The mapped call operand must resolve to this trampoline before publishing a
 * result address. A matching Type alone never authorizes another native C entry. */
static int
native64sysprogramtarget(const Native64SysSite *site)
{
    Modlink *target;
    uchar *base=UDST(site->call.add)==AFP?R.FP:R.MP;
    unsigned int ordinal=site->call.reg;
    if(base==nil || base==H)return 0;
    memmove(&target,base+site->call.d.ind,sizeof(target));
    if(target==nil || target==H || (uintptr)target>(uintptr)-1-sizeof(*target) ||
       target->prog!=nil || target->nlinks<=0 || target->nlinks>65536 || ordinal>=(unsigned int)target->nlinks)return 0;
    return native64sysprogramentryframe(target->links[ordinal].frame,site) && target->links[ordinal].u.runt==Sys_programcall64;
}

/* The generic local binder delegates only through this explicit owned hook.
 * A matching PC owns exactly one private LEA. Unrelated instructions continue
 * through the normal C/JIT path and the normal fixed/local result binder. */
static int
native64sysprogrambind(void)
{
    VmFetch fetched;
    Native64SysSites *table;
    Native64SysSite *site;
    Frame *pending;
    unsigned int i;
    if(!vmfetch(&fetched))return 0;
    if(fetched.module->native64sysstep!=native64sysprogrambind ||
       fetched.module->native64sysframesfree!=native64syssitesretire ||
       (table=fetched.module->native64sysframes)==nil || table->owner!=fetched.module)error(exBounds);
    for(i=0;i<table->count;i++){
        site=table->sites+i;
        if(site->bindpc!=fetched.pc)continue;
        if(memcmp(site->bindpc,&site->binding,sizeof(Inst)) ||
           memcmp(site->callpc,&site->call,sizeof(Inst)) || !native64sysprogramtarget(site))error(exBounds);
        if(R.FP==nil || R.FP==H)error(exBounds);
        memmove(&pending,R.FP+40,sizeof(pending));
        if(!native64syssitesrun(fetched.module,i,currun(),pending,0))error(exBounds);
        vmdecode(&fetched);
        return 1;
    }
    return 0;
}

/* Use MCALL's retained caller and return PC, never an ambient process-global
 * site index. The callee must be this exact trampoline and expected frame Type. */
static void
native64sysprogramcall(void *frame)
{
    Native64SysSites *table;
    Native64SysSite *site;
    Module *module;
    Modlink *caller;
    Frame active;
    unsigned int i, ordinal;
    char diagnostic[ERRMAX];
    if(frame==nil || frame==H || R.FP!=frame || R.M==nil || R.M==H || R.M->prog!=nil)error(exBounds);
    memmove(&active,frame,sizeof(active));caller=active.mr;
    if(caller==nil || caller==H || (module=caller->m)==nil || module==H ||
       module->ref<=0 || module->native64sysstep!=native64sysprogrambind ||
       module->native64sysframesfree!=native64syssitesretire ||
       (table=module->native64sysframes)==nil || table->owner!=module)error(exBounds);
    for(i=0;i<table->count;i++){
        site=table->sites+i;
        if(site->callpc==nil || active.lr!=site->callpc+1)continue;
        ordinal=site->call.reg;
        if(memcmp(site->bindpc,&site->binding,sizeof(Inst)) || memcmp(site->callpc,&site->call,sizeof(Inst)) ||
           R.M->nlinks<=0 || R.M->nlinks>65536 || ordinal>=(unsigned int)R.M->nlinks || !native64sysprogramentryframe(R.M->links[ordinal].frame,site) ||
           R.M->links[ordinal].u.runt!=Sys_programcall64)error(exBounds);
        if(!native64syssitesrun(module,i,currun(),frame,1)){
            kgerrstr(diagnostic,sizeof(diagnostic));
            error(diagnostic[0]?diagnostic:exBounds);
        }
        return;
    }
    error(exBounds);
}

/* Complete all PC/type checks before the existing all-or-nothing owning install.
 * Recipes, Types, slots and instruction snapshots remain module-owned until
 * nonthrowing retirement. Installation does not change the caller's CODE/JIT. */
static int
native64sysprograminstall(Module *module, const Native64SysProgramSpec *specs, unsigned int count)
{
    Native64SysFrameSpec frames[256];
    Native64SysSites *table;
    Native64SysSite *site;
    unsigned int i,j;
    if(module==nil || module==H || specs==nil || count==0 || count>nelem(frames) ||
       module->native64sysstep!=nil || module->native64sysframes!=nil || module->native64sysframesfree!=nil)return 0;
    for(i=0;i<count;i++){
        if(!native64sysprogramrecord(module,specs+i))return 0;
        for(j=0;j<i;j++)if(specs[j].bindpc==specs[i].bindpc || specs[j].callpc==specs[i].callpc)return 0;
        frames[i]=specs[i].frame;
    }
    if(!native64syssitesinstall(module,frames,count))return 0;
    table=module->native64sysframes;
    for(i=0;i<count;i++){
        site=table->sites+i;site->bindpc=module->prog+specs[i].bindpc;site->callpc=module->prog+specs[i].callpc;
        memmove(&site->binding,site->bindpc,sizeof(Inst));memmove(&site->call,site->callpc,sizeof(Inst));
    }
    module->native64sysstep=native64sysprogrambind;
    return 1;
}
#endif
