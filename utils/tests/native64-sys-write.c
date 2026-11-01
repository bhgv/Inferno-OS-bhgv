/* Append after the complete formatter/Sys gateway tests and actual protected collector services. */
#include "native64-sys-write.h"

Type *TFD; // Persistent port descriptor boundary, created using actual dtype for every case.
static int writemode, writefprint, releasedvm, releases, acquires, writes, writeresolves; // Explicit native/scheduler boundary observations.
static int movedword, fdretired; // Logical scalar destination transfer and independently retired FD owner.
static F_Roots_caller *writecaller; // Generated caller with mapped source roots and scalar result storage.
static Sys_FD *writefd; // Caller-owned live FD, independently pinned until transfer/retirement.
static char nativebytes[128]; // Exact bytes passed to the one native write, including NUL.
static long nativecount; // Exact native count, independent of C string termination.


/* Model the scheduler handoff only; arguments must remain real external collector roots. */
void
release(void)
{
	check(!releasedvm);
	releasedvm=1; releases++;
}


/* Require exactly one matching handoff even after a native write raises. */
void
acquire(void)
{
	check(releasedvm);
	releasedvm=0; acquires++;
	kwerrstr("%s", "scheduler changed diagnostic");
}


/* Exercise one irreversible native boundary, short/error counts, GC and logical destination movement. */
long
kwrite(int descriptor, void *bytes, long count)
{
	WORD old;

	check(releasedvm && count >= 0 && count < sizeof(nativebytes));
	check(descriptor == (writefprint ? writemode == 15 ? -1 : 7 : 1));
	writes++;
	nativecount=count; memmove(nativebytes, bytes, count);
	if(writemode == 8){
		memmove(&old, &writecaller->padding, sizeof(old));
		memmove((uchar*)&writecaller->padding+sizeof(WORD), &old, sizeof(old));
		movedword=1;
		if(writefprint){ ptrdel(D2H(writefd)); destroy(writefd); fdretired=1; }
	}
	epochs(6);
	if(writefprint) check(!freed[4]);
	if(writemode == 4) error("native write raised");
	if(writemode == 1) return count-1;
	if(writemode == 2) return 0;
	if(writemode == 3 || descriptor == -1){
		kwerrstr("%s", "native write failed");
		return -1;
	}
	if(writemode == 11) return count+1;
	if(writemode == 12) return -2;
	return count;
}


/* Resolve before the side effect and again after reacquisition; never return a cached pre-write cell. */
static void*
writecell(void *context)
{
	USED(context);
	check(!releasedvm);
	writeresolves++;
	epochs(6);
	if(writemode == 6 || (writemode == 9 && writeresolves == 2)){
		kwerrstr("%s", "write cancelled");
		return nil;
	}
	if(writemode == 7 || (writemode == 10 && writeresolves == 2)) error("write resolver raised");
	if(writemode == 21) return H;
	if(writemode == 22) return (void*)((uintptr)-2);
	return (uchar*)&writecaller->padding+(movedword ? sizeof(WORD) : 0);
}


