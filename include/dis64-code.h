/* Initial scalar Dis64 code subset. Include isa.h once before this header. */
#ifndef DIS64_CODE_H
#define DIS64_CODE_H // Checked scalar code framing and operand-role guard.

#include "dis64.h"
#include "dis-operand.h"

typedef struct Dis64Instruction Dis64Instruction;
struct Dis64Instruction {
	unsigned int op; // Existing ISA opcode; no new numeric IDs.
	unsigned int mode; // Existing middle/source/destination mode byte.
	long middle; // Signed30 immediate or byte offset, decoded first.
	long source; // Signed30 scalar immediate or byte offset.
	long destination; // Byte offset or absolute Dis-PC, according to opcode.
};

typedef struct Dis64CodeView Dis64CodeView;
struct Dis64CodeView {
	const unsigned char *data; // Complete immutable code payload with count.
	unsigned int bytes; // Exact serialized extent.
	unsigned int count; // Number of decoded Dis instructions.
};


/* Decode signed7/14/30 mathematically without signed shifts or narrowing casts. */
static int
dis64operandread(const unsigned char *data, unsigned int bytes, long *value, unsigned int *used)
{
	unsigned int length, bits, mask, sign, i;
	long decoded;

	if(data == 0 || value == 0 || used == 0 || bytes == 0 || (unsigned int)-1 != 0xffffffffU)
		return 0;
	length = data[0] < 0x80 ? 1 : data[0] < 0xc0 ? 2 : 4;
	if(bytes < length)
		return 0;
	bits = data[0]&(length == 1 ? 0x7f : 0x3f);
	for(i=1; i<length; i++)
		bits = (bits<<8)|data[i];
	mask = length == 1 ? 0x7f : length == 2 ? 0x3fff : 0x3fffffff;
	sign = (mask>>1)+1;
	decoded = (bits&sign) != 0 ? -(long)(mask-bits)-1 : (long)bits;
	*value = decoded;
	*used = length;
	return 1;
}


/* Decode only direct modes; unknown/indirect roles never fall into the old reader. */
static int
dis64instructionread(const unsigned char *data, unsigned int bytes,
	Dis64Instruction *result, unsigned int *used)
{
	Dis64Instruction instruction;
	unsigned int at, count, sm, dm;

	if(data == 0 || result == 0 || used == 0 || bytes < 2)
		return 0;
	instruction.op = data[0];
	instruction.mode = data[1];
	instruction.middle = instruction.source = instruction.destination = 0;
	sm = USRC(instruction.mode);
	dm = UDST(instruction.mode);
	if(sm > AXXX || dm > AXXX)
		return 0;
	at = 2;
	if((instruction.mode&ARM) != AXNON){
		if(!dis64operandread(data+at, bytes-at, &instruction.middle, &count))
			return 0;
		at += count;
	}
	if(sm != AXXX){
		if(!dis64operandread(data+at, bytes-at, &instruction.source, &count))
			return 0;
		at += count;
	}
	if(dm != AXXX){
		if(!dis64operandread(data+at, bytes-at, &instruction.destination, &count))
			return 0;
		at += count;
	}
	*result = instruction;
	*used = at;
	return 1;
}


/* Bound a scalar MP/local-FP slot; protected Frame64 header slots are excluded. */
static int
dis64codeslot(unsigned int mode, long offset, unsigned int width,
	unsigned int mpbytes, unsigned int fpbytes, int immediate)
{
	unsigned int extent, at;

	if(mode == AIMM)
		return immediate;
	if((mode != AMP && mode != AFP) || offset < 0)
		return 0;
	at = (unsigned int)offset;
	extent = mode == AMP ? mpbytes : fpbytes;
	return at%width == 0 && at <= extent && extent-at >= width &&
		(mode != AFP || at >= 64);
}


