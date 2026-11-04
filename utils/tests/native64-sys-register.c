#define LIMBO_NATIVE64_TABLES // Complete specifications, explicitly selected implementations.
#include "sys-native64.h"

static int initialized, invoked[10], rejectinit; // Bounded OS descriptor boundary and observed callback identities.
static const char *names[] = {"byte2char", "char2byte", "utfbytes", "read", "readn", "write", "pread", "pwrite", "pctl", "file2chan"}; // Exact trusted profile.


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

SYS_PROFILE
SYS_REGISTRATION


/* Run the owning registration body through the real protected VM error frame. */
static int
initialize(void *unused)
{
	USED(unused);
	sysmodinit();
	return 1;
}


/* Fail every constructor allocation, then resolve the exact published fixed-frame profile. */
int
main(void)
{
	Module *m;
	int i, allocations, failures;

	baseline.ref = 7;
	check(kprotect(initialize, nil));
	allocations = attempts;
	check(initialized == 1 && modules != &baseline && modules->ntype == nelem(names));
	m = modules;
	for(i=0; i<nelem(names); i++){
		check(strcmp(m->ext[i].name, names[i]) == 0);
		check(m->type[i]->size == native64sysio[i]->frame_size);
		check(m->type[i]->np == native64sysio[i]->map_bytes);
		check(memcmp(m->type[i]->map, native64sysio[i]->map, m->type[i]->np) == 0);
		m->ext[i].u.runt(&baseline);
		check(invoked[i] == 1);
	}
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
	printf("PASS trusted Sys profile: 10 published entries, %d allocation refusals, descriptor refusal and retry; callbacks bounded\n", failures);
	return 0;
}
