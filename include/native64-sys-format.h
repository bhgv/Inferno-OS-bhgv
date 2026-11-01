#ifndef INFERNO_NATIVE64_SYS_FORMAT_H
#define INFERNO_NATIVE64_SYS_FORMAT_H // Explicit trusted C gateways; generated Sys prefixes, no wire variadic ABI.

#include "native64-sys-call.h"

typedef struct Native64SysFormatCall Native64SysFormatCall;

struct Native64SysFormatCall
{
	Native64SysFormatRequest request; // Stable synchronous control snapshot outside protected frames.
	int array; // Zero for sprint String, one for aprint byte array.
};


/* Inspect only trusted managed identities; numeric checks cannot authenticate arbitrary C pointers. */
static int
native64sysformatreference(void *context, void *value, Type *type)
{
	Heap *heap;

	USED(context);
	if(value == H)
		return 1;
	if(value == nil || (uintptr)value <= sizeof(Heap) ||
	   ((uintptr)value-sizeof(Heap))%sizeof(void*) != 0)
		return 0;
	heap = D2H(value);
	return heap->ref > 0 && heap->t == type;
}


/* The generated sprint/aprint fixed body is exactly one managed String argument. */
static int
native64sysformatprefix(void *context, const void *bytes, unsigned int count, int live)
{
	void *value;

	if(count != sizeof(value))
		return 0;
	memmove(&value, bytes, sizeof(value));
	if(value == nil)
		return 0;
	return !live || native64sysformatreference(context, value, &Tstring);
}


/* Read the copied generated prefix, then delegate growth/conversion to the protected owning producer. */
static int
native64sysformatproduce(Native64Result *result, const Native64Arguments *arguments, void *context)
{
	Native64SysFormatCall *call;
	String *pattern;

	call = context;
	memmove(&pattern, (uchar*)arguments->frame.value+offsetof(F_Sys_sprint, s), sizeof(pattern));
	return native64formatresult(result, arguments, call->request.prog, pattern,
		call->request.initial, call->request.limit, call->array);
}


/* Validate the old return identity after late resolution, before any result owner is replaced. */
static void*
native64sysformatresolve(void *context)
{
	Native64SysFormatCall *call;
	void *cell, *old;

	call = context;
	cell = call->request.resolve(call->request.argument);
	if(cell == nil || cell == H || (uintptr)cell > (uintptr)-1-sizeof(old))
		return cell;
	memmove(&old, cell, sizeof(old));
	if(!native64sysformatreference(nil, old, call->array ? &Tarray : &Tstring))
		error(exType);
	if(call->array && old != H && ((Array*)old)->t != &Tbyte)
		error(exType);
	return cell;
}


/* Enter through checked generated Sys geometry; input values remain caller-rooted until construction returns. */
static int
native64sysformat(const Native64SysFormatRequest *request, int array)
{
	uchar sprintmap[] = F_Sys_sprint_map;
	uchar aprintmap[] = F_Sys_aprint_map;
	LimboNative64Entry fixed;
	Native64ArgumentSource source;
	Native64SysFormatCall call;

	if(!native64model() || request == nil || request->prog == nil || request->prog == H ||
	   request->resolve == nil ||
	   request->initial <= UTFmax+2 || request->limit < request->initial ||
	   sizeof(F_Sys_sprint) != 72 || sizeof(F_Sys_aprint) != 72 ||
	   offsetof(F_Sys_sprint, s) != 64 || offsetof(F_Sys_aprint, s) != 64 ||
	   sizeof(sprintmap) != 2 || sizeof(aprintmap) != 2 ||
	   sprintmap[0] != 0 || sprintmap[1] != 0x80 || memcmp(sprintmap, aprintmap, 2) != 0)
		return 0;
	call.request = *request;
	call.array = array;
	memset(&fixed, 0, sizeof(fixed));
	fixed.name = array ? "aprint" : "sprint";
	fixed.frame_size = sizeof(F_Sys_sprint);
	fixed.map_bytes = sizeof(sprintmap);
	fixed.map = sprintmap;
	memset(&source, 0, sizeof(source));
	source.fixed = &fixed;
	source.prefix = &call.request.pattern;
	source.prefix_bytes = sizeof(call.request.pattern);
	source.slots = call.request.slots;
	source.count = call.request.count;
	source.values = call.request.values;
	source.value_bytes = call.request.value_bytes;
	source.prefix_check = native64sysformatprefix;
	source.reference_check = native64sysformatreference;
	return native64argumentscall(&source, native64sysformatproduce, native64sysformatresolve, &call);
}


/* Trusted native64 sprint gateway; the packed Sys_sprint entry remains unavailable to native64 files. */
static int
native64syssprint(const Native64SysFormatRequest *request)
{
	return native64sysformat(request, 0);
}


/* Trusted native64 aprint gateway; publication returns exact bytes through the current typed result owner. */
static int
native64sysaprint(const Native64SysFormatRequest *request)
{
	return native64sysformat(request, 1);
}
#endif
