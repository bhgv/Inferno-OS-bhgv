#define _GNU_SOURCE 1 // Expose the Linux native exception-enable API to the independent oracle.
#include "lib9.h"
#include "mathi.h"
#include <fenv.h>
#include <pthread.h>
#include <signal.h>
#include <sys/wait.h>
#include <stdio.h>


/* Keep every environment predicate active in optimized and NDEBUG builds. */
static void
check(int condition)
{
	if(!condition)
		abort();
}


/* Compute expected hardware flags independently from the production conversion. */
static int
hardware(int flags)
{
	int result;

	result = 0;
	if(flags & 1) result |= FE_INVALID;
	if(flags & 2) result |= FE_DIVBYZERO;
	if(flags & 4) result |= FE_OVERFLOW;
	if(flags & 8) result |= FE_UNDERFLOW;
	if(flags & 16) result |= FE_INEXACT;
	return result;
}


/* Exercise a separate thread's real environment while the parent keeps its own. */
static void*
threadenvironment(void *unused)
{
	USED(unused);
	check(getFPcontrol() == RND_PINF);
	FPcontrol(RND_NINF|INVAL, RND_MASK|31);
	check(fegetround() == FE_DOWNWARD && fegetexcept() == FE_INVALID);
	FPstatus(INEX, 31);
	check(fetestexcept(FE_ALL_EXCEPT) == FE_INEXACT);
	return nil;
}


/* Verify all masked status transitions, rounding, trap enables and thread isolation. */
int
main(void)
{
	int initial, flags, mask, round, rm, next, before, status;
	int modes[] = {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO};
	int masks[] = {0, 0x100, 0x200, RND_MASK};
	ulong expected;
	unsigned long checks;
	pthread_t thread;
	pid_t child;

	checks = 0;
	fesetenv(FE_DFL_ENV);
	for(initial = 0; initial < 32; initial++)
	for(flags = 0; flags < 32; flags++)
	for(mask = 0; mask < 32; mask++){
		FPstatus(initial, 31);
		check(getFPstatus() == initial);
		check(FPstatus(flags, mask) == (initial & mask));
		expected = (flags & mask) | (initial & ~mask);
		check(getFPstatus() == expected);
		check(fetestexcept(FE_ALL_EXCEPT) == hardware(expected));
		check(getFPcontrol() == 0);
		checks++;
	}
	FPstatus(0, 31);
	for(initial = 0; initial < 32; initial++)
	for(flags = 0; flags < 32; flags++)
	for(mask = 0; mask < 32; mask++)
	for(round = 0; round < 4; round++)
	for(rm = 0; rm < nelem(masks); rm++){
		FPcontrol(initial|RND_Z, 31|RND_MASK);
		before = initial|RND_Z;
		check(FPcontrol(flags|(round<<8), mask|masks[rm]) == (before & (mask|masks[rm])));
		next = ((flags|(round<<8)) & (mask|masks[rm])) | (before & ~(mask|masks[rm]));
		check(getFPcontrol() == next);
		check(fegetexcept() == hardware(next & 31));
		check(fegetround() == modes[(next & RND_MASK)>>8]);
		checks++;
	}
	FPcontrol(0, 31|RND_MASK);
	FPcontrol(RND_PINF, RND_MASK);
	check(pthread_create(&thread, nil, threadenvironment, nil) == 0);
	check(pthread_join(thread, nil) == 0);
	check(getFPcontrol() == RND_PINF && getFPstatus() == 0);
	FPinit();
	check(getFPcontrol() == (INVAL|ZDIV|OVFL|UNFL));
	check(getFPstatus() == 0);
	FPcontrol(0, 31);
	/* Publishing a sticky enabled exception must not itself execute a trapping operation. */
	child = fork();
	check(child >= 0);
	if(child == 0){
		FPcontrol(ZDIV, 31);
		FPstatus(ZDIV, 31);
		check(getFPstatus() == ZDIV);
		FPcontrol(0, 31);
		_exit(0);
	}
	check(waitpid(child, &status, 0) == child);
	check(WIFEXITED(status) && WEXITSTATUS(status) == 0);
	child = fork();
	check(child >= 0);
	if(child == 0){
		volatile double one, zero, result;

		one = 1.0;
		zero = 0.0;
		FPcontrol(ZDIV, 31);
		result = one/zero;
		USED(result);
		_exit(71);
	}
	check(waitpid(child, &status, 0) == child);
	check(WIFSIGNALED(status) && WTERMSIG(status) == SIGFPE);
	fesetenv(FE_DFL_ENV);
	printf("PASS native FP: %lu status/control transitions, thread isolation and real SIGFPE\n", checks);
	return 0;
}
