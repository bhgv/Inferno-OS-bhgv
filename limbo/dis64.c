#include "limbo.h"
#include "dis64-lower.h"
#include "dis64-inline.h"
#include "dis64-signature.h"
#include "dis64-numeric-storage.h"
#include "dis64-managed-trial.h"

#define ScalarOutputLimit (8U*1024U*1024U) // Match the complete-image admission bound.


/* Keep diagnostics in the ordinary compiler error and output-cleanup path. */
static void
dis64error(Decl *entry, char *message)
{
	error(entry == nil ? impdecl->src.start : entry->src.start, "Dis64 scalar: %s", message);
}


/* Pointer-free source scalars retain their existing WORD32/eight-byte storage geometry. */
static int
dis64scalarlist(Decl *decl, int tables)
{
	for(; decl != nil; decl = decl->next){
		if(dis64inlineshape(decl->ty) != 0 && (!tables || decl->ty->kind != Ttuple || decl->init == nil))
			continue;
		if(tables && dis64scalartable(decl))
			continue;
		return 0;
	}
	return 1;
}


/* A zero legacy map is necessary; no pointer descriptor is widened by inference. */
static int
dis64nomap(Desc *desc)
{
	long i;

	if(desc == nil || desc->nmap < 0 || (desc->nmap != 0 && desc->map == nil))
		return 0;
	for(i=0; i<desc->nmap; i++)
		if(desc->map[i] != 0)
			return 0;
	return 1;
}


/* Stage shortest signed30 operands; any refusal discards the complete private image. */
static int
dis64encode(uchar *output, unsigned int *at, const Dis64Instruction *instruction)
{
	long values[3];
	unsigned int count, i, used;

	count = 0;
	if((instruction->mode&ARM) != AXNON)
		values[count++] = instruction->middle;
	if(USRC(instruction->mode) != AXXX)
		values[count++] = instruction->source;
	if(UDST(instruction->mode) != AXXX)
		values[count++] = instruction->destination;
	output[(*at)++] = instruction->op;
	output[(*at)++] = instruction->mode;
	for(i=0; i<count; i++){
		used = disoperand(output+*at, 4, values[i]);
		if(used == 0)
			return 0;
		*at += used;
	}
	return 1;
}


/* Escape a source name as JSON without treating its contents as a format string. */
static void
dis64tracequote(char *text)
{
	uchar *p;

	fprint(2, "\"");
	for(p = (uchar*)text; *p != 0; p++){
		if(*p == '"' || *p == '\\')
			fprint(2, "\\%c", *p);
		else if(*p < 32)
			fprint(2, "\\u%04x", *p);
		else
			write(2, p, 1);
	}
	fprint(2, "\"");
}


/* Preserve one compiler cursor, including an explicitly unknown source location. */
static void
dis64tracepoint(Line *point)
{
	Fline location;

	if(point->line <= 0){
		fprint(2, "null");
		return;
	}
	location = fline(point->line);
	fprint(2, "{\"file\":");
	dis64tracequote(location.file->name);
	fprint(2, ",\"line\":%d,\"column\":%d}", location.line, point->pos);
}


/* Export optional compiler diagnostics only after the complete scalar image is admitted. */
static void
dis64trace(Dis64Lower *lower)
{
	Inst *inst;
	unsigned int pc;
	char *enabled;

	enabled = getenv("LIMBO_DIS64_TRACE");
	if(enabled == nil || strcmp(enabled, "1") != 0)
		return;
	for(pc = 0; pc < lower->count; pc++){
		inst = lower->origin[pc];
		fprint(2, "LIMBO64LOC {\"pc\":%ud,\"op\":%ud,\"start\":", pc, lower->code[pc].op);
		dis64tracepoint(&inst->src.start);
		fprint(2, ",\"stop\":");
		dis64tracepoint(&inst->src.stop);
		fprint(2, "}\n");
	}
}


typedef struct Dis64ClockCall Dis64ClockCall;
struct Dis64ClockCall {
	unsigned int binding;
	uchar modes[2];
	long values[2];
	int save; // Native destination of a persistent returned value, or zero.
};

typedef struct Dis64ClockWord Dis64ClockWord;
struct Dis64ClockWord {
	unsigned int before; // Number of completed calls before this scalar step.
	Dis64Instruction instruction;
};

typedef struct Dis64ClockPlan Dis64ClockPlan;
struct Dis64ClockPlan {
	Dis64ClockCall calls[Dis64ClockCalls];
	uchar arguments[3];
	unsigned int count;
	Dis64ClockWord words[Dis64ClockWords];
	unsigned int nwords;
};


/* Legacy scalar slots are remapped only after the complete caller body matches. */
static int
dis64clockslot(long offset, long bytes)
{
	return offset >= ScalarLegacyPrefix && offset%IBY2WD == 0 && offset <= bytes-IBY2WD;
}


/* Persistent cells are assigned only after a defining instruction is checked. */
static long
dis64clockcell(long *locals, long legacy)
{
	unsigned int i;

	for(i=0; i<Dis64ClockLocals; i++)
		if(locals[i] >= 0 && locals[i] == legacy)
			return i == 2 ? 76 : 64+4*i;
	return 0;
}


/* Resolve initialized scalar values independently of legacy register placement. */
static int
dis64clockvalue(int mode, Addr *address, long *locals, long previous, Addr *mapped)
{
	long cell;

	if(mode == Aimm){
		if(address->offset < -536870912L || address->offset > 536870911L)
			return 0;
		mapped->offset = address->offset;
		return 1;
	}
	if(mode != Afp)
		return 0;
	if(previous >= 0 && address->reg == previous)
		cell = 72;
	else if((cell = dis64clockcell(locals, address->reg)) == 0)
		return 0;
	mapped->reg = cell-32; // Existing scalar lowering rebases legacy FP by 32.
	return 1;
}