/* Check the implemented scalar role subset; calls/ref/indirect/table ops wait. */
static int
dis64instructioncheck(const Dis64Instruction *instruction, unsigned int count,
	unsigned int mpbytes, unsigned int fpbytes)
{
	unsigned int sm, dm, mm, middlemode, width, sourcewidth;
	int arithmetic, branch, floating;
	long lhs;

	if(instruction == 0 || instruction->mode > 255)
		return 0;
	sm = USRC(instruction->mode);
	dm = UDST(instruction->mode);
	mm = instruction->mode&ARM;
	width = 4;
	arithmetic = branch = floating = 0;
	switch(instruction->op){
	case IEXIT:
		return sm == AXXX && dm == AXXX && mm == AXNON;
	case IJMP:
		return sm == AXXX && dm == AIMM && mm == AXNON &&
			instruction->destination >= 0 && (unsigned long)instruction->destination < count;
	case IMOVW: break;
	case IMOVL: width = 8; break;
	case IMOVF: width = 8; floating = 1; break;
	case IADDW: case ISUBW: case IMULW: case IDIVW: case IMODW:
	case IANDW: case IORW: case IXORW: case ISHLW: case ISHRW:
		arithmetic = 1; break;
	case IADDL: case ISUBL: case IMULL: case IDIVL: case IMODL:
	case IANDL: case IORL: case IXORL: case ISHLL: case ISHRL:
		arithmetic = 1; width = 8; break;
	case IBEQW: case IBNEW: case IBLTW: case IBLEW: case IBGTW: case IBGEW:
		branch = 1; break;
	case IBEQL: case IBNEL: case IBLTL: case IBLEL: case IBGTL: case IBGEL:
		branch = 1; width = 8; break;
	case IBEQF: case IBNEF: case IBLTF: case IBLEF: case IBGTF: case IBGEF:
		branch = 1; floating = 1; width = 8; break;
	default: return 0;
	}
	sourcewidth = instruction->op == ISHLL || instruction->op == ISHRL ? 4 : width;
	if(!dis64codeslot(sm, instruction->source, sourcewidth, mpbytes, fpbytes, !floating))
		return 0;
	if(!arithmetic && !branch)
		return mm == AXNON && dis64codeslot(dm, instruction->destination, width, mpbytes, fpbytes, 0);
	if(branch && (dm != AIMM || instruction->destination < 0 ||
	   (unsigned long)instruction->destination >= count || mm == AXNON))
		return 0;
	if(arithmetic && !dis64codeslot(dm, instruction->destination, width, mpbytes, fpbytes, 0))
		return 0;
	lhs = mm == AXNON ? instruction->destination : instruction->middle;
	middlemode = mm == AXNON ? dm : mm == AXIMM ? AIMM : mm == AXINF ? AFP : AMP;
	return dis64codeslot(middlemode, lhs, width, mpbytes, fpbytes, !floating);
}


/* Validate complete code framing and every role before publishing an immutable view. */
static int
dis64codecheck(const unsigned char *data, unsigned int bytes, unsigned int limit,
	unsigned int mpbytes, unsigned int fpbytes, Dis64CodeView *result)
{
	Dis64CodeView view;
	Dis64Instruction instruction;
	unsigned int i, at, used;

	if(data == 0 || result == 0 || bytes < 4 || (unsigned int)-1 != 0xffffffffU)
		return 0;
	view.count = dis64u32(data);
	if(view.count == 0 || view.count > limit || view.count > (bytes-4)/2)
		return 0;
	at = 4;
	for(i=0; i<view.count; i++){
		if(!dis64instructionread(data+at, bytes-at, &instruction, &used) ||
		   !dis64instructioncheck(&instruction, view.count, mpbytes, fpbytes))
			return 0;
		at += used;
	}
	if(at != bytes)
		return 0;
	view.data = data;
	view.bytes = bytes;
	*result = view;
	return 1;
}


/* Resolve one checked instruction by Dis-PC, never by native address. */
static int
dis64codeget(const Dis64CodeView *code, unsigned int pc, Dis64Instruction *result)
{
	Dis64Instruction instruction;
	unsigned int i, at, used;

	if(code == 0 || result == 0 || code->data == 0 || pc >= code->count)
		return 0;
	at = 4;
	for(i=0; i<=pc; i++){
		if(at > code->bytes || !dis64instructionread(code->data+at, code->bytes-at, &instruction, &used))
			return 0;
		if(i == pc){
			*result = instruction;
			return 1;
		}
		at += used;
	}
	return 0;
}
#endif
