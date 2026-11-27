/* Private MP-only numeric code over checked storage; include isa.h once first. */
#ifndef DIS64_NUMERIC_STORAGE_H
#define DIS64_NUMERIC_STORAGE_H // Numeric roles/reference exclusion, never file admission.

#include "dis64-storage-image.h"


/* Existing role checks precede these MP map checks; no FP local can be uninitialized. */
static int
dis64numericstorageinstruction(const Dis64Instruction *instruction, unsigned int count,
	const Dis64Type *data, unsigned int framebytes)
{
	unsigned int source, destination, middle, width, sourcewidth;
	int branch;

	if(!dis64instructioncheck(instruction, count, data->bytes, framebytes))
		return 0;
	if(instruction->op == IEXIT || instruction->op == IJMP)
		return 1;
	source = USRC(instruction->mode);
	destination = UDST(instruction->mode);
	middle = instruction->mode&ARM;
	if(source == AFP || destination == AFP || middle == AXINF)
		return 0;
	width = 4;
	branch = 0;
	switch(instruction->op){
	case IMOVL: case IMOVF:
	case IADDL: case ISUBL: case IMULL: case IDIVL: case IMODL:
	case IANDL: case IORL: case IXORL: case ISHLL: case ISHRL:
		width = 8; break;
	case IBEQL: case IBNEL: case IBLTL: case IBLEL: case IBGTL: case IBGEL:
	case IBEQF: case IBNEF: case IBLTF: case IBLEF: case IBGTF: case IBGEF:
		width = 8;
		branch = 1; break;
	case IBEQW: case IBNEW: case IBLTW: case IBLEW: case IBGTW: case IBGEW:
		branch = 1; break;
	}
	sourcewidth = instruction->op == ISHLL || instruction->op == ISHRL ? 4 : width;
	if(source == AMP && !dis64initspan(data, (unsigned int)instruction->source, sourcewidth))
		return 0;
	if(!branch && destination == AMP && !dis64initspan(data, (unsigned int)instruction->destination, width))
		return 0;
	if(middle == AXINM && !dis64initspan(data, (unsigned int)instruction->middle, width))
		return 0;
	return 1;
}


/* Check the complete MP-only instruction stream for the selected role. */
static int
dis64numericbody(const Dis64StorageImage *image, unsigned int limit, int returns,
	int library, unsigned int framebytes, Dis64CodeView *result)
{
	Dis64CodeView code;
	Dis64Instruction instruction;
	unsigned int i, at, used;

	if(image->imports.count != 0 ||
	   (image->module.databytes != 0 && (image->present&(1U<<(Dis64Data-1))) == 0) ||
	   !dis64typevalid(&image->data))
		return 0;
	code = image->code;
	if(code.count == 0 || code.count > limit)
		return 0;
	at = 4;
	for(i=0; i<code.count; i++){
		if(at > code.bytes || !dis64instructionread(code.data+at, code.bytes-at, &instruction, &used))
			return 0;
		if(instruction.op == IRET){
			if(!returns || instruction.mode != ((AXXX<<3)|AXXX))
				return 0;
		}else if((library && instruction.op == IEXIT) ||
		   !dis64numericstorageinstruction(&instruction, code.count, &image->data, framebytes))
			return 0;
		at += used;
	}
	if(at != code.bytes || (library && instruction.op != IRET))
		return 0;
	*result = code;
	return 1;
}


/* Validate the reserved frame prefix before allowing native frame links. */
static int
dis64numericframe(const Dis64StorageImage *image, unsigned int ordinal, Dis64Type *frame)
{
	return dis64typeget(&image->types, ordinal, frame) &&
		frame->bytes >= 64 && frame->align == 8 && frame->map[0] == 0;
}


/* Check an executable entry without taking code, frame or module ownership. */
static int
dis64numericentrycheck(const Dis64StorageImage *image, unsigned int limit, int returns, Dis64CodeView *result)
{
	Dis64Type frame;

	return image != 0 && result != 0 && image->module.profile == 1 &&
		image->module.entry < image->code.count &&
		dis64numericframe(image, image->module.frametype, &frame) &&
		dis64numericbody(image, limit, returns, 0, frame.bytes, result);
}


/* An entryless provider executes only through independently checked exports. */
static int
dis64numericlibrarycheck(const Dis64StorageImage *image, unsigned int limit, Dis64CodeView *result)
{
	return image != 0 && result != 0 && image->module.profile == 0 &&
		image->exports.count != 0 && dis64numericbody(image, limit, 1, 1, 64, result);
}


/* The standalone numeric path accepts only modules without exported bindings. */
static int
dis64numericstoragecheck(const Dis64StorageImage *image, unsigned int limit, Dis64CodeView *result)
{
	return image != 0 && image->exports.count == 0 &&
		dis64numericentrycheck(image, limit, 0, result);
}
#endif