/* Reuse scalar lowering after resolving the initialized integer identities. */
static int
dis64clockword(Inst *input, long module, long bytes, long *locals, long *previous,
	Dis64Instruction *output)
{
	Inst mapped;
	long destination;
	unsigned int i;
	int allocate;

	if(!dis64clockwordop(input->op) || input->dm != Afp ||
	   !dis64clockslot(input->d.reg, bytes) || input->d.reg == module)
		return 0;
	destination = dis64clockcell(locals, input->d.reg);
	if(destination == 0 && input->d.reg == *previous)
		destination = 72;
	if(input->op != IMOVW && input->mm == Anone && destination == 0)
		return 0;
	allocate = -1;
	if(destination == 0){
		for(i=0; i<Dis64ClockLocals; i++)
			if(locals[i] < 0){
				allocate = i;
				destination = i == 2 ? 76 : 64+4*i;
				break;
			}
		if(allocate < 0)
			return 0;
	}
	mapped = *input;
	if(!dis64clockvalue(input->sm, &input->s, locals, *previous, &mapped.s))
		return 0;
	if(input->mm != Anone &&
	   !dis64clockvalue(input->mm, &input->m, locals, *previous, &mapped.m))
		return 0;
	mapped.d.reg = destination-32;
	if(!dis64scalarinst(&mapped, 1, 0, 80, 64, output))
		return 0;
	if(allocate >= 0)
		locals[allocate] = input->d.reg;
	if(destination != 72 && *previous == input->d.reg)
		*previous = -1;
	return 1;
}


/* Match the complete lowered init body before changing its frame geometry. */
static int
dis64clockshape(Decl *entry, Decl *globals, Decl *ldts, Inst *input, long count, long databytes,
	Dis64ClockPlan *output)
{
	Decl *import, *export, *path, *argument, *target;
	unsigned int i, j, phase, binding, used, parameter, seen, copies;
	Dis64ClockPlan plan = {0};
	long frame, module, result, previous, locals[Dis64ClockLocals], cell;
	int kind;

	if(entry == nil || entry->pc == nil || entry->pc->pc != 0 ||
	   entry->ty->ids != nil || entry->ty->varargs || entry->ty->tof->kind != Tnone ||
	   count < 5 || count > 5*Dis64ClockCalls+2+Dis64ClockWords || databytes != 8 ||
	   entry->desc == nil || entry->desc->size < ScalarLegacyPrefix+IBY2WD ||
	   entry->desc->size > ScalarObjectLimit ||
	   ldts == nil || ldts->next != nil || ldts->init == nil ||
	   ldts->init->decl == nil)
		return 0;
	path = nil;
	for(; globals != nil; globals=globals->next){
		if(globals->store != Dglobal || globals->init == nil)
			continue;
		if(path != nil || globals->offset != 0 || globals->ty->kind != Tstring ||
		   globals->init->decl == nil || strcmp(globals->init->decl->sym->name, "$Sys") != 0)
			return 0;
		path = globals;
	}
	if(path == nil)
		return 0;
	import = ldts->init->decl->ty->ids;
	if(ldts->init->val < 1 || ldts->init->val > 3)
		return 0;
	seen = 0;
	for(export=import, i=0; export != nil; export=export->next, i++){
		if(i >= 3 || export->ty->kind != Tfn || export->ty->varargs ||
		   export->ty->polys != nil || export->ty->tof->kind != Tint)
			return 0;
		kind = strcmp(export->sym->name, "millisec") == 0 ? 0 :
			strcmp(export->sym->name, "sleep") == 0 ? 1 :
			strcmp(export->sym->name, "dup") == 0 ? 2 : -1;
		if(kind < 0 || (seen & (1U<<kind)))
			return 0;
		seen |= 1U<<kind;
		plan.arguments[i] = kind;
		for(argument=export->ty->ids, j=0; argument != nil; argument=argument->next, j++)
			if(j >= plan.arguments[i] || argument->ty->kind != Tint)
				return 0;
		if(j != plan.arguments[i])
			return 0;
	}
	if(i != ldts->init->val)
		return 0;
	for(export=impdecl->ty->tof->ids; export != nil; export=export->next)
		if(export->store == Dglobal || (export->store == Dfn && export->sym != entry->sym))
			return 0;
	i = phase = binding = used = copies = 0;
	frame = module = result = 0;
	previous = -1;
	for(j=0; j<Dis64ClockLocals; j++)
		locals[j] = -1;
	target = nil;
	parameter = 0;
	for(; input != nil; input=input->next){
		if(input->op == INOOP)
			continue;
		if(i >= count)
			return -1;
		if(i == 0){
			if(input->op != ILOAD || input->sm != Amp || input->s.reg != 0 ||
			   input->mm != Aldt || input->m.reg != 0 || input->dm != Afp ||
			   !dis64clockslot(input->d.reg, entry->desc->size))
				return -1;
			module = input->d.reg;
		}else if(i == count-1){
			if(phase != 0 || used == 0 || input->op != IRET ||
			   input->sm != Anone || input->mm != Anone || input->dm != Anone)
				return -1;
		}else switch(phase){
		case 0:
			if(dis64clockwordop(input->op)){
				if(plan.nwords == Dis64ClockWords ||
				   !dis64clockword(input, module, entry->desc->size, locals, &previous,
				     &plan.words[plan.nwords].instruction))
					return -1;
				plan.words[plan.nwords++].before = used;
				break;
			}
			if(input->op != IMFRAME || input->sm != Afp || input->s.reg != module ||
			   input->dm != Afp || !dis64clockslot(input->d.reg, entry->desc->size) ||
			   input->d.reg == module || dis64clockcell(locals, input->d.reg) != 0 ||
			   input->mm != Aoff || input->m.decl == nil)
				return -1;
			if(used == Dis64ClockCalls)
				return -1;
			for(target=import, binding=0; target != nil && target != input->m.decl->iface;
			    target=target->next, binding++)
				;
			if(target == nil || target->offset != binding)
				return -1;
			parameter = 0;
			frame = input->d.reg;
			phase = plan.arguments[binding] ? 3 : 1;
			break;
		case 3:
			if(input->op != IMOVW || input->mm != Anone || input->dm != Afpind ||
			   input->d.reg != frame || input->d.offset != 32+4*parameter)
				return -1;
			if(input->sm == Aimm){
				if(input->s.offset < -536870912L || input->s.offset > 536870911L)
					return -1;
				plan.calls[used].modes[parameter] = AIMM;
				plan.calls[used].values[parameter] = input->s.offset;
			}else if(input->sm == Afp && input->s.reg != frame && input->s.reg != module){
				plan.calls[used].modes[parameter] = AFP;
				if(used != 0 && input->s.reg == previous)
					plan.calls[used].values[parameter] = 72;
				else if((cell = dis64clockcell(locals, input->s.reg)) != 0)
					plan.calls[used].values[parameter] = cell;
				else
					return -1;
			}else
				return -1;
			if(++parameter == plan.arguments[binding])
				phase = 1;
			break;
		case 1:
			if(input->op != ILEA || input->sm != Afp || !dis64clockslot(input->s.reg, entry->desc->size) ||
			   input->s.reg == module || input->s.reg == frame ||
			   input->mm != Anone || input->dm != Afpind ||
			   input->d.reg != frame || input->d.offset != 16)
				return -1;
			result = input->s.reg;
			phase = 2;
			break;
		case 2:
			if(input->op != IMCALL || input->sm != Afp || input->s.reg != frame ||
			   input->dm != Afp || input->d.reg != module || input->mm != Aoff ||
			   input->m.decl == nil || input->m.decl->iface != target)
				return -1;
			plan.calls[used].binding = binding;
			plan.calls[used++].save = dis64clockcell(locals, result);
			if(dis64clockcell(locals, result) != 0)
				copies++;
			previous = result;
			phase = 0;
			break;
		}
		i++;
	}
	if(i != count || plan.nwords+copies > Dis64ClockWords)
		return -1;
	plan.count = used;
	*output = plan;
	return ldts->init->val;
}


