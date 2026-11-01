/* Included only by the owning Math implementation after its selected generated declarations. */
#include "native64-math-call.h"

#ifdef DIS_NATIVE64_FIXED_MATH_CALL
#if DIS_NATIVE64_FIXED_MATH_CALL != 1
#error DIS_NATIVE64_FIXED_MATH_CALL must be 1 when explicitly selected
#endif
#if !defined(DIS_WORD32) || DIS_WORD32 != 1
#error fixed Math call requires DIS_WORD32=1
#endif
#if !defined(DIS_NATIVE64_MATH_HEADER) || !defined(DIS_NATIVE64_MATH_REGISTER)
#error fixed Math call requires explicit native64 Math registration metadata
#endif
#endif

#if defined(DIS_NATIVE64_FIXED_MATH_CALL)
#include "kernel.h"
#include "dis64-bindings.h"
#include "native64-model.h"

static const uchar native64bits_signature[] = {
	0,0,0,1, 0,0,0,3, 0,0,0,1, 0,0,0,2
}; // Agreed v1 Real(Big), never a legacy signature hash.

typedef struct Native64BitsState Native64BitsState;

struct Native64BitsState
{
	Native64MathBitsCall request; // Snapshot; no request/serialized bytes are reread after allocation starts.
	Module *module; // One retained registry owner through callback and retirement.
	Type *type; // One retained full generated frame descriptor.
	void (*function)(void*); // Matched registered implementation, not a wire address.
	F_Math_bits64real *frame; // Owned non-GC scalar frame; no managed pointer slots.
	REAL value; // Private aligned scalar result, published only after successful completion.
};


/* Compare a checked Rune32 name with its exact ASCII identity, preserving all high bits. */
static int
native64bitsname(const DisStringPoolView *pool, unsigned int id, const char *name)
{
	DisRuneView view;
	unsigned int i;

	if(!disstringpoolget(pool, id, &view))
		return 0;
	for(i=0; name[i] != 0; i++)
		if(i >= view.count || disruneget(view.values+4*i) != (uchar)name[i])
			return 0;
	return i == view.count;
}


/* Match complete canonical import/name bytes before touching the runtime registry. */
static int
native64bitsimport(const Native64MathBitsCall *request)
{
	DisStringPoolView pool;
	Dis64Bindings imports;
	Dis64Binding binding;
	Dis64Signature expected;
	int equal;

	if(request->string_bytes > 65536 || request->import_bytes > 65536 ||
	   !disstringpoolcheck(request->strings, request->string_bytes, 256, &pool) ||
	   !dis64bindingscheck(request->imports, request->import_bytes, 0, &pool, nil, 0, 256, 16, &imports) ||
	   !dis64bindingget(&imports, request->ordinal, &binding) ||
	   !native64bitsname(&pool, binding.module, "$Math") ||
	   !native64bitsname(&pool, binding.symbol, "bits64real") ||
	   !dis64signaturecheck(native64bits_signature, sizeof(native64bits_signature), 1, &expected) ||
	   !dis64signatureequal(&binding.signature, &expected, &equal))
		return 0;
	return equal;
}


/* Retain exact generated metadata; hash equality alone never selects a callback. */
static int
native64bitsbind(Native64BitsState *state)
{
	const LimboNative64Entry *entry;
	Module *module;
	Type *type;
	int i, j;
	unsigned int signature;

	entry = &limbo_native64_Math_bits64real_entry;
	for(module=modules; module != nil; module=module->link){
		if(module->path == nil || strcmp(module->path, "$Math") != 0)
			continue;
		if(module->ref <= 0 || module->ref == 0x7fffffff || module->ntype < 0 ||
		   module->ntype > 4096 || module->ext == nil || module->type == nil)
			return 0;
		for(i=0; i<module->ntype; i++){
			if(module->ext[i].name == nil || strcmp(module->ext[i].name, "bits64real") != 0)
				continue;
			type = module->ext[i].frame;
			memmove(&signature, &module->ext[i].sig, sizeof(signature));
			if(type == nil || type == H || type != module->type[i] ||
			   type->ref <= 0 || type->ref == 0x7fffffff ||
			   module->ext[i].u.runt != entry->function || signature != entry->legacy_signature ||
			   type->size != entry->frame_size || type->np != entry->map_bytes ||
			   type->size != sizeof(F_Math_bits64real))
				return 0;
			for(j=0; j<type->np; j++)
				if(type->map[j] != 0 || type->map[j] != entry->map[j])
					return 0;
			module->ref++;
			type->ref++;
			state->module = module;
			state->type = type;
			state->function = module->ext[i].u.runt;
			return 1;
		}
		return 0;
	}
	return 0;
}


