/*
 * Experimental layout contract for a future explicit Dis ABI32/ABI64.
 * Not included by the active compiler/VM yet: legacy module offsets must not
 * silently be reinterpreted under a new pointer layout. See doc/dis64-design.md.
 * Native addresses can be 64-bit while bytecode offsets and int stay 32-bit.
 */
#ifndef DIS_LAYOUT_H
#define DIS_LAYOUT_H

enum {
	DisByte, DisShort, DisWord, DisBig, DisReal, DisPointer,
	DisMapQuantum=4,
	DisMaxOffset=0x7fffffff
};
typedef struct DisAbi DisAbi;
struct DisAbi {
	unsigned int ptrbytes;
};
typedef struct DisLayout DisLayout;
struct DisLayout {
	unsigned int next, align;
};

static int
disabinit(DisAbi *abi, unsigned int ptrbytes)
{
	if(ptrbytes != 4 && ptrbytes != 8)
		return 0;
	abi->ptrbytes=ptrbytes;
	return 1;
}

static unsigned int
disabsize(DisAbi *abi, int kind)
{
	switch(kind) {
	case DisByte: return 1;
	case DisShort: return 2;
	case DisWord: return 4;
	case DisBig: case DisReal: return 8;
	case DisPointer: return abi->ptrbytes;
	}
	return 0;
}

static int
disabfield(DisAbi *abi, DisLayout *layout, int kind, unsigned int *offset)
{
	unsigned int size, at, padding;

	if(abi->ptrbytes != 4 && abi->ptrbytes != 8)
		return 0;
	size=disabsize(abi, kind);
	if(size == 0 || layout->next > DisMaxOffset)
		return 0;
	padding=(-layout->next)&(size-1);
	if(layout->next > DisMaxOffset-padding)
		return 0;
	at=layout->next+padding;
	if(at > DisMaxOffset-size)
		return 0;
	*offset=at;
	layout->next=at+size;
	if(layout->align < size)
		layout->align=size;
	return 1;
}

static int
disabfinish(DisLayout *layout, unsigned int *size)
{
	unsigned int padding;
	if(layout->align == 0 || layout->align > 8 ||
	   (layout->align&(layout->align-1)) != 0 || layout->next > DisMaxOffset)
		return 0;
	padding=(-layout->next)&(layout->align-1);
	if(layout->next > DisMaxOffset-padding)
		return 0;
	*size=layout->next+padding;
	return 1;
}

/* Retain a four-byte bitmap quantum. In ABI64 a marked pointer covers two
 * quanta and is eight-byte aligned; its second quantum must not be marked.
 * An ordinary int can occupy either unmarked four-byte quantum. */
static int
disabmap(DisAbi *abi, unsigned int size, unsigned char *map, unsigned int nmap)
{
	unsigned int i, bit, offset;

	if((abi->ptrbytes != 4 && abi->ptrbytes != 8) || size > DisMaxOffset ||
	   nmap > (size+31U)/32U || (nmap != 0 && map == 0))
		return 0;
	for(i=0; i<nmap; i++)
		for(bit=0; bit<8; bit++) {
			if((map[i]&(0x80U>>bit)) == 0)
				continue;
			offset=(i*8+bit)*DisMapQuantum;
			if(offset%abi->ptrbytes != 0 || offset > size ||
			   size-offset < abi->ptrbytes)
				return 0;
		}
	return 1;
}

/* Four runtime pointer fields, a return-address slot, three temporary slots.
 * Reserve pointer-width temporary slots even when the current value is int32.
 * This is a proposal for regenerated frames/stubs, not the active MaxTemp. */
static unsigned int
disabframe(DisAbi *abi)
{
	return abi->ptrbytes*8;
}
#endif
