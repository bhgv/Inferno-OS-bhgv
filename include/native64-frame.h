/* Private generated-frame descriptors; this does not admit a wire module or native call. */
#ifndef NATIVE64_FRAME_H
#define NATIVE64_FRAME_H // Explicit native64-to-native-GC descriptor adapter.

#include "native64-bind.h"


/* Copy a previously bound frame map into an owned ordinary runtime descriptor.
 * The caller first checks generated C geometry and successful table binding.
 * Result/header/temp slots remain outside argument roots; caller roots are separate.
 * No source map is borrowed by the returned Type. Release it with freetype. */
static Type*
native64frametype(const LimboNative64Entry *record)
{
	if(sizeof(WORD) != 4 || sizeof(UWORD) != 4 || sizeof(void*) != 8 ||
	   !native64bindrecord(record, 1))
		return nil;
	return dtype(freeheap, record->frame_size, (uchar*)record->map, record->map_bytes);
}

#endif
