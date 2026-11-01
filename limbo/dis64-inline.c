#include "limbo.h"
#include "dis64-lower.h"
#include "dis64-inline.h"

#define ScalarInlineFunctions 16U // Bound source specialization, including the sole exported entry.
#define ScalarInlineFrame 128U // Bound scalar scratch and local storage in each straightline helper.
#define ScalarInlineBody 32U // Bound straightline helper bodies, excluding the terminal RET.
#define ScalarInlineLabels 8U // Match the existing numeric case lowering bound.

typedef struct InlineFunction InlineFunction;

struct InlineFunction {
	Decl *declaration; // Borrowed source function declaration.
	unsigned int start; // Original resolved first Dis PC.
	unsigned int end; // Exclusive original function extent.
	unsigned int body; // Validated helper-body instruction count, excluding RET.
	unsigned int zeros; // Four-byte stores that renew a helper with declared scalar locals.
};

typedef struct InlineTable InlineTable;

struct InlineTable {
	Decl declaration; // Private declaration attached only to the flattened case instruction.
	Type type; // Private type whose Case points into this record.
	Node initializer; // Private self-referencing compiler initializer identity.
	Case table; // Private arm/default pointers into the flattened entry.
	Label labels[ScalarInlineLabels]; // Private label records borrowing original constant Nodes.
};


/* Select an exact function start; no name/hash or interior-PC approximation is accepted. */
static int
inlinefunction(InlineFunction *functions, unsigned int count, long pc)
{
	unsigned int i;

	for(i = 0; i < count; i++)
		if(pc >= 0 && functions[i].start == pc)
			return i;
	return -1;
}


/* Accept helper-owned scratch/temporary offsets while excluding its reserved header. */
static int
inlinescratch(int mode, Addr *address, unsigned int bytes)
{
	if(mode != Afp)
		return 1;
	return address->reg >= STemp && address->reg < bytes;
}


/* Validate one no-argument/no-result helper against its own scratch geometry and shared scalar roles. */
static int
inlinebody(InlineFunction *function, Inst **original, unsigned int count, unsigned int data)
{
	Decl *decl;
	Inst *input, copy;
	Decl *local;
	Dis64Instruction checked;
	unsigned int pc, i;

	decl = function->declaration;
	if(decl->ty == nil || decl->ty->kind != Tfn || decl->ty->ids != nil ||
	   decl->ty->varargs || decl->ty->tof == nil || decl->ty->tof->kind != Tnone ||
	   decl->desc == nil || decl->desc->size < ScalarLegacyPrefix || decl->desc->size > ScalarInlineFrame || decl->desc->nmap < 0 ||
	   (decl->desc->nmap != 0 && decl->desc->map == nil) ||
	   function->end-function->start > ScalarInlineBody+1)
		return 0;
	function->zeros = 0;
	for(local = decl->locals; local != nil; local = local->next){
		if(local->sym == nil || local->sym->name == nil || local->ty == nil ||
		   (local->ty->kind != Tint && local->ty->kind != Tbig && local->ty->kind != Treal))
			return 0;
		if(local->sym->name[0] != '.'){
			if(decl->desc->size%IBY2WD != 0)
				return 0;
			function->zeros = (decl->desc->size-ScalarScratchStart)/IBY2WD;
		}
	}
	for(i = 0; i < decl->desc->nmap; i++)
		if(decl->desc->map[i] != 0)
			return 0;
	if(function->end <= function->start || original[function->end-1]->op != IRET)
		return 0;
	for(pc = function->start; pc+1 < function->end; pc++){
		input = original[pc];
		if(input->op == IRET || input->op == IEXIT || input->branch != nil || input->dm == Apc ||
		   !inlinescratch(input->sm, &input->s, decl->desc->size) ||
		   !inlinescratch(input->mm, &input->m, decl->desc->size) ||
		   !inlinescratch(input->dm, &input->d, decl->desc->size))
			return 0;
		/* Validate the whole contiguous helper span, without the legacy scratch/local discontinuity. */
		copy = *input;
		if(copy.sm == Afp) copy.s.reg += ScalarLegacyPrefix-ScalarScratchStart;
		if(copy.mm == Afp) copy.m.reg += ScalarLegacyPrefix-ScalarScratchStart;
		if(copy.dm == Afp) copy.d.reg += ScalarLegacyPrefix-ScalarScratchStart;
		if(!dis64scalarinst(&copy, count, data, decl->desc->size+ScalarNativePrefix-ScalarScratchStart, ScalarNativePrefix, &checked))
			return 0;
	}
	function->body = function->end-function->start-1;
	return 1;
}


