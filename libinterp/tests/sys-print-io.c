
static Fgrp iogroup;
static Chan iostdout;
static Chan *iofds[2];
static uchar ioframe[512], ioframebefore[512], ioslot[32], ioslotbefore[32], iofd[32];
static char ioerror[ERRMAX], ioothererror[ERRMAX];
static void *ioalternate;
static String *ioformat;
static WORD ionumber;
static long iostatus;
static int ioop, iofs, iors, iods, iomode, iophase, iocallbacks;
static int ioreleased, ioenters, ioexits, iowrites, iocopies, iobad;
static unsigned long iocases;


/* Print the complete active I/O/frame geometry before stopping a predicate failure. */
static void
iocheck(int test)
{
	if(!test) {
		fprintf(stderr, "print I/O op=%d frame=%d result=%d fd=%d mode=%d callback=%d\n",
			ioop, iofs, iors, iods, iomode, iophase);
		abort();
	}
}


/* Model callbacks borrowing frame fields and process error-storage selection. */
static void
ioborrow(int phase)
{
	void *ret;
	String *poison;
	Sys_FD *handle;
	int ro, so;

	if(iophase != phase || getenv("PRINT_IO_NO_BORROW"))
		return;
	ro = ioop == 0 ? offsetof(F_Sys_print, ret) : ioop == 1 ? offsetof(F_Sys_fprint, ret) : offsetof(F_Sys_werrstr, ret);
	so = ioop == 0 ? offsetof(F_Sys_print, s) : ioop == 1 ? offsetof(F_Sys_fprint, s) : offsetof(F_Sys_werrstr, s);
	ret = &ioalternate;
	poison = (String*)1;
	memmove(ioframe+iofs+ro, &ret, sizeof(ret));
	memmove(ioframe+iofs+so, &poison, sizeof(poison));
	if(ioop == 1) {
		handle = H;
		memmove(ioframe+iofs+offsetof(F_Sys_fprint, fd), &handle, sizeof(handle));
	}
	worker.env->errstr = ioothererror;
	iocallbacks++;
}


/* Observe release before formatting and allow the callback to borrow the frame. */
void
release(void)
{
	iocheck(!ioreleased);
	ioreleased = 1;
	ioenters++;
	ioborrow(1);
}


/* Every successful native write must return to the held VM state exactly once. */
void
acquire(void)
{
	iocheck(ioreleased);
	ioreleased = 0;
	ioexits++;
}


/* Invalid high descriptor bits report an argument error without release or output. */
void
seterror(char *text, ...)
{
	iocheck(text == Ebadarg && !ioreleased);
	iobad++;
}


/* Snapshot tests use the true UTF/format engine and a controlled final write only. */
long
kwrite(int descriptor, void *bytes, long length)
{
	int expected, i;
	char *bigtext = "-9223372036854775808";

	iocheck(ioreleased && ioop < 2);
	expected = iomode == 1 ? 2048 : iomode == 4 ? 20 : 3;
	iocheck(length == expected);
	for(i = 0; i < length; i++)
		iocheck(((char*)bytes)[i] == (iomode == 4 ? bigtext[i] : 'x'));
	iocheck(descriptor == (ioop == 0 ? 1 : iomode == 2 ? -1 : (int)ionumber));
	iocheck(live == (iomode == 1));
	iowrites++;
	ioborrow(3);
	return iostatus;
}


/* Model the conversion allocation boundary while retaining the captured source. */
char*
string2c(String *source)
{
	iocheck(!ioreleased && ioop == 2 && source == ioformat);
	ioborrow(3);
	return "error text";
}


/* Preserve the original error buffer even if conversion changed its environment field. */
void
kstrcpy(char *destination, char *source, int size)
{
	iocheck(!ioreleased && ioop == 2 && destination == ioerror && size == ERRMAX);
	iocheck(strcmp(source, "error text") == 0);
	strcpy(destination, source);
	iocopies++;
	ioborrow(4);
}


