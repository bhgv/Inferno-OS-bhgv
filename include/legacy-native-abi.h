#ifndef INFERNO_LEGACY_NATIVE_ABI_H
#define INFERNO_LEGACY_NATIVE_ABI_H // The installed legacy builtin tables describe int32/ref32 frames only.


/* Refuse known incompatible native tables before registering modules or executing a legacy frame. */
static int
legacynativeabi(void)
{
	return sizeof(WORD) == 4 && sizeof(void*) == 4;
}
#endif