/* Rebase callee scratch into its own appended local region, retaining byte aliases without caller scratch overlap. */
static void
inlinerelocate(int mode, Addr *address, unsigned int scratch)
{
	if(mode == Afp)
		address->reg = scratch+address->reg-ScalarScratchStart;
}


/* Resolve an entry-local target only; entering a CALL without its paired FRAME is forbidden. */
static Inst*
inlinetarget(long pc, unsigned int start, unsigned int end, unsigned int *mapping, Inst **original, Inst *flat)
{
	if(pc < start || pc >= end || original[pc]->op == ICALL)
		return nil;
	return &flat[mapping[pc]];
}


/* Clone case target metadata without changing the old compiler's table, type, initializer or constant Nodes. */
static int
inlinetable(Inst *copy, InlineTable *clone, unsigned int start, unsigned int end,
	unsigned int *mapping, Inst **original, Inst *flat)
{
	Decl *decl;
	Case *source;
	unsigned int i;

	decl = copy->d.decl;
	if(!dis64scalartable(decl))
		return 0;
	source = decl->ty->cse;
	if(source->nlab < 0 || source->nlab > ScalarInlineLabels ||
	   (source->nlab != 0 && source->labs == nil) || source->iwild == nil)
		return 0;
	clone->declaration = *decl;
	clone->type = *decl->ty;
	clone->initializer = *decl->init;
	clone->table = *source;
	clone->declaration.ty = &clone->type;
	clone->declaration.init = &clone->initializer;
	clone->initializer.decl = &clone->declaration;
	clone->type.cse = &clone->table;
	clone->table.labs = clone->labels;
	clone->table.iwild = inlinetarget(source->iwild->pc, start, end, mapping, original, flat);
	if(clone->table.iwild == nil)
		return 0;
	for(i = 0; i < source->nlab; i++){
		if(source->labs[i].inst == nil)
			return 0;
		clone->labels[i] = source->labs[i];
		clone->labels[i].inst = inlinetarget(source->labs[i].inst->pc, start, end, mapping, original, flat);
		if(clone->labels[i].inst == nil)
			return 0;
	}
	copy->d.decl = &clone->declaration;
	return 1;
}


