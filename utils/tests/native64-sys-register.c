#define LIMBO_NATIVE64_TABLES // Complete specifications, explicitly selected implementations.
#include "sys-native64.h"

char exNilref[] = "dereference of nil", exBounds[] = "array bounds error"; // VM frame diagnostics.

static int initialized, rejectinit, clockreads; // Bounded OS descriptor boundary and observed callback identities.
static const char *names[] = {"byte2char", "char2byte", "utfbytes", "read", "readn", "write", "pread", "pwrite", "pctl", "file2chan", "millisec", "open", "fildes", "sleep", "dup", "werrstr", "chdir", "remove", "unmount", "bind", "iounit"}; // Exact trusted profile.
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
void
Sys_sleep(void *frame)
{
	check(frame == &baseline);
	invoked[13]++;
}

/* Observe the two-integer descriptor-duplication entry without changing fixture descriptors. */
void
Sys_dup(void *frame)
{
	check(frame == &baseline);
	invoked[14]++;
}

/* Observe registration identity without modifying process error text here. */
void
Sys_werrstr(void *frame)
{
    check(frame == &baseline);
    invoked[15]++;
}

void Sys_chdir(void *frame){check(frame == &baseline);invoked[16]++;}
void Sys_remove(void *frame){check(frame == &baseline);invoked[17]++;}
void Sys_unmount(void *frame){check(frame == &baseline);invoked[18]++;}
void Sys_bind(void *frame){check(frame == &baseline);invoked[19]++;}
void Sys_iounit(void *frame){check(frame == &baseline);invoked[20]++;}

int native64stringlea8step(void){return 0;} // No result-binding VM execution in this registration gate.

SYS_PROFILE
SYS_REGISTRATION
SYS_IMPORTS


Type Tmodlink = {.ref = 1}; // Fixture descriptor for the production link allocator.
void (*heapmonitor)(int, void*, ulong); // Explicitly controlled allocation monitor.
char exHeap[] = "invalid heap"; // Link allocation diagnostic.
static Heap *clockroot; // Exactly one external root can be active in this fixture.
static Modlink *clocklink; // Instance used by the protected callback test.
static int rejectroot; // Inject an error before external root publication.


/* Supply tracked heap allocation to the real mklinkmod constructor. */
Heap*
nheap(int bytes)
{
	Heap *heap;

	heap = allocate(sizeof(*heap)+bytes);
	if(heap == nil)
		error(exNomem);
	heap->ref = 1;
	return heap;
}


/* Record publication or raise before making the instance visible. */
void
ptradd(Heap *heap)
{
	check(clockroot == nil);
	if(rejectroot)
		error(exNomem);
	clockroot = heap;
}


/* Verify the caller releases precisely the external root it owns. */
void
ptrdel(Heap *heap)
{
	check(clockroot == heap);
	clockroot = nil;
}


/* Retire the link and release its module under the fixture's bounded heap. */
static void
destroysyslink(void *object)
{
	Modlink *link;

	link = object;
	check(D2H(link) != clockroot && D2H(link)->t == &Tmodlink && link->MP == H);
	Tmodlink.ref--;
	unload(link->m);
	releaseowned(D2H(link));
}


REG R;
int mutator, nprop;
char exModule[] = "invalid module";

void
krethrow(void)
{
	nexterror();
}

/* This fixture supplies only the builtin name and forbids filesystem fallback. */
char*
string2c(String *name)
{
	check(name != nil && name != H);
	return name->Sascii;
}

Module*
lookmod(char *name)
{
	USED(name);
	abort();
}

Module*
readmod(char *name, Module *cached, int sync)
{
	USED(name); USED(cached); USED(sync);
	abort();
}

int
native64lea8step(void)
{
	abort();
}

void
newdyndata(Modlink *link)
{
	USED(link);
	abort();
}

void
newmp(void *destination, void *source, Type *type)
{
	USED(destination); USED(source); USED(type);
	abort();
}

SYS_LINKER

#define OP(name) void name(void)
SYS_ILOAD
#undef OP

static struct {
	Heap heap;
	String value;
} sysname;
static int rejectname;

String*
c2string(char *text, int bytes)
{
	check(bytes == 4 && memcmp(text, "$Sys", bytes) == 0);
	if(rejectname)
		return H;
	memmove(sysname.value.Sascii, "$Sys", 5);
	return &sysname.value;
}

SYS_LOAD_METADATA

static int
loadsysgroup(void *unused)
{
	USED(unused);
	iload();
	return 1;
}

