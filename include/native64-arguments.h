#ifndef INFERNO_NATIVE64_ARGUMENTS_H
#define INFERNO_NATIVE64_ARGUMENTS_H // Private synchronous argument owner; no target or file admission.
#include "native64-tail.h"
#include "native64-result.h"
#include "raise.h"

typedef struct Native64ArgumentSource Native64ArgumentSource;

struct Native64ArgumentSource
{
	const LimboNative64Entry *fixed; // Trusted callback-free fixed-prefix specification.
	const void *prefix; // Borrowed checked bytes after the reserved64-byte header.
	unsigned int prefix_bytes; // Exactly fixed->frame_size-64.
	const Native64TailSlot *slots; // Borrowed immutable typed-tail descriptions.
	unsigned int count; // Logical tail slot count.
	const uvlong *values; // Caller-rooted canonical tail values.
	unsigned int value_bytes; // Exact readable value span.
	int (*prefix_check)(void*, const void*, unsigned int, int); // Pure schema check: canonical phase0, references phase1.
	int (*reference_check)(void*, void*, Type*); // Pure nonthrowing exact tail-reference Type checker.
	void *argument; // Borrowed checker context, live only throughout construction.
};

typedef struct Native64Arguments Native64Arguments;

struct Native64Arguments
{
	Native64TailPlan plan; // Owned scalar geometry, independent of the source records.
	Native64TailSlot *slots; // Owned immutable copied descriptors after successful construction.
	uchar *map; // Owned complete fixed+tail map snapshot.
	Type *type; // One retained dynamic frame descriptor owner.
	Native64Result frame; // One owned/pinned managed frame, initialized before reference publication.
	unsigned int typed; // Processed slots whose reference Type owners have been acquired.
	uchar *prefix; // Private construction scratch; nil after success.
	uvlong *values; // Private construction scratch; nil after success.
};

typedef struct Native64ArgumentBuild Native64ArgumentBuild;

struct Native64ArgumentBuild
{
	Native64Arguments *owned; // Mutable resources outside the runtime's setjmp frame.
	const Native64ArgumentSource *source; // Readable immutable caller input until construction returns.
};


/* Initialize fresh or already retired storage; never use this to overwrite a live owner. */
static void
native64argumentsinit(Native64Arguments *owned)
{
	memset(owned, 0, sizeof(*owned));
	owned->frame.value = H;
}


/* Reject overwriting live resources before inspecting a proposed source. */
static int
native64argumentsempty(const Native64Arguments *owned)
{
	return owned != nil && owned->slots == nil && owned->map == nil && owned->type == nil &&
		owned->frame.value == H && owned->frame.rooted == 0 && owned->typed == 0 &&
		owned->prefix == nil && owned->values == nil;
}


/* Detach all resources before cleanup; payload destructors must not throw, yield or reenter construction. */
static void
native64argumentsclear(Native64Arguments *owned)
{
	Native64Arguments previous;
	unsigned int i;
	char saved[ERRMAX];

	if(owned == nil)
		return;
	if(native64argumentsempty(owned)){
		native64argumentsinit(owned);
		return;
	}
	kgerrstr(saved, sizeof(saved));
	previous = *owned;
	native64argumentsinit(owned);
	native64resultclear(&previous.frame);
	freetype(previous.type);
	for(i=0; i<previous.typed; i++)
		if(previous.slots[i].kind == Native64TailRef)
			freetype(previous.slots[i].type);
	free(previous.values);
	free(previous.prefix);
	free(previous.map);
	free(previous.slots);
	kwerrstr("%s", saved);
}


/* Preflight all geometry, value schemas and aggregate Type reservations before any allocation. */
static int
native64argumentscheck(const Native64ArgumentSource *source, Native64TailPlan *plan)
{
	unsigned int i, j, references;
	Type *type;

	if(!native64model() || source == nil ||
	   !native64tailplan(source->fixed, source->slots, source->count, plan) ||
	   source->prefix_bytes != plan->tail-64 ||
	   (source->prefix_bytes != 0 && (source->prefix == nil || source->prefix == H ||
		(uintptr)source->prefix > (uintptr)-1-source->prefix_bytes || source->prefix_check == nil)))
		return 0;
	if(!native64tailcanonical(source->fixed, source->slots, source->count, source->values, source->value_bytes))
		return 0;
	if(source->prefix_bytes != 0 &&
	   !source->prefix_check(source->argument, source->prefix, source->prefix_bytes, 0))
		return 0;
	for(i=0; i<source->count; i++){
		if(source->slots[i].kind != Native64TailRef)
			continue;
		type = source->slots[i].type;
		references = 0;
		for(j=0; j<source->count; j++)
			if(source->slots[j].kind == Native64TailRef && source->slots[j].type == type)
				references++;
		if(type->ref <= 0 || (unsigned int)type->ref > 0x7fffffffU-references)
			return 0;
	}
	if(source->prefix_bytes != 0 &&
	   !source->prefix_check(source->argument, source->prefix, source->prefix_bytes, 1))
		return 0;
	if(!native64tailvalues(source->fixed, source->slots, source->count, source->values,
	   source->value_bytes, source->reference_check, source->argument))
		return 0;
	return 1;
}


/* Acquire exactly one value owner before publishing its mapped cell; no fallible operation intervenes. */
static void
native64argumentstore(void *slot, void *value, Type *expected)
{
	Heap *heap;

	if(value != H){
		if(value == nil)
			error("bad native64 argument owner");
		heap = D2H(value);
		if(heap->ref <= 0 || heap->ref == 0x7fffffff || heap->t == nil || heap->t == H ||
		   (expected != nil && heap->t != expected))
			error("bad native64 argument owner");
		heap->ref++;
		Setmark(heap);
	}
	memmove(slot, &value, sizeof(value));
}


