#include "limbo.h"
#include "dis64-lower.h"

#define ScalarCaseLabels 8U // Bound linear lowering; larger or managed tables remain outside this backend.


/* Rebase direct locals and move scalar scratch bytes beyond the local extent, preserving aliases. */
static int
dis64address(int mode, Addr *address, unsigned int scratch, long *value, unsigned int *wire)
{
	switch(mode){
	case Anone:
		*wire = AXXX;
		*value = 0;
		return 1;
	case Aimm: case Apc:
		*wire = AIMM;
		*value = address->offset;
		return 1;
	case Amp:
		*wire = AMP;
		*value = address->reg;
		return 1;
	case Afp:
		if(address->reg < STemp || address->reg > ScalarObjectLimit-ScalarNativePrefix)
			return 0;
		*wire = AFP;
		if(address->reg < ScalarLegacyPrefix)
			*value = scratch+address->reg-ScalarScratchStart;
		else
			*value = address->reg+ScalarNativePrefix-ScalarLegacyPrefix;
		return 1;
	}
	return 0;
}


/* Convert one remapped instruction while retaining scalar widths and reserved-frame boundaries. */
int
dis64scalarinst(Inst *input, unsigned int count, unsigned int data, unsigned int frame, unsigned int scratch, Dis64Instruction *output)
{
	unsigned int source, middle, destination;

	memset(output, 0, sizeof(*output));
	if(!dis64address(input->sm, &input->s, scratch, &output->source, &source) ||
	   !dis64address(input->mm, &input->m, scratch, &output->middle, &middle) ||
	   !dis64address(input->dm, &input->d, scratch, &output->destination, &destination))
		return 0;
	output->op = input->op == IRET ? IEXIT : input->op;
	output->mode = SRC(source)|DST(destination);
	switch(middle){
	case AXXX: break;
	case AIMM:
		if(output->middle < -32768 || output->middle > 32767)
			return 0;
		/* The runtime's middle immediate is WORD-sized, never a complete LONG. */
		switch(output->op){
		case IADDL: case ISUBL: case IMULL: case IDIVL: case IMODL:
		case IANDL: case IORL: case IXORL: case ISHLL: case ISHRL:
		case IBEQL: case IBNEL: case IBLTL: case IBLEL: case IBGTL: case IBGEL:
			return 0;
		}
		output->mode |= AXIMM;
		break;
	case AFP: case AMP:
		if(output->middle < 0 || output->middle > 65535)
			return 0;
		output->mode |= middle == AFP ? AXINF : AXINM;
		break;
	default: return 0;
	}
	return dis64instructioncheck(output, count, data, frame);
}




/* Accept only inaccessible compiler-created globals, never user aggregates or string tables. */
int
dis64scalartable(Decl *decl)
{
	if(decl == nil || decl->store != Dglobal || decl->ty == nil || decl->sym == nil || decl->sym->name == nil ||
	   decl->sym->name[0] != '.' || decl->init == nil || decl->init->op != Oname ||
	   decl->init->decl != decl || decl->ty->cse == nil)
		return 0;
	return decl->ty->kind == Tcase || decl->ty->kind == Tcasel || decl->ty->kind == Tgoto;
}


/* Validate all borrowed numeric bounds and target identities before allocating emitted instructions. */
Case*
dis64scalarcase(Inst *input, unsigned int count)
{
	Case *table;
	Label *label;
	int i, kind;

	if(input->op != ICASE && input->op != ICASEL && input->op != IGOTO)
		return nil;
	kind = input->op == ICASE ? Tcase : input->op == ICASEL ? Tcasel : Tgoto;
	if(input->dm != Amp || input->mm != Anone || !dis64scalartable(input->d.decl) ||
	   input->d.decl->ty->kind != kind || (input->sm != Afp && input->sm != Amp) ||
	   input->d.reg != input->d.decl->offset+(input->op == IGOTO ? IBY2WD : 0))
		return nil;
	table = input->d.decl->ty->cse;
	if(table->nlab < 0 || table->nlab > ScalarCaseLabels ||
	   (table->nlab != 0 && table->labs == nil) || table->iwild == nil ||
	   table->iwild->pc < 0 || table->iwild->pc >= count)
		return nil;
	for(i = 0; i < table->nlab; i++){
		label = &table->labs[i];
		if(label->start == nil || label->stop == nil || label->start->op != Oconst ||
		   label->stop->op != Oconst || label->start->val > label->stop->val ||
		   label->inst == nil || label->inst->pc < 0 || label->inst->pc >= count)
			return nil;
		if(input->op != ICASEL && (label->start->val < -2147483647LL-1 || label->stop->val > 2147483647LL))
			return nil;
		if(input->op == IGOTO && (label->start->val != label->stop->val ||
		   (i != 0 && label->start->val != table->labs[i-1].start->val+1)))
			return nil;
	}
	return table;
}


/* Append a canonical scalar bound in a middle-addressable cell, preserving the original MP region. */
static int
dis64bound(Dis64Lower *lower, Long value, int big)
{
	Dis64Initializer *record;
	unsigned int width, offset;
	uvlong bits;

	width = big ? 8 : 4;
	offset = (lower->databytes+width-1)&~(width-1);
	if(lower->nconstants == Dis64PilotInitializers || offset > 65535 || offset > ScalarObjectLimit-width)
		return 0;
	record = &lower->constants[lower->nconstants++];
	record->offset = offset;
	record->kind = big ? Dis64Big : Dis64Int;
	bits = value;
	record->high = big ? bits>>32 : 0;
	record->low = (unsigned int)bits;
	lower->databytes = offset+width;
	return 1;
}