/* Load two generated exports through production ILOAD and linkmod. */
static void
checksysgroup(Module *sys)
{
	const Link *entries[] = {&sys->ext[11], &sys->ext[10]};
	Module *caller;
	Import *imports;
	Modlink current = {.MP = H}, *destination, *published;
	String *source;
	WORD ordinal = 0;
	int before, failure;

	before = live;
	for(failure=0; failure<5; failure++){
		caller = mallocz(sizeof(*caller), 1);
		check(caller != nil);
		caller->ref = 1;
		source = H;
		caller->origmp = (uchar*)&source;
		fail_after = failure < 4 ? failure : -1;
		rejectname = failure == 4;
		check(!dis64sysloadmetadata(caller, entries, nelem(entries)));
		check(source == H && !(caller->rt & HASLDT));
		fail_after = -1;
		rejectname = 0;
		caller->origmp = H;
		unload(caller);
		check(live == before);
	}

	caller = mallocz(sizeof(*caller), 1);
	check(caller != nil);
	caller->ref = 1;
	source = H;
	caller->origmp = (uchar*)&source;
	caller->cstep = native64lea8step;
	check(dis64sysloadmetadata(caller, entries, nelem(entries)));
	check(source == &sysname.value && caller->ldt[1] == nil);
	imports = caller->ldt[0];
	check(imports[2].name == nil);
	current.m = caller;
	R.M = &current;
	R.s = &source;
	R.m = &ordinal;
	R.d = &destination;
	destination = H;
	check(kprotect(loadsysgroup, nil));
	check(destination != H && destination->m == sys && destination->nlinks == 2);
	check(destination->links[0].u.runt == Sys_open && destination->links[0].frame == sys->ext[11].frame);
	check(destination->links[1].u.runt == Sys_millisec && destination->links[1].frame == sys->ext[10].frame);
	check(sys->ref == 2 && clockroot == nil);
	published = destination;
	imports[1].sig ^= 1;
	check(!kprotect(loadsysgroup, nil) && strcmp(diagnostic, exModule) == 0);
	check(destination == published && sys->ref == 2);
	imports[1].sig ^= 1;
	rejectroot = 1;
	check(!kprotect(loadsysgroup, nil) && strcmp(diagnostic, exNomem) == 0);
	rejectroot = 0;
	check(destination == published && sys->ref == 2);
	destroy(destination);
	caller->origmp = H;
	unload(caller);
	check(sys->ref == 1 && live == before && clockroot == nil && Tmodlink.ref == 1);
	puts("PASS production Sys metadata, ILOAD and linkmod: two exports and seven rollback cases");
}


/* Borrow a link only for the import-matching tests. */
static const Link*
clockimport(const uchar *image, ulong bytes, unsigned int ordinal)
{
	Module *owner;
	const Link *entry;

	owner = nil;
	entry = dis64sysclockimport(image, bytes, ordinal, &owner);
	check(owner == (entry == nil ? nil : modules));
	return entry;
}


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
	check(clocklink != nil && clocklink->nlinks == 1);
	clocklink->links[0].u.runt(argument);
	return 1;
}


/* Fail every constructor allocation, then resolve the exact published fixed-frame profile. */
int
main(void)
{
	Module *m, *owner;
	F_Sys_millisec frame = {0};
	WORD stamp = -1;
	int i, allocations, failures, before;

	baseline.ref = 7;
	check(clockimport(clock_image, sizeof(clock_image), 0) == nil);
	check(kprotect(initialize, nil));
	allocations = attempts;
	check(initialized == 1 && modules != &baseline && modules->ntype == nelem(names));
	m = modules;
	for(i=0; i<nelem(names); i++){
		owner = nil;
		check(native64sysexport(names[i], &owner) == &m->ext[i] && owner == m);
		check(strcmp(m->ext[i].name, names[i]) == 0);
		check(m->type[i]->size == native64sysio[i]->frame_size);
		check(m->type[i]->np == native64sysio[i]->map_bytes);
		check(memcmp(m->type[i]->map, native64sysio[i]->map, m->type[i]->np) == 0);
		if(i == 10)
			continue;
		m->ext[i].u.runt(&baseline);
		check(invoked[i] == 1);
	}
	owner = nil;
	check(native64sysexport("fprint", &owner) == nil && owner == nil);
	check(native64sysexport("Millisec", &owner) == nil && owner == nil);
	m->ext[11].sig ^= 1;
	check(native64sysexport("open", &owner) == nil && owner == nil);
	m->ext[11].sig ^= 1;
	checksysgroup(m);
	check(clockimport(clock_image, sizeof(clock_image), 0) == &m->ext[10]);
	check(clockimport(clock_image, sizeof(clock_image), 1) == nil);
	check(clockimport(clock_image, sizeof(clock_image)-1, 0) == nil);
	check(clockimport(wrong_module, sizeof(wrong_module), 0) == nil);
	check(clockimport(wrong_symbol, sizeof(wrong_symbol), 0) == nil);
	check(clockimport(wrong_result, sizeof(wrong_result), 0) == nil);
	check(clockimport(wrong_parameters, sizeof(wrong_parameters), 0) == nil);
	m->ext[10].u.runt = Sys_open;
	check(clockimport(clock_image, sizeof(clock_image), 0) == nil);
	m->ext[10].u.runt = Sys_millisec;
	m->type[10]->map[0] ^= 1;
	check(clockimport(clock_image, sizeof(clock_image), 0) == nil);
	m->type[10]->map[0] ^= 1;
	check(m->ref == 1 && baseline.ref == 7);
	before = live;
	fail_after = 0;
	check(prepare64sysclocklink(clock_image, sizeof(clock_image), 0) == nil);
	fail_after = -1;
	check(m->ref == 1 && live == before && clockroot == nil && Tmodlink.ref == 1);
	rejectroot = 1;
	check(prepare64sysclocklink(clock_image, sizeof(clock_image), 0) == nil);
	rejectroot = 0;
	check(m->ref == 1 && live == before && clockroot == nil && Tmodlink.ref == 1);
	clocklink = prepare64sysclocklink(clock_image, sizeof(clock_image), 0);
	check(clocklink != nil && clockroot == D2H(clocklink));
	check(clocklink->m == m && m->ref == 2 && clocklink->prog == nil && clocklink->MP == H);
	check(clocklink->links[0].frame == m->ext[10].frame);
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
	ptrdel(D2H(clocklink));
	destroy(clocklink);
	check(m->ref == 1 && live == before && clockroot == nil && Tmodlink.ref == 1);
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
	printf("PASS trusted Sys profile: %d published entries, %d allocation refusals, descriptor refusal and retry, rooted clock import, two rollback controls and production callback\n", (int)nelem(names), failures);
	return 0;
}
