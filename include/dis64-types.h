/* Agreed structural type descriptors for Dis64, independent of native C structs. */
#ifndef DIS64_TYPES_H
#define DIS64_TYPES_H // Dis64 structural descriptor codec guard.

#include "dis64.h"
#include "dis-layout.h"

typedef struct Dis64TypeTable Dis64TypeTable;
struct Dis64TypeTable {
	const unsigned char *data; // Complete immutable type payload with count.
	unsigned int bytes; // Checked serialized extent.
	unsigned int count; // Ordinal descriptor IDs.
};

typedef struct Dis64Type Dis64Type;
struct Dis64Type {
	unsigned int bytes; // Object/array stride after natural padding.
	unsigned int align; // Target alignment: one, two, four or eight.
	const unsigned char *map; // Full map8; not a host pointer array.
	unsigned int mapbytes; // Exact serialized bitmap length.
};


/* Validate structural alignment and a full map without assigning a type-kind ID. */
static int
dis64typevalid(const Dis64Type *type)
{
	DisAbi abi;
	unsigned int i;

	if(type == 0 || (unsigned int)-1 != 0xffffffffU || type->align == 0 || type->align > 8 ||
	   (type->align&(type->align-1)) != 0 || type->bytes%type->align != 0 ||
	   (type->bytes == 0 && type->align != 1) || !disabinit(&abi, 8))
		return 0;
	if(!disabmap(&abi, type->bytes, type->map, type->mapbytes))
		return 0;
	if(type->align < 8){
		for(i=0; i<type->mapbytes; i++)
			if(type->map[i] != 0)
				return 0;
	}
	return 1;
}


/* Check every descriptor before publishing a borrowed type-table view. */
static int
dis64typescheck(const unsigned char *data, unsigned int bytes,
	unsigned int countlimit, unsigned int objectlimit, Dis64TypeTable *result)
{
	Dis64TypeTable view;
	Dis64Type type;
	unsigned int i, at;

	if(data == 0 || result == 0 || bytes < 4 || (unsigned int)-1 != 0xffffffffU)
		return 0;
	view.count = dis64u32(data);
	if(view.count > countlimit || view.count > (bytes-4)/8)
		return 0;
	at = 4;
	for(i=0; i<view.count; i++){
		if(bytes-at < 8)
			return 0;
		type.bytes = dis64u32(data+at);
		type.align = dis64u32(data+at+4);
		type.mapbytes = type.bytes/64+(type.bytes%64 != 0);
		at += 8;
		if(type.bytes > objectlimit || type.mapbytes > bytes-at)
			return 0;
		type.map = data+at;
		if(!dis64typevalid(&type))
			return 0;
		at += type.mapbytes;
	}
	if(at != bytes)
		return 0;
	view.data = data;
	view.bytes = bytes;
	*result = view;
	return 1;
}


/* Resolve a descriptor from a previously checked immutable table. */
static int
dis64typeget(const Dis64TypeTable *types, unsigned int index, Dis64Type *result)
{
	Dis64Type type;
	unsigned int i, at;

	if(types == 0 || result == 0 || types->data == 0 || index >= types->count)
		return 0;
	at = 4;
	for(i=0; i<=index; i++){
		if(at > types->bytes || types->bytes-at < 8)
			return 0;
		type.bytes = dis64u32(types->data+at);
		type.align = dis64u32(types->data+at+4);
		type.mapbytes = type.bytes/64+(type.bytes%64 != 0);
		at += 8;
		if(type.mapbytes > types->bytes-at)
			return 0;
		type.map = types->data+at;
		if(i == index){
			*result = type;
			return 1;
		}
		at += type.mapbytes;
	}
	return 0;
}


/* Emit after full preflight; input descriptors/maps must not overlap output/written. */
static int
dis64typeswrite(unsigned char *output, unsigned int capacity,
	const Dis64Type *types, unsigned int count, unsigned int *written)
{
	unsigned int i, j, total, at;

	if(output == 0 || written == 0 || capacity < 4 || (unsigned int)-1 != 0xffffffffU ||
	   (count != 0 && types == 0) ||
	   count > (capacity-4)/8)
		return 0;
	total = 4;
	for(i=0; i<count; i++){
		if(!dis64typevalid(&types[i]) || capacity-total < 8 ||
		   types[i].mapbytes > capacity-total-8)
			return 0;
		total += 8+types[i].mapbytes;
	}
	dis64put32(output, count);
	at = 4;
	for(i=0; i<count; i++){
		dis64put32(output+at, types[i].bytes);
		dis64put32(output+at+4, types[i].align);
		at += 8;
		for(j=0; j<types[i].mapbytes; j++)
			output[at++] = types[i].map[j];
	}
	*written = total;
	return 1;
}
#endif
