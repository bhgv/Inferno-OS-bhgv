#ifndef INFERNO_NATIVE64_TAIL_H
#define INFERNO_NATIVE64_TAIL_H // Private typed-tail preflight/map prototype; no invocation or wire admission.
#include "native64-entry.h"
#include "native64-bind.h"
#include "native64-model.h"

/* Private C tags only; never serialize their numeric values. */
enum {
	Native64TailByte, // Unsigned8 zero-extended to the slot width.
	Native64TailInt, // Signed32 sign-extended to the slot width.
	Native64TailFix, // Signed32 raw scaled value, with the same extension.
	Native64TailBig, // Exact signed64 bit pattern.
	Native64TailReal, // Exact IEEE binary64 bits, including NaNs.
	Native64TailRef, // Trusted managed reference or H, never nil0.
};

typedef struct Native64TailSlot Native64TailSlot;

struct Native64TailSlot
{
	unsigned int kind; // One private tag from the enum above.
	unsigned int offset; // Exact contiguous byte offset relative to the tail base.
	unsigned int bytes; // Always8 for this internal prototype.
	Type *type; // Trusted exact heap Type identity for references; nil for scalars.
};

typedef struct Native64TailPlan Native64TailPlan;

struct Native64TailPlan
{
	unsigned int tail; // Aligned end of the validated fixed prefix.
	unsigned int size; // Complete frame extent, including the tail.
	unsigned int map_bytes; // Complete ceil(size/64) map8 storage.
	unsigned int count; // Number of contiguous eight-byte tail slots.
};


/* Match a trusted generated variadic schema to the consuming C prefix, without granting a call. */
static const LimboNative64Entry*
native64tailprefix(const LimboNative64Prefix *prefix, const char *name, UWORD signature,
	unsigned int arguments, unsigned int bytes, const uchar *map, unsigned int mapbytes)
{
	if(!native64model() || prefix == nil || arguments > Native64BindEntries ||
	   prefix->arguments != arguments || !native64bindrecord(&prefix->fixed, 0) ||
	   !native64bindname(name) || !native64bindsame(prefix->fixed.name, name) ||
	   prefix->fixed.legacy_signature != signature || prefix->fixed.frame_size != bytes ||
	   prefix->fixed.map_bytes != mapbytes || map == nil ||
	   memcmp(prefix->fixed.map, map, mapbytes) != 0)
		return nil;
	return &prefix->fixed;
}

/* Validate all descriptors before reading values or changing output; borrowed metadata must remain immutable. */
static int
native64tailplan(const LimboNative64Entry *fixed, const Native64TailSlot *slots,
	unsigned int count, Native64TailPlan *output)
{
	Native64TailPlan plan;
	unsigned int i;

	if(!native64model() || output == nil || count > Native64BindEntries ||
	   !native64bindrecord(fixed, 0) || (count != 0 && slots == nil))
		return 0;
	if(count > (Native64BindFrame-fixed->frame_size)/8)
		return 0;
	for(i=0; i<count; i++){
		if(slots[i].kind > Native64TailRef || slots[i].offset != i*8 || slots[i].bytes != 8)
			return 0;
		if(slots[i].kind == Native64TailRef){
			if(slots[i].type == nil || slots[i].type == H)
				return 0;
		}else if(slots[i].type != nil)
			return 0;
	}
	plan.tail = fixed->frame_size;
	plan.size = plan.tail+count*8;
	plan.map_bytes = (plan.size+63)/64;
	plan.count = count;
	*output = plan;
	return 1;
}


/* Derive full fixed+tail roots without reading argument values; output storage must be disjoint from inputs. */
static int
native64tailmap(const LimboNative64Entry *fixed, const Native64TailSlot *slots,
	unsigned int count, uchar *output, unsigned int capacity)
{
	Native64TailPlan plan;
	unsigned int i, slot;

	if(!native64tailplan(fixed, slots, count, &plan) || output == nil || capacity < plan.map_bytes)
		return 0;
	memset(output, 0, plan.map_bytes);
	memmove(output, fixed->map, fixed->map_bytes);
	for(i=0; i<count; i++){
		if(slots[i].kind != Native64TailRef)
			continue;
		slot = plan.tail/8+i;
		output[slot/8] |= 0x80 >> (slot%8);
	}
	return 1;
}


/* Check all canonical scalar/nil bits after descriptor preflight, without consulting live reference services. */
static int
native64tailcanonical(const LimboNative64Entry *fixed, const Native64TailSlot *slots,
	unsigned int count, const uvlong *values, unsigned int bytes)
{
	Native64TailPlan plan;
	uvlong value, upper;
	unsigned int i;

	if(!native64tailplan(fixed, slots, count, &plan) || bytes != count*8 ||
	   (count != 0 && values == nil))
		return 0;
	for(i=0; i<count; i++){
		value = values[i];
		switch(slots[i].kind){
		case Native64TailByte:
			if(value > 255)
				return 0;
			break;
		case Native64TailInt:
		case Native64TailFix:
			upper = value >> 32;
			if(upper != ((value & 0x80000000ULL) ? 0xffffffffULL : 0))
				return 0;
			break;
		case Native64TailRef:
			if(value == 0)
				return 0;
			break;
		default:
			break;
		}
	}
	return 1;
}


/* Check exact live identities only after every canonical value passes; callers keep readable inputs rooted. */
static int
native64tailvalues(const LimboNative64Entry *fixed, const Native64TailSlot *slots,
	unsigned int count, const uvlong *values, unsigned int bytes,
	int (*reference)(void*, void*, Type*), void *context)
{
	unsigned int i;

	if(!native64tailcanonical(fixed, slots, count, values, bytes))
		return 0;
	for(i=0; i<count; i++){
		if(slots[i].kind != Native64TailRef || values[i] == (uvlong)(uintptr)H)
			continue;
		if(reference == nil || !reference(context, (void*)(uintptr)values[i], slots[i].type))
			return 0;
	}
	return 1;
}
#endif
