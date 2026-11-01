#ifndef INFERNO_NATIVE64_FORMAT_H
#define INFERNO_NATIVE64_FORMAT_H // Private typed adapter included after the shared runt.c formatter core.
#include "native64-arguments.h"

typedef struct Native64Format Native64Format;

struct Native64Format
{
	const Native64Arguments *arguments; // Borrowed constructed owner, pinned for the whole formatting attempt.
	unsigned int next; // Next logical eight-byte slot; fresh for each retry.
};


/* Decode native-order slot bits into exact host values, never a host va_list or packed Dis cursor. */
static void
native64formatread(void *context, int kind, void *output, unsigned int bytes)
{
	Native64Format *format;
	const Native64Arguments *arguments;
	const Native64TailSlot *slot;
	uvlong value;
	UWORD integer;
	void *reference;
	int valid;

	format = context;
	arguments = format->arguments;
	if(format->next >= arguments->plan.count)
		error(exBounds);
	slot = arguments->slots+format->next;
	if(slot->offset != format->next*8 || slot->bytes != 8)
		error(exInval);
	valid = 0;
	switch(kind){
	case XprintInt:
		valid = bytes == sizeof(integer) && (slot->kind == Native64TailByte ||
			slot->kind == Native64TailInt || slot->kind == Native64TailFix);
		break;
	case XprintBig:
		valid = bytes == 8 && slot->kind == Native64TailBig;
		break;
	case XprintReal:
		valid = bytes == 8 && slot->kind == Native64TailReal;
		break;
	case XprintStringArg:
		valid = bytes == sizeof(reference) && slot->kind == Native64TailRef && slot->type == &Tstring;
		break;
	case XprintReference:
		valid = bytes == sizeof(reference) && slot->kind == Native64TailRef;
		break;
	}
	if(!valid)
		error(exInval);
	memmove(&value, (uchar*)arguments->frame.value+arguments->plan.tail+slot->offset, 8);
	if(kind == XprintInt){
		integer = value;
		memmove(output, &integer, sizeof(integer));
	}else if(kind == XprintStringArg || kind == XprintReference){
		reference = (void*)(uintptr)value;
		memmove(output, &reference, sizeof(reference));
	}else
		memmove(output, &value, 8);
	format->next++;
}


/* Reuse the actual legacy format parser/output engine with an independent checked typed cursor. */
static int
native64format(Prog *prog, const Native64Arguments *owned, String *pattern, char *output, int bytes)
{
	Native64Format format;
	XprintArguments arguments;

	if(!native64model() || owned == nil || owned->frame.value == nil || owned->frame.value == H ||
	   !owned->frame.rooted || owned->type == nil || owned->typed != owned->plan.count)
		error(exInval);
	format.arguments = owned;
	format.next = 0;
	memset(&arguments, 0, sizeof(arguments));
	arguments.read = native64formatread;
	arguments.context = &format;
	return xprintwithargs(prog, &arguments, pattern, output, bytes);
}

typedef struct Native64Formatted Native64Formatted;

struct Native64Formatted
{
	char *data; // Owned native output, never a managed reference or a wire buffer.
	int length; // Exact successful byte count, including embedded NUL characters.
	int capacity; // Allocated byte span, bounded by the explicit caller limit.
};

typedef struct Native64FormatBuild Native64FormatBuild;

struct Native64FormatBuild
{
	Prog *prog; // Borrowed process diagnostic source.
	const Native64Arguments *arguments; // Borrowed pinned argument owner throughout every retry.
	String *pattern; // Borrowed caller-rooted format.
	Native64Formatted *output; // Mutable ownership outside the protected jump frame.
	int initial; // First positive allocation capacity.
	int limit; // Maximum permitted native allocation size.
};


/* Detach native output before releasing it and preserve the original VM diagnostic. */
static void
native64formattedclear(Native64Formatted *output)
{
	char *data;
	char saved[ERRMAX];

	if(output == nil || output->data == nil)
		return;
	kgerrstr(saved, sizeof(saved));
	data = output->data;
	memset(output, 0, sizeof(*output));
	free(data);
	kwerrstr("%s", saved);
}


