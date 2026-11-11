#include "lib9.h"
#include <limits.h>
#include <float.h>
#include "isa.h"
#include "interp.h"
#include "raise.h"
#include "kernel.h"
#include "dis64-pilot.h"
#include "dis64-call-infer.h"
#include "dis64-storage-image.h"
#include "dis64-numeric-storage.h"
#if defined(DIS_NATIVE64_SCALAR_BOOT) && defined(DIS64_PILOT_LOADER)
#include "dis64-managed-signature.h"
#include "dis64-managed-trial.h"
#endif

#ifdef DIS_NATIVE64_MANAGED_FILES
#include "string-native64.h"
#endif

#define Dis64RuntimeBytes (8U*1024U*1024U) // Maximum complete input image for this experimental loader.
#define Dis64RuntimeObject (1024U*1024U) // Maximum scalar data or entry-frame extent.
#define Dis64RuntimeCode 65536U // Maximum instructions admitted before allocation.
#define Dis64RuntimeName 255U // Maximum ASCII display-name length in the scalar runtime subset.

typedef struct Dis64Load Dis64Load;

struct Dis64Load {
	char *path; // Borrowed caller label, copied before successful return.
	Dis64Pilot pilot; // Complete checked immutable source, retained through construction.
	Module *module; // Partial unpublished owner, visible to protected failure cleanup.
	int jit; // Explicit request for the existing native leaf compiler.
};


/* Reject native models which cannot preserve Frame64, scalar widths and IEEE binary64. */
static int
dis64runtimemodel(void)
{
	return sizeof(WORD) == 4 && sizeof(UWORD) == 4 && sizeof(void*) == 8 &&
		sizeof(LONG) == 8 && sizeof(REAL) == 8 && sizeof(Adr) >= 8 &&
		sizeof(ushort) == 2 && CHAR_BIT == 8 && FLT_RADIX == 2 &&
		DBL_MANT_DIG == 53 && DBL_MAX_EXP == 1024 && sizeof(Frame) == 32 &&
		O(Frame, lr) == 0 && O(Frame, fp) == 8 && O(Frame, mr) == 16 && O(Frame, t) == 24;
}


static int dis64calls; // Explicit Inferno startup selection, never inferred from file bytes.
static int dis64sealed; // First namespace use freezes selection for the process lifetime.


/* Select the agreed experimental profile before namespace use or cache publication. */
int
dis64selectlaunch(char *name)
{
#if defined(DIS64_PILOT_LOADER) && DIS64_PILOT_LOADER == 1 && defined(DIS_WORD32) && DIS_WORD32 == 1
	if(name == nil || name == H || dis64sealed || modules != nil ||
	   strcmp(name, "dis64-calls") != 0 || !dis64runtimemodel())
		return 0;
	dis64calls = 1;
	return 1;
#else
	USED(name);
	return 0;
#endif
}


/* Freeze before file I/O or cache reuse; existing VM admission serializes this startup state. */
int
dis64launchmode(void)
{
	dis64sealed = 1;
	return dis64calls;
}


/* Filter cache mode only; this is not raw proof, provenance, retention or an execution capability. */
int
dis64launchcompatible(Module *module)
{
	int calls;

	calls = dis64launchmode();
	if(module == nil)
		return 0;
#ifdef DIS_NATIVE64_MANAGED_FILES
	if(module->rt & Dis64ManagedFile)
		return dis64bootmanaged && !calls;
	if(dis64bootmanaged)
		return 0;
#endif
	if(calls)
		return module->cstep == native64lea8step;
	return module->cstep != native64lea8step;
}


/* Identify source operands whose native decoder must expose a complete LONG. */
static int
dis64longsource(unsigned int op)
{
	switch(op){
	case IMOVL:
	case IADDL: case ISUBL: case IMULL: case IDIVL: case IMODL:
	case IANDL: case IORL: case IXORL:
	case IBEQL: case IBNEL: case IBLTL: case IBLEL: case IBGTL: case IBGEL:
		return 1;
	}
	return 0;
}


/* Bound legacy middle slots separately from the wider, unchanged wire operands. */
static int
dis64runtimeinstruction(const Dis64Instruction *instruction)
{
	unsigned int middle;

	middle = instruction->mode&ARM;
	if(middle == AXIMM){
		if(instruction->middle < -32768 || instruction->middle > 32767)
			return 0;
		/* R.t is WORD-sized; a LONG comparison/arithmetic source cannot borrow it. */
		if(dis64longsource(instruction->op) || instruction->op == ISHLL || instruction->op == ISHRL)
			return 0;
	}else if(middle != AXNON && (instruction->middle < 0 || instruction->middle > 65535))
		return 0;
	return 1;
}


/* Convert checked roles to existing decoder storage without truncating immediates. */
static void
dis64runtimeinst(Inst *target, const Dis64Instruction *source, Inst *program)
{
	LONG immediate;
	unsigned int mode;

	memset(target, 0, sizeof(*target));
	target->op = source->op;
	target->add = source->mode;
	if((source->mode&ARM) != AXNON)
		target->reg = source->middle < 0 ? source->middle+65536 : source->middle;
	mode = USRC(source->mode);
	if(mode == AIMM && dis64longsource(source->op)){
		immediate = source->source;
		memmove(&target->s, &immediate, sizeof(immediate));
	}else if(mode != AXXX)
		target->s.imm = source->source;
	mode = UDST(source->mode);
	if(mode == AIMM)
		target->d.ins = program+source->destination;
	else if(mode != AXXX)
		target->d.ind = source->destination;
}


/* Build actual descriptors, data and instructions while every resource stays owned. */
static int
dis64construct(void *argument)
{
	Dis64Load *load;
	Dis64Instruction instruction;
	DisRuneView name;
	Module *module;
	Heap *data;
	unsigned int i, rune, at, used;

	load = argument;
	load->module = module = mallocz(sizeof(*module), 1);
	if(module == nil)
		return 0;
	module->origmp = H;
	module->ref = 1;
	module->rt = DONTCOMPILE;
	module->ntype = 2;
	module->type = mallocz(2*sizeof(Type*), 1);
	if(module->type == nil)
		return 0;
	/* No type-index operations exist in this subset: runtime slot0 is always MP. */
	module->type[0] = dtype(freeheap, load->pilot.data.bytes, nil, 0);
	if(module->type[0] == nil)
		return 0;
	module->type[1] = dtype(freeheap, load->pilot.frame.bytes, nil, 0);
	if(module->type[1] == nil)
		return 0;
	module->entryt = module->type[1];
	module->nprog = load->pilot.code.count;
	module->prog = mallocz(module->nprog*sizeof(Inst), 1);
	if(module->prog == nil)
		return 0;
	at = 4;
	for(i=0; i<load->pilot.code.count; i++){
		if(!dis64instructionread(load->pilot.code.data+at, load->pilot.code.bytes-at,
		   &instruction, &used))
			return 0;
		dis64runtimeinst(module->prog+i, &instruction, module->prog);
		at += used;
	}
	module->entry = module->prog+load->pilot.module.entry;
	if(load->pilot.data.bytes != 0){
		data = heapz(module->type[0]);
		module->origmp = H2D(uchar*, data);
		if(!dis64initializersapply(&load->pilot.initializers, &load->pilot.data,
		   module->origmp, load->pilot.data.bytes, nil))
			return 0;
	}
	if(!disstringpoolget(&load->pilot.strings, load->pilot.module.name, &name))
		return 0;
	module->name = mallocz(name.count+1, 1);
	if(module->name == nil)
		return 0;
	for(i=0; i<name.count; i++){
		if(!disruneat(&name, i, &rune))
			return 0;
		module->name[i] = rune;
	}
	module->path = strdup(load->path);
	if(module->path == nil)
		return 0;
	if(load->jit && !compile(module, module->nprog, nil))
		return 0;
	return 1;
}


/* Return an unpublished scalar Module; the caller owns it and must unload or schedule it.
 * This explicit construction API does not route readmod, choose file policy or publish
 * a cache entry. Input bytes remain immutable and disjoint through this call. */
