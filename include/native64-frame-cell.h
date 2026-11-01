#ifndef INFERNO_NATIVE64_FRAME_CELL_H
#define INFERNO_NATIVE64_FRAME_CELL_H // Private current-frame scalar cell; no wire-call admission.
#include "dis-layout.h"
#include "native64-model.h"


/* Resolve only an aligned eight-byte non-reference field in an exact active frame boundary.
 * Caller publishes live REG in this rooted Prog, retains descriptors and serializes
 * stack/registry access throughout the synchronous call. The active extent and
 * its Type pointers are trusted readable VM storage, not arbitrary native input.
 * The exact scalar kind/declared-field offset still comes from trusted compiler
 * metadata; a GC bitmap alone cannot distinguish Big, Real or scalar padding.
 * This first profile intentionally accepts only the current extent, not suspended
 * or older stack segments. No allocation/callback follows successful resolution. */
static void*
native64framecell(Prog *prog, Type *expected, unsigned int offset)
{
	uintptr first, stop, top, current, target, end;
	Type *type;
	Frame frame;
	Stkext *extent;
	DisAbi abi;
	unsigned int size, mapbytes, slot;
	int found;

	if(prog == nil || prog == H || expected == nil || expected == H ||
	   !native64model() || offset < 64 || offset%8 != 0 ||
	   prog->R.EX == nil || prog->R.EX == H || prog->R.FP == nil || prog->R.FP == H)
		return nil;
	first = (uintptr)prog->R.EX;
	if(first%sizeof(void*) != 0 || first > (uintptr)-1-OA(Stkext, reg.tos.fu))
		return nil;
	first += OA(Stkext, reg.tos.fu);
	stop = (uintptr)prog->R.SP;
	top = (uintptr)prog->R.TS;
	target = (uintptr)prog->R.FP;
	if(stop < first || top < stop || top-(uintptr)prog->R.EX > 1024*1024 ||
	   target < first || target >= stop || !disabinit(&abi, 8))
		return nil;
	extent = (Stkext*)prog->R.EX;
	found = 0;
	for(current=first; current<stop; current=end){
		if(stop-current < sizeof(Frame))
			return nil;
		memmove(&frame, (void*)current, sizeof(frame));
		type = frame.t == nil ? extent->reg.TR : frame.t;
		if(type == nil || type == H || (uintptr)type%sizeof(void*) != 0 ||
		   (uintptr)type > (uintptr)-1-O(Type, map) || type->ref <= 0 ||
		   type->size < 64 || type->size%8 != 0 || (unsigned int)type->size > stop-current)
			return nil;
		size = type->size;
		mapbytes = (size+63)/64;
		if(type->np != mapbytes || (uintptr)type+O(Type, map) > (uintptr)-1-mapbytes ||
		   type->map[0] != 0 || !disabmap(&abi, size, type->map, mapbytes))
			return nil;
		end = current+size;
		if(current == target){
			if(type != expected || offset > size || size-offset < 8)
				return nil;
			slot = offset/8;
			if(type->map[slot/8] & (0x80>>(slot%8)))
				return nil;
			found = 1;
		}
	}
	return found ? (void*)(target+offset) : nil;
}
#endif
