#include "sys-native64.h"

#ifdef NATIVE64_NO_LSAN


/* Optional host limit: allocation ownership is checked without process-inspection leak scans. */
int
__lsan_is_turned_off(void)
{
	return 1;
}
#endif

#ifdef SYS_INIT_WIDE
typedef struct WideRead WideRead;

struct WideRead
{
	uchar prefix[64]; // Move the reference into the second map byte.
	Channel *t3; // Native request reply reference.
};

typedef struct WideWrite WideWrite;

struct WideWrite
{
	uchar prefix[64]; // Exercise byte indexing beyond the first eight slots.
	Channel *t1; // First reference at slot8.
	ulong scalar; // Unmapped intervening data.
	Channel *t3; // Second reference at slot10.
};
#define Sys_FileIO_read WideRead // Test-only native payload shape, not a wire format change.
#define Sys_FileIO_write WideWrite // Test-only wider request payload.
#endif

FD_DECLARATION
static Type *TFD, *TFileIO, *FioTread, *FioTwrite, *Tdir; // Published descriptor owners.
static uchar FDmap[] = Sys_FD_map; // Generated scalar descriptor map.
static uchar FileIOmap[] = Sys_FileIO_map; // Generated channel references.
static uchar dmap[] = Sys_Dir_map; // Generated directory reference map.
#if defined(SYS_INIT_WIDE) && !defined(SYS_INIT_SMALL_MAPS)
static uchar rmap[2], wmap[2]; // Complete maps for the test's wider request shapes.
#else
static uchar rmap[] = Sys_FileIO_read_map; // Actual generated request map capacity.
static uchar wmap[] = Sys_FileIO_write_map; // Actual generated request map capacity.
#endif
static uchar oldread[sizeof(rmap)], oldwrite[sizeof(wmap)]; // Exact pre-call metadata snapshots.
static int observed; // Allocation entries observed before any publication.


/* Descriptor retirement must not invoke a heap-payload finalizer. */
void
freeFD(Heap *heap, int swept)
{
	USED(heap);
	USED(swept);
	abort();
}


/* The fixture owns descriptors, not live FileIO heap payloads. */
void
freeFileIO(Heap *heap, int swept)
{
	USED(heap);
	USED(swept);
	abort();
}


/* Every allocator callback must still see the original empty descriptor registry and maps. */
static void
observeinit(void)
{
	check(TFD == nil && TFileIO == nil && FioTread == nil && FioTwrite == nil && Tdir == nil);
	check(memcmp(rmap, oldread, sizeof(rmap)) == 0 && memcmp(wmap, oldwrite, sizeof(wmap)) == 0);
	observed++;
}

SYS_INITIALIZER


/* Enter the actual initializer through the owning runtime's protected-error function. */
static int
initialize(void *unused)
{
	USED(unused);
	sysinit();
	return 1;
}


/* Release the fixture's successful registry exactly once using actual freetype. */
static void
retire(void)
{
	Type *types[5];
	int i;

	types[0]=TFD; types[1]=TFileIO; types[2]=FioTread; types[3]=FioTwrite; types[4]=Tdir;
	TFD=TFileIO=FioTread=FioTwrite=Tdir=nil;
	for(i=0; i<5; i++){
		check(types[i] != nil && types[i]->ref == 1);
		freetype(types[i]);
	}
	check(live == 0 && worker.nerr == 0);
}


/* Independently compare every request-map bit with the actual managed field positions. */
static void
maps(void)
{
	int slot, expected;

	check(TFD->size == sizeof(FD) && TFileIO->size == sizeof(Sys_FileIO));
	check(FioTread->size == sizeof(Sys_FileIO_read) && FioTwrite->size == sizeof(Sys_FileIO_write));
	check(Tdir->size == sizeof(Sys_Dir) && Tdir->np == sizeof(dmap));
	check(FioTread->np == sizeof(rmap) && FioTwrite->np == sizeof(wmap));
	for(slot=0; slot<8*sizeof(rmap); slot++){
		expected = slot*sizeof(void*) == offsetof(Sys_FileIO_read, t3);
		check(!!(FioTread->map[slot/8] & (0x80 >> (slot%8))) == expected);
	}
	for(slot=0; slot<8*sizeof(wmap); slot++){
		expected = slot*sizeof(void*) == offsetof(Sys_FileIO_write, t1) ||
			slot*sizeof(void*) == offsetof(Sys_FileIO_write, t3);
		check(!!(FioTwrite->map[slot/8] & (0x80 >> (slot%8))) == expected);
	}
}


/* Fault each allocation, retry, repeat initialization and reject every partial published state. */
int
main(void)
{
	Type borrowed[5] = {{0}};
	Type *saved[5];
	int cutoff, before, mask, i;

	baseline.ref = 7;
	memmove(oldread, rmap, sizeof(rmap));
	memmove(oldwrite, wmap, sizeof(wmap));
#ifdef SYS_INIT_SMALL_MAPS
	before = attempts;
	check(!kprotect(initialize, nil));
	check(strcmp(diagnostic, "bad Sys request maps") == 0 && attempts == before && live == 0);
	puts("PASS Sys initializer: insufficient wide-map capacity rejected before allocation");
	return 0;
#else
	for(cutoff=0; cutoff<5; cutoff++){
		memmove(rmap, oldread, sizeof(rmap));
		memmove(wmap, oldwrite, sizeof(wmap));
		observed = 0;
		fail_after = cutoff;
		check(!kprotect(initialize, nil));
		fail_after = -1;
		check(strcmp(diagnostic, exNomem) == 0 && observed == cutoff+1 && live == 0);
		observeinit();
		check(worker.nerr == 0 && modules == &baseline);
		check(kprotect(initialize, nil));
		check(live == 5);
		maps();
		saved[0]=TFD; saved[1]=TFileIO; saved[2]=FioTread; saved[3]=FioTwrite; saved[4]=Tdir;
		before = attempts;
		for(i=0; i<5; i++) saved[i]->ref += 3;
		fail_after = 0;
		check(kprotect(initialize, nil));
		fail_after = -1;
		check(attempts == before && live == 5 && TFD == saved[0] && TFileIO == saved[1] &&
			FioTread == saved[2] && FioTwrite == saved[3] && Tdir == saved[4]);
		for(i=0; i<5; i++){
			check(saved[i]->ref == 4);
			saved[i]->ref -= 3;
		}
		retire();
	}
	for(mask=1; mask<31; mask++){
		for(i=0; i<5; i++) borrowed[i].ref = 7;
		TFD=mask&1 ? borrowed : nil;
		TFileIO=mask&2 ? borrowed+1 : nil;
		FioTread=mask&4 ? borrowed+2 : nil;
		FioTwrite=mask&8 ? borrowed+3 : nil;
		Tdir=mask&16 ? borrowed+4 : nil;
		before = attempts;
		check(!kprotect(initialize, nil));
		check(strcmp(diagnostic, "partial Sys type initialization") == 0 && attempts == before);
		check(TFD == (mask&1 ? borrowed : nil) && TFileIO == (mask&2 ? borrowed+1 : nil) &&
			FioTread == (mask&4 ? borrowed+2 : nil) && FioTwrite == (mask&8 ? borrowed+3 : nil) &&
			Tdir == (mask&16 ? borrowed+4 : nil));
		for(i=0; i<5; i++) check(borrowed[i].ref == 7);
		TFD=TFileIO=FioTread=FioTwrite=Tdir=nil;
		check(live == 0 && worker.nerr == 0);
	}
	puts("PASS Sys initializer: 5 allocation failures/retries, 5 idempotent calls, 30 partial states and full maps");
	return 0;
#endif
}
