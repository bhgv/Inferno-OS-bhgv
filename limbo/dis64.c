#include "limbo.h"
#include "dis64-pilot.h"

#define ScalarObjectLimit (1024U*1024U) // Match the existing experimental runtime object bound.
#define ScalarCodeLimit 65536U // Match the existing runtime instruction bound.
#define ScalarOutputLimit (8U*1024U*1024U) // Match the complete-image admission bound.
#define ScalarLegacyPrefix 32U // This backend translates only the compiler's fixed legacy scalar prefix.
#define ScalarNativePrefix 64U // Existing Dis64 np0 entry-frame reservation.
#define ScalarScratchStart 16U // Align the legacy scratch span without admitting its return-address word.
#define ScalarScratchBytes 16U // Preserve byte aliases in legacy scratch cells20..31 in a private frame tail.


/* Keep diagnostics in the ordinary compiler error and output-cleanup path. */
static void
dis64error(Decl *entry, char *message)
{
	error(entry == nil ? impdecl->src.start : entry->src.start, "Dis64 scalar: %s", message);
}


/* Pointer-free source scalars retain their existing WORD32/eight-byte storage geometry. */
static int
dis64scalarlist(Decl *decl)
{
	for(; decl != nil; decl = decl->next)
		if(decl->ty == nil || (decl->ty->kind != Tint && decl->ty->kind != Tbig && decl->ty->kind != Treal))
			return 0;
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


/* Rebase direct locals and move scalar scratch bytes beyond the local extent, preserving aliases. */
static int
dis64address(int mode, Addr *address, unsigned int scratch, long *value, unsigned int *wire)
{
	switch(mode){
	case Anone: *wire = AXXX; *value = 0; return 1;
	case Aimm: case Apc: *wire = AIMM; *value = address->offset; return 1;
	case Amp: *wire = AMP; *value = address->reg; return 1;
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


/* Preserve instruction PCs while turning the sole void entry's return into EXIT. */
static int
dis64convert(Inst *input, unsigned int count, unsigned int data, unsigned int frame, unsigned int scratch, Dis64Instruction *output)
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


/* Compile the existing np0 scalar profile; no imports, managed ABI or shared IDs are added. */
void
dis64scalar(Decl *entry, Decl *globals, Inst *instructions, long count, long databytes)
{
	Dis64ModuleInfo module;
	Dis64Type types[2];
	Dis64Initializer *records;
	Dis64Payload sections[5];
	DisStringPoolRecord string;
	Dis64Instruction instruction;
	Dis64Pilot checked;
	Decl *decl;
	Inst *inst;
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
	   nfns != 1 || fns[0] != entry || entry->pc == nil || entry->pc->pc != 0 ||
	   entry->ty->ids != nil || entry->ty->varargs || entry->ty->tof->kind != Tnone ||
	   !dis64scalarlist(entry->locals) || !dis64scalarlist(globals) || !dis64nomap(entry->desc) ||
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
	scratch = (entry->desc->size+ScalarNativePrefix-ScalarLegacyPrefix+7)&~7U;
	frame = scratch+ScalarScratchBytes;
	memset(parts, 0, sizeof(parts));
	memset(maps, 0, sizeof(maps));
	runes = output = nil;
	records = nil;
	failure = "unsupported instruction or operand shape in the existing scalar profile";
	parts[1] = allocmem(4+14*count);
	dis64put32(parts[1], count);
	at = 4;
	n = 0;
	for(inst = instructions; inst != nil; inst = inst->next){
		if(inst->op == INOOP)
			continue;
		if(n >= count || inst->pc != n || !dis64convert(inst, count, databytes, frame, scratch, &instruction) ||
		   !dis64encode(parts[1], &at, &instruction))
			goto done;
		n++;
	}
	if(n != count)
		goto done;
	sizes[1] = at;
	failure = "unsupported scalar initializer or too many initializers";
	records = allocmem(Dis64PilotInitializers*sizeof(*records));
	n = 0;
	for(decl = globals; decl != nil; decl = decl->next){
		if(decl->store != Dglobal)
			goto done;
		if(decl->init == nil)
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
	failure = nil;
done:
	for(i=0; i<5; i++)
		free(parts[i]);
	for(i=0; i<2; i++)
		free(maps[i]);
	free(runes); free(output); free(records);
	if(failure != nil)
		dis64error(entry, failure);
}
