#include "limbo.h"
#include "dis64-lower.h"
#include "dis64-inline.h"
#include "dis64-signature.h"
#include "dis64-numeric-storage.h"

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
dis64clockshape(Decl *entry, Decl *globals, Decl *ldts, Inst *input, long count, long databytes,
	uchar *calls, unsigned int *ncalls)
{
	Decl *import, *export, *path;
	unsigned int i, phase, binding, used;
	long frame;
	int sleep;

	if(entry == nil || entry->pc == nil || entry->pc->pc != 0 ||
	   entry->ty->ids != nil || entry->ty->varargs || entry->ty->tof->kind != Tnone ||
	   count < 5 || count > 4*Dis64ClockCalls+2 || databytes != 8 ||
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
	if(import == nil || strcmp(import->sym->name, "millisec") != 0 ||
	   import->ty->kind != Tfn || import->ty->ids != nil || import->ty->varargs ||
	   import->ty->tof->kind != Tint)
		return 0;
	sleep = import->next != nil;
	if(ldts->init->val != 1+sleep)
		return 0;
	if(sleep){
		Decl *second = import->next;
		if(second->next != nil || strcmp(second->sym->name, "sleep") != 0 ||
		   second->ty->kind != Tfn || second->ty->ids == nil || second->ty->ids->next != nil ||
		   second->ty->ids->ty->kind != Tint || second->ty->varargs || second->ty->tof->kind != Tint)
			return 0;
	}
	for(export=impdecl->ty->tof->ids; export != nil; export=export->next)
		if(export->store == Dglobal || (export->store == Dfn && export->sym != entry->sym))
			return 0;
	i = phase = binding = used = 0;
	frame = 0;
	for(; input != nil; input=input->next){
		if(input->op == INOOP)
			continue;
		if(i >= count)
			return 0;
		if(i == 0){
			if(input->op != ILOAD || input->sm != Amp || input->s.reg != 0 ||
			   input->mm != Aldt || input->m.reg != 0 || input->dm != Afp || input->d.reg != 32)
				return 0;
		}else if(i == count-1){
			if(phase != 0 || used == 0 || input->op != IRET ||
			   input->sm != Anone || input->mm != Anone || input->dm != Anone)
				return 0;
		}else switch(phase){
		case 0:
			if(input->op != IMFRAME || input->sm != Afp || input->s.reg != 32 ||
			   input->dm != Afp || (input->d.reg != 36 && input->d.reg != 40) ||
			   input->mm != Aoff || input->m.decl == nil)
				return 0;
			binding = sleep && input->m.decl->iface == import->next;
			if(input->m.decl->iface != (binding ? import->next : import) ||
			   input->m.decl->iface->offset != binding)
				return 0;
			frame = input->d.reg;
			phase = binding ? 3 : 1;
			break;
		case 3:
			if(input->op != IMOVW || input->sm != Aimm || input->s.offset != 0 ||
			   input->mm != Anone || input->dm != Afpind ||
			   input->d.reg != frame || input->d.offset != 32)
				return 0;
			phase = 1;
			break;
		case 1:
			if(input->op != ILEA || input->sm != Afp || input->s.reg != (frame == 36 ? 40 : 36) ||
			   input->mm != Anone || input->dm != Afpind ||
			   input->d.reg != frame || input->d.offset != 16)
				return 0;
			phase = 2;
			break;
		case 2:
			if(input->op != IMCALL || input->sm != Afp || input->s.reg != frame ||
			   input->dm != Afp || input->d.reg != 32 || input->mm != Aoff ||
			   input->m.decl == nil || input->m.decl->iface != (binding ? import->next : import) ||
			   used == Dis64ClockCalls)
				return 0;
			calls[used++] = binding;
			phase = 0;
			break;
		}
		i++;
	}
	*ncalls = used;
	return i == count ? 1+sleep : 0;
}


/* Encode the checked imported clock pilot with the existing Dis64 sections. */
int
dis64clock(Decl *entry, Decl *globals, Decl *ldts, Inst *input, long count, long databytes)
{
	static const uchar load[] = {ILOAD, SRC(AMP)|AXIMM|DST(AMP), 0, 0, 8};
	static const uchar call[] = {
		IMFRAME, SRC(AMP)|AXIMM|DST(AFP), 0, 8, 40,
		ILEA, SRC(AFP)|DST(AFP|AIND), 0x80, 72, 40, 32,
		IMCALL, SRC(AFP)|AXIMM|DST(AMP), 0, 40, 8,
	};
	static const uchar sleepcall[] = {
		IMFRAME, SRC(AMP)|AXIMM|DST(AFP), 1, 8, 40,
		IMOVW, SRC(AIMM)|DST(AFP|AIND), 0, 40, 0x80, 64,
		ILEA, SRC(AFP)|DST(AFP|AIND), 0x80, 72, 40, 32,
		IMCALL, SRC(AFP)|AXIMM|DST(AMP), 1, 40, 8,
	};
	static const uchar exit[] = {IEXIT, SRC(AXXX)|DST(AXXX)};
	static const uchar datamap[] = {0xc0}, framemap[] = {0, 0};
	Dis64Type types[] = {{16, 8, datamap, 1}, {80, 8, framemap, 2}};
	Dis64ModuleInfo module = {0};
	DisStringPoolRecord names[4];
	Dis64Payload sections[5];
	uchar metadata[24], code[4+sizeof(load)+Dis64ClockCalls*sizeof(sleepcall)+sizeof(exit)];
	uchar typedata[32], imports[56], runes[4][4+4*255], strings[4+sizeof(runes)], output[2048];
	uchar calls[Dis64ClockCalls];
	unsigned int sizes[5], i, j, length, written, bindings, ncalls, sleep, period = Dis64Int;
	const char *labels[] = {"$Sys", "millisec", impdecl->sym->name, "sleep"};

	bindings = dis64clockshape(entry, globals, ldts, input, count, databytes, calls, &ncalls);
	if(bindings == 0)
		return 0;
	sleep = bindings == 2;
	for(i=0; i<3+sleep; i++){
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
	dis64put32(code, count);
	sizes[1] = 4+sizeof(load);
	memmove(code+4, load, sizeof(load));
	for(i=0; i<ncalls; i++){
		if(calls[i]){
			memmove(code+sizes[1], sleepcall, sizeof(sleepcall));
			sizes[1] += sizeof(sleepcall);
		}else{
			memmove(code+sizes[1], call, sizeof(call));
			sizes[1] += sizeof(call);
		}
	}
	memmove(code+sizes[1], exit, sizeof(exit));
	sizes[1] += sizeof(exit);
	dis64put32(imports, bindings);
	dis64put32(imports+4, 0);
	dis64put32(imports+8, 1);
	dis64put32(imports+12, 12);
	if(!dis64modulewrite(metadata, sizeof(metadata), &module, &sizes[0]) ||
	   !dis64typeswrite(typedata, sizeof(typedata), types, nelem(types), &sizes[2]) ||
	   !dis64signaturewrite(imports+16, 12, Dis64Int, nil, 0, &written) ||
	   !disstringpoolwrite(strings, sizeof(strings), names, 3+sleep, &sizes[4])){
		dis64error(entry, "cannot encode checked clock import");
		return 1;
	}
	sizes[3] = 28;
	if(sleep){
		dis64put32(imports+28, 0);
		dis64put32(imports+32, 3);
		dis64put32(imports+36, 16);
		if(!dis64signaturewrite(imports+40, 16, Dis64Int, &period, 1, &written)){
			dis64error(entry, "cannot encode checked sleep import");
			return 1;
		}
		sizes[3] = sizeof(imports);
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