/* Restart the typed cursor on every bounded retry and keep all native ownership visible to cleanup. */
static int
native64formatbuild(void *argument)
{
	Native64FormatBuild *build;
	Native64Formatted *output;
	int capacity;

	build = argument;
	output = build->output;
	capacity = build->initial;
	for(;;){
		output->data = mallocz(capacity, 0);
		if(output->data == nil)
			error(exNomem);
		output->capacity = capacity;
		output->length = native64format(build->prog, build->arguments, build->pattern, output->data, capacity);
		/* The shared parser can stop before the last UTF sequence fits; reserve its full safety margin. */
		if(output->length < capacity-UTFmax-2)
			return 1;
		if(capacity == build->limit)
			error(exNomem);
		native64formattedclear(output);
		capacity = capacity > build->limit/2 ? build->limit : capacity*2;
	}
}


/* Build independent output without overwriting a live owner; zero/invalid geometry never allocates. */
static int
native64formatgrow(Prog *prog, const Native64Arguments *arguments, String *pattern,
	int initial, int limit, Native64Formatted *output)
{
	Native64FormatBuild build;

	if(output == nil || output->data != nil || output->length != 0 || output->capacity != 0 ||
	   initial <= UTFmax+2 || limit < initial)
		return 0;
	build.prog = prog;
	build.arguments = arguments;
	build.pattern = pattern;
	build.output = output;
	build.initial = initial;
	build.limit = limit;
	if(!kprotect(native64formatbuild, &build)){
		native64formattedclear(output);
		memset(output, 0, sizeof(*output));
		return 0;
	}
	return 1;
}

typedef struct Native64FormatResult Native64FormatResult;

struct Native64FormatResult
{
	Native64Formatted output; // Native bytes retained until managed allocation and pinning finish.
	Native64Result *result; // Borrowed enclosing result guard, responsible for adopted-owner cleanup.
	const Native64Arguments *arguments; // Borrowed immutable pinned arguments.
	Prog *prog; // Borrowed process used by percent-r formatting.
	String *pattern; // Borrowed rooted format String.
	int initial, limit; // Explicit bounded output geometry.
	int array; // Zero converts UTF bytes to String; one preserves exact byte-array contents.
};


/* Convert through actual UTF/array routines and adopt before fallible pinning.
 * Native bytes remain visible to outer cleanup throughout conversion. */
static int
native64formatresultbuild(void *argument)
{
	Native64FormatResult *build;
	void *value;

	build = argument;
	if(!native64formatgrow(build->prog, build->arguments, build->pattern,
	   build->initial, build->limit, &build->output))
		return 0;
	value = build->array ? (void*)mem2array(build->output.data, build->output.length) :
		(void*)c2string(build->output.data, build->output.length);
	if(!native64resultadopt(build->result, value)){
		destroy(value);
		return 0;
	}
	return 1;
}


/* Produce String or bytes under an enclosing result transaction; free output on allocation/pin/error refusal. */
static int
native64formatresult(Native64Result *result, const Native64Arguments *arguments, Prog *prog,
	String *pattern, int initial, int limit, int array)
{
	Native64FormatResult build;
	int status;

	if(result == nil || result->value != H || result->rooted || (array != 0 && array != 1))
		return 0;
	memset(&build, 0, sizeof(build));
	build.result = result;
	build.arguments = arguments;
	build.prog = prog;
	build.pattern = pattern;
	build.initial = initial;
	build.limit = limit;
	build.array = array;
	status = kprotect(native64formatresultbuild, &build);
	native64formattedclear(&build.output);
	return status;
}


/* Convert complete UTF output to the VM's ordinary String representation. */
static int
native64formatstring(Native64Result *result, const Native64Arguments *arguments, Prog *prog,
	String *pattern, int initial, int limit)
{
	return native64formatresult(result, arguments, prog, pattern, initial, limit, 0);
}


/* Preserve the exact formatted bytes, including embedded NUL and multibyte UTF sequences. */
static int
native64formatarray(Native64Result *result, const Native64Arguments *arguments, Prog *prog,
	String *pattern, int initial, int limit)
{
	return native64formatresult(result, arguments, prog, pattern, initial, limit, 1);
}
#endif
