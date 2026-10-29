/*
 * Shared target-layout prototype, not included by the active compiler or VM.
 * ABI32 uses map4; the agreed ABI64 direction uses full map8, WORD32/ref64.
 * No file discriminator is assigned here. See doc/dis64-inferno-journal.md.
 */
#ifndef DIS_LAYOUT_H
#define DIS_LAYOUT_H // Shared target-layout prototype guard.

/* Scalar kinds and the independent byte-offset limit of this prototype. */
enum {
	DisByte,
	DisShort,
	DisWord,
	DisBig,
	DisReal,
	DisPointer,
	DisMaxOffset=0x7fffffff
};

typedef struct DisAbi DisAbi;
struct DisAbi {
	unsigned int ptrbytes; // Target reference width and alignment.
	unsigned int mapquantum; // Bytes represented by one GC bit.
};

typedef struct DisLayout DisLayout;
struct DisLayout {
	unsigned int next; // End of the last placed field in bytes.
	unsigned int align; // Maximum field alignment, initially one.
};

typedef struct DisFrameLayout DisFrameLayout;
struct DisFrameLayout {
	unsigned int lr; // Saved runtime instruction pointer.
	unsigned int fp; // Caller frame pointer.
	unsigned int mr; // Module link.
	unsigned int type; // Frame descriptor.
	unsigned int result; // Caller-owned result address, not an ordinary heap root.
	unsigned int stemp; // Pointer-capable source temporary.
	unsigned int rtemp; // Pointer-capable middle temporary.
	unsigned int dtemp; // Pointer-capable destination temporary.
	unsigned int args; // Start of declared arguments and local data.
};


/* Reject partially initialized profiles before computing target offsets. */
static int
disabvalid(const DisAbi *abi)
{
	return abi != 0 && (abi->ptrbytes == 4 || abi->ptrbytes == 8) &&
		abi->mapquantum == abi->ptrbytes;
}


/* Select explicit target widths without consulting the compiler host ABI. */
static int
disabinit(DisAbi *abi, unsigned int ptrbytes)
{
	if(abi == 0 || (ptrbytes != 4 && ptrbytes != 8))
		return 0;
	abi->ptrbytes = ptrbytes;
	abi->mapquantum = ptrbytes;
	return 1;
}


/* Return zero for an invalid profile or scalar kind. */
static unsigned int
disabsize(const DisAbi *abi, int kind)
{
	if(!disabvalid(abi))
		return 0;
	switch(kind){
	case DisByte: return 1;
	case DisShort: return 2;
	case DisWord: return 4;
	case DisBig: case DisReal: return 8;
	case DisPointer: return abi->ptrbytes;
	}
	return 0;
}


/* Place a scalar or finished nested record; leave outputs unchanged on failure. */
static int
disabplace(DisLayout *layout, unsigned int size, unsigned int align, unsigned int *offset)
{
	unsigned int padding, at;

	if(layout == 0 || offset == 0 || align == 0 || align > 8 ||
	   (align&(align-1)) != 0 || layout->align == 0 || layout->align > 8 ||
	   (layout->align&(layout->align-1)) != 0 || layout->next > DisMaxOffset ||
	   size > DisMaxOffset)
		return 0;
	padding = (-layout->next)&(align-1);
	if(layout->next > DisMaxOffset-padding)
		return 0;
	at = layout->next+padding;
	if(size > DisMaxOffset-at)
		return 0;
	layout->next = at+size;
	if(layout->align < align)
		layout->align = align;
	*offset = at;
	return 1;
}


/* Place a naturally aligned scalar without widening ordinary int fields. */
static int
disabfield(const DisAbi *abi, DisLayout *layout, int kind, unsigned int *offset)
{
	unsigned int size;

	size = disabsize(abi, kind);
	if(size == 0)
		return 0;
	return disabplace(layout, size, size, offset);
}


/* Finish record padding without changing the field cursor. */
static int
disabfinish(const DisLayout *layout, unsigned int *size)
{
	unsigned int padding;

	if(layout == 0 || size == 0 || layout->align == 0 || layout->align > 8 ||
	   (layout->align&(layout->align-1)) != 0 || layout->next > DisMaxOffset)
		return 0;
	padding = (-layout->next)&(layout->align-1);
	if(layout->next > DisMaxOffset-padding)
		return 0;
	*size = layout->next+padding;
	return 1;
}


/* Bound an array extent independently of signed30 wire operands. */
static int
disabarray(unsigned int stride, unsigned int count, unsigned int *size)
{
	if(size == 0 || stride > DisMaxOffset ||
	   (stride != 0 && count > DisMaxOffset/stride))
		return 0;
	*size = stride*count;
	return 1;
}


/* Compute the complete bitmap length using division, with no rounding overflow. */
static int
disabmapbytes(const DisAbi *abi, unsigned int size, unsigned int *bytes)
{
	unsigned int span;

	if(!disabvalid(abi) || size > DisMaxOffset || bytes == 0)
		return 0;
	span = abi->mapquantum*8;
	*bytes = size/span+(size%span != 0);
	return 1;
}


/* Require full maps; every marked bit must describe a complete aligned ref. */
static int
disabmap(const DisAbi *abi, unsigned int size, const unsigned char *map, unsigned int nmap)
{
	unsigned int expected, i, bit, offset;

	if(!disabmapbytes(abi, size, &expected) || nmap != expected ||
	   (nmap != 0 && map == 0))
		return 0;
	for(i=0; i<nmap; i++)
		for(bit=0; bit<8; bit++){
			if((map[i]&(0x80U>>bit)) == 0)
				continue;
			offset = (i*8+bit)*abi->mapquantum;
			if(offset > size || size-offset < abi->ptrbytes)
				return 0;
		}
	return 1;
}


/* Publish explicit VM offsets; these do not describe SysV, Windows or AAPCS. */
static int
disabframes(const DisAbi *abi, DisFrameLayout *frame)
{
	unsigned int p;

	if(!disabvalid(abi) || frame == 0)
		return 0;
	p = abi->ptrbytes;
	frame->lr = 0;
	frame->fp = p;
	frame->mr = 2*p;
	frame->type = 3*p;
	frame->result = 4*p;
	frame->stemp = 5*p;
	frame->rtemp = 6*p;
	frame->dtemp = 7*p;
	frame->args = 8*p;
	return 1;
}


/* Preserve the prototype size helper while rejecting invalid profiles. */
static unsigned int
disabframe(const DisAbi *abi)
{
	DisFrameLayout frame;

	if(!disabframes(abi, &frame))
		return 0;
	return frame.args;
}
#endif