/* Populate packed fixed frame fields while keeping varargs at their original address. */
static void
ioframeinit(void *ret, String *format, Sys_FD *handle)
{
	int ro, so;

	memset(ioframe, 0xa5, sizeof(ioframe));
	ro = ioop == 0 ? offsetof(F_Sys_print, ret) : ioop == 1 ? offsetof(F_Sys_fprint, ret) : offsetof(F_Sys_werrstr, ret);
	so = ioop == 0 ? offsetof(F_Sys_print, s) : ioop == 1 ? offsetof(F_Sys_fprint, s) : offsetof(F_Sys_werrstr, s);
	memmove(ioframe+iofs+ro, &ret, sizeof(ret));
	memmove(ioframe+iofs+so, &format, sizeof(format));
	if(ioop == 1)
		memmove(ioframe+iofs+offsetof(F_Sys_fprint, fd), &handle, sizeof(handle));
}


/* Invoke actual emu/os public entries under actual runtime error frames. */
static int
ioinvoke(void *frame)
{
	if(ioop == 0)
		Sys_print(frame);
	else if(ioop == 1)
		Sys_fprint(frame);
	else
		Sys_werrstr(frame);
	return 1;
}


/* Reset independent native-call and process-environment observations. */
static void
ioreset(void)
{
	worker.env = &worker.defenv;
	worker.env->fgrp = &iogroup;
	worker.env->errstr = ioerror;
	iogroup.fd = iofds;
	iogroup.nfd = 2;
	iofds[1] = &iostdout;
	memset(ioerror, 0x5a, sizeof(ioerror));
	memset(ioothererror, 0x79, sizeof(ioothererror));
	memset(ioslot, 0xa5, sizeof(ioslot));
	ioreleased = ioenters = ioexits = iowrites = iocopies = iobad = iocallbacks = 0;
	ioalternate = H;
}


