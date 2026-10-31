#define LIMBO_NATIVE64_TABLES // Validate every fixed Sys specification without linking all OS callbacks.
#include "sys-native64.h"
#include "native64-bind.h"
extern void sysmodinit(void);

#ifdef NATIVE64_NO_LSAN


/* Optional host limitation: process leak scans are unavailable; allocation checks remain active. */
int
__lsan_is_turned_off(void)
{
	return 1;
}
#endif


/* Unsupported paths must report a VM error before reading an arbitrary frame or allocating. */
static int
unsupported(void *pointer)
{
	int *which;

	which = pointer;
	switch(*which){
	case 0: Sys_print((void*)1); break;
	case 1: Sys_fprint((void*)1); break;
	case 2: Sys_sprint((void*)1); break;
	case 3: Sys_aprint((void*)1); break;
	case 4: sysmodinit(); break;
	}
	return 1;
}


/* Do not silently pass fixed-prefix metadata to a legacy variadic or registration implementation. */
int
main(void)
{
	int which, before;
	const char *expected;

	baseline.ref = 7;
	if(!limbo_native64_Sys_layout() ||
	   !native64bindcheck(limbo_native64_Sys_specs, limbo_native64_Sys_spec_count, nil, 0))
		abort();
	for(which=0; which<5; which++){
		before = attempts;
		expected = which == 4 ? "experimental native64 Sys registration is not supported" :
			"native64 Sys variadic calls are not supported";
		if(kprotect(unsupported, &which) || strcmp(diagnostic, expected) != 0 ||
		   attempts != before || live != 0 || worker.nerr != 0 || modules != &baseline)
			abort();
	}
	puts("PASS native64 Sys: four variadic refusals and pre-allocation registration refusal");
	return 0;
}
