#define LIMBO_NATIVE64_TABLES // Complete specifications, explicitly selected implementations.
#include "sys-native64.h"

char exNilref[] = "dereference of nil", exBounds[] = "array bounds error"; // VM frame diagnostics.

static int initialized, rejectinit, clockreads; // Bounded OS descriptor boundary and observed callback identities.
static const char *names[] = {"byte2char", "char2byte", "utfbytes", "read", "readn", "write", "pread", "pwrite", "pctl", "file2chan", "millisec", "open", "fildes"}; // Exact trusted profile.
static int invoked[nelem(names)]; // One dispatch counter per expected export.


/* Observe the descriptor boundary; its real owning body is tested separately. */
void
sysinit(void)
{
	initialized++;
	if(rejectinit)
		error("descriptor initialization failed");
}

/* Observe the published byte2char callback without performing OS or UTF work here. */
void
Sys_byte2char(void *frame)
{
	check(frame == &baseline);
	invoked[0]++;
}

/* Observe the published char2byte callback without performing OS or UTF work here. */
void
Sys_char2byte(void *frame)
{
	check(frame == &baseline);
	invoked[1]++;
}

/* Observe the published utfbytes callback without performing OS or UTF work here. */
void
Sys_utfbytes(void *frame)
{
	check(frame == &baseline);
	invoked[2]++;
}

/* Observe the published read callback without performing OS or UTF work here. */
void
Sys_read(void *frame)
{
	check(frame == &baseline);
	invoked[3]++;
}

/* Observe the published readn callback without performing OS or UTF work here. */
void
Sys_readn(void *frame)
{
	check(frame == &baseline);
	invoked[4]++;
}

/* Observe the published write callback without performing OS or UTF work here. */
void
Sys_write(void *frame)
{
	check(frame == &baseline);
	invoked[5]++;
}

/* Observe the published pread callback without performing OS or UTF work here. */
void
Sys_pread(void *frame)
{
	check(frame == &baseline);
	invoked[6]++;
}

/* Observe the published pwrite callback without performing OS or UTF work here. */
void
Sys_pwrite(void *frame)
{
	check(frame == &baseline);
	invoked[7]++;
}



/* Observe pctl registration without changing host groups in this fixture. */
void
Sys_pctl(void *frame)
{
	check(frame == &baseline);
	invoked[8]++;
}


/* Observe file2chan registration without publishing a service here. */
void
Sys_file2chan(void *frame)
{
	check(frame == &baseline);
	invoked[9]++;
}

/* Supply a known time and count calls across the production Sys frame boundary. */
long
osmillisec(void)
{
	clockreads++;
	return 123456;
}


/* Observe open registration without opening a file in this fixture. */
void
Sys_open(void *frame)
{
	check(frame == &baseline);
	invoked[11]++;
}


/* Observe fildes registration without duplicating a host descriptor. */
void
Sys_fildes(void *frame)
{
	check(frame == &baseline);
	invoked[12]++;
}

SYS_MILLISEC
SYS_PROFILE
SYS_REGISTRATION
SYS_IMPORTS


/* Run the owning registration body through the real protected VM error frame. */
static int
initialize(void *unused)
{
	USED(unused);
	sysmodinit();
	return 1;
}


/* Call the resolved Sys clock with a fixture-owned frame and result cell. */
static int
readclock(void *argument)
{
	F_Sys_millisec *frame;
	const Link *link;

	frame = argument;
	link = dis64sysclockimport(clock_image, sizeof(clock_image), 0);
	check(link != nil);
	link->u.runt(frame);
	return 1;
}


/* Fail every constructor allocation, then resolve the exact published fixed-frame profile. */
int
main(void)
{
	Module *m;
	F_Sys_millisec frame = {0};
	WORD stamp = -1;
	int i, allocations, failures;

	baseline.ref = 7;
	check(dis64sysclockimport(clock_image, sizeof(clock_image), 0) == nil);
	check(kprotect(initialize, nil));
	allocations = attempts;
	check(initialized == 1 && modules != &baseline && modules->ntype == nelem(names));
	m = modules;
	for(i=0; i<nelem(names); i++){
		check(strcmp(m->ext[i].name, names[i]) == 0);
		check(m->type[i]->size == native64sysio[i]->frame_size);
		check(m->type[i]->np == native64sysio[i]->map_bytes);
		check(memcmp(m->type[i]->map, native64sysio[i]->map, m->type[i]->np) == 0);
		if(i == 10)
			continue;
		m->ext[i].u.runt(&baseline);
		check(invoked[i] == 1);
	}
	check(dis64sysclockimport(clock_image, sizeof(clock_image), 0) == &m->ext[10]);
	check(dis64sysclockimport(clock_image, sizeof(clock_image), 1) == nil);
	check(dis64sysclockimport(clock_image, sizeof(clock_image)-1, 0) == nil);
	check(dis64sysclockimport(wrong_module, sizeof(wrong_module), 0) == nil);
	check(dis64sysclockimport(wrong_symbol, sizeof(wrong_symbol), 0) == nil);
	check(dis64sysclockimport(wrong_result, sizeof(wrong_result), 0) == nil);
	check(dis64sysclockimport(wrong_parameters, sizeof(wrong_parameters), 0) == nil);
	m->ext[10].u.runt = Sys_open;
	check(dis64sysclockimport(clock_image, sizeof(clock_image), 0) == nil);
	m->ext[10].u.runt = Sys_millisec;
	m->type[10]->map[0] ^= 1;
	check(dis64sysclockimport(clock_image, sizeof(clock_image), 0) == nil);
	m->type[10]->map[0] ^= 1;
	check(m->ref == 1 && baseline.ref == 7);
	frame.ret = H;
	check(!kprotect(readclock, &frame));
	check(strcmp(diagnostic, exNilref) == 0);
	frame.ret = (WORD*)((uintptr)-2);
	check(!kprotect(readclock, &frame));
	check(strcmp(diagnostic, exBounds) == 0);
	check(clockreads == 0 && stamp == -1 && worker.nerr == 0);
	frame.ret = &stamp;
	check(kprotect(readclock, &frame));
	check(clockreads == 1 && stamp == 123456);
	unload(m);
	check(modules == &baseline && live == 0 && baseline.ref == 7);
	failures = 0;
	for(i=0; i<allocations; i++){
		fail_after = i;
		check(!kprotect(initialize, nil));
		check(strcmp(diagnostic, "native64 Sys I/O registration failed") == 0);
		check(modules == &baseline && live == 0 && worker.nerr == 0);
		failures++;
	}
	fail_after = -1;
	rejectinit = 1;
	i = attempts;
	check(!kprotect(initialize, nil));
	check(strcmp(diagnostic, "descriptor initialization failed") == 0 && attempts == i);
	check(modules == &baseline && live == 0);
	rejectinit = 0;
	check(kprotect(initialize, nil));
	unload(modules);
	check(modules == &baseline && live == 0);
	printf("PASS trusted Sys profile: %d published entries, %d allocation refusals, descriptor refusal and retry, serialized clock import resolution and production callback\n", (int)nelem(names), failures);
	return 0;
}
