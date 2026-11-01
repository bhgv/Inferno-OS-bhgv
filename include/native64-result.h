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


typedef struct Native64ResolvedResult Native64ResolvedResult;

struct Native64ResolvedResult
{
	Native64ResultCall call; // Mutable result ownership remains outside the protected error frame.
	void *(*resolve)(void*); // Trusted resolver of the current live caller-mapped owner cell.
	void *slot; // Used only after the resolver returns, with no intervening allocating callback.
};


/* Produce and resolve under the same error owner; no destination address is cached before production. */
static int
native64resultresolve(void *argument)
{
	Native64ResolvedResult *resolved;
	void *old;

	resolved = argument;
	if(!native64resultproduce(&resolved->call))
		return 0;
	resolved->slot = resolved->resolve(resolved->call.argument);
	/* Nil is an explicit decline/cancellation; retain the resolver's diagnostic. */
	if(resolved->slot == nil)
		return 0;
	if(resolved->slot == H || (uintptr)resolved->slot > (uintptr)-1-sizeof(void*)){
		kwerrstr("bad native64 result destination");
		return 0;
	}
	memmove(&old, resolved->slot, sizeof(old));
	if(old == nil){
		kwerrstr("nil native64 result owner");
		return 0;
	}
	return 1;
}


/* Resolve a possibly relocated destination after production, preserving its old owner on every refusal.
 * The caller roots its logical destination and input owners; resolver returns a trusted live owner cell.
 * Resolver may allocate or throw, but must preserve the old logical result and report nil on cancellation.
 * Caller serializes destination access; no callback runs between resolution and publication.
 * Destructors must not throw or move the destination. */
static int
native64resultresolved(int (*produce)(Native64Result*, void*), void *(*resolve)(void*), void *argument)
{
	Native64ResolvedResult resolved;
	void *old, *owned;
	int rooted;
	char saved[ERRMAX];

	if(produce == nil || resolve == nil)
		return 0;
	resolved.call.result.value = H;
	resolved.call.result.rooted = 0;
	resolved.call.produce = produce;
	resolved.call.argument = argument;
	resolved.resolve = resolve;
	resolved.slot = nil;
	if(!kprotect(native64resultresolve, &resolved)){
		kgerrstr(saved, sizeof(saved));
		native64resultclear(&resolved.call.result);
		kwerrstr("%s", saved);
		return 0;
	}
	owned = resolved.call.result.value;
	rooted = resolved.call.result.rooted;
	memmove(&old, resolved.slot, sizeof(old));
	memmove(resolved.slot, &owned, sizeof(owned));
	resolved.call.result.value = H;
	resolved.call.result.rooted = 0;
	if(rooted)
		ptrdel(D2H(owned));
	destroy(old);
	return 1;
}

#endif
