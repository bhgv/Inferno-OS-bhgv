#include "runt.h"

#define Loadermodlen 0 // Export registration is an observed boundary, not a generated ABI claim.
static Runtab Loadermodtab[1]; // No guest exports are enabled by this fixture.
static int registration, registrations, foundations, failfoundation, cases; // Injected boundaries and case counters.
LOADER_GLOBALS


/* Keep every source mutation failure semantic and retire fixture memory before fatal exit. */
static void
loadercheck(int condition)
{
	int i;

	if(!condition){
		fprintf(stderr, "Loader type initialization mismatch\n");
		for(i=0; i<nelem(owned); i++)
			releaseowned(owned[i]);
		exit(10);
	}
}


/* Allocation and registration must never observe an incompletely published Loader. */
static void
loaderobserve(void)
{
	loadercheck(Tinst == nil && Tdesc == nil && Tlink == nil);
}


/* Sys ownership is independent; observe only entry and error propagation here. */
void
sysinit(void)
{
	loaderobserve();
	foundations++;
	if(failfoundation)
		error("Sys foundation refused");
}


/* Owning builtin construction has independent gates; verify only this caller's ordering. */
Module*
builtinmod(char *name, void *table, int length)
{
	loaderobserve();
	loadercheck(strcmp(name, "$Loader") == 0 && table == Loadermodtab && length == Loadermodlen);
	loadercheck(foundations == 1 && live == 3);
	registrations++;
	if(registration){
		strcpy(diagnostic, registration == 1 ? "refused Loader ABI" : "");
		return nil;
	}
	return &baseline;
}

LOADER_INITIALIZER


/* Deliver errors through the actual owning kprotect body without replacing initializer logic. */
static int
initialize(void *argument)
{
	USED(argument);
	loadermodinit();
	return 1;
}


/* Each episode begins with no Loader descriptor owner and no leaked allocation. */
static void
reset(void)
{
	loadercheck(live == 0 && worker.nerr == 0 && modules == &baseline);
	Tinst = Tdesc = Tlink = nil;
	registration = registrations = foundations = failfoundation = disturb = 0;
	fail_after = -1;
	strcpy(diagnostic, "before");
}


/* Compute maps from the real native fields, checking all nonreference bits too. */
static void
descriptor(Type *type, int size, uchar *map, ulong bytes, int pointer, ulong offset)
{
	uchar expected[16];
	ulong slot;

	loadercheck(type != nil && type->ref == 1 && type->free == freeheap && type->mark == markheap);
	loadercheck(type->size == size && type->np == bytes && bytes <= sizeof(expected));
	memset(expected, 0, sizeof(expected));
	if(pointer){
		loadercheck(offset%sizeof(void*) == 0);
		slot = offset/sizeof(void*);
		loadercheck(slot/8 < bytes);
		expected[slot/8] |= 0x80 >> (slot%8);
	}
	loadercheck(memcmp(type->map, expected, bytes) == 0 && memcmp(map, expected, bytes) == 0);
}


/* Preserve exact descriptors across repeated initialization, then release their creator owners. */
static void
successful(void)
{
	Type *saved[3];
	int before;

	loadercheck(live == 3 && registrations == 1 && foundations == 1 && worker.nerr == 0);
	descriptor(Tinst, sizeof(Loader_Inst), Instmap, sizeof(Instmap), 0, 0);
	descriptor(Tdesc, sizeof(Loader_Typedesc), Tdescmap, sizeof(Tdescmap), 1, offsetof(Loader_Typedesc, map));
	descriptor(Tlink, sizeof(Loader_Link), Tlinkmap, sizeof(Tlinkmap), 1, offsetof(Loader_Link, name));
	saved[0] = Tinst; saved[1] = Tdesc; saved[2] = Tlink;
	before = attempts;
	loadercheck(kprotect(initialize, nil) == 1 && attempts == before && registrations == 1 && foundations == 1);
	loadercheck(Tinst == saved[0] && Tdesc == saved[1] && Tlink == saved[2]);
	freetype(Tinst); freetype(Tdesc); freetype(Tlink);
	Tinst = Tdesc = Tlink = nil;
	loadercheck(live == 0 && worker.nerr == 0);
}


/* Cover each allocation, registration diagnostics, independent Sys refusal and partial states. */
int
main(void)
{
	Type sentinel;
	int failure, mode, noisy, mask, before;

	for(failure=0; failure<3; failure++){
		reset();
		fail_after = failure;
		loadercheck(kprotect(initialize, nil) == 0 && live == 0 && worker.nerr == 0);
		loadercheck(strcmp(diagnostic, exNomem) == 0 && registrations == 0 && foundations == 1);
		loaderobserve();
		fail_after = -1;
		foundations = 0;
		loadercheck(kprotect(initialize, nil) == 1);
		successful();
		cases++;
	}
	for(mode=1; mode<=2; mode++)
	for(noisy=0; noisy<2; noisy++){
		reset();
		registration = mode;
		disturb = noisy;
		loadercheck(kprotect(initialize, nil) == 0 && live == 0 && worker.nerr == 0 && registrations == 1);
		loadercheck(strcmp(diagnostic, mode == 1 ? "refused Loader ABI" : "Loader module registration failed") == 0);
		loaderobserve();
		registration = registrations = foundations = disturb = 0;
		loadercheck(kprotect(initialize, nil) == 1);
		successful();
		cases++;
	}
	reset();
	failfoundation = 1;
	before = attempts;
	loadercheck(kprotect(initialize, nil) == 0 && live == 0 && worker.nerr == 0 && attempts == before);
	loadercheck(strcmp(diagnostic, "Sys foundation refused") == 0 && foundations == 1 && registrations == 0);
	loaderobserve();
	failfoundation = foundations = 0;
	loadercheck(kprotect(initialize, nil) == 1);
	successful();
	cases++;
	for(mask=1; mask<8; mask++){
		reset();
		Tinst = mask & 1 ? &sentinel : nil;
		Tdesc = mask & 2 ? &sentinel : nil;
		Tlink = mask & 4 ? &sentinel : nil;
		before = attempts;
		loadercheck(kprotect(initialize, nil) == (mask == 7) && attempts == before && live == 0 && registrations == 0 && foundations == 0 && worker.nerr == 0);
		loadercheck(strcmp(diagnostic, mask == 7 ? "before" : "partial Loader type initialization") == 0);
		loadercheck(Tinst == (mask & 1 ? &sentinel : nil) && Tdesc == (mask & 2 ? &sentinel : nil) && Tlink == (mask & 4 ? &sentinel : nil));
		Tinst = Tdesc = Tlink = nil;
		cases++;
	}
	printf("PASS Loader initialization WORD%d: %d failure/retry/partial cases; three native maps; independent Sys refusal; idempotent ownership\n", (int)sizeof(WORD)*8, cases);
	return 0;
}