/* Construct one comparison using an original key and an initialized MP bound, with no borrowed jump pointers. */
static int
dis64casebranch(Inst *input, unsigned int op, unsigned int bound, unsigned int target,
	unsigned int frame, unsigned int scratch, Dis64Lower *lower, unsigned int at)
{
	Dis64Instruction *output;
	unsigned int source;

	output = &lower->code[at];
	memset(output, 0, sizeof(*output));
	if(!dis64address(input->sm, &input->s, scratch, &output->source, &source))
		return 0;
	output->op = op;
	output->mode = SRC(source)|DST(AIMM)|AXINM;
	output->middle = bound;
	output->destination = target;
	lower->origin[at] = input;
	return dis64instructioncheck(output, lower->count, lower->databytes, frame);
}


/* Emit the final default jump after all small-case comparisons, preserving absolute Dis-PC units. */
static int
dis64casejump(Inst *input, unsigned int target, unsigned int frame, Dis64Lower *lower, unsigned int at)
{
	Dis64Instruction *output;

	output = &lower->code[at];
	memset(output, 0, sizeof(*output));
	output->op = IJMP;
	output->mode = SRC(AXXX)|DST(AIMM);
	output->destination = target;
	lower->origin[at] = input;
	return dis64instructioncheck(output, lower->count, lower->databytes, frame);
}


/* Plan all PCs and bound cells first, then translate without changing any original IR, descriptor or table. */
int
dis64lower(Inst *instructions, unsigned int count, unsigned int data,
	unsigned int frame, unsigned int scratch, Dis64Lower *lower)
{
	Inst *input, copy;
	Case *table;
	unsigned int n, at, amount, i, bound, low, high, target;
	int iscase, big;

	lower->databytes = data;
	lower->pcs = allocmem(count*sizeof(*lower->pcs));
	lower->constants = allocmem(Dis64PilotInitializers*sizeof(*lower->constants));
	n = 0;
	for(input = instructions; input != nil; input = input->next){
		if(input->op == INOOP)
			continue;
		if(n >= count || input->pc != n)
			return 0;
		lower->pcs[n++] = lower->count;
		amount = 1;
		iscase = input->op == ICASE || input->op == ICASEL || input->op == IGOTO;
		if(iscase){
			table = dis64scalarcase(input, count);
			if(table == nil)
				return 0;
			amount += table->nlab*(input->op == IGOTO ? 1 : 2);
			for(i = 0; i < table->nlab; i++){
				big = input->op == ICASEL;
				if(!dis64bound(lower, input->op == IGOTO ? i : table->labs[i].start->val, big) ||
				   (input->op != IGOTO && !dis64bound(lower, table->labs[i].stop->val, big)))
					return 0;
			}
		}
		if(amount > ScalarCodeLimit-lower->count)
			return 0;
		lower->count += amount;
	}
	if(n != count)
		return 0;
	lower->code = allocmem(lower->count*sizeof(*lower->code));
	lower->origin = allocmem(lower->count*sizeof(*lower->origin));
	at = bound = 0;
	for(input = instructions; input != nil; input = input->next){
		if(input->op == INOOP)
			continue;
		iscase = input->op == ICASE || input->op == ICASEL || input->op == IGOTO;
		if(!iscase){
			copy = *input;
			if(copy.dm == Apc){
				if(copy.d.offset < 0 || copy.d.offset >= count)
					return 0;
				copy.d.offset = lower->pcs[copy.d.offset];
			}
			if(!dis64scalarinst(&copy, lower->count, lower->databytes, frame, scratch, &lower->code[at]))
				return 0;
			lower->origin[at++] = input;
			continue;
		}
		table = input->d.decl->ty->cse;
		for(i = 0; i < table->nlab; i++){
			low = lower->constants[bound++].offset;
			target = lower->pcs[table->labs[i].inst->pc];
			if(input->op == IGOTO){
				if(!dis64casebranch(input, IBEQW, low, target, frame, scratch, lower, at++))
					return 0;
			}else{
				high = lower->constants[bound++].offset;
				/* Label.stop is inclusive; never manufacture high+1 at signed maxima. */
				if(!dis64casebranch(input, input->op == ICASEL ? IBLTL : IBLTW, low, at+2, frame, scratch, lower, at))
					return 0;
				at++;
				if(!dis64casebranch(input, input->op == ICASEL ? IBLEL : IBLEW, high, target, frame, scratch, lower, at++))
					return 0;
			}
		}
		if(!dis64casejump(input, lower->pcs[table->iwild->pc], frame, lower, at++))
			return 0;
	}
	return at == lower->count && bound == lower->nconstants;
}


/* Free private arrays on either admission or refusal; compiler IR remains borrowed and immutable. */
void
dis64lowerfree(Dis64Lower *lower)
{
	free(lower->code);
	free(lower->origin);
	free(lower->pcs);
	free(lower->constants);
	memset(lower, 0, sizeof(*lower));
}
