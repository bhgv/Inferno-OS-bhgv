#ifndef DIS_DEC_ADDR_H
#define DIS_DEC_ADDR_H // Shared native-safe address decoding emitted by decgen.
#include "raise.h"
#include "kernel.h"

#if defined(__GNUC__)
#define DecInline static __inline__ __attribute__((always_inline)) // Keep native decoding checks in the generated hot path.
#else
#define DecInline static // Preserve compilers without GNU inline attributes.
#endif


/* Capture the full instruction before register publication or unaligned field reads. */
DecInline void
decinstruction(Inst *pc)
{
	if(R.PC == nil || R.PC == H)
		error(exNilref);
	if((uintptr)R.PC > (uintptr)-1-sizeof(*pc))
		error(exBounds);
	memmove(pc, R.PC, sizeof(*pc));
}


/* Preserve signed Dis offsets without pointer overflow or negating the minimum WORD. */
DecInline uchar*
decaddress(uchar *base, WORD offset)
{
	uintptr address;
	UWORD amount;

	if(base == nil || base == H)
		error(exNilref);
	address = (uintptr)base;
	if(offset >= 0){
		amount = (UWORD)offset;
		if(amount > (uintptr)-1-address)
			error(exBounds);
		address += amount;
	}else{
		amount = (UWORD)0-(UWORD)offset;
		if(amount > address)
			error(exBounds);
		address -= amount;
	}
	return (uchar*)address;
}


/* Copy native pointer cells at any byte alignment before applying the second offset. */
DecInline uchar*
decindirect(uchar *base, ushort first, ushort second)
{
	uchar *cell, *target;

	cell = decaddress(base, first);
	if((uintptr)cell > (uintptr)-1-sizeof(target))
		error(exBounds);
	memmove(&target, cell, sizeof(target));
	return decaddress(target, second);
}
#undef DecInline
#endif
