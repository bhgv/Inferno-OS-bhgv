#include "limbo.h"
#include "dis64-lower.h"
#include "dis64-inline.h"

#define ScalarInlineFunctions 16U // Bound source specialization, including the sole exported entry.
#define ScalarInlineFrame 128U // Bound scalar scratch and local storage in each scalar helper.
#define ScalarInlineBody 32U // Bound helper bodies, excluding the terminal RET.
#define ScalarInlineLabels 8U // Match the existing numeric case lowering bound.

typedef struct InlineFunction InlineFunction;

struct InlineFunction {
	Decl *declaration; // Borrowed source function declaration.
	unsigned int start; // Original resolved first Dis PC.
	unsigned int end; // Exclusive original function extent.
	unsigned int expanded; // Expanded body size, excluding per-call zero stores.
	unsigned int copies; // Case tables including descendant call copies.
	unsigned int state; // DAG visit state: unseen, active or complete.
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
	Case *table;
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
		if(input->op == IFRAME || input->op == ICALL)
			continue; // Pair identity and recursion are checked before expansion.
		if(input->op == IEXIT ||
		   !inlinescratch(input->sm, &input->s, decl->desc->size) ||
		   !inlinescratch(input->mm, &input->m, decl->desc->size) ||
		   !inlinescratch(input->dm, &input->d, decl->desc->size))
			return 0;
		if(input->dm == Apc){
			if(input->d.offset < function->start || input->d.offset >= function->end ||
			   (input->branch != nil && input->branch->pc != input->d.offset))
				return 0;
		}else if(input->branch != nil)
			return 0;
		if(input->op == ICASE || input->op == ICASEL || input->op == IGOTO){
			table = dis64scalarcase(input, count);
			if(table == nil || table->iwild->pc < function->start || table->iwild->pc >= function->end)
				return 0;
			for(i = 0; i < table->nlab; i++)
				if(table->labs[i].inst->pc < function->start || table->labs[i].inst->pc >= function->end)
					return 0;
			continue;
		}
		/* Validate the whole contiguous helper span, without the legacy scratch/local discontinuity. */
		copy = *input;
		if(copy.sm == Afp) copy.s.reg += ScalarLegacyPrefix-ScalarScratchStart;
		if(copy.mm == Afp) copy.m.reg += ScalarLegacyPrefix-ScalarScratchStart;
		if(copy.dm == Afp) copy.d.reg += ScalarLegacyPrefix-ScalarScratchStart;
		if(!dis64scalarinst(&copy, count, data, decl->desc->size+ScalarNativePrefix-ScalarScratchStart, ScalarNativePrefix, &checked))
			return 0;
	}
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
	return &flat[mapping[pc-start]];
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


typedef struct InlineContext InlineContext;

struct InlineContext {
	InlineFunction *functions; // Borrowed validated source function spans and expansion sizes.
	Inst **original; // Borrowed original instructions indexed by resolved PC.
	Dis64Prepared *prepared; // Owned output arrays under construction.
	unsigned int entry; // Sole exported function index; calls back into it are refused.
	unsigned int bases[ScalarInlineFunctions]; // Private scratch base for each active call depth.
	unsigned int sizes[ScalarInlineFunctions]; // Largest helper span at each active call depth.
	uchar seen[ScalarInlineFunctions][ScalarInlineFunctions]; // Bound repeated DAG depth visits.
	unsigned int table; // Next unconsumed private case-table record.
};


/* Match an exact adjacent source call pair against its caller and callee frame geometry. */
static int
inlinecall(InlineContext *context, InlineFunction *caller, unsigned int pc)
{
	Inst *frame, *call;
	int callee;

	if(pc+1 >= caller->end)
		return -1;
	frame = context->original[pc];
	call = context->original[pc+1];
	if(frame->op != IFRAME || call->op != ICALL || frame->sm != Adesc || frame->mm != Anone || frame->dm != Afp ||
	   frame->d.reg < ScalarLegacyPrefix || frame->d.reg > caller->declaration->desc->size-IBY2WD ||
	   call->sm != Afp || call->s.reg != frame->d.reg || call->mm != Anone || call->dm != Apc)
		return -1;
	callee = inlinefunction(context->functions, nfns, call->d.offset);
	if(callee < 0 || callee == context->entry || frame->s.offset != context->functions[callee].declaration->desc->id)
		return -1;
	return callee;
}


