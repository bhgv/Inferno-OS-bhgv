#include "lib9.h"
#include "isa.h"
#include "interp.h"
#define LIMBO_NATIVE64_TABLES // Include both qualified method specifications.
#include "methods-native64.h"
#include "native64-bind.h"
#include <stdlib.h>
#include <string.h>


/* Method fixtures supply distinct symbols; this test concerns names and binding. */
void
MethodProbe_A_f(void *frame)
{
	USED(frame);
}


/* Preserve the second ADT method despite its identical unqualified source name. */
void
MethodProbe_B_f(void *frame)
{
	USED(frame);
}


/* Both methods must bind simultaneously; a bare f/f namespace would collide. */
int
main(void)
{
	const LimboNative64Entry *selected[] = {
		&limbo_native64_MethodProbe_A_f_entry,
		&limbo_native64_MethodProbe_B_f_entry,
	};
	const LimboNative64Entry *output[2];

	if(!limbo_native64_MethodProbe_layout() || limbo_native64_MethodProbe_spec_count != 2 ||
	   strcmp(selected[0]->name, "A.f") != 0 || strcmp(selected[1]->name, "B.f") != 0 ||
	   !native64bind(limbo_native64_MethodProbe_specs, limbo_native64_MethodProbe_spec_count,
		selected, 2, output, 2) || output[0] != selected[0] || output[1] != selected[1])
		abort();
	return 0;
}