Module*
prepare64pilot(char *path, uchar *image, ulong bytes, int jit)
{
	Dis64Load load;
	Dis64Instruction instruction;
	DisRuneView name;
	unsigned int i, rune, at, used;

	memset(&load, 0, sizeof(load));
	if(!dis64runtimemodel() ||
	   path == nil || path == H || image == nil || image == H ||
	   bytes > Dis64RuntimeBytes || (uintptr)image > (uintptr)-1-bytes ||
	   (jit != 0 && jit != 1)){
		kwerrstr("unsupported Dis64 runtime model or input");
		return nil;
	}
	if(!dis64pilotcheck(image, bytes, Dis64RuntimeObject, Dis64RuntimeCode, &load.pilot) ||
	   !disstringpoolget(&load.pilot.strings, load.pilot.module.name, &name) ||
	   name.count == 0 || name.count > Dis64RuntimeName){
		kwerrstr("unsupported Dis64 scalar module");
		return nil;
	}
	for(i=0; i<name.count; i++)
		if(!disruneat(&name, i, &rune) || rune == 0 || rune > 127){
			kwerrstr("unsupported Dis64 runtime display name");
			return nil;
		}
	at = 4;
	for(i=0; i<load.pilot.code.count; i++){
		if(!dis64instructionread(load.pilot.code.data+at, load.pilot.code.bytes-at,
		   &instruction, &used) || !dis64runtimeinstruction(&instruction)){
			kwerrstr("unsupported Dis64 runtime operand");
			return nil;
		}
		at += used;
	}
	load.path = path;
	load.jit = jit;
	if(kprotect(dis64construct, &load))
		return load.module;
	if(load.module != nil){
		destroy(load.module->origmp);
		freemod(load.module);
	}
	kwerrstr("Dis64 module construction failed");
	return nil;
}

#define Dis64CallRuntimeCode 256U // Bounded experimental constructor policy, not a wire code limit.

typedef struct Dis64CallRuntime Dis64CallRuntime;

struct Dis64CallRuntime {
	Dis64CallInference inferred; // Disposable raw-byte local-kind authority, never a manifest.
	Dis64CallDecoded decoded[Dis64CallRuntimeCode]; // Owned complete lowering inputs.
	Dis64InitWork scratch[Dis64CallRuntimeCode]; // Disposable definite-assignment/return proof workspace.
	unsigned int queue[Dis64CallRuntimeCode]; // Bounded verification work queue.
	Dis64BoundCall proof[Dis64LocalFunctions]; // Complete proof records, not a persistent execution token.
	Dis64CallImage image; // Borrowed checked source, used only before constructor return.
};

typedef struct Dis64CallLoad Dis64CallLoad;

struct Dis64CallLoad {
	char *path; // Immutable caller label copied before publication.
	Dis64CallRuntime *work; // Temporary native allocation released on all return paths.
	DisRuneView name; // Validated ASCII name view, never installed as borrowed Module storage.
	Module *module; // Partial unpublished owner retained across protected VM errors.
	int jit; // Explicit request for existing guarded native leaf compilation.
};


/* Construct owned descriptors/instructions only after raw inference and complete semantic proof. */
static int
dis64callconstruct(void *argument)
{
	Dis64CallLoad *load;
	Dis64CallRuntime *work;
	Dis64Type type;
	Dis64CallDecoded *decoded;
	Module *module;
	Inst *instruction;
	unsigned int i, rune;

	load = argument;
	work = load->work;
	load->module = module = mallocz(sizeof(*module), 1);
	if(module == nil)
		return 0;
	module->origmp = H;
	module->ref = 1;
	module->rt = DONTCOMPILE;
	module->ntype = work->image.types.count;
	module->type = mallocz(module->ntype*sizeof(Type*), 1);
	if(module->type == nil)
		return 0;
	/* Preserve original Type ordinals: FRAME operands refer to these exact indices. */
	for(i=0; i<work->image.types.count; i++){
		if(!dis64typeget(&work->image.types, i, &type))
			return 0;
		module->type[i] = dtype(freeheap, type.bytes, nil, 0);
		if(module->type[i] == nil)
			return 0;
	}
	module->entryt = module->type[work->image.module.frametype];
	module->nprog = work->image.instructions;
	module->prog = mallocz(module->nprog*sizeof(Inst), 1);
	if(module->prog == nil)
		return 0;
	for(i=0; i<work->image.instructions; i++){
		decoded = work->decoded+i;
		instruction = module->prog+i;
		dis64runtimeinst(instruction, &decoded->instruction, module->prog);
		if(UDST(decoded->instruction.mode) == (AFP|AIND)){
			instruction->d.i.f = decoded->instruction.destination;
			instruction->d.i.s = decoded->secondary;
		}
	}
	module->entry = module->prog+work->image.module.entry;
	module->name = mallocz(load->name.count+1, 1);
	if(module->name == nil)
		return 0;
	for(i=0; i<load->name.count; i++){
		if(!disruneat(&load->name, i, &rune))
			return 0;
		module->name[i] = rune;
	}
	module->path = strdup(load->path);
	if(module->path == nil)
		return 0;
	/* Full-width result-address semantics belong only to this proved unpublished Module. */
	module->cstep = native64lea8step;
	if(load->jit && !compile(module, module->nprog, nil))
		return 0;
	return 1;
}


/* Return an unpublished scalar-call Module with no borrowed image/workspace storage.
 * One bounded native analysis allocation may occur before semantic refusal; no VM
 * owner is allocated until complete proof. This API does not select file policy,
 * cache a module, route readmod/parsemod or authorize imports/managed/Bools. */
Module*
prepare64calls(char *path, uchar *image, ulong bytes, int jit)
{
	Dis64CallLoad load;
	Dis64CallImage structural;
	Dis64CallDecoded *decoded;
	Dis64Section section;
	DisStringPoolView strings;
	Module *module;
	unsigned int i, rune, records;
	int success;

	memset(&load, 0, sizeof(load));
	if(!dis64runtimemodel() || path == nil || path == H || image == nil || image == H ||
	   bytes > Dis64CallImageBytes || (uintptr)image > (uintptr)-1-bytes ||
	   (jit != 0 && jit != 1)){
		kwerrstr("unsupported Dis64 call runtime model or input");
		return nil;
	}
	if(!dis64callimageopen(image, bytes, Dis64RuntimeObject, &structural) ||
	   structural.instructions == 0 || structural.instructions > Dis64CallRuntimeCode){
		kwerrstr("unsupported Dis64 call image");
		return nil;
	}
	/* Name/geometry refusals precede even the bounded temporary workspace allocation. */
	for(i=0; i<structural.envelope.sections; i++){
		if(!dis64section(&structural.envelope, i, &section))
			return nil;
		if(section.kind == Dis64Strings)
			break;
	}
	if(i == structural.envelope.sections ||
	   !disstringpoolcheck(image+section.offset, section.bytes, 65536, &strings) ||
	   !disstringpoolget(&strings, structural.module.name, &load.name) ||
	   load.name.count == 0 || load.name.count > Dis64RuntimeName){
		kwerrstr("unsupported Dis64 call display name");
		return nil;
	}
	for(i=0; i<load.name.count; i++)
		if(!disruneat(&load.name, i, &rune) || rune == 0 || rune > 127){
			kwerrstr("unsupported Dis64 call display name");
			return nil;
		}
	load.work = mallocz(sizeof(*load.work), 1);
	if(load.work == nil){
		kwerrstr("Dis64 call workspace allocation failed");
		return nil;
	}
	if(!dis64callinfercheck(image, bytes, Dis64RuntimeObject, &load.work->inferred,
	   load.work->decoded, load.work->scratch, load.work->queue, Dis64CallRuntimeCode,
	   Dis64CallLimit, load.work->proof, Dis64LocalFunctions, &records, &load.work->image)){
		free(load.work);
		kwerrstr("unsupported Dis64 scalar call module");
		return nil;
	}
	for(i=0; i<load.work->image.instructions; i++){
		decoded = load.work->decoded+i;
		if(!dis64runtimeinstruction(&decoded->instruction) ||
		   (UDST(decoded->instruction.mode) == (AFP|AIND) &&
		   (decoded->instruction.destination < 0 || decoded->instruction.destination > 65535 ||
		   decoded->secondary < 0 || decoded->secondary > 65535))){
			free(load.work);
			kwerrstr("unsupported Dis64 call runtime operand");
			return nil;
		}
	}
	load.path = path;
	load.jit = jit;
	success = kprotect(dis64callconstruct, &load);
	module = load.module;
	if(!success && module != nil){
		destroy(module->origmp);
		freemod(module);
		module = nil;
	}
	free(load.work);
	if(!success)
		kwerrstr("Dis64 call module construction failed");
	return module;
}


/* Native staging state is deliberately distinct from Module/Modlink execution owners. */
struct Dis64StorageOwner {
	Type **types; // Owned copies of every structurally checked descriptor and its map8.
	unsigned int count; // Complete array population, with nil partial entries on refusal.
	uchar *data; // Owned unpublished module-data object, or runtime H for zero bytes.
	int pinned; // Exactly one external GC root acquired by this lease.
};

typedef struct Dis64StorageLoad Dis64StorageLoad;

struct Dis64StorageLoad {
	Dis64StorageImage image; // Borrowed immutable checked input, used only during construction.
	Dis64StorageOwner *owner; // Partial native owner retained across protected errors.
};


/* Consume a staging lease under VM exclusion; neither a failed pin nor another root is removed. */
void
free64storage(Dis64StorageOwner *owner)
{
	unsigned int i;
	uchar *data;

	if(owner == nil)
		return;
	data = owner->data;
	owner->data = H;
	if(owner->pinned){
		owner->pinned = 0;
		ptrdel(D2H(data));
	}
	destroy(data);
	if(owner->types != nil){
		for(i=0; i<owner->count; i++)
			freetype(owner->types[i]);
		free(owner->types);
	}
	free(owner);
}