/* Specialize only direct FRAME/CALL pairs to validated leaf procedures; keep the sole-entry path byte-identical. */
int
dis64prepare(Decl *entry, Inst *instructions, unsigned int count, unsigned int data, Dis64Prepared *prepared)
{
	InlineFunction functions[ScalarInlineFunctions];
	Inst **original, *input, *copy, *target;
	InlineTable *tables;
	unsigned int *mapping, n, i, j, start, end, at, amount, tablecount, scratch, helperbytes;
	int current, callee, ok;

	prepared->instructions = instructions;
	prepared->count = count;
	prepared->localbytes = entry->desc->size;
	if(nfns == 1)
		return fns[0] == entry && entry->pc->pc == 0;
	if(nfns < 1 || nfns > ScalarInlineFunctions)
		return 0;
	original = allocmem(count*sizeof(*original));
	mapping = allocmem(count*sizeof(*mapping));
	ok = 0;
	n = 0;
	for(input = instructions; input != nil; input = input->next){
		if(input->op == INOOP)
			continue;
		if(n >= count || input->pc != n)
			goto done;
		original[n++] = input;
	}
	if(n != count)
		goto done;
	current = -1;
	for(i = 0; i < nfns; i++){
		if(fns[i] == nil || fns[i]->pc == nil || fns[i]->pc->pc < 0 || fns[i]->pc->pc >= count)
			goto done;
		functions[i].declaration = fns[i];
		functions[i].start = fns[i]->pc->pc;
		functions[i].end = count;
		for(j = 0; j < i; j++)
			if(functions[j].start == functions[i].start)
				goto done;
		if(fns[i] == entry)
			current = i;
	}
	if(current < 0)
		goto done;
	helperbytes = ScalarLegacyPrefix;
	for(i = 0; i < nfns; i++){
		for(j = 0; j < nfns; j++)
			if(functions[j].start > functions[i].start && functions[j].start < functions[i].end)
				functions[i].end = functions[j].start;
		if(i != current){
			if(!inlinebody(&functions[i], original, count, data))
				goto done;
			if(functions[i].declaration->desc->size > helperbytes)
				helperbytes = functions[i].declaration->desc->size;
		}
	}
	start = functions[current].start;
	end = functions[current].end;
	if(original[end-1]->op != IRET && original[end-1]->op != IEXIT)
		goto done;
	at = tablecount = 0;
	for(i = start; i < end; i++){
		input = original[i];
		mapping[i] = at;
		amount = 1;
		if(input->op == IFRAME){
			if(i+1 >= end || original[i+1]->op != ICALL || input->sm != Adesc || input->mm != Anone || input->dm != Afp ||
			   input->d.reg < ScalarLegacyPrefix || input->d.reg > entry->desc->size-IBY2WD)
				goto done;
			target = original[i+1];
			callee = inlinefunction(functions, nfns, target->d.offset);
			if(callee < 0 || callee == current || target->sm != Afp || target->s.reg != input->d.reg ||
			   target->mm != Anone || target->dm != Apc || input->s.offset != functions[callee].declaration->desc->id)
				goto done;
		}else if(input->op == ICALL){
			if(i == start || original[i-1]->op != IFRAME)
				goto done;
			callee = inlinefunction(functions, nfns, input->d.offset);
			if(callee < 0 || callee == current)
				goto done;
			amount = functions[callee].body+functions[callee].zeros;
			if(amount == 0)
				amount = 1;
		}
		if(input->op == ICASE || input->op == ICASEL || input->op == IGOTO)
			tablecount++;
		if(amount > ScalarCodeLimit-at)
			goto done;
		at += amount;
	}
	prepared->count = at;
	prepared->storage = allocmem(at*sizeof(*prepared->storage));
	prepared->instructions = prepared->storage;
	prepared->tables = allocmem((tablecount+1)*sizeof(InlineTable));
	tables = prepared->tables;
	scratch = (entry->desc->size+7)&~7U;
	prepared->localbytes = scratch+helperbytes-ScalarScratchStart;
	if(prepared->localbytes > ScalarObjectLimit-ScalarNativePrefix-ScalarScratchBytes)
		goto done;
	at = tablecount = 0;
	for(i = start; i < end; i++){
		input = original[i];
		copy = &prepared->storage[at];
		if(input->op == ICALL){
			callee = inlinefunction(functions, nfns, input->d.offset);
			/* Renew the entire private span, including padding and both halves of Big/Real locals. */
			for(j = 0; j < functions[callee].zeros; j++){
				memset(copy, 0, sizeof(*copy));
				copy->src = input->src;
				copy->op = IMOVW;
				copy->sm = Aimm;
				copy->mm = Anone;
				copy->dm = Afp;
				copy->d.reg = scratch+j*IBY2WD;
				copy = &prepared->storage[++at];
			}
			for(j = functions[callee].start; j+1 < functions[callee].end; j++){
				*copy = *original[j];
				inlinerelocate(copy->sm, &copy->s, scratch);
				inlinerelocate(copy->mm, &copy->m, scratch);
				inlinerelocate(copy->dm, &copy->d, scratch);
				copy = &prepared->storage[++at];
			}
			if(functions[callee].body != 0 || functions[callee].zeros != 0)
				continue;
		}
		*copy = *input;
		if(input->op == IFRAME || input->op == ICALL){
			copy->op = IJMP;
			copy->sm = copy->mm = Anone;
			copy->dm = Apc;
			copy->d.offset = at+1;
			copy->branch = nil;
		}else if(input->op == ICASE || input->op == ICASEL || input->op == IGOTO){
			if(!inlinetable(copy, &tables[tablecount++], start, end, mapping, original, prepared->storage))
				goto done;
		}else if(input->dm == Apc){
			target = inlinetarget(input->d.offset, start, end, mapping, original, prepared->storage);
			if(target == nil)
				goto done;
			copy->d.offset = target-prepared->storage;
			copy->branch = target;
		}
		at++;
	}
	if(at != prepared->count)
		goto done;
	for(i = 0; i < at; i++){
		prepared->storage[i].pc = i;
		prepared->storage[i].next = i+1 == at ? nil : &prepared->storage[i+1];
	}
	ok = 1;
done:
	free(original);
	free(mapping);
	return ok;
}


/* Retire private entry/table copies after scalar lowering and trace publication have finished. */
void
dis64preparefree(Dis64Prepared *prepared)
{
	free(prepared->storage);
	free(prepared->tables);
	memset(prepared, 0, sizeof(*prepared));
}