/* Count finite DAG expansion before allocating output; active revisits reject direct or mutual recursion. */
static int
inlineextent(InlineContext *context, unsigned int index)
{
	InlineFunction *function, *child;
	Inst *input;
	unsigned int pc, end, amount, count, tables;
	int callee;

	function = &context->functions[index];
	if(function->state != 0)
		return function->state == 2;
	function->state = 1;
	end = function->end-(index != context->entry);
	count = tables = 0;
	for(pc = function->start; pc < end; pc++){
		input = context->original[pc];
		amount = 1;
		if(input->op == IFRAME){
			if(inlinecall(context, function, pc) < 0)
				return 0;
		}else if(input->op == ICALL){
			if(pc == function->start || (callee = inlinecall(context, function, pc-1)) < 0 ||
			   !inlineextent(context, callee))
				return 0;
			child = &context->functions[callee];
			amount = child->zeros+child->expanded;
			if(amount == 0)
				amount = 1;
			tables += child->copies;
		}else if(input->op == ICASE || input->op == ICASEL || input->op == IGOTO)
			tables++;
		if(amount > ScalarCodeLimit-count)
			return 0;
		count += amount;
	}
	function->expanded = count;
	function->copies = tables;
	function->state = 2;
	return 1;
}


/* Reserve disjoint spans for simultaneously active helpers, while reusing spans across sibling calls. */
static int
inlineslots(InlineContext *context, unsigned int index, unsigned int depth)
{
	InlineFunction *function;
	unsigned int pc, bytes;
	int callee;

	if(depth >= ScalarInlineFunctions)
		return 0;
	if(context->seen[index][depth])
		return 1;
	context->seen[index][depth] = 1;
	function = &context->functions[index];
	if(depth != 0){
		bytes = function->declaration->desc->size-ScalarScratchStart;
		if(bytes > context->sizes[depth])
			context->sizes[depth] = bytes;
	}
	for(pc = function->start; pc < function->end; pc++)
		if(context->original[pc]->op == ICALL){
			callee = inlinecall(context, function, pc-1);
			if(callee < 0 || !inlineslots(context, callee, depth+1))
				return 0;
		}
	return 1;
}


/* Emit one independent function copy; parent maps survive recursive emission of child bodies and tables. */
static int
inlineemit(InlineContext *context, unsigned int index, unsigned int depth, unsigned int begin, Inst *call)
{
	InlineFunction *function, *child;
	Inst *input, *copy, *target;
	InlineTable *tables;
	unsigned int *mapping, pc, end, at, amount, i, stop;
	int callee, ok;

	function = &context->functions[index];
	mapping = allocmem((function->end-function->start)*sizeof(*mapping));
	tables = context->prepared->tables;
	at = begin;
	ok = 0;
	for(i = 0; i < function->zeros; i++){
		copy = &context->prepared->storage[at++];
		memset(copy, 0, sizeof(*copy));
		copy->src = call->src;
		copy->op = IMOVW;
		copy->sm = Aimm;
		copy->mm = Anone;
		copy->dm = Afp;
		copy->d.reg = context->bases[depth]+i*IBY2WD;
	}
	end = function->end-(index != context->entry);
	stop = at+function->expanded;
	for(pc = function->start; pc < function->end; pc++){
		mapping[pc-function->start] = at;
		if(pc == end)
			break; // A helper's terminal RET resumes at this copy's continuation.
		amount = 1;
		if(context->original[pc]->op == ICALL){
			callee = inlinecall(context, function, pc-1);
			child = &context->functions[callee];
			amount = child->zeros+child->expanded;
			if(amount == 0)
				amount = 1;
		}
		at += amount;
	}
	at = begin+function->zeros;
	for(pc = function->start; pc < end; pc++){
		input = context->original[pc];
		if(input->op == ICALL){
			callee = inlinecall(context, function, pc-1);
			child = &context->functions[callee];
			if(!inlineemit(context, callee, depth+1, at, input))
				goto done;
			amount = child->zeros+child->expanded;
			at += amount == 0 ? 1 : amount;
			continue;
		}
		copy = &context->prepared->storage[at];
		*copy = *input;
		if(depth != 0){
			inlinerelocate(copy->sm, &copy->s, context->bases[depth]);
			inlinerelocate(copy->mm, &copy->m, context->bases[depth]);
			inlinerelocate(copy->dm, &copy->d, context->bases[depth]);
		}
		if(input->op == IFRAME || (depth != 0 && input->op == IRET)){
			copy->op = IJMP;
			copy->sm = copy->mm = Anone;
			copy->dm = Apc;
			copy->d.offset = input->op == IRET ? stop : at+1;
			copy->branch = nil;
		}else if(input->op == ICASE || input->op == ICASEL || input->op == IGOTO){
			if(!inlinetable(copy, &tables[context->table++], function->start, function->end,
			   mapping, context->original, context->prepared->storage))
				goto done;
		}else if(input->dm == Apc){
			target = inlinetarget(input->d.offset, function->start, function->end, mapping,
				context->original, context->prepared->storage);
			if(target == nil)
				goto done;
			copy->d.offset = target-context->prepared->storage;
			copy->branch = target;
		}
		at++;
	}
	if(at != stop)
		goto done;
	if(depth != 0 && function->zeros+function->expanded == 0){
		copy = &context->prepared->storage[at];
		*copy = *call;
		copy->op = IJMP;
		copy->sm = copy->mm = Anone;
		copy->dm = Apc;
		copy->d.offset = at+1;
		copy->branch = nil;
	}
	ok = 1;
done:
	free(mapping);
	return ok;
}