/* Adopt each allocation immediately; map bytes become independent of the input before data allocation. */
static int
dis64storageconstruct(void *argument)
{
	Dis64StorageLoad *load;
	Dis64StorageOwner *owner;
	Dis64Type type;
	Heap *heap;
	void *nilvalue;
	unsigned int i;

	load = argument;
	load->owner = owner = mallocz(sizeof(*owner), 1);
	if(owner == nil)
		return 0;
	owner->data = H;
	owner->count = load->image.types.count;
	if(owner->count != 0){
		owner->types = mallocz(owner->count*sizeof(Type*), 1);
		if(owner->types == nil)
			return 0;
	}
	for(i=0; i<owner->count; i++){
		if(!dis64typeget(&load->image.types, i, &type))
			return 0;
		owner->types[i] = dtype(freeheap, type.bytes, (uchar*)type.map, type.mapbytes);
		if(owner->types[i] == nil)
			return 0;
	}
	if(load->image.module.databytes != 0){
		heap = heapz(owner->types[load->image.module.datatype]);
		owner->data = H2D(uchar*, heap);
		/* ptradd's native metadata allocation does not run a VM collector poll.
		 * Acquire this root before any subsequent VM allocation or publication. */
		ptradd(heap);
		owner->pinned = 1;
		/* Absent DATA keeps private heapz zero/nil staging, not an executable wire default. */
		if((load->image.present&(1U<<(Dis64Data-1))) != 0){
			nilvalue = H;
			if(!dis64initializersapply(&load->image.initializers, &load->image.data,
			   owner->data, load->image.module.databytes, (uchar*)&nilvalue))
				return 0;
		}
	}
	return 1;
}


/* Build a storage lease from a view already checked by its owning profile. */
static Dis64StorageOwner*
dis64storagefromview(const Dis64StorageImage *view)
{
	Dis64StorageLoad load;

	memset(&load, 0, sizeof(load));
	load.image = *view;
	if(kprotect(dis64storageconstruct, &load))
		return load.owner;
	free64storage(load.owner);
	kwerrstr("Dis64 storage construction failed");
	return nil;
}


/* Prepare owned Types/data with actual GC rooting, never executable instructions or dependency links.
 * Caller holds ordinary VM exclusion; input remains immutable/disjoint until return.
 * Success requires one free64storage call under that same exclusion. The opaque lease
 * retains no source pointers and cannot be scheduled as a Module. Structural
 * success grants no code/import/Frame64 root semantics or managed file admission. */
Dis64StorageOwner*
prepare64storage(uchar *image, ulong bytes)
{
	Dis64StorageImage view;
	Dis64StorageLimits limits = {64, 65536, Dis64RuntimeObject, 65536,
		Dis64RuntimeCode, 1024, 256, 1024};

	if(!dis64runtimemodel() || image == nil || image == H ||
	   bytes > Dis64RuntimeBytes || (uintptr)image > (uintptr)-1-bytes ||
	   !dis64storageimagecheck(image, bytes, &limits, &view)){
		kwerrstr("unsupported Dis64 storage image or runtime model");
		return nil;
	}
	return dis64storagefromview(&view);
}


typedef struct Dis64StorageClone Dis64StorageClone;

struct Dis64StorageClone {
	Dis64StorageOwner *source; // Borrowed opaque source, independently live throughout the operation.
	Dis64StorageOwner *owner; // Partial destination owned across protected errors.
	Type *datatype; // Actual MP Heap descriptor, independent of its original table ordinal.
};


/* Share retained descriptor identities, then root a fresh nil-initialized MP before recursive C copying. */
static int
dis64storagecopy(void *argument)
{
	Dis64StorageClone *copy;
	Dis64StorageOwner *source, *owner;
	Heap *heap;
	unsigned int i;

	copy = argument;
	source = copy->source;
	copy->owner = owner = mallocz(sizeof(*owner), 1);
	if(owner == nil)
		return 0;
	owner->data = H;
	owner->count = source->count;
	if(owner->count != 0){
		owner->types = mallocz(owner->count*sizeof(Type*), 1);
		if(owner->types == nil)
			return 0;
	}
	for(i=0; i<owner->count; i++){
		owner->types[i] = source->types[i];
		owner->types[i]->ref++;
	}
	if(copy->datatype != nil){
		heap = heapz(copy->datatype);
		owner->data = H2D(uchar*, heap);
		ptradd(heap);
		owner->pinned = 1;
		/* newmp preserves native array-copy semantics and owns partial mapped cleanup. */
		newmp(owner->data, source->data, copy->datatype);
	}
	return 1;
}


/* Clone only a live opaque lease under VM exclusion; success owns independent rooted data.
 * The caller keeps source unchanged and live until return. Original type ordinals
 * and descriptor identities are preserved without assuming the MP descriptor is0.
 * No executable Module, import, cache, frame or JIT capability is constructed. */
Dis64StorageOwner*
clone64storage(Dis64StorageOwner *source)
{
	Dis64StorageClone copy;
	Type *type;
	unsigned int i;
	int found;

	memset(&copy, 0, sizeof(copy));
	if(!dis64runtimemodel() || source == nil || source == H || source->count > 65536 ||
	   (source->count != 0 && (source->types == nil || source->types == H)) || source->data == nil){
		kwerrstr("invalid Dis64 storage source");
		return nil;
	}
	if(source->data != H){
		if(!source->pinned){
			kwerrstr("unrooted Dis64 storage source");
			return nil;
		}
		copy.datatype = D2H(source->data)->t;
		if(copy.datatype == nil || copy.datatype == H){
			kwerrstr("invalid Dis64 storage data type");
			return nil;
		}
	}
	found = copy.datatype == nil;
	/* Official preparations/clones have one distinct Type per ordinal. Reserve
	 * one table retain and the MP Type's additional heapz retain before allocation. */
	for(i=0; i<source->count; i++){
		type = source->types[i];
		if(type == nil || type == H || type->ref <= 0 ||
		   type->ref > 0x7ffffffe - (type == copy.datatype)){
			kwerrstr("unavailable Dis64 storage type retain");
			return nil;
		}
		if(type == copy.datatype)
			found = 1;
	}
	if(!found){
		kwerrstr("foreign Dis64 storage data type");
		return nil;
	}
	copy.source = source;
	if(kprotect(dis64storagecopy, &copy))
		return copy.owner;
	free64storage(copy.owner);
	kwerrstr("Dis64 storage clone failed");
	return nil;
}


/* Retain prepared storage in an empty native Module without installing code or imports.
 * Caller holds VM exclusion and keeps the genuine source lease independently live
 * and unchanged through attachment AND until the Module gains a real cache/root
 * owner. This function does not consume that lease or grant file execution policy.
 * Target/source are disjoint mapped native storage. Failure preserves both owners. */
int
attach64storage(Module *target, Dis64StorageOwner *source)
{
	Module *cached;
	Type **types, *datatype;
	Heap *data;
	unsigned int i, ordinal;
	int found;

	if(!dis64runtimemodel() || target == nil || target == H || source == nil || source == H ||
	   target->ref != 1 || target->rt != DONTCOMPILE || target->type != nil || target->ntype != 0 ||
	   target->origmp != H || target->entryt != nil || target->entry != nil || target->prog != nil ||
	   target->nprog != 0 || target->compiled || target->jit != nil || target->cstep != nil ||
	   target->ext != nil || target->ldt != nil || target->htab != nil || target->pctab != nil || target->link != nil ||
	   source->count > 65536 || (source->count != 0 && (source->types == nil || source->types == H)) ||
	   source->data == nil){
		kwerrstr("invalid Dis64 storage attachment");
		return 0;
	}
	for(cached=modules; cached != nil; cached=cached->link)
		if(cached == target){
			kwerrstr("Dis64 storage target is already published");
			return 0;
		}
	data = nil;
	datatype = nil;
	if(source->data != H){
		data = D2H(source->data);
		if(!source->pinned || data->ref == 0 || data->ref == (ulong)~0 || data->t == nil || data->t == H){
			kwerrstr("unavailable Dis64 module data retain");
			return 0;
		}
		datatype = data->t;
	}
	ordinal = 0;
	found = datatype == nil;
	for(i=0; i<source->count; i++){
		if(source->types[i] == nil || source->types[i] == H ||
		   source->types[i]->ref <= 0 || source->types[i]->ref == 0x7fffffff){
			kwerrstr("unavailable Dis64 attachment type retain");
			return 0;
		}
		if(source->types[i] == datatype){
			ordinal = i;
			found = 1;
		}
	}
	if(!found){
		kwerrstr("foreign Dis64 attachment data type");
		return 0;
	}
	types = nil;
	if(source->count != 0){
		types = mallocz(source->count*sizeof(Type*), 1);
		if(types == nil){
			kwerrstr("Dis64 attachment table allocation failed");
			return 0;
		}
	}
	/* All refusal points precede ownership acquisition/publication. No VM poll or
	 * callback occurs between these checked retains and the final native commit. */
	for(i=0; i<source->count; i++){
		types[i] = source->types[i];
		types[i]->ref++;
	}
	if(data != nil){
		data->ref++;
		Setmark(data);
	}
	target->type = types;
	target->ntype = source->count;
	target->mptype = ordinal;
	target->origmp = source->data;
	return 1;
}


