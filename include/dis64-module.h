/* Agreed module metadata framing; linking and execution are separate stages. */
#ifndef DIS64_MODULE_H
#define DIS64_MODULE_H // Dis64 module metadata codec guard.

#include "dis64.h"

typedef struct Dis64ModuleInfo Dis64ModuleInfo;
struct Dis64ModuleInfo {
	unsigned int name; // Non-nil ordinal string-pool ID.
	unsigned int profile; // Zero library, one executable.
	unsigned int databytes; // Runtime module-data extent, not serialized payload size.
	unsigned int datatype; // Present on wire only for nonzero databytes.
	unsigned int entry; // Executable-only decoded Dis-PC.
	unsigned int frametype; // Executable-only entry frame descriptor ID.
};


/* Consume exactly the fields required by the profile; absent fields have no sentinel. */
static int
dis64modulecheck(const unsigned char *data, unsigned int bytes, Dis64ModuleInfo *result)
{
	Dis64ModuleInfo module;
	unsigned int at, expected;

	if(data == 0 || result == 0 || bytes < 12 || (unsigned int)-1 != 0xffffffffU)
		return 0;
	module.name = dis64u32(data);
	module.profile = dis64u32(data+4);
	module.databytes = dis64u32(data+8);
	if(module.profile > 1)
		return 0;
	expected = 12+(module.databytes != 0 ? 4 : 0)+(module.profile == 1 ? 8 : 0);
	if(bytes != expected)
		return 0;
	at = 12;
	module.datatype = module.entry = module.frametype = 0;
	if(module.databytes != 0){
		module.datatype = dis64u32(data+at);
		at += 4;
	}
	if(module.profile == 1){
		module.entry = dis64u32(data+at);
		module.frametype = dis64u32(data+at+4);
	}
	*result = module;
	return 1;
}


/* Serialize only present fields, after checking the complete output extent. */
static int
dis64modulewrite(unsigned char *output, unsigned int capacity,
	const Dis64ModuleInfo *module, unsigned int *written)
{
	unsigned int bytes, at;

	if(output == 0 || module == 0 || written == 0 || module->profile > 1 ||
	   (unsigned int)-1 != 0xffffffffU)
		return 0;
	bytes = 12+(module->databytes != 0 ? 4 : 0)+(module->profile == 1 ? 8 : 0);
	if(capacity < bytes)
		return 0;
	dis64put32(output, module->name);
	dis64put32(output+4, module->profile);
	dis64put32(output+8, module->databytes);
	at = 12;
	if(module->databytes != 0){
		dis64put32(output+at, module->datatype);
		at += 4;
	}
	if(module->profile == 1){
		dis64put32(output+at, module->entry);
		dis64put32(output+at+4, module->frametype);
	}
	*written = bytes;
	return 1;
}
#endif
