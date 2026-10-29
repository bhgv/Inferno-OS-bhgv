/*
 * Candidate Dis64 typed-table payload checks. No section/opcode IDs are assigned.
 * Callers supply decoded instruction count and resolved string comparison.
 * This prototype is not included by the active loader or JIT.
 */
#ifndef DIS_CONTROL_H
#define DIS_CONTROL_H // Candidate typed-table helper guard.

/* Local API selectors, not serialized registry IDs. */
enum {
	DisControlI32,
	DisControlBig,
	DisControlString,
	DisControlGoto
};

typedef struct DisControlView DisControlView;
struct DisControlView {
	const unsigned char *rows; // Borrowed immutable validated row bytes.
	unsigned int count; // Number of rows, independent of instruction count.
	unsigned int rowbytes; // Wire row width, with no host padding.
	unsigned int fallback; // Default Dis-PC; unused for goto.
};

typedef int (*DisControlCompare)(void*, unsigned int, unsigned int, int*);


/* Decode a previously bounded big-endian u32 without alignment assumptions. */
static unsigned int
discontrolu32(const unsigned char *p)
{
	return ((unsigned int)p[0]<<24)|((unsigned int)p[1]<<16)|
		((unsigned int)p[2]<<8)|(unsigned int)p[3];
}


/* Compare signed BE integers by flipping the sign bit, without signed casts. */
static int
discontrolsigned(const unsigned char *a, const unsigned char *b, unsigned int width)
{
	unsigned int i, x, y;

	for(i=0; i<width; i++){
		x = a[i];
		y = b[i];
		if(i == 0){
			x ^= 0x80;
			y ^= 0x80;
		}
		if(x != y)
			return x < y ? -1 : 1;
	}
	return 0;
}


/* Validate complete extents, inclusive ranges and every target before publication.
 * String comparison returns false for unresolved IDs and orders string contents.
 * The callback must be read-only; no VM heap or native-PC cache is constructed. */
static int
discontrolcheck(int kind, const unsigned char *data, unsigned int bytes,
	unsigned int instructions, DisControlCompare compare, void *context,
	DisControlView *result)
{
	DisControlView view;
	const unsigned char *row, *previous;
	unsigned int count, rowbytes, trailer, width, i;
	int order;

	if(data == 0 || result == 0 || bytes < 4 || instructions == 0 ||
	   (unsigned int)-1 != 0xffffffffU)
		return 0;
	width = 4;
	trailer = 4;
	switch(kind){
	case DisControlI32: rowbytes = 12; break;
	case DisControlBig: rowbytes = 20; width = 8; break;
	case DisControlString:
		if(compare == 0)
			return 0;
		rowbytes = 12;
		break;
	case DisControlGoto: rowbytes = 4; trailer = 0; break;
	default: return 0;
	}
	count = discontrolu32(data);
	if(bytes < 4+trailer || count > (bytes-4-trailer)/rowbytes ||
	   count*rowbytes != bytes-4-trailer)
		return 0;
	view.rows = data+4;
	view.count = count;
	view.rowbytes = rowbytes;
	view.fallback = 0;
	if(trailer != 0){
		view.fallback = discontrolu32(data+bytes-4);
		if(view.fallback >= instructions)
			return 0;
	}
	previous = 0;
	for(i=0; i<count; i++){
		row = view.rows+i*rowbytes;
		if(discontrolu32(row+rowbytes-4) >= instructions)
			return 0;
		if(kind == DisControlGoto)
			continue;
		if(kind == DisControlString){
			if(!compare(context, discontrolu32(row), discontrolu32(row+4), &order))
				return 0;
		}else
			order = discontrolsigned(row, row+width, width);
		if(order > 0)
			return 0;
		if(previous != 0){
			if(kind == DisControlString){
				if(!compare(context, discontrolu32(previous+4), discontrolu32(row), &order))
					return 0;
			}else
				order = discontrolsigned(previous+width, row, width);
			if(order >= 0)
				return 0;
		}
		previous = row;
	}
	*result = view;
	return 1;
}
#endif
