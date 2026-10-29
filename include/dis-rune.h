/* Lossless Dis64 candidate non-nil string records: count:u32 then BE Rune32. */
#ifndef DIS_RUNE_H
#define DIS_RUNE_H // Shared lossless string-record helper guard.

typedef struct DisRuneView DisRuneView;
struct DisRuneView {
	const unsigned char *values; // Borrowed immutable BE Rune32 payload.
	unsigned int count; // Rune count, independent of UTF-8 or UTF-16 length.
};


/* Decode one already bounded BE Rune32, preserving every payload bit. */
static unsigned int
disruneget(const unsigned char *p)
{
	return ((unsigned int)p[0]<<24)|((unsigned int)p[1]<<16)|
		((unsigned int)p[2]<<8)|(unsigned int)p[3];
}


/* Write a previously bounded Rune32 without host-endian or scalar conversion. */
static void
disruneput(unsigned char *p, unsigned int value)
{
	p[0] = (value>>24)&0xff;
	p[1] = (value>>16)&0xff;
	p[2] = (value>>8)&0xff;
	p[3] = value&0xff;
}


/* Validate an exact non-nil record before publishing its borrowed view. */
static int
disrunecheck(const unsigned char *data, unsigned int bytes, DisRuneView *view)
{
	unsigned int count;

	if(data == 0 || view == 0 || bytes < 4 || (unsigned int)-1 != 0xffffffffU)
		return 0;
	count = disruneget(data);
	if(count > (bytes-4)/4 || count*4 != bytes-4)
		return 0;
	view->values = data+4;
	view->count = count;
	return 1;
}


/* Read one Rune from a validated view without deciding its comparison order. */
static int
disruneat(const DisRuneView *view, unsigned int index, unsigned int *value)
{
	if(view == 0 || value == 0 || view->values == 0 || index >= view->count)
		return 0;
	*value = disruneget(view->values+index*4);
	return 1;
}


/* Encode a borrowed non-overlapping Rune array; preflight leaves output intact.
 * Nil is a separate reference initializer, never a special count here. */
static int
disrunewrite(unsigned char *output, unsigned int capacity, const unsigned int *values,
	unsigned int count, unsigned int *bytes)
{
	unsigned int i, length;

	if(output == 0 || bytes == 0 || (count != 0 && values == 0) ||
	   (unsigned int)-1 != 0xffffffffU || capacity < 4 || count > (capacity-4)/4)
		return 0;
	length = 4+count*4;
	disruneput(output, count);
	for(i=0; i<count; i++)
		disruneput(output+4+i*4, values[i]);
	*bytes = length;
	return 1;
}

/* Compare validated strings in the agreed signed32 Rune lexicographic order.
 * Null views represent VM nil strings and compare equal to empty contents;
 * callers retain the distinct nil/empty ownership and wire-reference tags. */
static int
disrunecompare(const DisRuneView *left, const DisRuneView *right, int *order)
{
	unsigned int nl, nr, n, i, a, b;

	if(order == 0 || (unsigned int)-1 != 0xffffffffU)
		return 0;
	nl = left == 0 ? 0 : left->count;
	nr = right == 0 ? 0 : right->count;
	if((nl != 0 && left->values == 0) || (nr != 0 && right->values == 0))
		return 0;
	n = nl < nr ? nl : nr;
	for(i=0; i<n; i++){
		a = disruneget(left->values+i*4)^0x80000000U;
		b = disruneget(right->values+i*4)^0x80000000U;
		if(a != b){
			*order = a < b ? -1 : 1;
			return 1;
		}
	}
	*order = nl < nr ? -1 : nl > nr;
	return 1;
}

#endif
