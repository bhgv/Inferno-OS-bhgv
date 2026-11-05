#include "limbo.h"
#include "dis64-lower.h"
#include "dis64-inline.h"
#include "dis64-signature.h"

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


/* Match the complete lowered init body before changing its frame geometry. */
static int
dis64clockshape(Decl *entry, Decl *globals, Decl *ldts, Inst *input, long count, long databytes)
{
	static const ushort ops[] = {ILOAD, IMFRAME, ILEA, IMCALL, IRET};
	static const uchar sm[] = {Amp, Afp, Afp, Afp, Anone};
	static const uchar dm[] = {Afp, Afp, Afpind, Afp, Anone};
	static const long source[] = {0, 32, 40, 36, 0};
	static const long destination[] = {32, 36, 36, 32, 0};
	Decl *import, *export, *path;
	unsigned int i;

	if(entry == nil || entry->pc == nil || entry->pc->pc != 0 ||
	   entry->ty->ids != nil || entry->ty->varargs || entry->ty->tof->kind != Tnone ||
	   count != nelem(ops) || databytes != 8 ||
	   ldts == nil || ldts->next != nil || ldts->init == nil ||
	   ldts->init->val != 1 || ldts->init->decl == nil)
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
	if(import == nil || import->next != nil || strcmp(import->sym->name, "millisec") != 0 ||
	   import->ty->kind != Tfn || import->ty->ids != nil || import->ty->varargs ||
	   import->ty->tof->kind != Tint)
		return 0;
	for(export=impdecl->ty->tof->ids; export != nil; export=export->next)
		if(export->store == Dglobal || (export->store == Dfn && export->sym != entry->sym))
			return 0;
	i = 0;
	for(; input != nil; input=input->next){
		if(input->op == INOOP)
			continue;
		if(i >= nelem(ops) || input->op != ops[i] || input->sm != sm[i] ||
		   input->dm != dm[i] || (i < 4 &&
		   (input->s.reg != source[i] || input->d.reg != destination[i])))
			return 0;
		if(i == 0 && (input->mm != Aldt || input->m.reg != 0))
			return 0;
		if((i == 1 || i == 3) && (input->mm != Aoff || input->m.decl == nil ||
		   input->m.decl->iface != import || import->offset != 0))
			return 0;
		if((i == 2 || i == 4) && input->mm != Anone)
			return 0;
		if(i == 2 && input->d.offset != 16)
			return 0;
		i++;
	}
	return i == nelem(ops);
}


/* Encode the checked imported clock pilot with the existing Dis64 sections. */
int
dis64clock(Decl *entry, Decl *globals, Decl *ldts, Inst *input, long count, long databytes)
{
	static const uchar instructions[] = {
		ILOAD, SRC(AMP)|AXIMM|DST(AMP), 0, 0, 8,
		IMFRAME, SRC(AMP)|AXIMM|DST(AFP), 0, 8, 40,
		ILEA, SRC(AFP)|DST(AFP|AIND), 0x80, 72, 40, 32,
		IMCALL, SRC(AFP)|AXIMM|DST(AMP), 0, 40, 8,
		IEXIT, SRC(AXXX)|DST(AXXX),
	};
	static const uchar datamap[] = {0xc0}, framemap[] = {0, 0};
	Dis64Type types[] = {{16, 8, datamap, 1}, {80, 8, framemap, 2}};
	Dis64ModuleInfo module = {0};
	DisStringPoolRecord names[3];
	Dis64Payload sections[5];
	uchar metadata[24], code[4+sizeof(instructions)], typedata[32], imports[28];
	uchar runes[3][4+4*255], strings[4+sizeof(runes)], output[2048];
	unsigned int sizes[5], i, j, length, written;
	const char *labels[] = {"$Sys", "millisec", impdecl->sym->name};

	if(!dis64clockshape(entry, globals, ldts, input, count, databytes))
		return 0;
	for(i=0; i<nelem(names); i++){
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
	dis64put32(code, 5);
	memmove(code+4, instructions, sizeof(instructions));
	sizes[1] = sizeof(code);
	dis64put32(imports, 1);
	dis64put32(imports+4, 0);
	dis64put32(imports+8, 1);
	dis64put32(imports+12, 12);
	if(!dis64modulewrite(metadata, sizeof(metadata), &module, &sizes[0]) ||
	   !dis64typeswrite(typedata, sizeof(typedata), types, nelem(types), &sizes[2]) ||
	   !dis64signaturewrite(imports+16, 12, Dis64Int, nil, 0, &written) ||
	   !disstringpoolwrite(strings, sizeof(strings), names, nelem(names), &sizes[4])){
		dis64error(entry, "cannot encode checked clock import");
		return 1;
	}
	sizes[3] = sizeof(imports);
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
	ulong real[2];
	uvlong bits;
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
		if(n == Dis64PilotInitializers || decl->init->op != Oconst || decl->offset < 0 ||
		   decl->init->ty->kind != decl->ty->kind)
			goto done;
		records[n].offset = decl->offset;
		records[n].kind = decl->ty->kind == Tint ? Dis64Int : decl->ty->kind == Tbig ? Dis64Big : Dis64Real;
		bits = decl->init->val;
		if(decl->ty->kind == Treal){
			dtocanon(decl->init->rval, real);
			records[n].high = real[0]; records[n].low = real[1];
		}else{
			records[n].high = decl->ty->kind == Tint ? 0 : bits>>32;
			records[n].low = (unsigned int)bits;
		}
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