/* Preserve one checked immediate or initialized scalar slot in the prepared native frame. */
static int
dis64clockargument(uchar *code, unsigned int *at, unsigned int mode, long value, unsigned int ordinal)
{
	unsigned int used;

	code[(*at)++] = IMOVW;
	code[(*at)++] = SRC(mode)|DST(AFP|AIND);
	used = disoperand(code+*at, 4, value);
	if(used == 0)
		return 0;
	*at += used;
	code[(*at)++] = 40;
	used = disoperand(code+*at, 4, 64+4*ordinal);
	if(used == 0)
		return 0;
	*at += used;
	return 1;
}


/* Encode the checked imported clock pilot with the existing Dis64 sections. */
int
dis64clock(Decl *entry, Decl *globals, Decl *ldts, Inst *input, long count, long databytes)
{
	static const uchar load[] = {ILOAD, SRC(AMP)|AXIMM|DST(AMP), 0, 0, 8};
	static const uchar frame[] = {IMFRAME, SRC(AMP)|AXIMM|DST(AFP), 0, 8, 40};
	static const uchar result[] = {ILEA, SRC(AFP)|DST(AFP|AIND), 0x80, 72, 40, 32};
	static const uchar call[] = {IMCALL, SRC(AFP)|AXIMM|DST(AMP), 0, 40, 8};
	static const uchar exit[] = {IEXIT, SRC(AXXX)|DST(AXXX)};
	static const uchar datamap[] = {0xc0}, framemap[] = {0, 0};
	Dis64Type types[] = {{16, 8, datamap, 1}, {80, 8, framemap, 2}};
	Dis64ModuleInfo module = {0};
	DisStringPoolRecord names[5];
	Dis64Payload sections[5];
	uchar metadata[24], code[4+sizeof(load)+Dis64ClockCalls*(sizeof(frame)+18+sizeof(result)+sizeof(call))+Dis64ClockWords*14+sizeof(exit)];
	uchar typedata[32], imports[100], runes[5][4+4*255], strings[4+sizeof(runes)], output[4096];
	Dis64ClockPlan plan;
	Dis64Instruction copy = {IMOVW, SRC(AFP)|DST(AFP), 0, 72, 64};
	unsigned int sizes[5], i, j, length, written, bindings, instructions, ordinal, step, parameters[] = {Dis64Int, Dis64Int};
	const char *labels[] = {"$Sys", nil, impdecl->sym->name, nil, nil};
	Decl *import;
	int matched;

	matched = dis64clockshape(entry, globals, ldts, input, count, databytes, &plan);
	if(matched <= 0){
		if(matched < 0)
			dis64error(entry, "unsupported checked Sys body or uninitialized scalar argument");
		return matched < 0;
	}
	bindings = matched;
	for(import=ldts->init->decl->ty->ids, i=0; import != nil; import=import->next, i++)
		labels[i == 0 ? 1 : i+2] = import->sym->name;
	for(i=0; i<2+bindings; i++){
		length = strlen(labels[i]);
		if(length == 0 || length > 255)
			return 0;
		dis64put32(runes[i], length);
		for(j=0; j<length; j++){
			if((uchar)labels[i][j] > 127)
				return 0;
			dis64put32(runes[i]+4+4*j, (uchar)labels[i][j]);
		}
		names[i].data = runes[i];
		names[i].bytes = 4+4*length;
	}
	module.profile = 1;
	module.name = 2;
	module.databytes = 16;
	module.frametype = 1;
	instructions = 2;
	sizes[1] = 4+sizeof(load);
	memmove(code+4, load, sizeof(load));
	step = 0;
	for(i=0; i<=plan.count; i++){
		while(step < plan.nwords && plan.words[step].before == i){
			if(!dis64encode(code, &sizes[1], &plan.words[step++].instruction)){
				dis64error(entry, "cannot lower checked Sys scalar expression");
				return 1;
			}
			instructions++;
		}
		if(i == plan.count)
			break;
		ordinal = plan.calls[i].binding;
		memmove(code+sizes[1], frame, sizeof(frame));
		code[sizes[1]+2] = ordinal;
		sizes[1] += sizeof(frame);
		for(j=0; j<plan.arguments[ordinal]; j++)
			if(!dis64clockargument(code, &sizes[1], plan.calls[i].modes[j], plan.calls[i].values[j], j)){
				dis64error(entry, "cannot encode checked Sys argument");
				return 1;
			}
		memmove(code+sizes[1], result, sizeof(result));
		sizes[1] += sizeof(result);
		memmove(code+sizes[1], call, sizeof(call));
		code[sizes[1]+2] = ordinal;
		sizes[1] += sizeof(call);
		instructions += 3+plan.arguments[ordinal];
		if(plan.calls[i].save){
			copy.destination = plan.calls[i].save;
			if(!dis64encode(code, &sizes[1], &copy)){
				dis64error(entry, "cannot preserve checked Sys local");
				return 1;
			}
			instructions++;
		}
	}
	memmove(code+sizes[1], exit, sizeof(exit));
	sizes[1] += sizeof(exit);
	dis64put32(code, instructions);
	dis64put32(imports, bindings);
	sizes[3] = 4;
	for(i=0; i<bindings; i++){
		length = 12+4*plan.arguments[i];
		dis64put32(imports+sizes[3], 0);
		dis64put32(imports+sizes[3]+4, i == 0 ? 1 : i+2);
		dis64put32(imports+sizes[3]+8, length);
		if(!dis64signaturewrite(imports+sizes[3]+12, length, Dis64Int,
		   parameters, plan.arguments[i], &written)){
			dis64error(entry, "cannot encode checked Sys import");
			return 1;
		}
		sizes[3] += 12+length;
	}
	if(!dis64modulewrite(metadata, sizeof(metadata), &module, &sizes[0]) ||
	   !dis64typeswrite(typedata, sizeof(typedata), types, nelem(types), &sizes[2]) ||
	   !disstringpoolwrite(strings, sizeof(strings), names, 2+bindings, &sizes[4])){
		dis64error(entry, "cannot encode checked Sys imports");
		return 1;
	}
	sections[0] = (Dis64Payload){Dis64Module, Dis64Required, metadata, sizes[0]};
	sections[1] = (Dis64Payload){Dis64Code, Dis64Required, code, sizes[1]};
	sections[2] = (Dis64Payload){Dis64Types, Dis64Required, typedata, sizes[2]};
	sections[3] = (Dis64Payload){Dis64Imports, Dis64Required, imports, sizes[3]};
	sections[4] = (Dis64Payload){Dis64Strings, Dis64Required, strings, sizes[4]};
	if(!dis64write(output, sizeof(output), nil, 0, sections, nelem(sections), &written) ||
	   Bwrite(bout, output, written) != written)
		dis64error(entry, "cannot write checked clock image");
	return 1;
}


