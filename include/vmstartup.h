#ifndef INFERNO_VMSTARTUP_H
#define INFERNO_VMSTARTUP_H // Shared root-module admission before scheduler publication.

#include "raise.h"


/* Reject unusable root entries before cloning MP or publishing a runnable Prog.
 * Native-PC modules retain opaque entry tokens; bytecode/AOT/leaf-JIT modules
 * must name a complete instruction in their original Inst image. */
static Type*
vmstartup(Module *module)
{
	Type *type;
	uintptr base, bytes, entry;
	int count;

	if(module == nil || module == H || (uintptr)module%sizeof(void*) != 0 ||
	   (uintptr)module > (uintptr)-1-sizeof(*module))
		error(exModule);
	if(module->entry == nil || module->entry == H)
		error(exBounds);
	if(!module->compiled){
		count = module->nprog;
		base = (uintptr)module->prog;
		entry = (uintptr)module->entry;
		if(count <= 0 || module->prog == nil || module->prog == H ||
		   (uintptr)count > (uintptr)-1/sizeof(Inst))
			error(exBounds);
		bytes = (uintptr)count*sizeof(Inst);
		if(base > (uintptr)-1-bytes || entry < base || entry-base >= bytes ||
		   (entry-base)%sizeof(Inst) != 0)
			error(exBounds);
	}
	type = module->entryt;
	if(type == nil || type == H || (uintptr)type%sizeof(void*) != 0 ||
	   (uintptr)type > (uintptr)-1-O(Type, map))
		error(exHeap);
	if(type->ref <= 0 || type->size < (int)sizeof(Frame) ||
	   (ulong)type->size > 0x7fffffff-OA(Stkext, reg.tos.fu) || module->ss > 0x7fffffff ||
	   type->np < 0 || type->np > type->size/(8*sizeof(void*))+(type->size%(8*sizeof(void*)) != 0) ||
	   (uintptr)type+O(Type, map) > (uintptr)-1-(uintptr)type->np)
		error(exHeap);
	return type;
}
#endif