/* Invoke through the registered table, then resolve the logical scalar destination late. */
static int
native64bitsperform(void *argument)
{
	Native64BitsState *state;
	void *slot;

	state = argument;
	state->frame = mallocz(state->type->size, 1);
	if(state->frame == nil){
		kwerrstr("out of memory constructing fixed Math frame");
		return 0;
	}
	state->frame->regs.t = state->type;
	state->frame->ret = &state->value;
	memmove(&state->frame->b, &state->request.input, sizeof(state->frame->b));
	state->function(state->frame);
	slot = state->request.resolve(state->request.argument);
	if(slot == nil)
		return 0;
	if(slot == H || (uintptr)slot > (uintptr)-1-sizeof(state->value)){
		kwerrstr("bad fixed Math result destination");
		return 0;
	}
	memmove(slot, &state->value, sizeof(state->value));
	return 1;
}
#endif


/* Keep every owner outside the protected error frame and retire it once on every path. */
int
Math_bits64real64(const Native64MathBitsCall *request)
{
#if defined(DIS_NATIVE64_FIXED_MATH_CALL)
	Native64BitsState state;
	char saved[ERRMAX];
	int status;

	if(request == nil || request->resolve == nil || !native64model())
		return 0;
	memset(&state, 0, sizeof(state));
	state.request = *request;
	if(!native64bitsimport(&state.request) || !native64bitsbind(&state))
		return 0;
	status = kprotect(native64bitsperform, &state);
	kgerrstr(saved, sizeof(saved));
	free(state.frame);
	freetype(state.type);
	unload(state.module);
	kwerrstr("%s", saved);
	return status;
#else
	USED(request);
	return 0;
#endif
}


#if defined(DIS_NATIVE64_FIXED_MATH_CALL)
#include "native64-frame-cell.h"


/* Never retain a physical stack result address across a producer or safepoint. */
static void*
native64bitsframe(void *argument)
{
	Native64MathFrameCall *request;

	request = argument;
	return native64framecell(request->caller, request->type, request->offset);
}
#endif


/* Adapt only a live typed current frame; actual import dispatch keeps its existing owners. */
int
Math_bits64realframe64(const Native64MathFrameCall *request)
{
#if defined(DIS_NATIVE64_FIXED_MATH_CALL)
	Native64MathFrameCall owned;

	if(request == nil)
		return 0;
	owned = *request;
	if(native64bitsframe(&owned) == nil)
		return 0;
	owned.call.resolve = native64bitsframe;
	owned.call.argument = &owned;
	return Math_bits64real64(&owned.call);
#else
	USED(request);
	return 0;
#endif
}


#ifdef DIS_NATIVE64_CALLER_GUARD
#include "native64-caller.h"


/* Check the held process generation before consulting its stack storage. */
static void*
native64bitsguard(void *argument)
{
	return native64callerresolve(argument);
}
#endif


/* Keep one typed caller lease through protected production, late resolution and cleanup. */
static int
native64bitsleased(const Native64MathFrameCall *request, int live)
{
#ifdef DIS_NATIVE64_CALLER_GUARD
	Native64Caller caller;
	Native64MathBitsCall owned;
	char saved[ERRMAX];
	int status;

	if(request == nil)
		return 0;
	memset(&caller, 0, sizeof(caller));
	owned = request->call;
	if(!(live ? native64callerbeginlive(&caller, request->caller, request->type, request->offset) :
	             native64callerbegin(&caller, request->caller, request->type, request->offset)))
		return 0;
	owned.resolve = native64bitsguard; owned.argument = &caller;
	status = Math_bits64real64(&owned);
	kgerrstr(saved, sizeof(saved));
	native64callerclear(&caller);
	kwerrstr("%s", saved);
	return status;
#else
	USED(request); USED(live);
	return 0;
#endif
}


/* Retain the existing explicitly published current-frame contract. */
int
Math_bits64realguard64(const Native64MathFrameCall *request)
{
	return native64bitsleased(request, 0);
}


/* Acquire from interpreter-owned live R, publishing its validated caller roots before allocation. */
int
Math_bits64realvm64(const Native64MathFrameCall *request)
{
	return native64bitsleased(request, 1);
}


#ifdef DIS_NATIVE64_CALLER_GUARD
#include "native64-field.h"


/* Resolve declared scalar kind and retained caller/module generation after producer safepoints. */
static void*
native64bitsfield(void *argument)
{
	return native64fieldresolve(argument);
}
#endif


/* Keep the complete typed witness outside the protected producer's error frame and retire once. */
int
Math_bits64realfield64(const Native64MathFieldCall *request)
{
#ifdef DIS_NATIVE64_CALLER_GUARD
	Native64MathFieldCall owned;
	Native64FieldWitness witness;
	char saved[ERRMAX];
	int status;

	if(request == nil)
		return 0;
	owned = *request;
	memset(&witness, 0, sizeof(witness));
	if(!native64fieldbegin(&witness, owned.caller, owned.module, owned.type,
	   owned.fields, owned.ordinal, Dis64Real, owned.live))
		return 0;
	owned.call.resolve = native64bitsfield; owned.call.argument = &witness;
	status = Math_bits64real64(&owned.call);
	kgerrstr(saved, sizeof(saved));
	native64fieldclear(&witness);
	kwerrstr("%s", saved);
	return status;
#else
	USED(request);
	return 0;
#endif
}
