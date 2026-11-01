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
#endif
