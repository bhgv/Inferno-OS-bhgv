/* Scalar initializer framing and pre-publication native data initialization. */
#ifndef DIS64_DATA_H
#define DIS64_DATA_H // Dis64 scalar initializer codec guard.

#include "dis64-types.h"
#include "dis64-scalar.h"

typedef struct Dis64Initializer Dis64Initializer;
struct Dis64Initializer {
	unsigned int offset; // Target byte offset, aligned for scalar width.
	unsigned int kind; // Scalar namespace1..4, never a native pointer kind.
	unsigned int high; // High32 payload bits for big/real; zero for int/bool.
	unsigned int low; // Low32 payload bits; bool permits only zero or one.
};

typedef struct Dis64Initializers Dis64Initializers;
struct Dis64Initializers {
	const unsigned char *data; // Complete immutable count/records payload.
	unsigned int bytes; // Exact serialized extent.
	unsigned int count; // Number of checked non-overlapping scalar writes.
};


/* Check relative alignment, full extent and absence of any mapped reference overlap. */
static int
dis64initspan(const Dis64Type *type, unsigned int offset, unsigned int width)
{
	unsigned int slot;

	if(width == 0 || offset%width != 0 || offset > type->bytes || type->bytes-offset < width)
		return 0;
	slot = offset/8;
	return (type->map[slot/8]&(0x80U>>(slot%8))) == 0;
}


/* Validate all records and pairwise extents before publishing a borrowed view. */
static int
dis64initializerscheck(const unsigned char *data, unsigned int bytes,
	const Dis64Type *type, unsigned int limit, Dis64Initializers *result)
{
	Dis64Initializers view;
	unsigned int i, j, at, previous, offset, kind, width, other, otherwidth;

	if(data == 0 || result == 0 || bytes < 4 || !dis64typevalid(type))
		return 0;
	view.count = dis64u32(data);
	if(view.count > limit || view.count > (bytes-4)/12)
		return 0;
	at = 4;
	for(i=0; i<view.count; i++){
		if(bytes-at < 8)
			return 0;
		offset = dis64u32(data+at);
		kind = dis64u32(data+at+4);
		width = dis64scalarwidth(kind);
		if(width > bytes-at-8 || !dis64initspan(type, offset, width))
			return 0;
		if(kind == Dis64Bool && dis64u32(data+at+8) > 1)
			return 0;
		previous = 4;
		for(j=0; j<i; j++){
			other = dis64u32(data+previous);
			otherwidth = dis64scalarwidth(dis64u32(data+previous+4));
			if(offset < other+otherwidth && other < offset+width)
				return 0;
			previous += 8+otherwidth;
		}
		at += 8+width;
	}
	if(at != bytes)
		return 0;
	view.data = data;
	view.bytes = bytes;
	*result = view;
	return 1;
}


/* Emit only after validating maps, pairwise spans and the complete capacity.
 * Descriptor, records, output and written are independent borrowed spans. */
static int
dis64initializerswrite(unsigned char *output, unsigned int capacity,
	const Dis64Type *type, const Dis64Initializer *records, unsigned int count, unsigned int *written)
{
	unsigned int i, j, at, total, width, otherwidth;

	if(output == 0 || written == 0 || capacity < 4 || !dis64typevalid(type) ||
	   (count != 0 && records == 0) || count > (capacity-4)/12)
		return 0;
	total = 4;
	for(i=0; i<count; i++){
		width = dis64scalarwidth(records[i].kind);
		if(capacity-total < 8 || width > capacity-total-8 ||
		   !dis64initspan(type, records[i].offset, width) ||
		   (width == 4 && records[i].high != 0) ||
		   (records[i].kind == Dis64Bool && records[i].low > 1))
			return 0;
		for(j=0; j<i; j++){
			otherwidth = dis64scalarwidth(records[j].kind);
			if(records[i].offset < records[j].offset+otherwidth &&
			   records[j].offset < records[i].offset+width)
				return 0;
		}
		total += 8+width;
	}
	dis64put32(output, count);
	at = 4;
	for(i=0; i<count; i++){
		width = dis64scalarwidth(records[i].kind);
		dis64put32(output+at, records[i].offset);
		dis64put32(output+at+4, records[i].kind);
		at += 8;
		if(width == 8){
			dis64put32(output+at, records[i].high);
			at += 4;
		}
		dis64put32(output+at, records[i].low);
		at += 4;
	}
	*written = total;
	return 1;
}


/* Initialize unpublished native ABI1 storage after rechecking every fallible input.
 * Caller supplies exact runtime nil bytes; no all-zero pointer assumption is made.
 * Output must not overlap the immutable payload/map/nil storage or the view. */
static int
dis64initializersapply(const Dis64Initializers *input, const Dis64Type *type,
	unsigned char *output, unsigned int capacity, const unsigned char *nilbytes)
{
	Dis64Initializers checked;
	unsigned int i, j, at, offset, kind, width, bits32;
	unsigned long long bits64;
	const unsigned char *native;

	if(input == 0 || (output == 0 && type != 0 && type->bytes != 0) ||
	   sizeof(void*) != 8 || sizeof(bits32) != 4 || sizeof(bits64) != 8 ||
	   !dis64initializerscheck(input->data, input->bytes, type, input->count, &checked) ||
	   checked.count != input->count || capacity < type->bytes)
		return 0;
	for(i=0; i<type->mapbytes; i++)
		if(type->map[i] != 0 && nilbytes == 0)
			return 0;
	for(i=0; i<type->bytes; i++)
		output[i] = 0;
	for(i=0; i<type->mapbytes; i++)
		for(j=0; j<8; j++)
			if((type->map[i]&(0x80U>>j)) != 0){
				offset = (i*8+j)*8;
				for(at=0; at<8; at++)
					output[offset+at] = nilbytes[at];
			}
	at = 4;
	for(i=0; i<checked.count; i++){
		offset = dis64u32(checked.data+at);
		kind = dis64u32(checked.data+at+4);
		width = dis64scalarwidth(kind);
		at += 8;
		if(width == 4){
			bits32 = dis64u32(checked.data+at);
			native = (const unsigned char*)&bits32;
		}else{
			bits64 = ((unsigned long long)dis64u32(checked.data+at)<<32)|dis64u32(checked.data+at+4);
			native = (const unsigned char*)&bits64;
		}
		for(j=0; j<width; j++)
			output[offset+j] = native[j];
		at += width;
	}
	return 1;
}
#endif
