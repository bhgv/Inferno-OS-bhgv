#ifndef INFERNO_NATIVE64_SYS_CALL_H
#define INFERNO_NATIVE64_SYS_CALL_H // Explicit trusted native64 C call contract, not serialized Dis64 metadata.
#include "native64-tail.h"

typedef struct Native64SysFormatRequest Native64SysFormatRequest;

struct Native64SysFormatRequest
{
	Prog *prog; // Borrowed VM process used for percent-r diagnostics.
	String *pattern; // Caller-rooted immutable format, H allowed, nil rejected.
	const Native64TailSlot *slots; // Borrowed immutable typed-tail descriptors.
	unsigned int count; // Logical contiguous eight-byte tail count.
	const uvlong *values; // Canonical values rooted by the caller throughout construction.
	unsigned int value_bytes; // Exact readable tail span.
	void *(*resolve)(void*); // Trusted current caller-mapped result cell, resolved after production.
	void *argument; // Borrowed resolver context, live until retirement finishes.
	int initial, limit; // Explicit bounded native-output capacities.
};

/* Require the explicit native64 Sys header and formatter build options in the owning runt.c. */
int Sys_sprint64(const Native64SysFormatRequest*);
int Sys_aprint64(const Native64SysFormatRequest*);

typedef struct Native64SysWriteRequest Native64SysWriteRequest;

struct Native64SysWriteRequest
{
	Native64SysFormatRequest format; // Rooted typed arguments; resolver returns a live four-byte WORD result cell.
	void *fd; // Caller-rooted actual Sys_FD for fprint; ignored by print.
};

/* Explicit native64 typed output; completion reports status separately from the published write count. */
int Sys_print64(const Native64SysWriteRequest*);
int Sys_fprint64(const Native64SysWriteRequest*);
#endif