typedef struct Dis64NumericLoad Dis64NumericLoad;

struct Dis64NumericLoad {
	Dis64StorageImage image; // Complete immutable storage identity through construction.
	DisRuneView name; // Checked ASCII module label, copied before return.
	char *path; // Borrowed caller label, copied before return.
	Dis64StorageOwner *lease; // Separate construction root, returned on complete success.
	Module *module; // Partial unpublished native owner retired through real unload.
	int jit; // Explicit existing native leaf compilation request, never a file-mode choice.
	int indirect; // The selected proof admits indirect destination operands.
	int (*step)(void); // Trusted result binding selected by the owning constructor.
	const Link *clockload[3]; // Checked exports for an executing Sys LOAD.
	unsigned int clockimports;
};


#ifdef DIS_NATIVE64_BOOT_SYS_IO


/* Materialize one checked Sys import group in the existing ILOAD layout. */
static int
dis64sysloadmetadata(Module *module, const Link *const *entries, unsigned int count)
{
	Import *imports;
	String *path;
	unsigned int i;

	if(count == 0 || count > 32)
		return 0;
	module->ldt = mallocz(2*sizeof(Import*), 1);
	if(module->ldt == nil)
		return 0;
	module->ldt[0] = imports = mallocz((count+1)*sizeof(Import), 1);
	if(imports == nil)
		return 0;
	for(i=0; i<count; i++){
		imports[i].name = strdup(entries[i]->name);
		if(imports[i].name == nil)
			return 0;
		imports[i].sig = entries[i]->sig;
	}
	path = c2string("$Sys", 4);
	if(path == nil || path == H)
		return 0;
	memmove(module->origmp, &path, sizeof(path));
	Setmark(D2H(path));
	module->rt |= HASLDT;
	return 1;
}
#endif


/* Build an owned program/entry only after complete private role/map/native lowering proof. */
static int
dis64numericconstruct(void *argument)
{
	Dis64NumericLoad *load;
	Dis64CallDecoded decoded;
	Inst *instruction;
	Module *module;
	unsigned int i, at, used, rune;

	load = argument;
	load->module = module = mallocz(sizeof(*module), 1);
	if(module == nil)
		return 0;
	module->ref = 1;
	module->rt = DONTCOMPILE;
	module->origmp = H;
	load->lease = dis64storagefromview(&load->image);
	if(load->lease == nil || !attach64storage(module, load->lease))
		return 0;
	#ifdef DIS_NATIVE64_BOOT_SYS_IO
	if(load->clockimports != 0 && !dis64sysloadmetadata(module, load->clockload, load->clockimports))
		return 0;
#endif
	if(load->image.module.profile == 1)
		module->entryt = module->type[load->image.module.frametype];
	module->nprog = load->image.code.count;
	module->prog = mallocz(module->nprog*sizeof(Inst), 1);
	if(module->prog == nil)
		return 0;
	at = 4;
	for(i=0; i<load->image.code.count; i++){
		if(!dis64callread(load->image.code.data+at, load->image.code.bytes-at, &decoded, &used))
			return 0;
		instruction = module->prog+i;
		dis64runtimeinst(instruction, &decoded.instruction, module->prog);
		if(load->indirect && UDST(decoded.instruction.mode) == (AFP|AIND)){
			instruction->d.i.f = decoded.instruction.destination;
			instruction->d.i.s = decoded.secondary;
		}
		at += used;
	}
	if(load->image.module.profile == 1)
		module->entry = module->prog+load->image.module.entry;
	module->name = mallocz(load->name.count+1, 1);
	if(module->name == nil)
		return 0;
	for(i=0; i<load->name.count; i++){
		if(!disruneat(&load->name, i, &rune))
			return 0;
		module->name[i] = rune;
	}
	module->path = strdup(load->path);
	if(module->path == nil)
		return 0;
	module->cstep = load->step;
	if(load->jit && !compile(module, module->nprog, nil))
		return 0;
	return 1;
}


static const Dis64StorageLimits dis64numericlimits = {
	64, 65536, Dis64RuntimeObject, 65536, Dis64RuntimeCode, 1024, 256, 1024
};


#ifdef DIS_NATIVE64_BOOT_SYS_IO

#define Dis64ClockInstructions (5U*Dis64ClockCalls+2U+Dis64ClockWords) // LOAD, argument/call/copy groups, and exit.


/* Resolve one exact scalar import from an already validated image. */
static const Link*
dis64sysclockview(const Dis64StorageImage *consumer, unsigned int ordinal, Module **owner)
{
	static const uchar sys[] = {0,0,0,'$', 0,0,0,'S', 0,0,0,'y', 0,0,0,'s'};
	static const uchar millisec[] = {
		0,0,0,'m', 0,0,0,'i', 0,0,0,'l', 0,0,0,'l',
		0,0,0,'i', 0,0,0,'s', 0,0,0,'e', 0,0,0,'c',
	};
	Dis64Binding binding;
	DisRuneView module, symbol;

	if(consumer->module.profile != 1 ||
	   !dis64bindingget(&consumer->imports, ordinal, &binding) ||
	   binding.signature.result != Dis64Int || binding.signature.parameters != 0 ||
	   !disstringpoolget(&consumer->strings, binding.module, &module) ||
	   module.count != sizeof(sys)/4 || memcmp(module.values, sys, sizeof(sys)) != 0 ||
	   !disstringpoolget(&consumer->strings, binding.symbol, &symbol) ||
	   symbol.count != sizeof(millisec)/4 || memcmp(symbol.values, millisec, sizeof(millisec)) != 0)
		return nil;
	return native64sysclock(owner);
}

/* Resolve the fixed integer-argument entries against their structural signatures. */
static const Link*
dis64sysintegerview(const Dis64StorageImage *consumer, unsigned int ordinal,
	unsigned int *parameters, Module **owner)
{
	static const uchar sys[] = {0,0,0,'$', 0,0,0,'S', 0,0,0,'y', 0,0,0,'s'};
	Dis64Binding binding;
	DisRuneView module, symbol;
	char name[6];
	unsigned int i, rune, count;

	if(!dis64bindingget(&consumer->imports, ordinal, &binding) ||
	   binding.signature.result != Dis64Int ||
	   !disstringpoolget(&consumer->strings, binding.module, &module) ||
	   module.count != sizeof(sys)/4 || memcmp(module.values, sys, sizeof(sys)) != 0 ||
	   !disstringpoolget(&consumer->strings, binding.symbol, &symbol) ||
	   symbol.count == 0 || symbol.count >= sizeof(name))
		return nil;
	for(i=0; i<symbol.count; i++){
		if(!disruneat(&symbol, i, &rune) || rune == 0 || rune > 127)
			return nil;
		name[i] = rune;
	}
	name[symbol.count] = 0;
	if(strcmp(name, "sleep") == 0)
		count = 1;
	else if(strcmp(name, "dup") == 0)
		count = 2;
	else
		return nil;
	if(binding.signature.parameters != count)
		return nil;
	for(i=0; i<count; i++)
		if(dis64u32(binding.signature.data+12+4*i) != Dis64Int)
			return nil;
	*parameters = count;
	return native64sysexport(name, owner);
}


/* Resolve a direct-mode import without taking a module reference. */
const Link*
dis64sysclockimport(const uchar *image, ulong bytes, unsigned int ordinal, Module **owner)
{
	Dis64StorageImage view;

	if(owner == nil || image == nil || image == H || bytes > Dis64RuntimeBytes ||
	   (uintptr)image > (uintptr)-1-bytes ||
	   !dis64storageimagecheck(image, bytes, &dis64numericlimits, &view))
		return nil;
	return dis64sysclockview(&view, ordinal, owner);
}


/* A scalar value is available only after an actual defining step in the linear program. */
static int
dis64clockready(long offset, unsigned int calls, int initialized)
{
	int local;

	local = dis64clocklocal(offset);
	return (local >= 0 && (initialized & (1U<<local))) || (offset == 72 && calls != 0);
}


