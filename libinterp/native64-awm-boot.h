#ifndef INFERNO_NATIVE64_AWM_BOOT_H
#define INFERNO_NATIVE64_AWM_BOOT_H

typedef struct GraphSpec GraphSpec;
struct GraphSpec {
	const Native64ModuleSpec *spec;
	const Native64ModuleVariadic *variadic;
	uint count;
	const char *path;
};

static Modlink *graphload(Module*, WORD, char*);
#include "awm-native64.h"

static int
graphframe(const Native64ModuleImport *request, const Type *frame)
{
	return frame != nil && frame->size == request->bytes &&
		frame->np == request->mapbytes &&
		memcmp(frame->map, request->map, request->mapbytes) == 0;
}

static const Link*
graphexport(const Module *module, const Native64ModuleImport *request)
{
	const Link *entry;

	for(entry = module->ext; entry != nil && entry->name != nil; entry++)
		if(strcmp(entry->name, request->name) == 0 &&
		   (UWORD)entry->sig == request->signature)
			return entry;
	return nil;
}

typedef struct GraphSys GraphSys;
struct GraphSys {
	const Native64ModuleTable *table;
	Module *provider;
	Module *view;
};

static const LimboNative64Entry*
graphprefix(const char *name)
{
	if(strcmp(name, "sprint") == 0)
		return &limbo_native64_Sys_sprint_prefix.fixed;
	if(strcmp(name, "aprint") == 0)
		return &limbo_native64_Sys_aprint_prefix.fixed;
	if(strcmp(name, "fprint") == 0)
		return &limbo_native64_Sys_fprint_prefix.fixed;
	if(strcmp(name, "print") == 0)
		return &limbo_native64_Sys_print_prefix.fixed;
	return nil;
}

/* Give checked Limbo callers a private Sys view with typed variadic entries. */
static int
graphmakesys(void *arg)
{
	GraphSys *build = arg;
	const Native64ModuleTable *table = build->table;
	const Native64ModuleImport *request;
	const LimboNative64Entry *prefix;
	const Link *entry;
	Module *view;
	uint i;

	view = build->view = mallocz(sizeof(*view), 1);
	if(view == nil)
		error(exNomem);
	view->ref = 1;
	view->origmp = H;
	view->name = native64modulecopy("Sys");
	view->path = native64modulecopy("trusted/AWM/Sys");
	view->ntype = table->count;
	view->type = mallocz(table->count*sizeof(Type*), 1);
	view->ext = mallocz((table->count+1)*sizeof(Link), 1);
	if(view->type == nil || view->ext == nil)
		error(exNomem);
	for(i = 0; i < table->count; i++){
		request = table->imports+i;
		prefix = graphprefix(request->name);
		if(prefix != nil){
			if(prefix->legacy_signature != request->signature ||
			   prefix->frame_size != request->bytes ||
			   prefix->map_bytes != request->mapbytes ||
			   memcmp(prefix->map, request->map, request->mapbytes) != 0)
				error(exModule);
			view->type[i] = dtype(freeheap, request->bytes, (uchar*)request->map, request->mapbytes);
			if(view->type[i] == nil)
				error(exNomem);
			view->ext[i].u.runt = Sys_programcall64;
		}else{
			entry = graphexport(build->provider, request);
			if(entry == nil || !graphframe(request, entry->frame)){
				errorf("native64 AWM Sys frame: %s", request->name);
			}
			view->type[i] = entry->frame;
			view->type[i]->ref++;
			view->ext[i].u.runt = entry->u.runt;
		}
		view->ext[i].name = native64modulecopy(request->name);
		view->ext[i].sig = request->signature;
		view->ext[i].frame = view->type[i];
	}
	return 1;
}

static Module*
graphbuiltin(const char *path)
{
	Module *m;

	for(m = modules; m != nil; m = m->link)
		if(m->prog == nil && m->origmp == H && m->path != nil &&
		   strcmp(m->path, path) == 0)
			return m;
	error(exModule);
	return nil;
}

/* The source builder leaves one reference and a temporary root for its caller. */
static Module*
graphmake(const GraphSpec *graph, int jit)
{
	Native64ModuleOwner owner = {0};
	Module *m;
	int ok;

	if(graph->count)
		ok = native64sysmodule(graph->spec, graph->variadic, graph->count, jit, &owner);
	else
		ok = native64typedmodule(graph->spec, jit, &owner);
	if(!ok){
		kwerrstr("cannot construct native64 AWM module %s", graph->spec->name);
		return nil;
	}
	m = owner.module;
	m->link = modules;
	modules = m;
	if(owner.rooted)
		ptrdel(D2H(m->origmp));
	return m;
}

/* Only compiled-in source modules and coherent GUI builtins enter this graph. */
static Modlink*
graphload(Module *caller, WORD index, char *path)
{
	const Native64ModuleSpec *spec;
	const Native64ModuleTable *table;
	const Native64ModuleImport *request;
	const Link *entry;
	GraphSys build = {0};
	Module *target;
	Modlink *link;
	uint i, source;

	if(caller == nil || caller->path == nil || path == nil ||
	   caller->native64importload != graphload || index < 0)
		error(exModule);
	for(source = 0; source < nelem(graph_specs); source++)
		if(strcmp(caller->path, graph_specs[source].spec->path) == 0)
			break;
	if(source == nelem(graph_specs))
		error(exModule);
	spec = graph_specs[source].spec;
	if((uint)index >= spec->ntables)
		error(exModule);
	table = spec->tables+index;
	if(strcmp(path, "$Sys") == 0){
		build.table = table;
		build.provider = graphbuiltin(path);
		if(!kprotect(graphmakesys, &build)){
			if(build.view != nil)
				unload(build.view);
			krethrow();
		}
		target = build.view;
	}else if(strcmp(path, "$Draw") == 0 || strcmp(path, "$Tk") == 0){
		target = graphbuiltin(path);
		for(i = 0; i < table->count; i++){
			request = table->imports+i;
			entry = graphexport(target, request);
			if(entry == nil || !graphframe(request, entry->frame)){
				errorf("native64 AWM builtin frame: %s->%s", path, request->name);
				return H;
			}
		}
		target->ref++;
	}else{
		for(i = 1; i < nelem(graph_specs); i++)
			if(strcmp(path, graph_specs[i].path) == 0)
				break;
		if(i == nelem(graph_specs)){
			kwerrstr("unavailable in native64 AWM: %s", path);
			return H;
		}
		target = graphmake(graph_specs+i, caller->jit != nil);
	}
	link = linkmod(target, caller->ldt[index], 1);
	if(link != nil && link != H)
		ptradd(D2H(link));
	return link;
}

Module*
native64awmboot(int jit)
{
	return graphmake(graph_specs, jit);
}
#endif