/* Share exact scalar initializer widths between executable and library output. */
static int
dis64globalinitializer(Decl *decl, Dis64Initializer *record)
{
	ulong real[2];
	uvlong bits;

	if(decl->init->op != Oconst || decl->offset < 0 ||
	   decl->init->ty->kind != decl->ty->kind)
		return 0;
	record->offset = decl->offset;
	bits = decl->init->val;
	switch(decl->ty->kind){
	case Tint:
		record->kind = Dis64Int;
		record->high = 0;
		record->low = bits;
		break;
	case Tbig:
		record->kind = Dis64Big;
		record->high = bits>>32;
		record->low = bits;
		break;
	case Treal:
		record->kind = Dis64Real;
		dtocanon(decl->init->rval, real);
		record->high = real[0];
		record->low = real[1];
		break;
	default:
		return 0;
	}
	return 1;
}


#define ScalarLibraryExports 32U // Maximum exported procedures in the bounded library producer.


/* Encode one ASCII name as a borrowed Rune32 string-pool record. */
static int
dis64libraryname(const char *name, uchar *bytes, DisStringPoolRecord *record)
{
	unsigned int i, length;

	length = strlen(name);
	if(length == 0 || length > 255)
		return 0;
	dis64put32(bytes, length);
	for(i=0; i<length; i++){
		if((uchar)name[i] > 127)
			return 0;
		dis64put32(bytes+4+4*i, (uchar)name[i]);
	}
	record->data = bytes;
	record->bytes = 4+4*length;
	return 1;
}