/* Exercise output snapshots and native statuses over packed frame/result/FD cells. */
static void
checkio(void)
{
	char text[2049];
	WORD result, expected;
	int status, i, ok;
	long statuses[] = {0, 1, -1, 0x7fffffff, (-0x7fffffff-1)};

	for(ioop = 0; ioop < 3; ioop++)
	for(iofs = 0; iofs < 8; iofs++)
	for(iors = 0; iors < 8; iors++)
	for(iods = 0; iods < 8; iods++)
	for(iomode = 0; iomode < 5; iomode++)
	for(iophase = 0; iophase < 5; iophase++)
	for(status = 0; status < nelem(statuses); status++) {
		if(getenv("PRINT_IO_OP") && ioop != atoi(getenv("PRINT_IO_OP")))
			continue;
		if(ioop == 2 && iomode >= 3)
			continue;
		if((ioop < 2 && iophase != 0 && iophase != 1 && iophase != 3) ||
		   (ioop == 2 && iophase != 0 && iophase != 3 && iophase != 4))
			continue;
		ioreset();
		ionumber = 17;
		memmove(iofd+iods, &ionumber, sizeof(ionumber));
		iostatus = statuses[status];
		memset(text, 'x', iomode == 1 ? 2048 : 3);
		text[iomode == 1 ? 2048 : 3] = 0;
		ioformat = string(iomode == 3 ? "%c%c%c" : iomode == 4 ? "%bd" : text, 0);
		ioframeinit(ioslot+iors, ioformat, iomode == 2 ? H : (Sys_FD*)(iofd+iods));
		if(iomode >= 3) {
			int offset = ioop ? offsetof(F_Sys_fprint, vargs) : offsetof(F_Sys_print, vargs);
			int character = 'x';
			LONG big = (-9223372036854775807LL-1);
			if(iomode == 3) {
				for(i = 0; i < 3; i++)
					memmove(ioframe+iofs+offset+i*IBY2WD, &character, sizeof(character));
			} else {
				offset += (-offset)&7;
				memmove(ioframe+iofs+offset, &big, sizeof(big));
			}
		}
		memmove(ioframebefore, ioframe, sizeof(ioframebefore));
		if(ioop == 0 && iomode == 2)
			iofds[1] = nil;
		ok = kprotect(ioinvoke, ioframe+iofs);
		iocheck(ok && worker.nerr == 0 && !ioreleased);
		memmove(&result, ioslot+iors, sizeof(result));
		expected = ioop == 2 ? 0 : ioop == 0 && iomode == 2 ? -1 : (WORD)iostatus;
		iocheck(result == expected && ioalternate == H && live == 0 && vmlive == 0);
		iocheck(iowrites == (ioop < 2 && !(ioop == 0 && iomode == 2)));
		iocheck(ioenters == iowrites && ioexits == iowrites && iocopies == (ioop == 2) && iobad == 0);
		if(ioop == 2)
			iocheck(strcmp(ioerror, "error text") == 0);
		for(i = 0; i < sizeof(ioothererror); i++)
			iocheck(ioothererror[i] == 0x79);
		for(i = 0; i < sizeof(ioslot); i++)
			if(i < iors || i >= iors+sizeof(result))
				iocheck(ioslot[i] == 0xa5);
		if(iocallbacks) {
			void *ret = &ioalternate;
			String *poison = (String*)1;
			Sys_FD *handle = H;
			int ro = ioop == 0 ? offsetof(F_Sys_print, ret) : ioop == 1 ? offsetof(F_Sys_fprint, ret) : offsetof(F_Sys_werrstr, ret);
			int so = ioop == 0 ? offsetof(F_Sys_print, s) : ioop == 1 ? offsetof(F_Sys_fprint, s) : offsetof(F_Sys_werrstr, s);
			memmove(ioframebefore+iofs+ro, &ret, sizeof(ret));
			memmove(ioframebefore+iofs+so, &poison, sizeof(poison));
			if(ioop == 1)
				memmove(ioframebefore+iofs+offsetof(F_Sys_fprint, fd), &handle, sizeof(handle));
			iocheck(worker.env->errstr == ioothererror);
		} else
			iocheck(worker.env->errstr == ioerror);
		iocheck(memcmp(ioframe, ioframebefore, sizeof(ioframe)) == 0);
		free(ioformat);
		iocases++;
	}
}


/* Reject malformed call/result/FD spans and high native descriptor bits before I/O. */
static void
checkioerrors(void)
{
	WORD result;
	void *frame, *ret;
	Sys_FD *handle;
	int mode, ok, extent;
	char *why;

	iophase = iomode = iods = 0;
	for(ioop = 0; ioop < 3; ioop++)
	for(iofs = 0; iofs < 8; iofs++)
	for(iors = 0; iors < 8; iors++)
	for(mode = 0; mode < 7; mode++) {
		if(getenv("PRINT_IO_OP") && ioop != atoi(getenv("PRINT_IO_OP")))
			continue;
		if(getenv("PRINT_IO_ERROR") && mode != atoi(getenv("PRINT_IO_ERROR")))
			continue;
		if(mode >= 4 && ioop != 1)
			continue;
		if(mode == 6 && sizeof(WORD) == 4)
			continue;
		ioreset();
		ionumber = mode == 6 ? (WORD)((uvlong)1<<32)+17 : 17;
		memmove(iofd, &ionumber, sizeof(ionumber));
		handle = (Sys_FD*)iofd;
		ret = ioslot+iors;
		if(mode == 0)
			ret = (void*)((uintptr)-1-sizeof(result)+1);
		if(mode == 1)
			ret = nil;
		if(mode == 4)
			handle = (Sys_FD*)((uintptr)-1-sizeof(WORD)+1);
		if(mode == 5)
			handle = nil;
		ioformat = (String*)1;
		ioframeinit(ret, ioformat, handle);
		frame = ioframe+iofs;
		extent = ioop == 0 ? offsetof(F_Sys_print, vargs) : ioop == 1 ? offsetof(F_Sys_fprint, vargs) : offsetof(F_Sys_werrstr, s)+sizeof(String*);
		if(mode == 2)
			frame = (void*)((uintptr)-1-extent+1);
		if(mode == 3)
			frame = H;
		memmove(ioslotbefore, ioslot, sizeof(ioslotbefore));
		raised = nil;
		ok = kprotect(ioinvoke, frame);
		if(mode == 6) {
			memmove(&result, ioslot+iors, sizeof(result));
			iocheck(ok && result == -1 && iobad == 1);
		} else {
			why = mode == 1 || mode == 3 || mode == 5 ? exNilref : exBounds;
			iocheck(!ok && raised == why && memcmp(ioslot, ioslotbefore, sizeof(ioslot)) == 0);
		}
		iocheck(worker.nerr == 0 && !ioreleased && !ioenters && !ioexits && !iowrites && !iocopies && live == 0);
		iocases++;
	}
}