/* Check zero pre-write effects, one actual write, precise scalar publication and every cleanup owner. */
static void
syswritecase(int phase, int state, int fprint, int mode)
{
	Native64SysWriteRequest request;
	Native64TailSlot slots[3];
	uvlong values[3];
	uchar map[] = {0};
	Type *caller_type, *fdtype;
	String *pattern, *text;
	WORD original[2], result[2], expected;
	int i, status, success, attempted, baseline;

	contextstart(phase, state);
	writemode=mode; writefprint=fprint;
	releasedvm=releases=acquires=writes=writeresolves=movedword=fdretired=0;
	nativecount=-99; memset(nativebytes, 0x5a, sizeof(nativebytes));
	caller_type=native64frametype(&limbo_native64_Roots_caller_entry);
	check(caller_type != nil);
	writecaller=(F_Roots_caller*)stackframes(caller_type, 1);
	pattern=newstring(mode == 20 ? 6 : 5);
	memmove(pattern->Sascii, mode == 20 ? "%d%c%s" : "%d|%s", pattern->len+1);
	writecaller->result=(Array*)pattern;
	text=newstring(2); memmove(text->Sascii, "ok", 3);
	writecaller->tail=(Array*)text;
	fdtype=TFD=dtype(freeheap, sizeof(Sys_FD), map, sizeof(map));
	check(fdtype != nil);
	writefd=H2D(Sys_FD*, heapz(fdtype)); writefd->fd=7; ptradd(D2H(writefd));
	original[0]=77; original[1]=88;
	memmove(&writecaller->padding, original, sizeof(original));
	for(i=0; i<3; i++){
		slots[i].kind=Native64TailInt; slots[i].offset=i*8; slots[i].bytes=8; slots[i].type=nil;
	}
	values[0]=(uvlong)(vlong)-1;
	slots[1].kind=Native64TailRef; slots[1].type=&Tstring; values[1]=(uvlong)(uintptr)text;
	if(mode == 20){
		slots[1].kind=Native64TailInt; slots[1].type=nil; values[1]=0;
		slots[2].kind=Native64TailRef; slots[2].type=&Tstring; values[2]=(uvlong)(uintptr)text;
	}
	if(mode == 5){
		slots[1].type=&Tarray; values[1]=(uvlong)(uintptr)H;
	}
	memset(&request, 0, sizeof(request));
	request.format.prog=&process; request.format.pattern=pattern;
	request.format.slots=slots; request.format.count=mode == 20 ? 3 : 2;
	request.format.values=values; request.format.value_bytes=request.format.count*8;
	request.format.resolve=writecell; request.format.initial=8; request.format.limit=128;
	request.fd=writefd;
	if(mode == 13) contextfailure=contextattempt+6;
	if(mode == 14) contextheapfail=1;
	if(mode == 15) request.fd=H;
	if(mode == 16) request.fd=nil;
	if(mode == 17) request.fd=pattern;
	if(mode == 18) TFD=nil;
	if(mode == 19) values[0]=0xffffffffULL;
	if(mode == 23) request.format.resolve=nil;
	if(mode == 24) request.format.limit=8;
	baseline=contextlive;
	allocationcollections=1;
	status=fprint ? Sys_fprint64(&request) : Sys_print64(&request);
	allocationcollections=contextfailure=contextheapfail=0; TFD=fdtype;
	attempted = mode <= 4 || mode == 8 || mode == 9 || mode == 10 || mode == 11 || mode == 12 ||
		mode == 15 || mode == 20 || (!fprint && mode >= 16 && mode <= 18);
	success = attempted && mode != 4 && mode != 9 && mode != 10 && mode != 11 && mode != 12;
	check(status == success && writes == attempted && releases == attempted && acquires == attempted && !releasedvm);
	check(contextlive == baseline && errdepth == 0 && D2H(pattern)->ref == 1 && D2H(text)->ref == 1);
	memmove(result, &writecaller->padding, sizeof(result));
	if(success){
		expected=mode == 1 ? 4 : mode == 2 ? 0 : mode == 3 || (mode == 15 && fprint) ? -1 : 5;
		check(result[movedword ? 1 : 0] == expected && result[movedword ? 0 : 1] == (movedword ? 77 : 88));
	}else check(result[0] == 77 && result[1] == 88);
	if(attempted){
		check(nativecount == 5 && memcmp(nativebytes, mode == 20 ? "-1\0ok" : "-1|ok", 5) == 0);
	}else check(nativecount == -99 && nativebytes[0] == 0x5a);
	if(mode == 3 || (mode == 15 && fprint)) check(strcmp(up->env->errstr, "native write failed") == 0);
	if(mode == 4) check(strcmp(up->env->errstr, "native write raised") == 0);
	if(mode == 6 || mode == 9) check(strcmp(up->env->errstr, "write cancelled") == 0);
	if(mode == 7 || mode == 10) check(strcmp(up->env->errstr, "write resolver raised") == 0);
	if(mode == 11 || mode == 12 || mode == 21 || mode == 22) check(strcmp(up->env->errstr, exBounds) == 0);
	if(!fdretired){ ptrdel(D2H(writefd)); destroy(writefd); }
	freeptrs(writecaller, caller_type); initmem(caller_type, writecaller);
	process.R.EX=process.R.SP=nil;
	epochs(6); freetype(caller_type); freetype(fdtype); TFD=nil; clearrootcache();
	check(contextlive == 0 && Tarray.ref == 1 && Tstring.ref == 1 && Tbyte.ref == 1 && errdepth == 0);
	heapmonitor=nil;
}


/* Exercise both generated output prefixes across actual collector and error-boundary modes. */
int
main(int argc, char **argv)
{
	int phase, state, fprint, mode, cases;

	native64_sysformat_main(argc, argv);
	cases=0;
	for(phase=0; phase<3; phase++)
		for(state=Palt; state<=Pbroken; state++)
			for(fprint=0; fprint<2; fprint++)
				for(mode=0; mode<25; mode++){
					syswritecase(phase, state, fprint, mode);
					cases++;
				}
	printf("PASS native64 Sys write gateways: %d release/write/acquire/scalar/FD/GC/error transactions\n", cases);
	return 0;
}
