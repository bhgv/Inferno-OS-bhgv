#include "limbo.h"
#include "dis64-lower.h"
#include "dis64-inline.h"
#include "dis64-signature.h"
#include "dis64-numeric-storage.h"
#include "dis64-managed-trial.h"
#include "dis64-bound-call.h"

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
	long legacy; // Original MFRAME boundary for checked scalar branch relocation.
	uchar modes[2];
	long values[2];
	int save; // Native destination of a persistent returned value, or zero.
};

typedef struct Dis64ClockWord Dis64ClockWord;
struct Dis64ClockWord {
	unsigned int before; // Number of completed calls before this scalar step.
	long legacy, target; // Original scalar PC and branch target, or -1.
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


/* Conditional calls need a stable Word identity instead of aliasing whichever
 * call most recently wrote the common result at byte72. */
static long
dis64clockretain(long *locals, long legacy)
{
    unsigned int i;
    long cell = dis64clockcell(locals,legacy);
    if(cell) return cell;
    for(i=0; i<Dis64ClockLocals; i++) if(locals[i] < 0) {
        locals[i] = legacy; return i == 2 ? 76 : 64+4*i;
    }
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


/* Map existing Word comparisons and direct jumps. Targets are
 * relocated after complete call-group expansion; no branch may enter a group. */
static int
dis64clocktest(Inst *input, long *locals, long previous, unsigned int count, Dis64Instruction *output)
{
    Inst mapped = *input;
    if(!dis64clockbranch(input->op) || input->branch == nil || input->dm != Apc ||
       input->branch->pc <= 0 || input->branch->pc >= count) return 0;
    if(input->op != IJMP) {
        if(!dis64clockvalue(input->sm,&input->s,locals,previous,&mapped.s) ||
           !dis64clockvalue(input->mm,&input->m,locals,previous,&mapped.m)) return 0;
    }
    mapped.d.offset = input->branch->pc;
    return dis64scalarinst(&mapped,count,0,80,64,output);
}

/* Match the complete lowered init body before changing its frame geometry. */
static int
dis64clockshape(Decl *entry, Decl *globals, Decl *ldts, Inst *input, long count, long databytes,
	Dis64ClockPlan *output)
{
	Decl *import, *export, *path, *argument, *target;
    Inst *scan, *between;
	unsigned int i, j, phase, binding, used, parameter, seen, copies;
	Dis64ClockPlan plan = {0};
	long frame, module, result, previous, locals[Dis64ClockLocals], cell;
	int kind, conditional = 0;

	if(entry == nil || entry->pc == nil || entry->pc->pc != 0 ||
	   entry->ty->ids != nil || entry->ty->varargs || entry->ty->tof->kind != Tnone ||
	   count < 5 || count > 5*Dis64ClockCalls+2+Dis64ClockWords || databytes != 8 ||
	   entry->desc == nil || entry->desc->size < ScalarLegacyPrefix+IBY2WD ||
	   entry->desc->size > ScalarObjectLimit ||
	   ldts == nil || ldts->next != nil || ldts->init == nil ||
	   ldts->init->decl == nil)
		return 0;
    for(scan=input; scan != nil; scan=scan->next) {
        if(scan->op == IRET && scan->pc < count-1) conditional = 1;
        if(scan->branch != nil && scan->branch->pc <= scan->pc) conditional = 1;
        if(scan->branch != nil && scan->branch->pc > scan->pc) {
            for(between=scan->next; between != nil && between->pc < scan->branch->pc; between=between->next)
                if(between->op == IMFRAME || between->op == IMCALL) conditional = 1;
        }
    }
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
            if(input->op == IRET && input->sm == Anone && input->mm == Anone && input->dm == Anone) {
                if(plan.nwords == Dis64ClockWords) return -1;
                plan.words[plan.nwords].legacy = input->pc;
                plan.words[plan.nwords].target = count-1;
                plan.words[plan.nwords].before = used;
                plan.words[plan.nwords++].instruction = (Dis64Instruction){IJMP,SRC(AXXX)|DST(AIMM),0,0,count-1};
                break;
            }
			if(dis64clockwordop(input->op) || dis64clockbranch(input->op)){
                if(plan.nwords < Dis64ClockWords) {
                    plan.words[plan.nwords].legacy = input->pc;
                    plan.words[plan.nwords].target = dis64clockbranch(input->op) && input->branch != nil ? input->branch->pc : -1;
                }
				if(plan.nwords == Dis64ClockWords ||
                   !(dis64clockbranch(input->op) ? dis64clocktest(input,locals,previous,count,&plan.words[plan.nwords].instruction) :
				   dis64clockword(input, module, entry->desc->size, locals, &previous,
				     &plan.words[plan.nwords].instruction)))
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
            plan.calls[used].legacy = input->pc;
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
            if(conditional && !dis64clockretain(locals,result)) return -1;
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
			previous = conditional ? -1 : result;
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
    Dis64Instruction flow[Dis64ClockInstructions];
    Dis64CallDecoded decoded;
    long pcs[Dis64ClockInstructions];
    unsigned int pc, at, used;
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
    for(i=0; i<nelem(pcs); i++) pcs[i] = -1;
    pc = 1; step = 0;
    for(i=0; i<=plan.count; i++) {
        while(step < plan.nwords && plan.words[step].before == i) {
            pcs[plan.words[step].legacy] = pc++; step++;
        }
        if(i != plan.count) { pcs[plan.calls[i].legacy] = pc; pc += 3+plan.arguments[plan.calls[i].binding]+!!plan.calls[i].save; }
    }
    pcs[count-1] = pc;
    for(i=0; i<plan.nwords; i++) if(plan.words[i].target >= 0) {
        if(plan.words[i].target >= count || pcs[plan.words[i].target] < 0) { dis64error(entry,"branch enters a prepared Sys call"); return 1; }
        plan.words[i].instruction.destination = pcs[plan.words[i].target];
    }
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
    at = 4;
    for(i=0; i<instructions; i++) {
        if(!dis64callread(code+at,sizes[1]-at,&decoded,&used)) { dis64error(entry,"invalid Sys flow encoding"); return 1; }
        flow[i] = decoded.instruction; at += used;
    }
    if(!dis64clockflow(flow,instructions)) { dis64error(entry,"unsupported Sys scalar control flow or initialization"); return 1; }
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

typedef struct Dis64ManagedCompilePlan Dis64ManagedCompilePlan;
struct Dis64ManagedCompilePlan {
	unsigned int bindings, calls;
	uchar directions[2], reloads[Dis64ManagedCalls], transfers[Dis64ManagedCalls], ordinals[Dis64ManagedCalls];
	char *path;
};

#define Dis64ManagedNil (-1L-Dis64ManagedCalls) // Distinct from unknown, values and LOAD identities.

/* Track pointer value identities across ordinary local assignments. */
static long*
dis64managedlocal(long *slots, long *values, unsigned int *used, long slot)
{
	unsigned int i;

	for(i=0; i<*used; i++)
		if(slots[i] == slot)
			return &values[i];
	if(*used == 12*Dis64ManagedCalls+1)
		return nil;
	slots[*used] = slot;
	values[*used] = 0;
	return &values[(*used)++];
}


/* Prove the one CONS/HEAD effect independently of legacy temporary allocation. */
static int
dis64managedlibraryshape(Decl *entry, Decl *globals, Inst *input, long count, long databytes,
	Dis64ManagedCompilePlan *plan)
{
	Decl *export, *functions[2], *temporary;
	unsigned int n = 0, i, seen = 0, used, effect, returned;
	long pc = 0, end, value, slots[12*Dis64ManagedCalls+1], values[12*Dis64ManagedCalls+1], *cell;
	int direction, hasnil;

	if(entry != nil || strcmp(impdecl->sym->name, "String") != 0 || count < 2 || count > 10*Dis64ManagedCalls)
		return 0;
	hasnil = 0;
	for(; globals != nil; globals=globals->next){
		if(globals->store != Dglobal) continue;
		if(hasnil || globals->offset != 0 || globals->init != nil ||
		   globals->ty->kind != Tany || strcmp(globals->sym->name, "nil") != 0)
			return 0;
		hasnil = 1;
	}
	if(databytes != (hasnil ? 4 : 0)) return 0;
	for(export=impdecl->ty->tof->ids; export != nil; export=export->next){
		if(export->store == Dglobal)
			return 0;
		if(export->store != Dfn)
			continue;
		if(n == 2 || export->pc == nil || export->desc == nil || dis64manageddirection(export) < 0)
			return 0;
		functions[n++] = export;
	}
	if(n == 0) return 0;
	if(n == 2 && functions[0]->pc->pc > functions[1]->pc->pc){
		temporary = functions[0]; functions[0] = functions[1]; functions[1] = temporary;
	}
	for(i=0; i<n; i++){
		direction = dis64manageddirection(functions[i]);
		if((seen & (1U<<direction)) || functions[i]->pc->pc != pc)
			return 0;
		seen |= 1U<<direction;
		plan->directions[i] = direction;
		end = i+1 < n ? functions[i+1]->pc->pc : count;
		if(end <= pc || end > count) return 0;
		used = effect = returned = 0;
		*dis64managedlocal(slots, values, &used, 32) = 2; /* 0 unknown, 1 nil, 2 argument, 3 result. */
		for(; pc<end; pc++){
			while(input != nil && input->op == INOOP) input = input->next;
			if(input == nil || input->mm != Anone) return 0;
			if(input->op == IRET){
				if(pc != end-1 || input->sm != Anone || input->dm != Anone || effect != 1 || returned != 1)
					return 0;
			}else{
				if(pc == end-1) return 0;
				if(input->sm == Amp && input->s.reg == 0 && hasnil)
					value = 1;
				else if(input->sm == Afp && dis64clockslot(input->s.reg, functions[i]->desc->size)){
					cell = dis64managedlocal(slots, values, &used, input->s.reg);
					if(cell == nil || *cell == 0) return 0;
					value = *cell;
				}else return 0;
				if(input->op == IHEADP){
					if(!direction || value != 2 || effect++) return 0;
					value = 3;
				}else if(input->op == ICONSP){
					if(direction || value != 2 || effect++ || input->dm != Afp ||
					   !dis64clockslot(input->d.reg, functions[i]->desc->size)) return 0;
					cell = dis64managedlocal(slots, values, &used, input->d.reg);
					if(cell == nil || *cell != 1) return 0;
					value = 3;
				}else if(input->op != IMOVP) return 0;
				if(input->dm == Afp && dis64clockslot(input->d.reg, functions[i]->desc->size)){
					cell = dis64managedlocal(slots, values, &used, input->d.reg);
					if(cell == nil) return 0;
					*cell = value;
				}else if(input->dm == Afpind && input->d.reg == 16 && input->d.offset == 0 && value == 3){
					if(returned++) return 0;
				}else return 0;
			}
			input = input->next;
		}
	}
	while(input != nil && input->op == INOOP) input = input->next;
	if(input != nil || pc != count) return 0;
	plan->bindings = n;
	return 1;
}


/* Prove legacy value identities and kinds while preserving import/reload positions. */
static int
dis64managedcallershape(Decl *entry, Decl *globals, Decl *ldts, Inst *input,
	long count, long databytes, Dis64ManagedCompilePlan *plan)
{
	static const ushort ops[] = {ILOAD, IMFRAME, IMOVP, ILEA, IMCALL, IRET};
	static const uchar sm[] = {Amp, Afp, Afp, Afp, Afp, Anone};
	static const uchar dm[] = {Afp, Afp, Afpind, Afpind, Afp, Anone};
	static const uchar mm[] = {Aldt, Aoff, Anone, Anone, Aoff, Anone};
	Decl *imports[2], *import, *export, *path, *argument, *nilvalue;
	long module = -1, modulevalue = 0, frame = -1, result = -1, current = 1, previous = 0;
	long slots[12*Dis64ManagedCalls+1], values[12*Dis64ManagedCalls+1], *source, *destination;
	unsigned int i, n, groups = 0, phase = 1, pending = 0, step, binding = 0, seen = 0, used = 0;
	int direction, currentkind, previouskind = 0;

	if(entry == nil || entry->pc == nil || entry->pc->pc != 0 || entry->ty->varargs ||
	   entry->ty->tof->kind != Tnone || entry->desc == nil || count < 6 || count > 12*Dis64ManagedCalls+1 ||
	   (databytes != 4 && databytes != 8) || ldts == nil || ldts->next != nil || ldts->init == nil ||
	   ldts->init->val < 1 || ldts->init->val > 2 || ldts->init->decl == nil)
		return 0;
	for(import=ldts->init->decl->ty->ids, n=0; import != nil; import=import->next, n++){
		direction = dis64manageddirection(import);
		if(n == 2 || direction < 0 || (seen & (1U<<direction)) || import->offset != n)
			return 0;
		seen |= 1U<<direction;
		imports[n] = import;
		plan->directions[n] = direction;
	}
	if(n != ldts->init->val)
		return 0;
	argument = entry->ty->ids;
	if(argument == nil || argument->next != nil)
		return 0;
	currentkind = argument->ty->kind == Tstring ? 1 :
		argument->ty->kind == Tlist && argument->ty->tof->kind == Tstring ? 2 : 0;
	if(currentkind == 0)
		return 0;
	for(export=impdecl->ty->tof->ids; export != nil; export=export->next)
		if(export->store == Dglobal || (export->store == Dfn && export->sym != entry->sym))
			return 0;
	path = nilvalue = nil;
	for(; globals != nil; globals=globals->next){
		if(globals->store != Dglobal) continue;
		if(globals->ty->kind == Tany && globals->init == nil && strcmp(globals->sym->name, "nil") == 0){
			if(nilvalue != nil) return 0;
			nilvalue = globals;
			continue;
		}
		if(path != nil || globals->ty->kind != Tstring || globals->init == nil ||
		   globals->init->op != Oconst || globals->init->decl == nil ||
		   (emitscalar64 == 3 ? globals->init->decl->sym->name[0] != '/' :
		    strcmp(globals->init->decl->sym->name, "/dis/lib/string.dis") != 0))
			return 0;
		path = globals;
	}
	if(path == nil || databytes != (nilvalue != nil ? 8 : 4) ||
	   (path->offset != 0 && path->offset != 4) ||
	   (nilvalue == nil ? path->offset != 0 : nilvalue->offset != 4-path->offset)) return 0;
	*dis64managedlocal(slots, values, &used, 32) = current;
	for(i=0; input != nil; input=input->next){
		if(input->op == INOOP) continue;
		if(i >= count) return 0;
		if(i != count-1 && phase == 1 && input->op == IMOVP &&
		   (input->sm == Afp || input->sm == Amp) && input->dm == Afp && input->mm == Anone){
			if(!dis64clockslot(input->d.reg, entry->desc->size))
				return 0;
			destination = dis64managedlocal(slots, values, &used, input->d.reg);
			if(destination == nil) return 0;
			if(input->sm == Amp){
				if(nilvalue == nil || input->s.reg != nilvalue->offset) return 0;
				*destination = Dis64ManagedNil; /* A known empty value, distinct from an unknown local. */
			}else{
				if(!dis64clockslot(input->s.reg, entry->desc->size)) return 0;
				source = dis64managedlocal(slots, values, &used, input->s.reg);
				if(source == nil || *source == 0) return 0;
				*destination = *source;
			}
			i++;
			continue;
		}
		step = module < 0 ? 0 : i == count-1 ? 5 : phase;
		if(i != 0 && i != count-1 && phase == 1 && input->op == ILOAD && emitscalar64 == 3 && !pending)
			step = 0;
		if(input->op != ops[step] || (input->sm != sm[step] && !(step == 2 && input->sm == Amp)) ||
		   input->dm != dm[step] || input->mm != mm[step])
			return 0;
		switch(step){
		case 0:
			if(pending || groups == Dis64ManagedCalls || input->s.reg != path->offset || input->m.reg != 0 ||
			   !dis64clockslot(input->d.reg, entry->desc->size) || input->d.reg == 32) return 0;
			module = input->d.reg;
			destination = dis64managedlocal(slots, values, &used, module);
			if(destination == nil) return 0;
			*destination = --modulevalue;
			plan->reloads[groups] = pending = 1;
			break;
		case 1:
			source = dis64managedlocal(slots, values, &used, input->s.reg);
			if(groups == Dis64ManagedCalls || source == nil || modulevalue == 0 || *source != modulevalue ||
			   input->m.decl == nil || !dis64clockslot(input->d.reg, entry->desc->size) ||
			   input->d.reg == input->s.reg || input->d.reg == 32) return 0;
			for(binding=0; binding<n; binding++)
				if(input->m.decl->iface == imports[binding]) break;
			if(binding == n) return 0;
			frame = input->d.reg;
			destination = dis64managedlocal(slots, values, &used, frame);
			if(destination == nil) return 0;
			*destination = 0;
			phase = 2;
			break;
		case 2:
			if(input->d.reg != frame || input->d.offset != 32 || input->s.reg == frame)
				return 0;
			if(input->sm == Amp){
				if(nilvalue == nil || input->s.reg != nilvalue->offset) return 0;
				source = nil;
			}else{
				if(!dis64clockslot(input->s.reg, entry->desc->size)) return 0;
				source = dis64managedlocal(slots, values, &used, input->s.reg);
				if(source == nil || *source == 0) return 0;
			}
			if(source == nil || *source == Dis64ManagedNil){
				plan->transfers[groups] = 2;
				current = Dis64ManagedNil;
				currentkind = plan->directions[binding] ? 2 : 1;
			}else if(previous != 0 && *source == previous){
				plan->transfers[groups] = 1;
				current = previous;
				currentkind = previouskind;
			}else if(*source != current)
				return 0;
			if(currentkind != (plan->directions[binding] ? 2 : 1)) return 0;
			phase = 3;
			break;
		case 3:
			result = input->s.reg;
			if(!dis64clockslot(result, entry->desc->size) || result == frame ||
			   input->d.reg != frame || input->d.offset != 16) return 0;
			phase = 4;
			break;
		case 4:
			source = dis64managedlocal(slots, values, &used, input->d.reg);
			if(input->s.reg != frame || source == nil || *source != modulevalue || input->m.decl == nil ||
			   input->m.decl->iface != imports[binding]) return 0;
			plan->ordinals[groups++] = binding;
			previous = groups+1;
			previouskind = plan->directions[binding] ? 1 : 2;
			destination = dis64managedlocal(slots, values, &used, result);
			if(destination == nil) return 0;
			*destination = previous;
			pending = 0; phase = 1;
			break;
		case 5:
			if(phase != 1 || pending || groups == 0) return 0;
			break;
		}
		i++;
	}
	if(i != count) return 0;
	plan->bindings = n;
	plan->calls = groups;
	plan->path = path->init->decl->sym->name;
	return 1;
}

void
dis64managed(Decl *entry, Decl *globals, Decl *ldts, Inst *input, long count, long databytes)
{
	static const uchar map[] = {0, 0x80}, callermap[] = {0, 0xc0}, mpmap[] = {0x80}, loadmap[] = {0xe0};
	static const uchar load[] = {ILOAD,SRC(AMP)|AXIMM|DST(AMP),0,16,0};
	static const uchar transfer[] = {IMOVP,SRC(AFP)|DST(AFP),0x80,72,0x80,64};
	static const uchar clear[] = {IMOVP,SRC(AMP)|DST(AFP),8,0x80,72};
	static const uchar empty[] = {IMOVP,SRC(AMP)|DST(AFP),8,0x80,64};
	static const uchar exit[] = {IEXIT,SRC(AXXX)|DST(AXXX)};
	static const uchar call[] = {IMFRAME,SRC(AMP)|AXIMM|DST(AFP),0,0,40,
		IMOVP,SRC(AFP)|DST(AFP|AIND),0x80,64,40,0x80,64,
		ILEA,SRC(AFP)|DST(AFP|AIND),0x80,72,40,32,
		IMCALL,SRC(AFP)|AXIMM|DST(AMP),0,40,0};
	Dis64ManagedCompilePlan plan = {0};
	Dis64Type types[] = {{72, 8, map, 2}, {8, 8, mpmap, 1}};
	Dis64ModuleInfo module = {0};
	DisStringPoolRecord names[5];
	Dis64Payload sections[5];
	uchar caller[4+Dis64ManagedCalls*(sizeof(load)+sizeof(call)+sizeof(transfer)+sizeof(clear))+2];
	uchar provider[22], metadata[24], typedata[32], pool[2304], name[28], symbols[2][36];
	uchar callername[1024], pathname[1024], binding[76], output[4096];
	unsigned int sizes[5], written, i, j, at, header, bindingbytes, poolcount, instructions;
	int consumer = entry != nil, fileload = consumer && emitscalar64 == 3, direction;

	if(!(consumer ? dis64managedcallershape(entry, globals, ldts, input, count, databytes, &plan) :
	     ldts == nil && dis64managedlibraryshape(entry, globals, input, count, databytes, &plan))){
		error(impdecl->src.start, "Dis64 managed trial: unsupported checked String provider or caller");
		return;
	}
	poolcount = consumer ? (fileload || plan.bindings == 2 ? 4 : 3) : 1+plan.bindings;
	if(!dis64libraryname("String", name, &names[0])) return;
	for(i=0; i<plan.bindings; i++)
		if(!dis64libraryname(plan.directions[i] ? "quoted" : "unquoted", symbols[i], &names[consumer && i ? 4 : 1+i])) return;
	if(consumer){
		module.name = 2; module.profile = 1; module.databytes = fileload ? 24 : plan.calls == 1 && plan.transfers[0] != 2 ? 8 : 16; module.datatype = 1;
		types[0] = (Dis64Type){80, 8, callermap, 2};
		if(plan.calls != 1 || plan.transfers[0] == 2) types[1] = (Dis64Type){16, 8, callermap+1, 1};
		if(fileload) types[1] = (Dis64Type){24, 8, loadmap, 1};
		if(!dis64libraryname(impdecl->sym->name, callername, &names[2]) ||
		   ((fileload || plan.bindings == 2) && !dis64libraryname(plan.path, pathname, &names[3]))){
			error(impdecl->src.start, "Dis64 managed file: unsupported name or absolute path"); return;
		}
		if(plan.bindings == 2) poolcount = 5;
		at = 4; instructions = 5*plan.calls;
		for(i=0; i<plan.calls; i++){
			if(plan.transfers[i] == 1){ memmove(caller+at, transfer, sizeof(transfer)); at += sizeof(transfer); instructions++; }
			if(i != 0){ memmove(caller+at, clear, sizeof(clear)); at += sizeof(clear); }
			if(fileload && plan.reloads[i]){ memmove(caller+at, load, sizeof(load)); at += sizeof(load); instructions++; }
			if(plan.transfers[i] == 2){ memmove(caller+at, empty, sizeof(empty)); at += sizeof(empty); instructions++; }
			memmove(caller+at, call, sizeof(call));
			caller[at+2] = caller[at+sizeof(call)-3] = plan.ordinals[i];
			at += sizeof(call);
		}
		memmove(caller+at, exit, sizeof(exit)); at += sizeof(exit);
		dis64put32(caller, instructions);
	}else{
		at = 4; memmove(provider+at, exit, sizeof(exit)); at += sizeof(exit);
		for(i=0; i<plan.bindings; i++){
			provider[at++] = plan.directions[i] ? IHEADP : ICONSP;
			provider[at++] = SRC(AFP)|DST(AFP|AIND);
			provider[at++] = 0x80; provider[at++] = 64; provider[at++] = 32; provider[at++] = 0;
			provider[at++] = IRET; provider[at++] = SRC(AXXX)|DST(AXXX);
		}
		dis64put32(provider, 1+2*plan.bindings);
	}
	dis64put32(binding, plan.bindings); bindingbytes = 4;
	for(i=0; i<plan.bindings; i++){
		j = bindingbytes;
		dis64put32(binding+j, consumer ? 0 : i+1);
		dis64put32(binding+j+4, consumer ? (i == 0 ? 1 : 4) : 1+2*i);
		if(!consumer) dis64put32(binding+j+8, 0);
		header = j+(consumer ? 12 : 16);
		dis64put32(binding+header-4, 20);
		dis64put32(binding+header, Dis64TrialVersion); dis64put32(binding+header+4, 1);
		direction = plan.directions[i];
		dis64put32(binding+header+8, direction ? Dis64TrialString : Dis64TrialList);
		dis64put32(binding+header+12, direction ? Dis64TrialList : Dis64TrialString);
		dis64put32(binding+header+16, Dis64TrialString);
		bindingbytes = header+20;
	}
	if(!dis64modulewrite(metadata, sizeof(metadata), &module, &sizes[0]) ||
	   !dis64typeswrite(typedata, sizeof(typedata), types, 1+consumer, &sizes[2]) ||
	   !disstringpoolwrite(pool, sizeof(pool), names, poolcount, &sizes[3])){
		error(impdecl->src.start, "Dis64 managed trial: cannot encode image"); return;
	}
	sections[0] = (Dis64Payload){Dis64Module, Dis64Required, metadata, sizes[0]};
	sections[1] = (Dis64Payload){Dis64Code, Dis64Required, consumer ? caller : provider, at};
	sections[2] = (Dis64Payload){Dis64Types, Dis64Required, typedata, sizes[2]};
	sections[3] = (Dis64Payload){Dis64Strings, Dis64Required, pool, sizes[3]};
	sections[4] = (Dis64Payload){consumer ? Dis64Imports : Dis64Exports, Dis64Required, binding, bindingbytes};
	if(!dis64write(output, sizeof(output), nil, 0, sections, nelem(sections), &written) || Bwrite(bout, output, written) != written)
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
