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


/* Prepare owned Types/data with actual GC rooting, never executable instructions or dependency links.
 * Caller holds ordinary VM exclusion; input remains immutable/disjoint until return.
 * Success requires one free64storage call under that same exclusion. The opaque lease
 * retains no source pointers and cannot be scheduled as a Module. Structural
 * success grants no code/import/Frame64 root semantics or managed file admission. */
Dis64StorageOwner*
prepare64storage(uchar *image, ulong bytes)
{
	Dis64StorageLoad load;
	Dis64StorageLimits limits = {64, 65536, Dis64RuntimeObject, 65536,
		Dis64RuntimeCode, 1024, 256, 1024};

	memset(&load, 0, sizeof(load));
	if(!dis64runtimemodel() || image == nil || image == H ||
	   bytes > Dis64RuntimeBytes || (uintptr)image > (uintptr)-1-bytes ||
	   !dis64storageimagecheck(image, bytes, &limits, &load.image)){
		kwerrstr("unsupported Dis64 storage image or runtime model");
		return nil;
	}
	if(kprotect(dis64storageconstruct, &load))
		return load.owner;
	free64storage(load.owner);
	kwerrstr("Dis64 storage construction failed");
	return nil;
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
};


/* Build an owned program/entry only after complete private role/map/native lowering proof. */
static int
dis64numericconstruct(void *argument)
{
	Dis64NumericLoad *load;
	Dis64Instruction instruction;
	Module *module;
	unsigned int i, at, used, rune;

	load = argument;
	load->module = module = mallocz(sizeof(*module), 1);
	if(module == nil)
		return 0;
	module->ref = 1;
	module->rt = DONTCOMPILE;
	module->origmp = H;
	load->lease = prepare64storage((uchar*)load->image.envelope.data, load->image.envelope.bytes);
	if(load->lease == nil || !attach64storage(module, load->lease))
		return 0;
	module->entryt = module->type[load->image.module.frametype];
	module->nprog = load->image.code.count;
	module->prog = mallocz(module->nprog*sizeof(Inst), 1);
	if(module->prog == nil)
		return 0;
	at = 4;
	for(i=0; i<load->image.code.count; i++){
		if(!dis64instructionread(load->image.code.data+at, load->image.code.bytes-at, &instruction, &used))
			return 0;
		dis64runtimeinst(module->prog+i, &instruction, module->prog);
		at += used;
	}
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
	if(load->jit && !compile(module, module->nprog, nil))
		return 0;
	return 1;
}


static const Dis64StorageLimits dis64numericlimits = {
	64, 65536, Dis64RuntimeObject, 65536, Dis64RuntimeCode, 1024, 256, 1024
};


/* Check a void, zero-argument import against the provider's own entry. */
static int
dis64providerentry(const Dis64StorageImage *consumer, unsigned int ordinal,
	const Dis64StorageImage *provider, Dis64CodeView *code)
{
	Dis64Binding imported;
	Dis64ImportMatch match;

	return dis64bindingget(&consumer->imports, ordinal, &imported) &&
		imported.signature.result == Dis64Void && imported.signature.parameters == 0 &&
		dis64importmatch(consumer, ordinal, provider, &match) &&
		match.entry == provider->module.entry && match.frame == provider->module.frametype &&
		dis64numericentrycheck(provider, Dis64RuntimeCode, 1, code);
}


/* Construct an owned numeric entry and keep its MP rooted through handoff. */
static Module*
dis64numericbuild(char *path, uchar *image, ulong bytes, int jit, Dis64StorageOwner **roots,
	const Dis64StorageImage *consumer, unsigned int ordinal)
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
	   !(consumer == nil ? dis64numericstoragecheck(&load.image, Dis64RuntimeCode, &code) :
	     dis64providerentry(consumer, ordinal, &load.image, &code)) ||
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
	return dis64numericbuild(path, image, bytes, jit, roots, nil, 0);
}


/* Build the explicitly selected provider for a checked import. The caller
 * owns the returned Module and lease under the same contract as the standalone
 * builder. Input spans stay immutable through the call and can be freed after it. */
Module*
prepare64provider(char *path, uchar *image, ulong bytes, int jit, Dis64StorageOwner **roots,
	const uchar *consumer, ulong consumerbytes, unsigned int ordinal)
{
	Dis64StorageImage view;

	if(!dis64runtimemodel() || consumer == nil || consumer == H ||
	   consumerbytes > Dis64RuntimeBytes || (uintptr)consumer > (uintptr)-1-consumerbytes ||
	   !dis64storageimagecheck(consumer, consumerbytes, &dis64numericlimits, &view)){
		kwerrstr("unsupported Dis64 consumer image");
		return nil;
	}
	return dis64numericbuild(path, image, bytes, jit, roots, &view, ordinal);
}


typedef struct Dis64ProviderLink Dis64ProviderLink;

struct Dis64ProviderLink {
	Module *module; // Owned until mklinkmod returns.
	Modlink *link; // Owned partial instance until external root acquisition.
};


/* Materialize the one matched binding and root it before releasing the MP lease. */
static int
dis64providerlink(void *argument)
{
	Dis64ProviderLink *build;
	Module *module;
	Modlink *link;

	build = argument;
	module = build->module;
	build->link = link = mklinkmod(module, 1);
	build->module = nil;
	link->links[0].u.pc = module->entry;
	link->links[0].frame = module->entryt;
	if(module->origmp != H){
		D2H(module->origmp)->ref++;
		link->MP = module->origmp;
	}
	ptradd(D2H(link));
	return 1;
}


/* Return an owned, externally rooted instance with binding slot zero.
 * The caller holds VM exclusion, then removes this one ptradd root after
 * publishing an independent collector root or immediately before destroy.
 * Allocation monitoring must stay disabled during private construction. */
Modlink*
prepare64providerlink(char *path, uchar *image, ulong bytes, int jit,
	const uchar *consumer, ulong consumerbytes, unsigned int ordinal)
{
	Dis64ProviderLink build;
	Dis64StorageOwner *lease;
	int ok;

	if(heapmonitor != nil){
		kwerrstr("Dis64 provider link requires unmonitored allocation");
		return nil;
	}
	lease = nil;
	build.link = nil;
	build.module = prepare64provider(path, image, bytes, jit, &lease,
		consumer, consumerbytes, ordinal);
	if(build.module == nil)
		return nil;
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