/* Return the supported symbolic direction for one exact Limbo signature. */
static int
dis64manageddirection(Decl *function)
{
	Decl *argument;
	Type *result;
	int quoted;

	if(function == nil || function->ty->kind != Tfn || function->ty->varargs ||
	   function->ty->polys != nil)
		return -1;
	quoted = strcmp(function->sym->name, "quoted") == 0;
	if(!quoted && strcmp(function->sym->name, "unquoted") != 0)
		return -1;
	argument = function->ty->ids;
	result = function->ty->tof;
	if(argument == nil || argument->next != nil ||
	   (quoted ? (argument->ty->kind != Tlist || argument->ty->tof->kind != Tstring ||
	    result->kind != Tstring) : (argument->ty->kind != Tstring || result->kind != Tlist ||
	    result->tof->kind != Tstring)))
		return -1;
	return quoted;
}

/* Check the exact legacy lowering before fusing its temporary list into the result. */
static int
dis64managedlibraryshape(Decl *entry, Decl *globals, Inst *input, long count, long databytes,
	int *quoted)
{
	static const ushort ops[][5] = {{IMOVP, ICONSP, IMOVP, IMOVP, IRET}, {IHEADP, IRET}};
	static const uchar sources[][5] = {{Amp, Afp, Afp, Amp, Anone}, {Afp, Anone}};
	static const uchar destinations[][5] = {{Afp, Afp, Afpind, Afp, Anone}, {Afpind, Anone}};
	static const long source[][5] = {{0, 32, 36, 0, 0}, {32, 0}};
	static const long destination[][5] = {{36, 36, 16, 36, 0}, {16, 0}};
	Decl *export, *function;
	int direction, i;

	if(entry != nil || strcmp(impdecl->sym->name, "String") != 0)
		return 0;
	function = nil;
	for(export=impdecl->ty->tof->ids; export != nil; export=export->next){
		if(export->store == Dglobal)
			return 0;
		if(export->store != Dfn)
			continue;
		if(function != nil)
			return 0;
		function = export;
	}
	direction = dis64manageddirection(function);
	if(direction < 0 || function->pc == nil || function->pc->pc != 0)
		return 0;
	if(direction ? (databytes != 0 || count != 2) : (databytes != 4 || count != 5))
		return 0;
	for(; globals != nil; globals=globals->next)
		if(globals->store == Dglobal &&
		   (direction || globals->offset != 0 || globals->init != nil ||
		    globals->ty->kind != Tany || strcmp(globals->sym->name, "nil") != 0))
			return 0;
	i = 0;
	for(; input != nil; input=input->next){
		if(input->op == INOOP)
			continue;
		if(i >= count || input->op != ops[direction][i] || input->mm != Anone ||
		   input->sm != sources[direction][i] || input->dm != destinations[direction][i] ||
		   (input->sm != Anone && input->s.reg != source[direction][i]) ||
		   (input->dm != Anone && input->d.reg != destination[direction][i]) ||
		   (input->dm == Afpind && input->d.offset != 0))
			return 0;
		i++;
	}
	if(i != count)
		return 0;
	*quoted = direction;
	return 1;
}

/* Check the legacy LOAD/call sequence before replacing LOAD with an explicit pair. */
static int
dis64managedcallershape(Decl *entry, Decl *globals, Decl *ldts, Inst *input,
	long count, long databytes, int *quoted)
{
	static const ushort ops[] = {ILOAD, IMFRAME, IMOVP, ILEA, IMCALL, IRET};
	static const uchar sm[] = {Amp, Afp, Afp, Afp, Afp, Anone};
	static const uchar dm[] = {Afp, Afp, Afpind, Afpind, Afp, Anone};
	static const uchar mm[] = {Aldt, Aoff, Anone, Anone, Aoff, Anone};
	static const long source[] = {0, 36, 32, 44, 40};
	static const long destination[] = {36, 40, 40, 40, 36};
	Decl *import, *export, *path, *argument;
	int direction;
	unsigned int i;

	if(entry == nil || entry->pc == nil || entry->pc->pc != 0 ||
	   entry->ty->varargs || entry->ty->tof->kind != Tnone || count != nelem(ops) ||
	   databytes != 4 || ldts == nil || ldts->next != nil || ldts->init == nil ||
	   ldts->init->val != 1 || ldts->init->decl == nil)
		return 0;
	import = ldts->init->decl->ty->ids;
	direction = dis64manageddirection(import);
	if(direction < 0 || import->next != nil || import->offset != 0)
		return 0;
	argument = entry->ty->ids;
	if(argument == nil || argument->next != nil ||
	   (direction ? (argument->ty->kind != Tlist || argument->ty->tof->kind != Tstring) :
	    argument->ty->kind != Tstring))
		return 0;
	for(export=impdecl->ty->tof->ids; export != nil; export=export->next)
		if(export->store == Dglobal || (export->store == Dfn && export->sym != entry->sym))
			return 0;
	path = nil;
	for(; globals != nil; globals=globals->next){
		if(globals->store != Dglobal)
			continue;
		if(path != nil || globals->offset != 0 || globals->ty->kind != Tstring ||
		   globals->init == nil || globals->init->op != Oconst || globals->init->decl == nil ||
		   strcmp(globals->init->decl->sym->name, "/dis/lib/string.dis") != 0)
			return 0;
		path = globals;
	}
	if(path == nil)
		return 0;
	i = 0;
	for(; input != nil; input=input->next){
		if(input->op == INOOP)
			continue;
		if(i >= nelem(ops) || input->op != ops[i] || input->sm != sm[i] ||
		   input->dm != dm[i] || input->mm != mm[i] ||
		   (i < nelem(source) && (input->s.reg != source[i] || input->d.reg != destination[i])))
			return 0;
		if((i == 0 && input->m.reg != 0) ||
		   (i == 2 && input->d.offset != 32) || (i == 3 && input->d.offset != 16) ||
		   ((i == 1 || i == 4) && (input->m.decl == nil || input->m.decl->iface != import)))
			return 0;
		i++;
	}
	if(i != nelem(ops))
		return 0;
	*quoted = direction;
	return 1;
}

