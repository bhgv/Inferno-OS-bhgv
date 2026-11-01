#ifndef INFERNO_NATIVE64_CALLER_H
#define INFERNO_NATIVE64_CALLER_H // Opt-in serialized caller lease; not a file calling capability or wire tag.
#ifdef DIS_NATIVE64_CALLER_GUARD
#if DIS_NATIVE64_CALLER_GUARD != 1
#error DIS_NATIVE64_CALLER_GUARD must be 1 when explicitly selected
#endif
#if !defined(DIS_WORD32) || DIS_WORD32 != 1
#error native64 caller guard requires DIS_WORD32=1
#endif
#if !defined(DIS_NATIVE64_FIXED_MATH_CALL) || DIS_NATIVE64_FIXED_MATH_CALL != 1
#error native64 caller guard requires the explicit fixed Math experiment
#endif
#include "native64-frame-cell.h"


/* Link-time coherence: owning scheduler, interpreter and stack must select the same guard profile. */
int native64callerruntime(void);
int native64callervmguard(void);
int native64callerstackguard(void);

typedef struct Native64Caller Native64Caller;

struct Native64Caller
{
	Native64Caller *self; // Noncopyable lease identity; nil after retirement.
	Prog *prog; // Scheduler-owned process, protected by native64holds until clear.
	Type *type; // One retained generated caller descriptor.
	uchar *frame, *extent; // Exact current frame/extent captured for this generation.
	uvlong epoch; // Captured monotonic invalidation generation.
	int pid; // Captured process identity, checked before reading stack storage.
	unsigned int offset; // Trusted compiler-selected aligned eight-byte non-reference field.
};


/* Never wrap generations: saturation permanently prevents new leases for this Prog. */
static void
native64calleradvance(Prog *prog)
{
	if(prog != nil && prog != H && prog->native64epoch != ~(uvlong)0)
		prog->native64epoch++;
}


/* Only the live original lease may consult the protected process or its stack. */
static int
native64callerready(const Native64Caller *caller)
{
	Prog *prog;

	if(caller == nil || caller->self != caller || caller->prog == nil)
		return 0;
	prog = caller->prog;
	return prog == currun() && prog->native64holds == 1 && prog->pid == caller->pid &&
		!(prog->flags & Pkilled) && prog->kill == nil && prog->state == Pready &&
		prog->native64epoch == caller->epoch && prog->native64epoch != ~(uvlong)0 &&
		prog->R.FP == caller->frame && prog->R.EX == caller->extent;
}


/* Acquire without allocation; the caller already supplies a live scheduler-rooted Prog. */
static int
native64callerbegin(Native64Caller *caller, Prog *prog, Type *type, unsigned int offset)
{
	Native64Caller owned;

	if(caller == nil || caller->self != nil || prog == nil || prog == H ||
	   native64callerruntime() != 1 || prog != currun() || prog->pid <= 0 || prog->native64holds != 0 || (prog->flags & Pkilled) ||
	   prog->kill != nil || prog->state != Pready || prog->native64epoch >= ~(uvlong)0-1 ||
	   native64framecell(prog, type, offset) == nil || type->ref == 0x7fffffff)
		return 0;
	native64calleradvance(prog);
	type->ref++;
	prog->native64holds = 1;
	owned.self = caller; owned.prog = prog; owned.type = type;
	owned.frame = prog->R.FP; owned.extent = prog->R.EX;
	owned.epoch = prog->native64epoch; owned.pid = prog->pid; owned.offset = offset;
	*caller = owned;
	return 1;
}


/* Resolve generation and identity before dereferencing the possibly retired frame. */
static void*
native64callerresolve(Native64Caller *caller)
{
	if(!native64callerready(caller))
		return nil;
	return native64framecell(caller->prog, caller->type, caller->offset);
}


/* Move a still-live logical result within the retained active extent; stale leases cannot revive. */
static int
native64callerrebind(Native64Caller *caller, void *frame)
{
	Prog candidate, *prog;

	if(frame == nil || frame == H || !native64callerready(caller) ||
	   caller->epoch >= ~(uvlong)0-1)
		return 0;
	prog = caller->prog;
	memset(&candidate, 0, sizeof(candidate));
	candidate.R = prog->R;
	candidate.R.FP = frame;
	if(native64framecell(&candidate, caller->type, caller->offset) == nil)
		return 0;
	native64calleradvance(prog);
	prog->R.FP = candidate.R.FP;
	caller->frame = prog->R.FP; caller->extent = prog->R.EX; caller->epoch = prog->native64epoch;
	return 1;
}


/* Detach first, then release exactly one process hold and descriptor owner; retirement is nonthrowing. */
static void
native64callerclear(Native64Caller *caller)
{
	Prog *prog;
	Type *type;

	if(caller == nil || caller->self != caller)
		return;
	prog = caller->prog; type = caller->type;
	caller->self = nil; caller->prog = nil; caller->type = nil;
	caller->frame = caller->extent = nil; caller->epoch = 0;
	prog->native64holds = 0;
	freetype(type);
}
#endif
#endif