/* Keep every fallible acquisition visible to cleanup; caller input owners remain rooted throughout. */
static int
native64argumentsconstruct(void *argument)
{
	Native64ArgumentBuild *build;
	Native64Arguments *owned;
	const Native64ArgumentSource *source;
	uchar *frame;
	void *value;
	Type *type;
	unsigned int i, slot;

	build = argument;
	owned = build->owned;
	source = build->source;
	if(source->count != 0){
		owned->slots = mallocz(source->count*sizeof(*owned->slots), 0);
		if(owned->slots == nil)
			goto nomem;
		memmove(owned->slots, source->slots, source->count*sizeof(*owned->slots));
		owned->values = mallocz(source->value_bytes, 0);
		if(owned->values == nil)
			goto nomem;
		memmove(owned->values, source->values, source->value_bytes);
	}
	if(source->prefix_bytes != 0){
		owned->prefix = mallocz(source->prefix_bytes, 0);
		if(owned->prefix == nil)
			goto nomem;
		memmove(owned->prefix, source->prefix, source->prefix_bytes);
	}
	owned->map = mallocz(owned->plan.map_bytes, 0);
	if(owned->map == nil)
		goto nomem;
	if(!native64tailmap(source->fixed, owned->slots, source->count, owned->map, owned->plan.map_bytes))
		return 0;
	for(i=0; i<source->count; i++){
		if(owned->slots[i].kind == Native64TailRef){
			type = owned->slots[i].type;
			if(type->ref <= 0 || type->ref == 0x7fffffff)
				error("bad native64 argument Type owner");
			type->ref++;
		}
		owned->typed = i+1;
	}
	owned->type = dtype(freeheap, owned->plan.size, owned->map, owned->plan.map_bytes);
	if(owned->type == nil)
		goto nomem;
	/* heapz initializes every mapped cell to H before the external pin can publish the frame. */
	frame = H2D(void*, heapz(owned->type));
	if(!native64resultadopt(&owned->frame, frame))
		return 0;
	for(i=64; i<owned->plan.tail; i+=8){
		slot = i/8;
		if(owned->map[slot/8] & (0x80 >> (slot%8))){
			memmove(&value, owned->prefix+i-64, sizeof(value));
			native64argumentstore(frame+i, value, nil);
		}else
			memmove(frame+i, owned->prefix+i-64, 8);
	}
	for(i=0; i<source->count; i++){
		if(owned->slots[i].kind == Native64TailRef){
			value = (void*)(uintptr)owned->values[i];
			native64argumentstore(frame+owned->plan.tail+i*8, value, owned->slots[i].type);
		}else
			memmove(frame+owned->plan.tail+i*8, owned->values+i, 8);
	}
	free(owned->values);
	owned->values = nil;
	free(owned->prefix);
	owned->prefix = nil;
	return 1;
nomem:
	error(exNomem);
	return 0;
}


/* Snapshot a checked synchronous argument context; no formatter/target executes and no file call is admitted.
 * Inputs/checker context stay readable, immutable and caller-rooted until return. Checkers are pure/nonthrowing.
 * Ordinary heap/pin allocation hooks must not yield or collect a newly returned unpinned owner. */
static int
native64argumentsbuild(Native64Arguments *owned, const Native64ArgumentSource *source)
{
	Native64ArgumentBuild build;
	Native64TailPlan plan;

	if(!native64argumentsempty(owned) || !native64argumentscheck(source, &plan))
		return 0;
	owned->plan = plan;
	build.owned = owned;
	build.source = source;
	if(!kprotect(native64argumentsconstruct, &build)){
		native64argumentsclear(owned);
		return 0;
	}
	return 1;
}

typedef struct Native64ArgumentCall Native64ArgumentCall;

struct Native64ArgumentCall
{
	const Native64Arguments *arguments; // Borrowed owned frame, pinned throughout the synchronous callback.
	int (*produce)(Native64Result*, const Native64Arguments*, void*); // Trusted producer adopts its result before fallible work.
	void *argument; // Borrowed callback state, valid until result and argument cleanup finish.
	void *(*resolve)(void*); // Trusted current logical result cell, resolved only after production.
};


/* Adapt the private typed context without exposing its construction scratch to the target. */
static int
native64argumentsproduce(Native64Result *result, void *argument)
{
	Native64ArgumentCall *call;

	call = argument;
	return call->produce(result, call->arguments, call->argument);
}


/* Resolve through the original callback state after all producer allocations have completed. */
static void*
native64argumentsresolve(void *argument)
{
	Native64ArgumentCall *call;

	call = argument;
	return call->resolve(call->argument);
}


/* Run one synchronous trusted C transaction; no escaping context, Sys registration or wire call admission. */
static int
native64argumentscall(const Native64ArgumentSource *source,
	int (*produce)(Native64Result*, const Native64Arguments*, void*),
	void *(*resolve)(void*), void *argument)
{
	Native64Arguments owned;
	Native64ArgumentCall call;
	int status;

	if(produce == nil || resolve == nil)
		return 0;
	native64argumentsinit(&owned);
	if(!native64argumentsbuild(&owned, source))
		return 0;
	call.arguments = &owned;
	call.produce = produce;
	call.argument = argument;
	call.resolve = resolve;
	status = native64resultresolved(native64argumentsproduce, native64argumentsresolve, &call);
	native64argumentsclear(&owned);
	return status;
}
#endif