void
dis64managed(Decl *entry, Decl *globals, Decl *ldts, Inst *input, long count, long databytes)
{
	static const uchar map[] = {0, 0x80}, callermap[] = {0, 0xc0}, mpmap[] = {0x80};
	static const uchar caller[] = {0,0,0,5,
		IMFRAME,SRC(AMP)|AXIMM|DST(AFP),0,0,40,
		IMOVP,SRC(AFP)|DST(AFP|AIND),0x80,64,40,0x80,64,
		ILEA,SRC(AFP)|DST(AFP|AIND),0x80,72,40,32,
		IMCALL,SRC(AFP)|AXIMM|DST(AMP),0,40,0,
		IEXIT,SRC(AXXX)|DST(AXXX)};
	uchar provider[] = {0,0,0,3, IEXIT,SRC(AXXX)|DST(AXXX),
		ICONSP,SRC(AFP)|DST(AFP|AIND),0x80,64,32,0, IRET,SRC(AXXX)|DST(AXXX)};
	Dis64Type types[] = {{72, 8, map, sizeof(map)}, {8, 8, mpmap, sizeof(mpmap)}};
	Dis64ModuleInfo module = {0};
	DisStringPoolRecord names[3];
	Dis64Payload sections[5];
	uchar metadata[24], typedata[32], pool[1100], name[28], symbol[36], callername[4+4*255];
	uchar binding[40], output[2048];
	unsigned int sizes[5], written, header;
	int quoted, consumer;

	consumer = entry != nil;
	if(!(consumer ? dis64managedcallershape(entry, globals, ldts, input, count, databytes, &quoted) :
	     ldts == nil && dis64managedlibraryshape(entry, globals, input, count, databytes, &quoted))){
		error(impdecl->src.start, "Dis64 managed trial: unsupported one-token String provider or caller");
		return;
	}
	if(consumer){
		module.name = 2;
		module.profile = 1;
		module.databytes = 8;
		module.datatype = 1;
		types[0] = (Dis64Type){80, 8, callermap, sizeof(callermap)};
		if(!dis64libraryname(impdecl->sym->name, callername, &names[2])){
			error(impdecl->src.start, "Dis64 managed trial: unsupported caller name");
			return;
		}
	}
	provider[6] = quoted ? IHEADP : ICONSP;
	dis64put32(binding, 1);
	dis64put32(binding+4, consumer ? 0 : 1);
	dis64put32(binding+8, 1);
	if(!consumer)
		dis64put32(binding+12, 0);
	header = consumer ? 16 : 20;
	dis64put32(binding+header-4, 20);
	dis64put32(binding+header, Dis64TrialVersion);
	dis64put32(binding+header+4, 1);
	dis64put32(binding+header+8, quoted ? Dis64TrialString : Dis64TrialList);
	dis64put32(binding+header+12, quoted ? Dis64TrialList : Dis64TrialString);
	dis64put32(binding+header+16, Dis64TrialString);
	if(!dis64libraryname("String", name, &names[0]) ||
	   !dis64libraryname(quoted ? "quoted" : "unquoted", symbol, &names[1]) ||
	   !dis64modulewrite(metadata, sizeof(metadata), &module, &sizes[0]) ||
	   !dis64typeswrite(typedata, sizeof(typedata), types, 1+consumer, &sizes[2]) ||
	   !disstringpoolwrite(pool, sizeof(pool), names, 2+consumer, &sizes[3])){
		error(impdecl->src.start, "Dis64 managed trial: cannot encode image");
		return;
	}
	sections[0] = (Dis64Payload){Dis64Module, Dis64Required, metadata, sizes[0]};
	sections[1] = (Dis64Payload){Dis64Code, Dis64Required, consumer ? caller : provider,
		consumer ? sizeof(caller) : sizeof(provider)};
	sections[2] = (Dis64Payload){Dis64Types, Dis64Required, typedata, sizes[2]};
	sections[3] = (Dis64Payload){Dis64Strings, Dis64Required, pool, sizes[3]};
	sections[4] = (Dis64Payload){consumer ? Dis64Imports : Dis64Exports, Dis64Required, binding, header+20};
	if(!dis64write(output, sizeof(output), nil, 0, sections, nelem(sections), &written) ||
	   Bwrite(bout, output, written) != written)
		error(impdecl->src.start, "Dis64 managed trial: cannot write image");
}