/* Reuse scalar role validation, then restrict reads and writes to the proven int cells. */
static int
dis64clockwordcheck(const Dis64Instruction *inst, unsigned int count,
	unsigned int calls, int *initialized)
{
	unsigned int sm, mm;

	if(!dis64clockwordop(inst->op) || !dis64instructioncheck(inst, count, 0, 80) ||
	   UDST(inst->mode) != AFP || (dis64clocklocal(inst->destination) < 0 && inst->destination != 72) ||
	   (inst->destination == 72 && calls == 0))
		return 0;
	sm = USRC(inst->mode);
	if(sm == AFP){
		if(!dis64clockready(inst->source, calls, *initialized))
			return 0;
	}else if(sm != AIMM)
		return 0;
	if(inst->op != IMOVW){
		mm = inst->mode&ARM;
		if(mm == AXNON){
			if(!dis64clockready(inst->destination, calls, *initialized))
				return 0;
		}else if(mm == AXINF){
			if(!dis64clockready(inst->middle, calls, *initialized))
				return 0;
		}else if(mm != AXIMM || inst->middle < -32768 || inst->middle > 32767)
			return 0;
	}
	if(dis64clocklocal(inst->destination) >= 0)
		*initialized |= 1U<<dis64clocklocal(inst->destination);
	return 1;
}


/* Decode bounded clock calls and an optional WORD copy after each completed call. */
static int
dis64syscode(const uchar *data, unsigned int bytes, unsigned int limit, Dis64CodeView *code,
	unsigned int bindings, int arguments)
{
	Dis64CallDecoded decoded;
	Dis64Instruction *inst;
	unsigned int at, i, used, count, phase, calls, slot, binding, parameter, expected, words;
	int initialized;

	if(bytes < 4)
		return 0;
	count = dis64u32(data);
	if(count < 4 || count > limit || count > Dis64ClockInstructions)
		return 0;
	at = 4;
	phase = calls = initialized = slot = binding = parameter = expected = words = 0;
	for(i=0; i<count; i++){
		if(!dis64callread(data+at, bytes-at, &decoded, &used))
			return 0;
		at += used;
		inst = &decoded.instruction;
		switch(inst->op){
		case ILOAD:
			if(i != 0 || inst->mode != (SRC(AMP)|AXIMM|DST(AMP)) ||
			   inst->source != 0 || inst->middle != 0 || inst->destination != 8)
				return 0;
			slot = 8;
			break;
		case IMFRAME:
			if(phase != 0 || inst->mode != (SRC(AMP)|AXIMM|DST(AFP)) ||
			   inst->source != slot || inst->destination != 40 ||
			   (inst->middle < 0 || inst->middle >= bindings ||
			    (inst->middle != 0 && slot != 8)))
				return 0;
			binding = inst->middle;
			expected = arguments < 0 ? 2 : (arguments >> (2*binding)) & 3;
			parameter = 0;
			phase = 1;
			break;
		case ILEA:
			if(phase != 1 || (arguments >= 0 && parameter != expected) ||
			   inst->mode != (SRC(AFP)|DST(AFP|AIND)) ||
			   inst->source != 72 || inst->destination != 40 || decoded.secondary != 32)
				return 0;
			phase = 2;
			break;
		case IMCALL:
			if(phase != 2 || inst->mode != (SRC(AFP)|AXIMM|DST(AMP)) ||
			   inst->source != 40 || inst->middle != binding || inst->destination != slot)
				return 0;
			phase = 0;
			if(++calls > Dis64ClockCalls)
				return 0;
			break;
		case IEXIT:
			if(phase != 0 || calls == 0 || i+1 != count ||
			   inst->mode != (SRC(AXXX)|DST(AXXX)) || at != bytes)
				return 0;
			code->data = data;
			code->bytes = bytes;
			code->count = count;
			return 1;
		case IMOVW:
			if(phase == 1){
				if(parameter >= expected || inst->destination != 40 || decoded.secondary != 64+4*parameter)
					return 0;
				if(inst->mode == (SRC(AFP)|DST(AFP|AIND))){
					if(!dis64clockready(inst->source, calls, initialized))
						return 0;
				}else if(inst->mode != (SRC(AIMM)|DST(AFP|AIND)))
					return 0;
				parameter++;
				break;
			}
			/* Fall through to the shared scalar dataflow checks. */
		default:
			if(phase != 0 || words == Dis64ClockWords ||
			   !dis64clockwordcheck(inst, count, calls, &initialized))
				return 0;
			words++;
			break;
		}
	}
	return 0;
}

static int
dis64clockcode(const uchar *data, unsigned int bytes, unsigned int limit, Dis64CodeView *code)
{
	return dis64syscode(data, bytes, limit, code, 3, -1);
}

#endif


#define Dis64AllBindings ((unsigned int)-1) // Resolve every consumer import against one provider.
#define Dis64PairBindings 32U // Maximum import ordinals in the paired console profile.

typedef struct Dis64PairPlan Dis64PairPlan;

struct Dis64PairPlan {
	Dis64ImportMatch matches[Dis64PairBindings]; // Checked provider coordinates by consumer ordinal.
	unsigned int count; // Zero until the complete provider match succeeds.
};


/* Check a void, zero-argument import against the selected provider. */
static int
dis64providerentry(const Dis64StorageImage *consumer, unsigned int ordinal,
	const Dis64StorageImage *provider, Dis64ImportMatch *match)
{
	Dis64Binding imported;
	Dis64Type frame;

	return dis64bindingget(&consumer->imports, ordinal, &imported) &&
		imported.signature.result == Dis64Void && imported.signature.parameters == 0 &&
		dis64importmatch(consumer, ordinal, provider, match) &&
		dis64numericframe(provider, match->frame, &frame);
}


/* Resolve every ordinal against the same validated numeric provider. */
static int
dis64providerplan(const Dis64StorageImage *consumer, unsigned int ordinal,
	const Dis64StorageImage *provider, Dis64CodeView *code, Dis64PairPlan *plan)
{
	unsigned int i;

	if(!(provider->module.profile == 0 ? dis64numericlibrarycheck(provider, Dis64RuntimeCode, code) :
	     dis64numericentrycheck(provider, Dis64RuntimeCode, 1, code)))
		return 0;
	if(ordinal != Dis64AllBindings){
		if(!dis64providerentry(consumer, ordinal, provider, &plan->matches[0]))
			return 0;
		plan->count = 1;
		return 1;
	}
	if(consumer->imports.count == 0 || consumer->imports.count > Dis64PairBindings)
		return 0;
	for(i=0; i<consumer->imports.count; i++)
		if(!dis64providerentry(consumer, i, provider, &plan->matches[i]))
			return 0;
	plan->count = consumer->imports.count;
	return 1;
}


/* Prove consecutive calls through checked imports and unmapped frame scratch. */
static int
dis64consumerentry(const Dis64StorageImage *image, Dis64CodeView *code, int siblings)
{
	Dis64Instruction frame, call, exit;
	Dis64Type type;
	unsigned int at, used, i, j, slots;
	Dis64Binding binding, previous;
	int order;

	*code = image->code;
	slots = siblings ? image->imports.count : 1;
	if(image->module.profile != 1 || image->module.entry != 0 ||
	   image->imports.count == 0 || image->imports.count > Dis64PairBindings ||
	   image->exports.count != 0 ||
	   image->data.bytes != 8*slots || image->data.mapbytes != (slots+7)/8 ||
	   image->initializers.count != 0 || code->count < 3 || (code->count & 1) == 0 ||
	   !dis64typeget(&image->types, image->module.frametype, &type) ||
	   type.bytes < 72 || type.align != 8 || type.map[0] != 0)
		return 0;
	for(i=0; i<slots; i++)
		if((image->data.map[i/8] & (0x80U>>(i%8))) == 0)
			return 0;
	if(siblings){
		if(slots < 2)
			return 0;
		for(i=0; i<slots; i++){
			if(!dis64bindingget(&image->imports, i, &binding))
				return 0;
			for(j=0; j<i; j++)
				if(!dis64bindingget(&image->imports, j, &previous) ||
				   !disstringpoolcompare(&image->strings, binding.module, previous.module, &order) ||
				   order == 0)
					return 0;
		}
	}
	at = 4;
	for(i=1; i<code->count; i+=2){
		if(!dis64instructionread(code->data+at, code->bytes-at, &frame, &used) ||
		   frame.op != IMFRAME || frame.mode != ((AMP<<3)|AXIMM|AFP) ||
		   (siblings ? (frame.source < 0 || frame.source%8 != 0 ||
		    frame.source/8 >= slots || frame.middle != 0) :
		    (frame.source != 0 || frame.middle < 0 || frame.middle >= image->imports.count)) ||
		   frame.destination < 64 ||
		   !dis64initspan(&type, frame.destination, 8))
			return 0;
		at += used;
		if(!dis64instructionread(code->data+at, code->bytes-at, &call, &used) ||
		   call.op != IMCALL || call.mode != ((AFP<<3)|AXIMM|AMP) ||
		   call.source != frame.destination || call.middle != frame.middle || call.destination != frame.source)
			return 0;
		at += used;
	}
	return dis64instructionread(code->data+at, code->bytes-at, &exit, &used) &&
		at+used == code->bytes && exit.op == IEXIT && exit.mode == ((AXXX<<3)|AXXX);
}


