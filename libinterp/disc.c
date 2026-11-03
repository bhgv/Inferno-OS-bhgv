#include "lib9.h"
#include "isa.h"
#include "interp.h"

extern int dontcompile;
static DisC *images; // Borrowed static records; the registry owns only their next links.


/* Registered image storage is borrowed and immutable until VM shutdown. */
static int
discimagevalid(const uchar *image, ulong length)
{
	return image != nil && image != H && length != 0 &&
		length <= 0x7fffffffUL && (uintptr)image <= (uintptr)-1-length;
}


/* Register static storage under the VM lock, or before interpreter startup. */
int
disc_register(DisC *code)
{
	DisC *p;

	if(code == nil || code == H || (uintptr)code%sizeof(void*) != 0 ||
	   (uintptr)code > (uintptr)-1-sizeof(*code))
		return 0;
	if(!discimagevalid(code->image, code->length) || code->nprog < 0 ||
	   code->step == nil || code->step == (int(*)(void))H)
		return 0;
	for(p = images; p != nil; p = p->next)
		if(p == code)
			return 1;
	code->next = images;
	images = code;
	return 1;
}

/* Bind only exact registered bytes, under the VM lock before module publication.
 * A callback identity is not permission to replace another mode or compose AOT
 * with cached JIT entries. Refusal leaves every module field unchanged. */
int
disc_tryattach(Module *module, const uchar *image, ulong length)
{
	DisC *p;

	if(module == nil || module == H || (uintptr)module%sizeof(void*) != 0 ||
	   (uintptr)module > (uintptr)-1-sizeof(*module) || !discimagevalid(image, length))
		return 0;
	if(module->jit != nil || module->compiled || dontcompile || (module->rt & DONTCOMPILE))
		return 0;
	for(p = images; p != nil; p = p->next)
		if(p->length == length && p->nprog == module->nprog &&
		   memcmp(p->image, image, length) == 0){
			if(module->cstep != nil && module->cstep != p->step)
				return 0;
			module->cstep = p->step;
			return 1;
		}
	return 0;
}


/* Preserve the classic loader interface while enforcing the same mode boundary. */
void
disc_attach(Module *module, const uchar *image, ulong length)
{
	disc_tryattach(module, image, length);
}
