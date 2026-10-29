#include "comp-amd64.c"
#include "tests/leaf-cons-stub.h"
#include "tests/leaf-plain-stubs.h"
#include "dis64-pilot.h"
#include <stdio.h>

REG R;
Type Tstring={1}, Tarray={1};
int mutator, nprop;
char exBounds[]="bounds";


/* Supply ordinary descriptor allocation for real RX compilation. */
void*
mallocz(ulong bytes, int clear)
{
	void *memory;

	memory = malloc(bytes);
	if(memory != nil && clear)
		memset(memory, 0, bytes);
	return memory;
}


/* Run the compiler's actual protected publication callback. */
int
kprotect(int (*callback)(void*), void *argument)
{
	return callback(argument);
}


/* The accepted valid scalar fixture must not raise a runtime bounds error. */
void
error(char *message)
{
	fprintf(stderr, "unexpected VM error: %s\n", message);
	abort();
}

#define OP(name) void name(void) // Actual extracted scalar instruction declaration.
MOVW_IMPLEMENTATION


/* Decode the full module, then exercise its scalar instruction in production C/RX.
 * EXIT is checked as the terminal instruction; no scheduler/VM entry is faked. */
int
main(int argc, char **argv)
{
	unsigned char image[4096], memory[16], before[16];
	unsigned int bytes, route, i;
	Dis64Pilot pilot;
	Dis64Instruction first, last;
	Inst instructions[2];
	Module module;
	WORD immediate, result;
	REG expected;
	FILE *file;

	if(argc != 2 || sizeof(WORD) != 4 || sizeof(void*) != 8)
		return 1;
	file = fopen(argv[1], "rb");
	if(file == nil)
		return 2;
	bytes = fread(image, 1, sizeof(image), file);
	if(ferror(file) || !feof(file)){
		fclose(file);
		return 2;
	}
	if(fclose(file) != 0 || !dis64pilotcheck(image, bytes, 4096, 64, &pilot) ||
	   pilot.module.entry != 0 || pilot.module.databytes != 4 || pilot.code.count != 2 ||
	   !dis64codeget(&pilot.code, 0, &first) || !dis64codeget(&pilot.code, 1, &last) ||
	   first.op != IMOVW || first.mode != (SRC(AIMM)|DST(AMP)) ||
	   first.source != 42 || first.destination != 0 || last.op != IEXIT)
		return 3;
	memset(&module, 0, sizeof(module));
	memset(instructions, 0, sizeof(instructions));
	instructions[0].op = IMOVW;
	instructions[1].op = IEXIT;
	module.prog = instructions;
	module.nprog = 2;
	if(!compile(&module, 2, nil) || module.jit->entry[IMOVW] == nil)
		return 4;
	immediate = first.source;
	for(route=0; route<2; route++){
		memset(memory, 0xa5, sizeof(memory));
		memset(memory, 0, pilot.module.databytes);
		memcpy(before, memory, sizeof(memory));
		memset(&R, 0, sizeof(R));
		R.s = &immediate;
		R.d = memory+first.destination;
		R.PC = instructions+1;
		expected = R;
		if(route == 0)
			movw();
		else if(module.jit->entry[IMOVW](&R) != 1)
			return 5;
		memmove(&result, memory, sizeof(result));
		if(result != 42 || memcmp(&R, &expected, sizeof(R)) != 0)
			return 6;
		for(i=sizeof(result); i<sizeof(memory); i++)
			if(memory[i] != before[i])
				return 7;
	}
	module.jit->freecode(module.jit->code, module.jit->size);
	free(module.jit);
	printf("PASS linked pilot42: production WORD32 MOVW C/RX, result42; EXIT decoded only, no VM entry/scheduler claim\n");
	return 0;
}