#if defined(DIS_NATIVE64_SCALAR_BOOT) && defined(DIS64_PILOT_LOADER)


/* Copy a safe namespace basename only after checking every Rune. */
static int
dis64dependencyname(const Dis64StorageImage *consumer, unsigned int ordinal,
	char *name, unsigned int capacity)
{
	Dis64Binding binding;
	DisRuneView dependency;
	unsigned int i, rune;

	if(name == nil || name == H || !dis64bindingget(&consumer->imports, ordinal, &binding) ||
	   !disstringpoolget(&consumer->strings, binding.module, &dependency) ||
	   dependency.count == 0 || dependency.count > Dis64RuntimeName || dependency.count >= capacity)
		return 0;
	for(i=0; i<dependency.count; i++){
		if(!disruneat(&dependency, i, &rune) ||
		   !((rune >= 'a' && rune <= 'z') || (rune >= 'A' && rune <= 'Z') ||
		     (rune >= '0' && rune <= '9') || rune == '_' || rune == '-'))
			return 0;
		name[i] = rune;
	}
	name[dependency.count] = 0;
	return 1;
}


/* Extract the shared safe namespace basename from the checked consumer imports. */
int
dis64pairdependency(const uchar *image, ulong bytes, char *name, unsigned int capacity)
{
	Dis64StorageImage consumer;
	Dis64CodeView code;
	Dis64Binding binding, next;
	unsigned int i;
	int order;

	if(image == nil || image == H || bytes > Dis64RuntimeBytes ||
	   (uintptr)image > (uintptr)-1-bytes ||
	   !dis64storageimagecheck(image, bytes, &dis64numericlimits, &consumer) ||
	   !dis64consumerentry(&consumer, &code, 0) ||
	   !dis64bindingget(&consumer.imports, 0, &binding))
		return 0;
	for(i=1; i<consumer.imports.count; i++)
		if(!dis64bindingget(&consumer.imports, i, &next) ||
		   !disstringpoolcompare(&consumer.strings, binding.module, next.module, &order) ||
		   order != 0)
			return 0;
	return dis64dependencyname(&consumer, 0, name, capacity);
}


/* Return the total count only for a checked per-import sibling layout. */
int
dis64siblingdependency(const uchar *image, ulong bytes, unsigned int ordinal,
	char *name, unsigned int capacity)
{
	Dis64StorageImage consumer;
	Dis64CodeView code;

	if(image == nil || image == H || bytes > Dis64RuntimeBytes ||
	   (uintptr)image > (uintptr)-1-bytes ||
	   !dis64storageimagecheck(image, bytes, &dis64numericlimits, &consumer) ||
	   !dis64consumerentry(&consumer, &code, 1) ||
	   !dis64dependencyname(&consumer, ordinal, name, capacity))
		return 0;
	return consumer.imports.count;
}
#endif


/* Construct an owned numeric entry and keep its MP rooted through handoff. */
static Module*
dis64numericbuild(char *path, uchar *image, ulong bytes, int jit, Dis64StorageOwner **roots,
	const Dis64StorageImage *consumer, unsigned int ordinal, int linked, Dis64PairPlan *plan)
{
	Dis64NumericLoad load;
	Dis64CodeView code;
	Dis64Instruction instruction;
	unsigned int i, at, used, rune;

	memset(&load, 0, sizeof(load));
	if(!dis64runtimemodel() || path == nil || path == H || image == nil || image == H ||
	   roots == nil || roots == H || (uintptr)roots%sizeof(*roots) != 0 ||
	   (uintptr)roots > (uintptr)-1-sizeof(*roots) || *roots != nil || bytes > Dis64RuntimeBytes ||
	   (uintptr)image > (uintptr)-1-bytes || (jit != 0 && jit != 1) ||
	   !dis64storageimagecheck(image, bytes, &dis64numericlimits, &load.image) ||
	   !(linked ? dis64consumerentry(&load.image, &code, linked == 2) :
	     consumer == nil ? dis64numericstoragecheck(&load.image, Dis64RuntimeCode, &code) :
	     dis64providerplan(consumer, ordinal, &load.image, &code, plan)) ||
	   !disstringpoolget(&load.image.strings, load.image.module.name, &load.name) ||
	   load.name.count == 0 || load.name.count > Dis64RuntimeName){
		kwerrstr("unsupported Dis64 private numeric Module");
		return nil;
	}
	for(i=0; i<load.name.count; i++)
		if(!disruneat(&load.name, i, &rune) || rune == 0 || rune > 127){
			kwerrstr("unsupported Dis64 numeric display name");
			return nil;
		}
	at = 4;
	for(i=0; i<code.count; i++){
		if(!dis64instructionread(code.data+at, code.bytes-at, &instruction, &used) ||
		   !dis64runtimeinstruction(&instruction)){
			kwerrstr("unsupported Dis64 numeric lowering");
			return nil;
		}
		at += used;
	}
	load.path = path;
	load.jit = jit;
	if(kprotect(dis64numericconstruct, &load)){
		*roots = load.lease;
		return load.module;
	}
	if(load.module != nil)
		unload(load.module);
	free64storage(load.lease);
	kwerrstr("Dis64 numeric Module construction failed");
	return nil;
}



/* Build the standalone MP-only entry. Keep the lease until MP is rooted and
 * initialize the entry frame with initmem before publication. */
Module*
prepare64numeric(char *path, uchar *image, ulong bytes, int jit, Dis64StorageOwner **roots)
{
	return dis64numericbuild(path, image, bytes, jit, roots, nil, 0, 0, nil);
}


/* Build an explicitly selected provider and keep its export coordinates separate. */
static Module*
prepare64provider(char *path, uchar *image, ulong bytes, int jit, Dis64StorageOwner **roots,
	const uchar *consumer, ulong consumerbytes, unsigned int ordinal, Dis64PairPlan *plan)
{
	Dis64StorageImage view;

	if(!dis64runtimemodel() || consumer == nil || consumer == H ||
	   consumerbytes > Dis64RuntimeBytes || (uintptr)consumer > (uintptr)-1-consumerbytes ||
	   !dis64storageimagecheck(consumer, consumerbytes, &dis64numericlimits, &view)){
		kwerrstr("unsupported Dis64 consumer image");
		return nil;
	}
	return dis64numericbuild(path, image, bytes, jit, roots, &view, ordinal, 0, plan);
}


typedef struct Dis64ProviderLink Dis64ProviderLink;

struct Dis64ProviderLink {
	Module *module; // Owned until mklinkmod returns.
	Modlink *link; // Owned partial instance until external root acquisition.
	int bindings; // Number of runtime links to allocate.
	const Dis64PairPlan *plan; // Borrowed checked provider coordinates for paired startup.
	const Link *builtin; // Borrowed trusted C export while the owned Module stays live.
};


/* Materialize matched bindings and root the link before releasing the MP lease. */
static int
dis64providerlink(void *argument)
{
	Dis64ProviderLink *build;
	Module *module;
	Modlink *link;
	int i;

	build = argument;
	module = build->module;
	build->link = link = mklinkmod(module, build->bindings);
	build->module = nil;
	if(build->builtin != nil){
		link->links[0].u.runt = build->builtin->u.runt;
		link->links[0].frame = build->builtin->frame;
	}else if(build->plan != nil){
		for(i=0; i<build->bindings; i++){
			link->links[i].u.pc = module->prog+build->plan->matches[i].entry;
			link->links[i].frame = module->type[build->plan->matches[i].frame];
		}
	}
	if(module->origmp != H){
		D2H(module->origmp)->ref++;
		link->MP = module->origmp;
	}
	ptradd(D2H(link));
	return 1;
}


/* Consume the module and its lease, returning one rooted instance on success. */
static Modlink*
dis64rootlink(Module *module, Dis64StorageOwner *lease, int bindings,
	const Dis64PairPlan *plan, const Link *builtin)
{
	Dis64ProviderLink build;
	int ok;

	build.module = module;
	build.link = nil;
	build.bindings = bindings;
	build.plan = plan;
	build.builtin = builtin;
	ok = kprotect(dis64providerlink, &build);
	if(!ok){
		if(build.link != nil)
			destroy(build.link);
		else
			unload(build.module);
	}
	free64storage(lease);
	return ok ? build.link : nil;
}

