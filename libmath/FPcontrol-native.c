#define _GNU_SOURCE 1 // Linux exposes exception-enable queries through its GNU fenv API.
#include "lib9.h"
#include "mathi.h"
#include <fenv.h>

#if !defined(__linux__) || !defined(__GLIBC__)
#error native_FPcontrol_requires_Linux_glibc_fenv
#endif


/* Translate language exception bits without depending on host register encodings. */
static int
nativeexceptions(ulong flags)
{
	int result;

	result = 0;
	if(flags & INVAL) result |= FE_INVALID;
	if(flags & ZDIV) result |= FE_DIVBYZERO;
	if(flags & OVFL) result |= FE_OVERFLOW;
	if(flags & UNFL) result |= FE_UNDERFLOW;
	if(flags & INEX) result |= FE_INEXACT;
	return result;
}


/* Recover language exception flags from the native floating-point environment. */
static ulong
languageexceptions(int flags)
{
	ulong result;

	result = 0;
	if(flags & FE_INVALID) result |= INVAL;
	if(flags & FE_DIVBYZERO) result |= ZDIV;
	if(flags & FE_OVERFLOW) result |= OVFL;
	if(flags & FE_UNDERFLOW) result |= UNFL;
	if(flags & FE_INEXACT) result |= INEX;
	return result;
}


/* Read sticky exceptions from the active thread rather than a software shadow. */
ulong
getFPstatus(void)
{
	return languageexceptions(fetestexcept(FE_ALL_EXCEPT));
}


/* Replace selected sticky flags without raising a newly enabled exception. */
ulong
FPstatus(ulong flags, ulong mask)
{
	fenv_t environment;
	fexcept_t status;
	ulong old;
	int selected;

	old = getFPstatus();
	selected = nativeexceptions(mask);
	if(selected != 0){
		feholdexcept(&environment);
		feraiseexcept(nativeexceptions(flags & mask));
		fegetexceptflag(&status, nativeexceptions(flags & mask));
		fesetenv(&environment);
		feclearexcept(selected);
		fesetexceptflag(&status, nativeexceptions(flags & mask));
	}
	return old & mask;
}


/* Report the real trap enables and rounding mode used by native scalar arithmetic. */
ulong
getFPcontrol(void)
{
	ulong result;

	result = languageexceptions(fegetexcept());
	switch(fegetround()){
	case FE_DOWNWARD: result |= RND_NINF; break;
	case FE_UPWARD: result |= RND_PINF; break;
	case FE_TOWARDZERO: result |= RND_Z; break;
	}
	return result;
}


/* Change only selected control bits, preserving unrelated flags and rounding. */
ulong
FPcontrol(ulong flags, ulong mask)
{
	ulong old, next;
	int rounding, exceptions;

	old = getFPcontrol();
	next = (flags & mask) | (old & ~mask);
	switch(next & RND_MASK){
	default: rounding = FE_TONEAREST; break;
	case RND_NINF: rounding = FE_DOWNWARD; break;
	case RND_PINF: rounding = FE_UPWARD; break;
	case RND_Z: rounding = FE_TOWARDZERO; break;
	}
	if(mask & RND_MASK)
		fesetround(rounding);
	exceptions = nativeexceptions(mask);
	if(exceptions != 0){
		fedisableexcept(exceptions);
		feenableexcept(nativeexceptions(next) & exceptions);
	}
	return old & mask;
}


/* Initialize the host compiler with the same rounding and traps as the Linux port. */
void
FPinit(void)
{
	fesetenv(FE_DFL_ENV);
	FPcontrol(INVAL|ZDIV|OVFL|UNFL, INVAL|ZDIV|OVFL|UNFL|INEX|RND_MASK);
}
