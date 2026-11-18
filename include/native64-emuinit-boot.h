#ifndef INFERNO_NATIVE64_EMUINIT_BOOT_H
#define INFERNO_NATIVE64_EMUINIT_BOOT_H
/* Only the explicitly selected startup profile includes this implementation in
 * runt.c, after coherent Sys declarations and typed formatter/program gateways. */
#include "native64-sys-module.h"

/* Private Sys view: fixed entries borrow verified boot implementations; only
 * this trusted caller receives the two program-owned variadic trampolines. */
static int
native64emuinitsysmake(void *argument)
{
    Module **out=argument,*m,*sys=nil;const Native64ModuleTable*t=nil;
    const Native64ModuleImport*r;const LimboNative64Entry*prefix;const Link*entry;unsigned int i,j;
    for(i=0;i<full_spec.ntables;i++)if(full_namespaces[i]==0)t=full_tables+i;
    if(t==nil)error(exModule);
    m=*out=mallocz(sizeof(*m),1);if(m==nil)error(exNomem);m->ref=1;m->origmp=H;
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
native64emuinitload(Module *caller,WORD table,char *path)
{
    Module *sys=nil;Modlink*link;unsigned int space;
    if(caller==nil || caller->native64importload!=native64emuinitload || table<0 || (uint)table>=full_spec.ntables || path==nil)error(exModule);
    space=full_namespaces[table];
    if(space==0){
        if(strcmp(path,"$Sys"))error(exModule);
        if(!kprotect(native64emuinitsysmake,&sys)){if(sys)unload(sys);krethrow();}
        link=linkmod(sys,caller->ldt[table],0);
        if(link!=nil && link!=H)ptradd(D2H(link));
        return link;
    }
    if(space==1){kwerrstr("native64 Arg module is not yet admitted");return H;}
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