#if defined(DIS_NATIVE64_SCALAR_BOOT) && defined(DIS64_PILOT_LOADER)
/* Construct only storage already checked by the explicit managed trial. */
static Modlink*
dis64managedowner(const Dis64StorageImage *image, char *path, int jit, const Dis64PairPlan *plan)
{
	Dis64NumericLoad load = {0};
	unsigned int i, rune;

	load.image = *image;
	load.path = path;
	load.jit = jit;
	load.indirect = 1;
	load.step = plan == nil ? native64managedlea8step : nil;
	if(!disstringpoolget(&image->strings, image->module.name, &load.name) ||
	   load.name.count == 0 || load.name.count > Dis64RuntimeName)
		return nil;
	for(i=0; i<load.name.count; i++)
		if(!disruneat(&load.name, i, &rune) || rune == 0 || rune > 127)
			return nil;
	if(kprotect(dis64numericconstruct, &load))
		return dis64rootlink(load.module, load.lease, plan == nil ? 0 : plan->count, plan, nil);
	if(load.module != nil)
		unload(load.module);
	free64storage(load.lease);
	return nil;
}

#ifdef DIS_NATIVE64_MANAGED_FILES
/* Generated from String.m by Limbo -H: legacy hashes are consistency checks only. */
static unsigned int
dis64managedfileview(const uchar *image, ulong bytes, Dis64StorageImage *view, Dis64Binding *selected, const LimboNative64Entry **entries)
{
	static const Dis64ManagedSchema schema = {Dis64TrialVersion, Dis64TrialString, Dis64TrialList};
	typedef char GeneratedStringShape[
		LIMBO_NATIVE64_ARGUMENT_String_unquoted == LimboNative64String &&
		LIMBO_NATIVE64_RESULT_String_unquoted == LimboNative64ListString &&
		LIMBO_NATIVE64_ARGUMENT_String_quoted == LimboNative64ListString &&
		LIMBO_NATIVE64_RESULT_String_quoted == LimboNative64String &&
		F_String_unquoted_size == 72 && F_String_quoted_size == 72 ? 1 : -1];
	static const uchar maps[2][2] = {F_String_unquoted_map, F_String_quoted_map};
	static const LimboNative64Entry specs[] = {
		{"unquoted", nil, LIMBO_NATIVE64_SIGNATURE_String_unquoted, F_String_unquoted_size, sizeof(maps[0]), maps[0]},
		{"quoted", nil, LIMBO_NATIVE64_SIGNATURE_String_quoted, F_String_quoted_size, sizeof(maps[1]), maps[1]},
	};
	static const LimboNative64Managed shapes[] = {
		{&specs[0], LimboNative64String, LimboNative64ListString},
		{&specs[1], LimboNative64ListString, LimboNative64String},
	};
	unsigned int i, ids[2];

	USED(sizeof(GeneratedStringShape));

	if(image == nil || image == H || bytes > Dis64RuntimeBytes || (uintptr)image > (uintptr)-1-bytes ||
	   !dis64storageimagecheckrecords(image, bytes, &dis64numericlimits, view,
	   dis64managedprovidercode, dis64managedtable, &schema) ||
	   !dis64managedexports(view, &schema, shapes, nelem(shapes), selected, ids))
		return 0;
	for(i=0; i<view->exports.count; i++)
		entries[i] = &specs[ids[i]];
	return view->exports.count;
}

typedef struct Dis64ManagedFileLoad Dis64ManagedFileLoad;
struct Dis64ManagedFileLoad {
	Dis64NumericLoad load;
	Dis64Binding selected[2];
	const LimboNative64Entry *entries[2];
	unsigned int count;
};

static int
dis64managedfileconstruct(void *argument)
{
	Dis64ManagedFileLoad *file;
	Module *module;
	unsigned int i;

	file = argument;
	if(!dis64numericconstruct(&file->load))
		return 0;
	module = file->load.module;
	module->ext = mallocz((file->count+1)*sizeof(Link), 1);
	if(module->ext == nil)
		return 0;
	for(i=0; i<file->count; i++)
		mlink(module, module->ext+i, (uchar*)file->entries[i]->name, file->entries[i]->legacy_signature,
			file->selected[i].entry, module->type[file->selected[i].frame]);
	module->rt |= Dis64ManagedFile;
	return 1;
}

/* The ordinary parser owns signing, namespace metadata and cache publication. */
Module*
prepare64managedfile(char *path, uchar *image, ulong bytes, int jit)
{
	Dis64ManagedFileLoad file = {0};
	Module *module;

	if(!dis64bootmanaged || !dis64runtimemodel() || path == nil || path == H ||
	   dis64bootprovider == nil || strcmp(path, dis64bootprovider) != 0 || (jit != 0 && jit != 1))
		return nil;
	file.count = dis64managedfileview(image, bytes, &file.load.image, file.selected, file.entries);
	if(file.count == 0 || !disstringpoolget(&file.load.image.strings, file.load.image.module.name, &file.load.name))
		return nil;
	file.load.path = path;
	file.load.jit = jit;
	file.load.indirect = 1;
	module = nil;
	if(kprotect(dis64managedfileconstruct, &file))
		module = file.load.module;
	else if(file.load.module != nil)
		unload(file.load.module);
	free64storage(file.load.lease);
	return module;
}

typedef struct Dis64ManagedFileCaller Dis64ManagedFileCaller;
struct Dis64ManagedFileCaller {
	Modlink *caller;
	const LimboNative64Entry *entries[2];
	unsigned int count;
	char *path;
};

static int
dis64managedfilemetadata(void *argument)
{
	Dis64ManagedFileCaller *file;
	Module *module;
	Import *imports;
	String *path;
	unsigned int i;

	file = argument;
	module = file->caller->m;
	module->ldt = mallocz(2*sizeof(Import*), 1);
	if(module->ldt == nil)
		return 0;
	module->ldt[0] = imports = mallocz((file->count+1)*sizeof(Import), 1);
	if(imports == nil)
		return 0;
	for(i=0; i<file->count; i++){
		imports[i].name = strdup(file->entries[i]->name);
		if(imports[i].name == nil)
			return 0;
		imports[i].sig = file->entries[i]->legacy_signature;
	}
	path = c2string(file->path, strlen(file->path));
	if(path == nil || path == H)
		return 0;
	memmove(file->caller->MP+16, &path, sizeof(path));
	Setmark(D2H(path));
	module->rt |= HASLDT|Dis64ManagedFile;
	return 1;
}
#endif

/* Only the explicit console option can select these unassigned trial values. */
Modlink*
prepare64managedtrial(char *path, const uchar *image, ulong bytes, char *peerpath,
	const uchar *peer, ulong peerbytes, int jit)
{
	static const Dis64ManagedSchema schema = {Dis64TrialVersion, Dis64TrialString, Dis64TrialList};
	static const uchar map[] = {0, 0x80};
	static const LimboNative64Entry entries[] = {
		{"unquoted", nil, 0, 72, sizeof(map), map},
		{"quoted", nil, 0, 72, sizeof(map), map},
	};
	static const LimboNative64Managed shapes[] = {
		{&entries[0], LimboNative64String, LimboNative64ListString},
		{&entries[1], LimboNative64ListString, LimboNative64String},
	};
	Dis64ManagedPlan checked;
	Dis64PairPlan plan = {0};
	Modlink *caller, *provider;
	unsigned int i;

	if(heapmonitor != nil || !dis64runtimemodel() || path == nil || path == H ||
	   peerpath == nil || peerpath == H || image == nil || image == H || peer == nil || peer == H ||
	   bytes > Dis64RuntimeBytes || peerbytes > Dis64RuntimeBytes ||
	   (uintptr)image > (uintptr)-1-bytes || (uintptr)peer > (uintptr)-1-peerbytes ||
	   (jit != 0 && jit != 1))
		return nil;
	if(!dis64managedimagegroup(image, bytes, peer, peerbytes, &dis64numericlimits,
	   &schema, shapes, nelem(shapes), &checked)){
		kwerrstr("unsupported Dis64 managed trial pair");
		return nil;
	}
#ifdef DIS_NATIVE64_MANAGED_FILES
	if(dis64managedfilecode(&checked.consumer.code)){
		Dis64ManagedFileCaller file = {0};
		Dis64StorageImage view;
		Dis64Binding selected[2];
		const LimboNative64Entry *entries[2];
		DisRuneView encoded;
		unsigned int rune, exports, j;

		exports = dis64managedfileview(peer, peerbytes, &view, selected, entries);
		if(exports == 0 || !disstringpoolget(&checked.consumer.strings, 3, &encoded) ||
		   encoded.count == 0 || encoded.count > Dis64RuntimeName || peerpath[0] != '/' ||
		   strlen(peerpath) != encoded.count)
			return nil;
		for(i=0; i<encoded.count; i++)
			if(!disruneat(&encoded, i, &rune) || rune == 0 || rune > 127 || rune != (uchar)peerpath[i])
				return nil;
		file.count = checked.count;
		for(i=0; i<file.count; i++){
			for(j=0; j<exports; j++)
				if(selected[j].entry == checked.matches[i].entry && selected[j].frame == checked.matches[i].frame)
					break;
			if(j == exports)
				return nil;
			file.entries[i] = entries[j];
		}
		file.path = peerpath;
		file.caller = caller = dis64managedowner(&checked.consumer, path, 0, nil);
		if(caller == nil)
			return nil;
		if(kprotect(dis64managedfilemetadata, &file))
			return caller;
		ptrdel(D2H(caller));
		destroy(caller);
		return nil;
	}
#else
	if(dis64managedfilecode(&checked.consumer.code))
		return nil;
#endif
	caller = dis64managedowner(&checked.consumer, path, 0, nil);
	if(caller == nil)
		return nil;
	plan.count = checked.count;
	for(i=0; i<plan.count; i++){
		plan.matches[i].entry = checked.matches[i].entry;
		plan.matches[i].frame = checked.matches[i].frame;
	}
	provider = dis64managedowner(&checked.provider, peerpath, jit, &plan);
	if(provider == nil){
		ptrdel(D2H(caller));
		destroy(caller);
		return nil;
	}
	memmove(caller->MP, &provider, sizeof(provider));
	Setmark(D2H(provider));
	ptrdel(D2H(provider));
	return caller;
}
#endif


