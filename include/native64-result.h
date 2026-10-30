/* Private C result ownership guard; no shared calling-signature or wire admission. */
#ifndef NATIVE64_RESULT_H
#define NATIVE64_RESULT_H // Explicit protected single-result transaction.

#include <kernel.h>

typedef struct Native64Result Native64Result;

struct Native64Result {
	void *value; // One owned reference, initially H; producer adopts through the helper.
	int rooted; // One successful external pin, never inferred from another alias's root.
};

typedef struct Native64ResultCall Native64ResultCall;

struct Native64ResultCall {
	Native64Result result; // State remains outside the runtime kprotect/setjmp frame.
	int (*produce)(Native64Result*, void*); // Trusted C producer; zero declines publication.
	void *argument; // Borrowed callback context, live through protected cleanup.
};


/* Adopt one owner before fallible pin allocation; on refusal the caller retains its owner.
 * A pin allocation error leaves value owned by this guard, with rooted still zero. */
static int
native64resultadopt(Native64Result *result, void *owned)
{
	if(result == nil || result->value != H || result->rooted || owned == nil)
		return 0;
	result->value = owned;
	if(owned != H){
		ptradd(D2H(owned));
		result->rooted = 1;
	}
	return 1;
}


/* Remove only this guard's pin and clear its state before a destructor can observe it. */
static void
native64resultclear(Native64Result *result)
{
	void *owned;
	int rooted;

	owned = result->value;
	rooted = result->rooted;
	result->value = H;
	result->rooted = 0;
	if(rooted)
		ptrdel(D2H(owned));
	destroy(owned);
}


/* Keep the mutable owner outside the platform-specific protected error frame. */
static int
native64resultproduce(void *argument)
{
	Native64ResultCall *call;

	call = argument;
	return call->produce(&call->result, call->argument);
}


/* Publish one result only after producer success; preserve the old owner on decline/error.
 * slot must be a live caller-mapped owner cell; producer must not mutate that cell.
 * Destructors must not throw. C producer owns any partial tree before adoption.
 * Existing single-owner pin allocation must not yield/collect an unpinned new owner. */
static int
native64resultcall(void *slot, int (*produce)(Native64Result*, void*), void *argument)
{
	Native64ResultCall call;
	void *old, *owned;
	int rooted;
	char saved[ERRMAX];

	if(slot == nil || slot == H || (uintptr)slot > (uintptr)-1-sizeof(void*) || produce == 0)
		return 0;
	call.result.value = H;
	call.result.rooted = 0;
	call.produce = produce;
	call.argument = argument;
	if(!kprotect(native64resultproduce, &call)){
		kgerrstr(saved, sizeof(saved));
		native64resultclear(&call.result);
		kwerrstr("%s", saved);
		return 0;
	}
	owned = call.result.value;
	rooted = call.result.rooted;
	memmove(&old, slot, sizeof(old));
	memmove(slot, &owned, sizeof(owned));
	call.result.value = H;
	call.result.rooted = 0;
	if(rooted)
		ptrdel(D2H(owned));
	destroy(old);
	return 1;
}

#endif