/* Produce an entryless MP-only library with independently named void exports. */
void
dis64library(Decl *globals, Inst *instructions, long count, long databytes)
{
	Dis64ModuleInfo module = {0};
	Dis64Type types[2];
	Dis64Lower lower = {0};
	Dis64StorageImage checked;
	Dis64CodeView codeview;
	Dis64StorageLimits limits = {64, 65536, ScalarObjectLimit, 65536, ScalarCodeLimit, 1024, 256, 1024};
	Dis64Initializer initializers[Dis64PilotInitializers];
	DisStringPoolRecord names[ScalarLibraryExports+1];
	Dis64Payload sections[6];
	Decl *export, *decl;
	uchar metadata[24], frame[2] = {0, 0}, exports[4+28*ScalarLibraryExports];
	uchar *code, *maps, *typedata, *data, *runes, *strings, *output;
	unsigned int sizes[6], i, n, at, used, ninit, poolbytes, typebytes;
	char *failure;

	code = maps = typedata = data = runes = strings = output = nil;
	failure = "entryless library requires bounded MP-only scalar code and void exports without arguments";
	if(count <= 0 || count > ScalarCodeLimit || databytes < 0 || databytes > ScalarObjectLimit ||
	   !dis64scalarlist(globals, 0) ||
	   !dis64lower(instructions, count, databytes, 80, 64, &lower) ||
	   lower.databytes != databytes || lower.nconstants != 0)
		goto done;
	maps = allocmem((databytes+63)/64+1);
	memset(maps, 0, (databytes+63)/64+1);
	types[0] = (Dis64Type){80, 8, frame, sizeof(frame)};
	types[1] = (Dis64Type){databytes, databytes == 0 ? 1 : 4, maps, (databytes+63)/64};
	code = allocmem(4+14*lower.count);
	dis64put32(code, lower.count);
	at = 4;
	for(i=0; i<lower.count; i++){
		if(lower.origin[i]->op == IRET)
			lower.code[i].op = IRET;
		else if(lower.code[i].op == IEXIT ||
			!dis64numericstorageinstruction(&lower.code[i], lower.count, &types[1], 64))
			goto done;
		if(!dis64encode(code, &at, &lower.code[i]))
			goto done;
	}
	sizes[1] = at;
	runes = allocmem((ScalarLibraryExports+1)*(4+4*255));
	if(!dis64libraryname(impdecl->sym->name, runes, &names[0]))
		goto done;
	n = 0;
	poolbytes = 4+names[0].bytes;
	for(export=impdecl->ty->tof->ids; export != nil; export=export->next){
		if(export->store == Dglobal)
			goto done;
		if(export->store != Dfn)
			continue;
		if(n == ScalarLibraryExports || export->pc == nil || export->pc->pc < 0 ||
		   export->pc->pc >= count || export->ty->ids != nil || export->ty->varargs ||
		   export->ty->tof->kind != Tnone || !dis64nomap(export->desc) ||
		   !dis64libraryname(export->sym->name, runes+(n+1)*(4+4*255), &names[n+1]))
			goto done;
		poolbytes += names[n+1].bytes;
		at = 4+28*n;
		dis64put32(exports+at, n+1);
		dis64put32(exports+at+4, lower.pcs[export->pc->pc]);
		dis64put32(exports+at+8, 0);
		dis64put32(exports+at+12, 12);
		if(!dis64signaturewrite(exports+at+16, 12, Dis64Void, nil, 0, &used))
			goto done;
		n++;
	}
	if(n == 0)
		goto done;
	dis64put32(exports, n);
	sizes[4] = 4+28*n;
	ninit = 0;
	for(decl=globals; decl != nil; decl=decl->next){
		if(decl->store != Dglobal || decl->init == nil)
			continue;
		if(ninit == nelem(initializers) || !dis64globalinitializer(decl, &initializers[ninit]))
			goto done;
		ninit++;
	}
	typebytes = 20+types[0].mapbytes+types[1].mapbytes;
	typedata = allocmem(typebytes);
	data = allocmem(4+16*ninit);
	strings = allocmem(poolbytes);
	module.databytes = databytes;
	module.datatype = 1;
	if(!dis64modulewrite(metadata, sizeof(metadata), &module, &sizes[0]) ||
	   !dis64typeswrite(typedata, typebytes, types, nelem(types), &sizes[2]) ||
	   !dis64initializerswrite(data, 4+16*ninit, &types[1], initializers, ninit, &sizes[3]) ||
	   !disstringpoolwrite(strings, poolbytes, names, n+1, &sizes[5]))
		goto done;
	sections[0] = (Dis64Payload){Dis64Module, Dis64Required, metadata, sizes[0]};
	sections[1] = (Dis64Payload){Dis64Code, Dis64Required, code, sizes[1]};
	sections[2] = (Dis64Payload){Dis64Types, Dis64Required, typedata, sizes[2]};
	sections[3] = (Dis64Payload){Dis64Data, Dis64Required, data, sizes[3]};
	sections[4] = (Dis64Payload){Dis64Exports, Dis64Required, exports, sizes[4]};
	sections[5] = (Dis64Payload){Dis64Strings, Dis64Required, strings, sizes[5]};
	output = allocmem(ScalarOutputLimit);
	if(!dis64write(output, ScalarOutputLimit, nil, 0, sections, nelem(sections), &used) ||
	   !dis64storageimagecheck(output, used, &limits, &checked) ||
	   !dis64numericlibrarycheck(&checked, ScalarCodeLimit, &codeview))
		goto done;
	if(Bwrite(bout, output, used) != used){
		failure = "cannot write the complete checked library";
		goto done;
	}
	dis64trace(&lower);
	failure = nil;
done:
	free(code); free(maps); free(typedata); free(data);
	free(runes); free(strings); free(output);
	dis64lowerfree(&lower);
	if(failure != nil)
		dis64error(nil, failure);
}


