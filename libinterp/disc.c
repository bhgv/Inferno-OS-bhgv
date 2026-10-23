#include "lib9.h"
#include "isa.h"
#include "interp.h"

extern int dontcompile;
static DisC *images;

/* Register static storage under the VM lock, or before interpreter startup. */
int
disc_register(DisC *code)
{
	DisC *p;

	if(code == nil || code->image == nil || code->length == 0 ||
	   code->length > 0x7fffffffUL || code->nprog < 0 || code->step == nil)
		return 0;
	for(p = images; p != nil; p = p->next)
		if(p == code)
			return 1;
	code->next = images;
	images = code;
	return 1;
}

/* Byte identity prevents stale translations from attaching to a changed module. */
void
disc_attach(Module *module, const uchar *image, ulong length)
{
	DisC *p;

	if(module == nil || image == nil || module->compiled || dontcompile || (module->rt & DONTCOMPILE))
		return;
	for(p = images; p != nil; p = p->next)
		if(p->length == length && p->nprog == module->nprog &&
		   memcmp(p->image, image, length) == 0){
			module->cstep = p->step;
			return;
		}
}
