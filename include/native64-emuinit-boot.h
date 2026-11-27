#ifndef INFERNO_NATIVE64_EMUINIT_BOOT_H
#define INFERNO_NATIVE64_EMUINIT_BOOT_H
/* Only the explicitly selected startup profile includes this implementation in
 * runt.c, after coherent Sys declarations and typed formatter/program gateways. */
#include "native64-sys-module.h"

/* Private Sys view: fixed entries borrow verified boot implementations; only
 * this trusted caller receives the two program-owned variadic trampolines. */
typedef struct Native64BootSys Native64BootSys;
struct Native64BootSys {const Native64ModuleTable *table;Module *module;};
static int
native64emuinitsysmake(void *argument)
{
    Native64BootSys *build=argument;Module *m,*sys=nil;const Native64ModuleTable*t=build->table;
    const Native64ModuleImport*r;const LimboNative64Entry*prefix;const Link*entry;unsigned int j;
    if(t==nil)error(exModule);
    m=build->module=mallocz(sizeof(*m),1);if(m==nil)error(exNomem);m->ref=1;m->origmp=H;
    m->name=native64modulecopy("Sys");m->path=native64modulecopy("trusted/Emuinit/Sys");m->ntype=t->count;
    m->type=mallocz(t->count*sizeof(Type*),1);m->ext=mallocz((t->count+1)*sizeof(Link),1);if(m->type==nil||m->ext==nil)error(exNomem);
    for(j=0;j<t->count;j++){
        r=t->imports+j;prefix=!strcmp(r->name,"sprint")?&limbo_native64_Sys_sprint_prefix.fixed:!strcmp(r->name,"fprint")?&limbo_native64_Sys_fprint_prefix.fixed:nil;
        if(prefix){
            if(prefix->legacy_signature!=r->signature || prefix->frame_size!=r->bytes || prefix->map_bytes!=r->mapbytes || memcmp(prefix->map,r->map,r->mapbytes))error(exModule);
            m->type[j]=dtype(freeheap,r->bytes,(uchar*)r->map,r->mapbytes);if(m->type[j]==nil)error(exNomem);
            m->ext[j].u.runt=Sys_programcall64;
        }else{
            entry=native64sysexport(r->name,&sys);
            if(entry==nil || (UWORD)entry->sig!=r->signature || entry->frame->size!=r->bytes || entry->frame->np!=r->mapbytes || memcmp(entry->frame->map,r->map,r->mapbytes))error(exModule);
            m->type[j]=entry->frame;m->type[j]->ref++;m->ext[j].u.runt=entry->u.runt;
        }
        m->ext[j].name=native64modulecopy(r->name);memmove(&m->ext[j].sig,&r->signature,4);m->ext[j].frame=m->type[j];
    }
    return 1;
}
static Modlink*
native64bootsyslink(Module *caller,WORD table,const Native64ModuleTable *request)
{
    Native64BootSys build={0};Modlink*link;build.table=request;
    if(!kprotect(native64emuinitsysmake,&build)){if(build.module)unload(build.module);krethrow();}
    link=linkmod(build.module,caller->ldt[table],0);
    if(link!=nil && link!=H)ptradd(D2H(link));return link;
}
static Modlink*
native64argload(Module *caller,WORD table,char *path)
{
    if(caller==nil || caller->native64importload!=native64argload || table!=0 || path==nil || strcmp(path,"$Sys"))error(exModule);
    return native64bootsyslink(caller,table,arg_source_tables);
}
static Modlink*
native64emuinitload(Module *caller,WORD table,char *path)
{
    Native64ModuleOwner owner={0};Module*m;Modlink*link;unsigned int space;
    if(caller==nil || caller->native64importload!=native64emuinitload || table<0 || (uint)table>=full_spec.ntables || path==nil)error(exModule);
    space=full_namespaces[table];
    if(space==0){
        if(strcmp(path,"$Sys"))error(exModule);
        return native64bootsyslink(caller,table,full_tables+table);
    }
    if(space==1){
        if(strcmp(path,"/dis/lib/arg.dis"))error(exModule);
        if(!native64sysmodule(&arg_source_spec,arg_source_variadics,nelem(arg_source_variadics),!!cflag,&owner))error(exModule);
        m=owner.module;m->link=modules;modules=m;
        if(owner.rooted)ptrdel(D2H(m->origmp));
        link=linkmod(m,caller->ldt[table],1);
        if(link!=nil && link!=H)ptradd(D2H(link));return link;
    }
    if(space!=2 || (strcmp(path,"/dis/wm/awm.dis") && strcmp(path,"/dis/sh.dis")))error(exModule);
    /* Existing reader owns real namespace I/O, signing policy, structural
     * Command.init verification and one temporary pin until ILOAD publishes. */
    return read64calllibrary(path,!!cflag,boot_command_requests,sizeof(boot_command_requests),boot_command_names,sizeof(boot_command_names),1);
}

Module*
native64emuinitboot(int jit)
{
    Native64ModuleOwner owner={0};Module*m;
    if(!native64sysmodule(&full_spec,full_variadics,nelem(full_variadics),jit,&owner)){
        kwerrstr("cannot construct checked native64 emuinit");return nil;
    }
    m=owner.module;
    /* Transfer its sole Module reference and GC root to the ordinary module
     * list. schedmod consumes that reference; unload owns the final teardown. */
    m->link=modules;modules=m;
    if(owner.rooted)ptrdel(D2H(m->origmp));
    return m;
}
#endif