/* Compile the existing np0 scalar profile; no imports, managed ABI or shared IDs are added. */
void
dis64scalar(Decl *entry, Decl *globals, Inst *instructions, long count, long databytes)
{
	Dis64ModuleInfo module;
	Dis64Type types[2];
	Dis64Initializer *records;
	Dis64Payload sections[5];
	DisStringPoolRecord string;
	Dis64Lower lower;
	Dis64Prepared prepared;
	Dis64Pilot checked;
	Decl *decl;
	Inst *input;
	uchar *parts[5], *maps[2], *runes, *output;
	unsigned int i, n, at, written, frame, scratch, length, typesbytes, sizes[5];
	char *failure;

	if(entry == nil || entry->ty == nil || entry->desc == nil){
		dis64error(entry, "requires an implemented void init() entry");
		return;
	}
	if(MaxTemp != ScalarLegacyPrefix || STemp != 20 || IBY2WD != 4 || IBY2LG != 8 || sizeof(Real) != 8 || sizeof(Long) != 8 ||
	   entry->pc == nil ||
	   entry->ty->ids != nil || entry->ty->varargs || entry->ty->tof->kind != Tnone ||
	   !dis64scalarlist(entry->locals, 0) || !dis64scalarlist(globals, 1) || !dis64nomap(entry->desc) ||
	   count <= 0 || count > ScalarCodeLimit || databytes < 0 || databytes > ScalarObjectLimit ||
	   entry->desc->size < ScalarLegacyPrefix || entry->desc->size > ScalarObjectLimit-ScalarNativePrefix-ScalarScratchBytes){
		dis64error(entry, "requires one pointer-free void init() with no parameters and bounded scalar storage");
		return;
	}
	for(decl = impdecl->ty->tof->ids; decl != nil; decl = decl->next)
		if(decl->store == Dglobal || (decl->store == Dfn && decl->sym != entry->sym)){
			dis64error(entry, "additional exported functions or module data are not admitted");
			return;
		}
	length = strlen(impdecl->sym->name);
	if(length == 0 || length > 255){
		dis64error(entry, "module display name must contain 1..255 ASCII bytes");
		return;
	}
	for(i=0; i<length; i++)
		if((uchar)impdecl->sym->name[i] > 127){
			dis64error(entry, "module display name must be ASCII");
			return;
		}
	memset(parts, 0, sizeof(parts));
	memset(maps, 0, sizeof(maps));
	runes = output = nil;
	records = nil;
	memset(&lower, 0, sizeof(lower));
	memset(&prepared, 0, sizeof(prepared));
	failure = "unsupported instruction or operand shape in the existing scalar profile";
	if(!dis64prepare(entry, instructions, count, databytes, &prepared))
		goto done;
	scratch = (prepared.localbytes+ScalarNativePrefix-ScalarLegacyPrefix+7)&~7U;
	frame = scratch+ScalarScratchBytes;
	for(input = prepared.instructions; input != nil; input = input->next)
		if(input->op == IMOVM){
			if(frame > ScalarObjectLimit-ScalarCopyBytes)
				goto done;
			frame += ScalarCopyBytes;
			break;
		}
	if(!dis64lower(prepared.instructions, prepared.count, databytes, frame, scratch, &lower))
		goto done;
	count = lower.count;
	databytes = lower.databytes;
	parts[1] = allocmem(4+14*count);
	dis64put32(parts[1], count);
	at = 4;
	for(n = 0; n < count; n++)
		if(!dis64encode(parts[1], &at, &lower.code[n]))
			goto done;
	sizes[1] = at;
	failure = "unsupported scalar initializer or too many initializers";
	records = allocmem(Dis64PilotInitializers*sizeof(*records));
	n = 0;
	for(decl = globals; decl != nil; decl = decl->next){
		if(decl->store != Dglobal)
			goto done;
		if(decl->init == nil || dis64scalartable(decl))
			continue;
		if(n == Dis64PilotInitializers || !dis64globalinitializer(decl, &records[n]))
			goto done;
		n++;
	}
	if(lower.nconstants > Dis64PilotInitializers-n)
		goto done;
	for(i = 0; i < lower.nconstants; i++)
		records[n++] = lower.constants[i];
	memset(types, 0, sizeof(types));
	types[0].bytes = databytes; types[0].align = databytes == 0 ? 1 : 4;
	types[1].bytes = frame; types[1].align = 8;
	for(i=0; i<2; i++){
		types[i].mapbytes = (types[i].bytes+63)/64;
		maps[i] = allocmem(types[i].mapbytes+1);
		memset(maps[i], 0, types[i].mapbytes+1);
		types[i].map = maps[i];
	}
	parts[3] = allocmem(4+16*n);
	if(!dis64initializerswrite(parts[3], 4+16*n, &types[0], records, n, &sizes[3]))
		goto done;
	failure = "cannot encode the agreed scalar module sections";
	typesbytes = 20+types[0].mapbytes+types[1].mapbytes;
	parts[2] = allocmem(typesbytes);
	if(!dis64typeswrite(parts[2], typesbytes, types, 2, &sizes[2]))
		goto done;
	memset(&module, 0, sizeof(module));
	module.profile = 1; module.databytes = databytes; module.frametype = 1;
	parts[0] = allocmem(24);
	if(!dis64modulewrite(parts[0], 24, &module, &sizes[0]))
		goto done;
	runes = allocmem(4+4*length);
	dis64put32(runes, length);
	for(i=0; i<length; i++)
		dis64put32(runes+4+4*i, (uchar)impdecl->sym->name[i]);
	string.data = runes; string.bytes = 4+4*length;
	parts[4] = allocmem(8+4*length);
	if(!disstringpoolwrite(parts[4], 8+4*length, &string, 1, &sizes[4]))
		goto done;
	for(i=0; i<5; i++){
		sections[i].kind = i == 4 ? Dis64Strings : i+1;
		sections[i].flags = Dis64Required;
		sections[i].data = parts[i]; sections[i].bytes = sizes[i];
	}
	output = allocmem(ScalarOutputLimit);
	if(!dis64write(output, ScalarOutputLimit, nil, 0, sections, 5, &written) ||
	   !dis64pilotcheck(output, written, ScalarObjectLimit, ScalarCodeLimit, &checked))
		goto done;
	if(Bwrite(bout, output, written) != written){
		failure = "cannot write the complete checked image";
		goto done;
	}
	dis64trace(&lower);
	failure = nil;
done:
	for(i=0; i<5; i++)
		free(parts[i]);
	for(i=0; i<2; i++)
		free(maps[i]);
	free(runes); free(output); free(records);
	dis64lowerfree(&lower);
	dis64preparefree(&prepared);
	if(failure != nil)
		dis64error(entry, failure);
}