/* Specialize a bounded acyclic graph of scalar procedures without admitting a runtime file-call ABI. */
int
dis64prepare(Decl *entry, Inst *instructions, unsigned int count, unsigned int data, Dis64Prepared *prepared)
{
	InlineFunction functions[ScalarInlineFunctions];
	InlineContext context;
	Inst **original, *input;
	unsigned int n, i, j, bytes;
	int current, ok;

	prepared->instructions = instructions;
	prepared->count = count;
	prepared->localbytes = entry->desc->size;
	if(nfns == 1)
		return fns[0] == entry && entry->pc->pc == 0;
	if(nfns < 1 || nfns > ScalarInlineFunctions)
		return 0;
	original = allocmem(count*sizeof(*original));
	memset(functions, 0, sizeof(functions));
	memset(&context, 0, sizeof(context));
	context.functions = functions;
	context.original = original;
	context.prepared = prepared;
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
	context.entry = current;
	context.sizes[1] = ScalarLegacyPrefix-ScalarScratchStart;
	for(i = 0; i < nfns; i++){
		for(j = 0; j < nfns; j++)
			if(functions[j].start > functions[i].start && functions[j].start < functions[i].end)
				functions[i].end = functions[j].start;
		if(i != current){
			if(!inlinebody(&functions[i], original, count, data))
				goto done;
			bytes = functions[i].declaration->desc->size-ScalarScratchStart;
			if(bytes > context.sizes[1])
				context.sizes[1] = bytes;
		}
	}
	input = original[functions[current].end-1];
	if(input->op != IRET && input->op != IEXIT)
		goto done;
	for(i = 0; i < nfns; i++)
		if(!inlineextent(&context, i))
			goto done;
	if(!inlineslots(&context, current, 0))
		goto done;
	bytes = (entry->desc->size+7)&~7U;
	for(i = 1; i < ScalarInlineFunctions; i++)
		if(context.sizes[i] != 0){
			bytes = (bytes+7)&~7U;
			context.bases[i] = bytes;
			bytes += context.sizes[i];
		}
	prepared->localbytes = bytes;
	if(bytes > ScalarObjectLimit-ScalarNativePrefix-ScalarScratchBytes)
		goto done;
	prepared->count = functions[current].expanded;
	prepared->storage = allocmem(prepared->count*sizeof(*prepared->storage));
	prepared->instructions = prepared->storage;
	prepared->tables = allocmem((functions[current].copies+1)*sizeof(InlineTable));
	if(!inlineemit(&context, current, 0, 0, nil) || context.table != functions[current].copies)
		goto done;
	for(i = 0; i < prepared->count; i++){
		prepared->storage[i].pc = i;
		prepared->storage[i].next = i+1 == prepared->count ? nil : &prepared->storage[i+1];
	}
	ok = 1;
done:
	free(original);
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
