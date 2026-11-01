#ifndef INFERNO_NATIVE64_FIELD_H
#define INFERNO_NATIVE64_FIELD_H // Private compiler field witness; no serialized capability or file-call admission.
#include "native64-entry.h"
#include "native64-frame.h"
#include "native64-caller.h"
#include "dis64-scalar.h"
#ifndef DIS_NATIVE64_CALLER_GUARD
#error scalar field witnesses require the synchronous caller guard
#endif

typedef struct Native64FieldWitness Native64FieldWitness;

struct Native64FieldWitness
{
	Native64FieldWitness *self; // Noncopyable owning identity, nil after clear.
	Native64Caller caller; // Acquired generation, exact frame Type and scheduler hold.
	Module *module; // One retained exact caller module; never an independent Prog lifetime.
	Modlink *instance; // Exact current module instance, not merely its source name.
	uchar *mp, *sp, *ts; // Captured instance data and active extent bounds.
	Link *exports; // Immutable export table identity during this synchronous scope.
	Type **types; // Immutable descriptor table identity retained by the module.
	unsigned int count, index; // Captured export count and selected callable ordinal.
	const LimboNative64Fields *table; // Trusted immutable compiler-produced metadata.
	const LimboNative64Entry *entry; // Exact generated callable identity, not a consistency hash.
	const LimboNative64Field *fields; // Captured compiler field table identity.
	unsigned int arguments; // Captured complete source parameter count.
	LimboNative64Field field; // Exact declared ordinal/kind/offset/width selected before callbacks.
};


/* Check every declared field; an all-zero map never turns padding or a Big into a Real. */
static int
native64fieldtable(const LimboNative64Fields *table, Type *type)
{
	const LimboNative64Entry *entry;
	const LimboNative64Field *field;
	unsigned int i, end = 64;

	if(table == nil || table->entry == nil || table->count > 4096 ||
	   (table->count != 0 && table->fields == nil) || type == nil || type == H)
		return 0;
	entry = table->entry;
	if(!native64bindrecord(entry, 1) || type->ref <= 0 || type->size != entry->frame_size ||
	   type->np != entry->map_bytes || memcmp(type->map, entry->map, entry->map_bytes) != 0)
		return 0;
	for(i=0; i<table->count; i++){
		field = table->fields+i;
		if(field->ordinal != i || field->offset < end || field->offset > entry->frame_size ||
		   field->bytes == 0 || field->bytes > entry->frame_size-field->offset)
			return 0;
		if(field->kind != Dis64Void &&
		   ((field->kind != Dis64Big && field->kind != Dis64Real) || field->bytes != 8 ||
		    field->offset%8 != 0 || (entry->map[field->offset/64] & (0x80>>(field->offset/8%8)))))
			return 0;
		end = field->offset+field->bytes;
	}
	return 1;
}


/* Bind generated callable identity to a unique owning native export; hashes are only consistency checks. */
static int
native64fieldexport(Module *module, Type *type, const LimboNative64Entry *entry, unsigned int *index)
{
	unsigned int i, found = 0;
	UWORD signature;

	if(module == nil || module == H || module->ref <= 0 ||
	   module->ntype <= 0 || module->ntype > Native64BindEntries || module->ext == nil || module->type == nil)
		return 0;
	for(i=0; i<(unsigned int)module->ntype; i++){
		if(!native64bindname(module->ext[i].name))
			return 0;
		if(!native64bindsame(module->ext[i].name, entry->name))
			continue;
		memmove(&signature, &module->ext[i].sig, sizeof(signature));
		if(found || module->ext[i].frame != type || module->type[i] != type ||
		   module->ext[i].u.runt != entry->function || signature != entry->legacy_signature)
			return 0;
		*index = i;
		found = 1;
	}
	return found;
}


/* Acquire only a compiler-declared eight-byte scalar in the exact current native module instance. */
static int
native64fieldbegin(Native64FieldWitness *witness, Prog *prog, Module *module, Type *type,
	const LimboNative64Fields *table, unsigned int ordinal, unsigned int kind, int live)
{
	REG registers;
	unsigned int index;

	if(witness == nil || witness->self != nil || witness->caller.self != nil ||
	   prog == nil || prog == H || prog != currun() || prog->state != Pready ||
	   prog->native64holds != 0 || prog->kill != nil || (prog->flags & Pkilled) || !native64model() ||
	   (live != 0 && live != 1) ||
	   (live ? prog->native64live != &R : !native64callerregisters(prog)) ||
	   (kind != Dis64Real && kind != Dis64Big) || !native64fieldtable(table, type) ||
	   ordinal >= table->count || table->fields[ordinal].kind != kind)
		return 0;
	registers = live ? R : prog->R;
	if(registers.M == nil || registers.M == H || (uintptr)registers.M%sizeof(void*) != 0 ||
	   (uintptr)registers.M > (uintptr)-1-sizeof(*registers.M) || registers.M->m != module ||
	   (!native64fieldexport(module, type, table->entry, &index) || module->ref == 0x7fffffff))
		return 0;
	if(!(live ? native64callerbeginlive(&witness->caller, prog, type, table->fields[ordinal].offset) :
	            native64callerbegin(&witness->caller, prog, type, table->fields[ordinal].offset)))
		return 0;
	module->ref++;
	witness->self = witness; witness->module = module; witness->instance = registers.M;
	witness->mp = registers.MP; witness->sp = registers.SP; witness->ts = registers.TS;
	witness->exports = module->ext; witness->types = module->type;
	witness->count = module->ntype; witness->index = index;
	witness->table = table; witness->entry = table->entry; witness->fields = table->fields;
	witness->arguments = table->count; witness->field = table->fields[ordinal];
	return 1;
}


/* Recheck retained module/export/field identity before touching the possibly invalidated result frame. */
static void*
native64fieldresolve(Native64FieldWitness *witness)
{
	Prog *prog;
	unsigned int index;
	const LimboNative64Field *field;

	if(witness == nil || witness->self != witness || !native64callerready(&witness->caller))
		return nil;
	prog = witness->caller.prog;
	if(prog->R.M != witness->instance || prog->R.MP != witness->mp ||
	   prog->R.SP != witness->sp || prog->R.TS != witness->ts ||
	   witness->instance->m != witness->module || witness->module->ext != witness->exports ||
	   witness->module->type != witness->types || witness->module->ntype != witness->count ||
	   witness->table->entry != witness->entry || witness->table->fields != witness->fields ||
	   witness->table->count != witness->arguments)
		return nil;
	field = witness->fields+witness->field.ordinal;
	if(field->ordinal != witness->field.ordinal || field->kind != witness->field.kind ||
	   field->offset != witness->field.offset || field->bytes != witness->field.bytes ||
	   !native64fieldexport(witness->module, witness->caller.type, witness->entry, &index) || index != witness->index)
		return nil;
	return native64callerresolve(&witness->caller);
}


/* Permit only the existing checked within-extent transfer; never revive a stale witness. */
static int
native64fieldrebind(Native64FieldWitness *witness, void *frame)
{
	return native64fieldresolve(witness) != nil && native64callerrebind(&witness->caller, frame);
}


/* Detach before nonthrowing retirement; copies and repeated clears never drop another owner's references. */
static void
native64fieldclear(Native64FieldWitness *witness)
{
	Module *module;

	if(witness == nil || witness->self != witness)
		return;
	module = witness->module;
	witness->self = nil; witness->module = nil;
	native64callerclear(&witness->caller);
	unload(module);
}
#endif