/* Preserve signed native descriptor endpoints and reject every higher WORD bit. */
static void
checkfdnumbers(void)
{
	vlong values[] = {17, 0, -1, 0x7fffffff, (-0x7fffffffLL-1),
		0x80000000LL, (-0x80000000LL-1), 0x100000011LL, -0x10000000000LL,
		0x7fffffffffffffffLL, (-0x7fffffffffffffffLL-1)};
	long statuses[] = {0, 1, -1, 0x7fffffff, (-0x7fffffff-1)};
	WORD result;
	int profile, status, valid, ok;

	ioop = 1;
	iomode = 0;
	iophase = getenv("PRINT_IO_NO_BORROW") ? 0 : 1;
	for(iofs = 0; iofs < 8; iofs++)
	for(iors = 0; iors < 8; iors++)
	for(iods = 0; iods < 8; iods++)
	for(profile = 0; profile < nelem(values); profile++)
	for(status = 0; status < nelem(statuses); status++) {
		if(sizeof(WORD) == 4 && profile >= 5)
			continue;
		valid = profile < 5;
		ioreset();
		ionumber = values[profile];
		iostatus = statuses[status];
		memmove(iofd+iods, &ionumber, sizeof(ionumber));
		ioformat = string("xxx", 0);
		ioframeinit(ioslot+iors, ioformat, (Sys_FD*)(iofd+iods));
		ok = kprotect(ioinvoke, ioframe+iofs);
		memmove(&result, ioslot+iors, sizeof(result));
		iocheck(ok && worker.nerr == 0 && !ioreleased && live == 0);
		iocheck(result == (valid ? (WORD)iostatus : -1));
		iocheck(ioenters == valid && ioexits == valid && iowrites == valid && iobad == !valid);
		iocheck(iocallbacks == (valid && iophase != 0) && ioalternate == H);
		free(ioformat);
		iocases++;
	}
}


/* The common driver supplies the registered pthread and real formatter helpers. */
static int
printspanmain(void)
{
	char *family;

	family = getenv("SYS_PRINT_CASE");
	if(family && strcmp(family, "preflight") == 0)
		checkioerrors();
	else if(family && strcmp(family, "nativefd") == 0)
		checkfdnumbers();
	else
		checkio();
	if(getenv("PRINT_IO_OP") == nil && getenv("PRINT_IO_ERROR") == nil) {
		unsigned long expected = family && strcmp(family, "preflight") == 0 ? (sizeof(WORD) == 8 ? 960 : 896) :
			family && strcmp(family, "nativefd") == 0 ? (sizeof(WORD) == 8 ? 28160 : 12800) : 99840;
		iocheck(iocases == expected);
	}
	printf("PASS print I/O: %lu frame/snapshot/status/side-effect cases WORD%zu\n", iocases, sizeof(WORD)*8);
	return 0;
}
