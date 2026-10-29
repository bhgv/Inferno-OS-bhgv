#include <fenv.h>

static int benchfpactive; // Whether the numerical section owns the saved environment.
static fenv_t benchfp; // Complete saved host environment, including enabled trap state.
static ulong benchcontrol; // Logical control mask exposed by the test API adapter.


/* Translate real host sticky flags into the public VM status bits. */
static ulong
benchvmflags(int flags)
{
	ulong value;

	value = 0;
	if(flags&FE_INVALID) value |= INVAL;
	if(flags&FE_DIVBYZERO) value |= ZDIV;
	if(flags&FE_OVERFLOW) value |= OVFL;
	if(flags&FE_UNDERFLOW) value |= UNFL;
	if(flags&FE_INEXACT) value |= INEX;
	return value;
}


/* Translate the public sticky-status bits back to the real host environment. */
static int
benchnativeflags(ulong flags)
{
	int value;

	value = 0;
	if(flags&INVAL) value |= FE_INVALID;
	if(flags&ZDIV) value |= FE_DIVBYZERO;
	if(flags&OVFL) value |= FE_OVERFLOW;
	if(flags&UNFL) value |= FE_UNDERFLOW;
	if(flags&INEX) value |= FE_INEXACT;
	return value;
}


/* Change controls independently of sticky flags, mirroring the production platform API. */
ulong
FPcontrol(ulong value, ulong mask)
{
	ulong old;
	int flags;

	if(mask != (RND_MASK|INVAL|ZDIV|OVFL|UNFL|INEX) || (!benchfpactive && value != RND_NR))
		abort();
	old = benchcontrol;
	flags = fetestexcept(FE_ALL_EXCEPT);
	if(!benchfpactive){
		benchfpactive = 1;
		feholdexcept(&benchfp);
		fesetround(FE_TONEAREST);
	}else{
		fesetenv(&benchfp);
		benchfpactive = 0;
	}
	feclearexcept(FE_ALL_EXCEPT);
	feraiseexcept(flags);
	benchcontrol = (benchcontrol&~mask)|(value&mask);
	return old&mask;
}


/* Status restoration is tested separately: restoring controls must not hide a missing call. */
ulong
FPstatus(ulong value, ulong mask)
{
	ulong old, next;

	old = benchvmflags(fetestexcept(FE_ALL_EXCEPT));
	next = (old&~mask)|(value&mask);
	feclearexcept(FE_ALL_EXCEPT);
	feraiseexcept(benchnativeflags(next));
	return old&mask;
}