#ifdef DIS_NATIVE64_BOOT_SYS_IO


/* Retain a checked builtin while its new instance acquires the collector root. */
static Modlink*
dis64rootclock(Module *module, const Link *entry)
{
	if(entry == nil || module->ref <= 0 || module->ref == INT_MAX){
		kwerrstr("unavailable Dis64 Sys clock import");
		return nil;
	}
	module->ref++;
	return dis64rootlink(module, nil, 1, nil, entry);
}


/* Build a rooted one-slot Sys clock instance under VM exclusion. */
Modlink*
prepare64sysclocklink(const uchar *image, ulong bytes, unsigned int ordinal)
{
	const Link *entry;
	Module *module;

	if(heapmonitor != nil){
		kwerrstr("Dis64 Sys clock link requires unmonitored allocation");
		return nil;
	}
	module = nil;
	entry = dis64sysclockimport(image, bytes, ordinal, &module);
	return dis64rootclock(module, entry);
}


/* Own the explicitly selected Sys clock consumer and link before scheduling. */
Modlink*
prepare64sysclockconsumer(char *path, const uchar *image, ulong bytes, int jit)
{
	Dis64NumericLoad load;
	Dis64Type frame;
	Module *sys;
	const Link *entry;
	Modlink *caller, *clock;
	unsigned int i, rune, arguments, parameters;
	int dynamic;

	memset(&load, 0, sizeof(load));
	if(heapmonitor != nil || !dis64runtimemodel() || path == nil || path == H ||
	   image == nil || image == H || bytes > Dis64RuntimeBytes ||
	   (uintptr)image > (uintptr)-1-bytes || (jit != 0 && jit != 1) ||
	   !dis64storageimagecheckcode(image, bytes, &dis64numericlimits, &load.image, dis64clockcode) ||
	   load.image.module.profile != 1 || load.image.module.entry != 0 ||
	   load.image.imports.count == 0 || load.image.imports.count > nelem(load.clockload) ||
	   load.image.exports.count != 0 ||
	   load.image.data.mapbytes != 1 || load.image.initializers.count != 0 ||
	   !dis64typeget(&load.image.types, load.image.module.frametype, &frame) ||
	   frame.bytes != 80 || frame.align != 8 || frame.mapbytes != 2 ||
	   frame.map[0] != 0 || frame.map[1] != 0 ||
	   !disstringpoolget(&load.image.strings, load.image.module.name, &load.name) ||
	   load.name.count == 0 || load.name.count > Dis64RuntimeName){
		kwerrstr("unsupported Dis64 Sys clock consumer");
		return nil;
	}
	dynamic = load.image.code.data[4] == ILOAD;
	if(!dynamic && load.image.imports.count != 1){
		kwerrstr("Dis64 direct Sys call requires one import");
		return nil;
	}
	if(load.image.data.bytes != (dynamic ? 16 : 8) ||
	   load.image.data.map[0] != (dynamic ? 0xc0 : 0x80)){
		kwerrstr("unsupported Dis64 Sys clock module data");
		return nil;
	}
	for(i=0; i<load.name.count; i++)
		if(!disruneat(&load.name, i, &rune) || rune == 0 || rune > 127){
			kwerrstr("unsupported Dis64 Sys clock display name");
			return nil;
		}
	sys = nil;
	clock = nil;
	arguments = 0;
	for(i=0; i<load.image.imports.count; i++){
		entry = dis64sysclockview(&load.image, i, &sys);
		if(entry == nil && dynamic){
			entry = dis64sysintegerview(&load.image, i, &parameters, &sys);
			if(entry != nil)
				arguments |= parameters << (2*i);
		}
		if(entry == nil)
			return nil;
		load.clockload[i] = entry;
	}
	if(!dis64syscode(load.image.code.data, load.image.code.bytes, Dis64ClockInstructions,
	   &load.image.code, load.image.imports.count, arguments)){
		kwerrstr("Dis64 Sys calls do not match the imported group");
		return nil;
	}
	if(dynamic)
		load.clockimports = load.image.imports.count;
	else{
		clock = dis64rootclock(sys, load.clockload[0]);
		if(clock == nil)
			return nil;
	}
	load.path = path;
	load.jit = jit;
	load.indirect = 1;
	load.step = native64lea8step;
	caller = nil;
	if(kprotect(dis64numericconstruct, &load))
		caller = dis64rootlink(load.module, load.lease, 0, nil, nil);
	else{
		if(load.module != nil)
			unload(load.module);
		free64storage(load.lease);
	}
	if(clock == nil)
		return caller;
	if(caller != nil){
		memmove(caller->MP, &clock, sizeof(clock));
		Setmark(D2H(clock));
	}
	ptrdel(D2H(clock));
	if(caller == nil)
		destroy(clock);
	return caller;
}
#endif


/* Return an owned, externally rooted instance with binding slot zero.
 * The caller holds VM exclusion, then removes this one ptradd root after
 * publishing an independent collector root or immediately before destroy.
 * Allocation monitoring must stay disabled during private construction. */
Modlink*
prepare64providerlink(char *path, uchar *image, ulong bytes, int jit,
	const uchar *consumer, ulong consumerbytes, unsigned int ordinal)
{
	Module *module;
	Dis64StorageOwner *lease;
	Dis64PairPlan plan;

	if(ordinal == Dis64AllBindings){
		kwerrstr("invalid Dis64 provider ordinal");
		return nil;
	}
	if(heapmonitor != nil){
		kwerrstr("Dis64 provider link requires unmonitored allocation");
		return nil;
	}
	lease = nil;
	module = prepare64provider(path, image, bytes, jit, &lease,
		consumer, consumerbytes, ordinal, &plan);
	if(module == nil)
		return nil;
	return dis64rootlink(module, lease, 1, &plan, nil);
}


/* Construct a private consumer and its single provider from two Dis64 images.
 * The caller owns the rooted link and initializes its entry frame before
 * process publication. VM exclusion and unmonitored allocation are required. */
Modlink*
prepare64consumerlink(char *path, uchar *image, ulong bytes, char *providerpath,
	uchar *providerimage, ulong providerbytes, int jit)
{
	Module *module;
	Modlink *caller, *provider;
	Dis64StorageOwner *lease;
	Dis64StorageImage consumer;
	Dis64PairPlan plan;

	if(heapmonitor != nil){
		kwerrstr("Dis64 consumer link requires unmonitored allocation");
		return nil;
	}
	lease = nil;
	module = dis64numericbuild(path, image, bytes, jit, &lease, nil, 0, 1, nil);
	if(module == nil)
		return nil;
	caller = dis64rootlink(module, lease, 0, nil, nil);
	if(caller == nil)
		return nil;
	lease = nil;
	provider = nil;
	if(dis64storageimagecheck(image, bytes, &dis64numericlimits, &consumer)){
		module = dis64numericbuild(providerpath, providerimage, providerbytes, jit,
			&lease, &consumer, Dis64AllBindings, 0, &plan);
		if(module != nil)
			provider = dis64rootlink(module, lease, plan.count, &plan, nil);
	}
	if(provider == nil){
		ptrdel(D2H(caller));
		destroy(caller);
		return nil;
	}
	memmove(caller->MP, &provider, sizeof(provider));
	Setmark(D2H(provider));
	ptrdel(D2H(provider));
	return caller;
}


#if defined(DIS_NATIVE64_SCALAR_BOOT) && defined(DIS64_PILOT_LOADER)


/* Return an independently rooted consumer for incremental sibling binding. */
Modlink*
prepare64siblings(char *path, uchar *image, ulong bytes, int jit)
{
	Dis64StorageOwner *lease;
	Module *module;

	if(heapmonitor != nil)
		return nil;
	lease = nil;
	module = dis64numericbuild(path, image, bytes, jit, &lease, nil, 0, 2, nil);
	return module == nil ? nil : dis64rootlink(module, lease, 0, nil, nil);
}
#endif
