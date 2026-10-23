#ifndef NATIVEPTRMAP_H
#define NATIVEPTRMAP_H

/* Describe a managed field in the active VM's native-pointer bitmap quantum. */
static void
nativeptrmap(uchar *map, ulong mapsize, ulong size, ulong offset)
{
	ulong slot;

	if(offset%sizeof(void*) != 0 || offset > size || size-offset < sizeof(void*))
		error(exType);
	slot = offset/sizeof(void*);
	if(slot/8 >= mapsize)
		error(exType);
	map[slot/8] |= 0x80 >> (slot%8);
}

#define nativefieldmap(map, record, field) \
	nativeptrmap(map, sizeof(map), sizeof(record), offsetof(record, field))

#endif
