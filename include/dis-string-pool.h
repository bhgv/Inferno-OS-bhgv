/* Agreed Dis64 string payload: pool count followed by complete non-nil Rune records. */
#ifndef DIS_STRING_POOL_H
#define DIS_STRING_POOL_H // Shared structural string-pool codec guard.

#include "dis-rune.h"

typedef struct DisStringPoolView DisStringPoolView;
struct DisStringPoolView {
	const unsigned char *data; // Borrowed complete immutable pool, including its count.
	unsigned int bytes; // Exact serialized extent, independent of host pointer width.
	unsigned int count; // Ordinal IDs are 0..count-1; content duplicates are legal.
};

typedef struct DisStringPoolRecord DisStringPoolRecord;
struct DisStringPoolRecord {
	const unsigned char *data; // Borrowed complete serialized non-nil Rune record.
	unsigned int bytes; // Includes that record's rune_count, never a native pointer.
};


/* Validate borrowed pool bytes and explicit record resource limit before publication.
 * data spans bytes accessible immutable bytes; result is independent caller storage.
 * Return one with a borrowed view on success, zero with result unchanged on failure.
 * All records must consume the payload exactly. No allocation, heap or blocking. */
static int
disstringpoolcheck(const unsigned char *data, unsigned int bytes,
	unsigned int recordlimit, DisStringPoolView *result)
{
	DisStringPoolView view;
	unsigned int i, at, count;

	if(data == 0 || result == 0 || bytes < 4 || (unsigned int)-1 != 0xffffffffU)
		return 0;
	view.count = disruneget(data);
	if(view.count > recordlimit || view.count > (bytes-4)/4)
		return 0;
	at = 4;
	for(i=0; i<view.count; i++){
		if(bytes-at < 4)
			return 0;
		count = disruneget(data+at);
		if(count > (bytes-at-4)/4)
			return 0;
		at += 4+count*4;
	}
	if(at != bytes)
		return 0;
	view.data = data;
	view.bytes = bytes;
	*result = view;
	return 1;
}


/* Resolve an ordinal into the existing borrowed Rune view without normalization.
 * pool is a previously validated immutable view; result remains unchanged on error.
 * Return one for an existing ID, zero otherwise. Lookup is linear in index;
 * a runtime may build a separate checked offset index without changing wire IDs. */
static int
disstringpoolget(const DisStringPoolView *pool, unsigned int index, DisRuneView *result)
{
	unsigned int i, at, count, length;

	if(pool == 0 || result == 0 || pool->data == 0 || pool->bytes < 4 || index >= pool->count)
		return 0;
	at = 4;
	for(i=0; i<=index; i++){
		if(at > pool->bytes || pool->bytes-at < 4)
			return 0;
		count = disruneget(pool->data+at);
		if(count > (pool->bytes-at-4)/4)
			return 0;
		length = 4+count*4;
		if(i == index)
			return disrunecheck(pool->data+at, length, result);
		at += length;
	}
	return 0;
}


/* Compare two checked pool IDs using the agreed signed32 Rune ordering.
 * pool/IDs are borrowed; order is separate caller-owned output. Return one on
 * success, zero without changing order on an invalid ID. Nil references are
 * outside this pool; content-equal records need not have the same ordinal ID. */
static int
disstringpoolcompare(const DisStringPoolView *pool, unsigned int left,
	unsigned int right, int *order)
{
	DisRuneView a, b;

	if(order == 0 || !disstringpoolget(pool, left, &a) || !disstringpoolget(pool, right, &b))
		return 0;
	return disrunecompare(&a, &b, order);
}


/* Concatenate checked complete non-nil records into a canonical pool.
 * records and all input spans are borrowed immutable, valid for the call, and
 * must not overlap output or written. Return one after publishing full bytes
 * and length, zero leaving both unchanged on any preflight/capacity failure.
 * Count/length fields are BE u32; no normalization, interning or VM allocation. */
static int
disstringpoolwrite(unsigned char *output, unsigned int capacity,
	const DisStringPoolRecord *records, unsigned int count, unsigned int *written)
{
	DisRuneView view;
	unsigned int i, j, total, at;

	if(output == 0 || written == 0 || capacity < 4 || (unsigned int)-1 != 0xffffffffU ||
	   (count != 0 && records == 0) || count > (capacity-4)/4)
		return 0;
	total = 4;
	for(i=0; i<count; i++){
		if(records[i].bytes > capacity-total ||
		   !disrunecheck(records[i].data, records[i].bytes, &view))
			return 0;
		total += records[i].bytes;
	}
	/* Every fallible validation precedes the first write. */
	disruneput(output, count);
	at = 4;
	for(i=0; i<count; i++)
		for(j=0; j<records[i].bytes; j++)
			output[at++] = records[i].data[j];
	*written = total;
	return 1;
}
#endif
