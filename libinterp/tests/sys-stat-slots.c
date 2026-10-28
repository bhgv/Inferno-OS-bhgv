/* This fixture uses the guarded heap/native adapters shared with sys-dirread.c. */
static int operation, returnoffset, nativecalls;
static Osenv environment;
static String *watchedstring;
static uchar *stringownerslot;

#include <pthread.h>


char Ebadarg[] = "argument";
static int querybadfd, querybadargs;
static int queryframeoffset, queryfdoffset, queryphase, queryactive, querymutated;
static WORD querynumber = 17;
static int querywantfd = 17, querysentinel;
static uchar queryframe[sizeof(F_Sys_stat)+sizeof(F_Sys_fstat)+sizeof(F_Sys_fd2path)+32];
static uchar queryimage[sizeof(queryframe)];
static uchar queryhandle[sizeof(Sys_FD)+8];
static uchar *queryoriginal;
static uintptr querysize;


/* Keep all predicates meaningful when testing a release-mode VM build. */
static void
statpredicate(int valid, int line)
{
	if(!valid) {
		fprintf(stderr, "Sys query predicate failed at %d op=%d phase=%d\n", line, operation, queryphase);
		abort();
	}
}


/* Observe ordinary descriptor-width errors without escaping the VM error frame. */
void
seterror(char *message, ...)
{
	stattest(message == Ebadarg && !released);
	querybadargs++;
}


/* Preserve deliberate callback mutations of the original fixed frame and FD view. */
static void
querycallback(int point)
{
	WORD number = 99;

	if(!queryactive || queryphase != point || querymutated)
		return;
	querymutated = 1;
	memset(queryoriginal, 0xa7, querysize);
	memmove(queryhandle+queryfdoffset+O(Sys_FD, fd), &number, sizeof(number));
}


/* Supply packed original operands before the wrapper gets its own aligned snapshot. */
static void*
querybegin(void *frame)
{
	Sys_FD handle;
	Sys_FD *pointer;
	uintptr offset;

	querysize = operation == 0 ? sizeof(F_Sys_stat) : operation == 1 ? sizeof(F_Sys_fstat) : sizeof(F_Sys_fd2path);
	queryoriginal = queryframe+8+queryframeoffset;
	memset(queryframe, 0x3c, sizeof(queryframe));
	memmove(queryoriginal, frame, querysize);
	memset(&handle, 0, sizeof(handle));
	handle.fd = querynumber;
	memmove(queryhandle+queryfdoffset, &handle, sizeof(handle));
	if(operation != 0) {
		offset = operation == 1 ? O(F_Sys_fstat, fd) : O(F_Sys_fd2path, fd);
		pointer = querysentinel ? (Sys_FD*)H : (Sys_FD*)(queryhandle+queryfdoffset);
		memmove(queryoriginal+offset, &pointer, sizeof(pointer));
	}
	memmove(queryimage, queryframe, sizeof(queryimage));
	queryactive = 1;
	querymutated = querybadargs = 0;
	return queryoriginal;
}


/* Compare complete frame images after native calls and partial conversion failures. */
static void
queryend(void)
{
	queryactive = 0;
	if(querymutated)
		memset(queryimage+(queryoriginal-queryframe), 0xa7, querysize);
	stattest(!memcmp(queryframe, queryimage, sizeof(queryimage)));
	stattest(querybadargs == querybadfd);
}


/* Keep scheduler ownership strict while exposing callbacks at both boundaries. */
void
release(void)
{
	stattest(!released);
	released = 1;
	querycallback(1);
}


/* Reacquisition can also change borrowed VM operands before conversion begins. */
void
acquire(void)
{
	stattest(released);
	querycallback(3);
	released = 0;
}


/* Reuse independently populated native metadata for a single descriptor query. */
Dir*
kdirfstat(int fd)
{
	Dir *record;

	stattest(released && fd==querywantfd && !nativecalls++);
	querycallback(2);
	stattest(kdirread(17,&record)==nativecount);
	return record;
}


/* Check the path before entering the same controlled native query. */
Dir*
kdirstat(char *path)
{
	stattest(!strcmp(path,"lookup"));
	return kdirfstat(17);
}


/* Return a separately owned UTF-8 native path or a native failure. */
char*
kfd2path(int fd)
{
	char *path;

	stattest(released && fd==querywantfd && !nativecalls++ && !nativelive);
	querycallback(2);
	if(!nativecount)
		return nil;
	path=malloc(strlen(texts[1])+1);
	stattest(path);
	strcpy(path,texts[1]);
	nativebuffer=path;
	nativelive=1;
	return path;
}


/* Keep conversion of the query argument independent of allocation-failure budgets. */
char*
string2c(String *string)
{
	stattest(released && string==(String*)(uintptr)0x1234);
	querycallback(5);
	return "lookup";
}


/* Observe publication before finalizing an old fd2path result. */
static void
watchstring(void *value)
{
	String *published;

	if(value!=H && value==watchedstring){
		memmove(&published,stringownerslot,sizeof(published));
		stattest(published==H);
		watchedstring=H;
	}
	querycallback(6);
}


/* Invoke each actual wrapper under a real outer runtime protection frame. */
static int
querystat(void *frame)
{
	frame = querybegin(frame);
	if(operation==0)
		Sys_stat(frame);
	else if(operation==1)
		Sys_fstat(frame);
	else
		Sys_fd2path(frame);
	queryend();
	return 1;
}


/* Compare metadata through an independent field oracle, preserving tuple and record padding. */
static void
metadata(Sys_Dir *record)
{
	record->qid.path=(uvlong)1<<48;
	record->qid.vers=123;
	record->qid.qtype=0;
	record->mode=0640;
	record->atime=1000;
	record->mtime=2000;
	record->length=(vlong)1<<45;
	record->dtype=7;
	record->dev=11;
}


/* Check incremental string ownership and unchanged scalar fields after every partial failure. */
static void
statcase(int available,int fail,int shared)
{
	struct { WORD status; Sys_Dir dir; } initial, actual;
	F_Sys_stat stat={0};
	F_Sys_fstat fstat={0};
	Sys_FD fd={17};
	union { uvlong align; uchar bytes[512]; } memory;
	uchar expected[512];
	String *old[4], *current[4];
	char *newtext;
	char *oldtext[]={"old name","old uid","old gid","old muid"};
	Sys_Dir wanted;
	WORD status;
	int i,ordinal,reached,success,budget;
	size_t directoryoffset=__builtin_offsetof(__typeof__(*stat.ret),t1);

	failure=0;
	memset(&initial,0xa5,sizeof(initial));
	initial.status=-77;
	for(i=0;i<4;i++){
		old[i]=shared && i ? old[0] : c2string(oldtext[i],strlen(oldtext[i]));
		if(shared && i)
			D2H(old[0])->ref++;
	}
	initial.dir.name=old[0]; initial.dir.uid=old[1];
	initial.dir.gid=old[2]; initial.dir.muid=old[3];
	memset(memory.bytes,0xa5,sizeof(memory.bytes));
	memmove(memory.bytes+16+returnoffset,&initial.status,sizeof(initial.status));
	memmove(memory.bytes+16+returnoffset+directoryoffset,&initial.dir,sizeof(initial.dir));
	memmove(expected,memory.bytes,sizeof(expected));
	stat.ret=(void*)(memory.bytes+16+returnoffset);
	stat.s=(String*)(uintptr)0x1234;
	fstat.ret=(void*)(memory.bytes+16+returnoffset);
	fstat.fd=&fd;
	nativecount=available;
	nativecalls=allocations=0;
	failure=fail;
	raised=nil;
	stattest(directoryoffset==__builtin_offsetof(__typeof__(*fstat.ret),t1));
	stattest(kprotect(querystat,operation ? (void*)&fstat : (void*)&stat));
	queryactive = 0;
	stattest(!released && !nativelive && nativecalls==!querybadfd && worker.nerr==0);
	failure=0;
	memmove(&actual.status,memory.bytes+16+returnoffset,sizeof(actual.status));
	memmove(&actual.dir,memory.bytes+16+returnoffset+directoryoffset,sizeof(actual.dir));
	current[0]=actual.dir.name; current[1]=actual.dir.uid;
	current[2]=actual.dir.gid; current[3]=actual.dir.muid;
	ordinal=1;
	reached=available && !querybadfd;
	for(i=0;i<4;i++){
		newtext=i==1 && pattern ? nil : texts[i];
		if(reached && newtext && fail==ordinal)
			reached=0;
		if(reached)
			checkstring(current[i],newtext);
		else{
			stattest(current[i]==old[i]);
			checkstring(current[i],oldtext[shared ? 0 : i]);
		}
		if(newtext)
			ordinal++;
	}
	budget=pattern ? 3 : 4;
	success=available && !querybadfd && !(fail>0 && fail<=budget);
	stattest((raised==exHeap)==(available && !querybadfd && fail>0 && fail<=budget));
	status=success ? 0 : -1;
	stattest(actual.status==status);
	wanted=initial.dir;
	wanted.name=current[0]; wanted.uid=current[1];
	wanted.gid=current[2]; wanted.muid=current[3];
	if(success)
		metadata(&wanted);
	memmove(expected+16+returnoffset,&status,sizeof(status));
	memmove(expected+16+returnoffset+directoryoffset,&wanted,sizeof(wanted));
	stattest(!memcmp(expected,memory.bytes,sizeof(expected)));
	for(i=0;i<4;i++)
		destroy(current[i]);
	stattest(!live && Tstring.ref==1);
	checks++;
}


/* Check nil-before-finalization, native-buffer cleanup and every result byte alignment. */
static void
pathcase(int available,int fail)
{
	F_Sys_fd2path frame={0};
	Sys_FD fd={17};
	union { uvlong align; uchar bytes[40]; } memory;
	uchar expected[40];
	String *old,*result;

	failure=0;
	old=c2string("previous",8);
	memset(memory.bytes,0xa5,sizeof(memory.bytes));
	memmove(memory.bytes+8+returnoffset,&old,sizeof(old));
	memmove(expected,memory.bytes,sizeof(expected));
	frame.ret=(void*)(memory.bytes+8+returnoffset);
	frame.fd=&fd;
	watchedstring=old;
	stringownerslot=memory.bytes+8+returnoffset;
	nativecount=available;
	nativecalls=allocations=0;
	failure=fail;
	raised=nil;
	stattest(kprotect(querystat,&frame));
	queryactive = 0;
	stattest(!released && !nativelive && nativecalls==!querybadfd && watchedstring==H && worker.nerr==0);
	failure=0;
	memmove(&result,memory.bytes+8+returnoffset,sizeof(result));
	stattest((raised==exHeap)==(available && !querybadfd && fail==1));
	if(available && !querybadfd && fail!=1)
		checkstring(result,texts[1]);
	else
		stattest(result==H);
	memmove(expected+8+returnoffset,&result,sizeof(result));
	stattest(!memcmp(expected,memory.bytes,sizeof(expected)));
	destroy(result);
	stattest(!live && Tstring.ref==1);
	checks++;
}


/* Invoke the wrapper directly when a deliberately invalid frame cannot be copied by the fixture. */
static int
rawquery(void *frame)
{
	if(operation == 0)Sys_stat(frame);
	else if(operation == 1)Sys_fstat(frame);
	else Sys_fd2path(frame);
	return 1;
}


/* Reject invalid frames, complete results and old owners without changing any result byte. */
static void
querypreflight(int mode)
{
	F_Sys_stat stat = {0};
	F_Sys_fstat fstat = {0};
	F_Sys_fd2path path = {0};
	Sys_FD fd = {17};
	String *old, *candidate, *empty = H;
	void *fp, *ret;
	uintptr width, offset;
	uchar before[512];
	union {
		uvlong align; // Align the backing store for valid preflight tuples.
		uchar bytes[512]; // Retain canaries around the complete generated result.
	} result;
	char *expected;
	int ok;

	failure = 0;
	old = c2string("previous", 8);
	width = operation == 0 ? sizeof(*stat.ret) : operation == 1 ? sizeof(*fstat.ret) : sizeof(*path.ret);
	offset = operation == 2 ? 0 : __builtin_offsetof(__typeof__(*stat.ret), t1);
	memset(result.bytes, 0xa5, sizeof(result.bytes));
	ret = result.bytes+16;
	if(operation != 2) {
		WORD status = -77;
		memmove(ret, &status, sizeof(status));
		memmove((uchar*)ret+offset+O(Sys_Dir, name), &old, sizeof(old));
		memmove((uchar*)ret+offset+O(Sys_Dir, uid), &empty, sizeof(empty));
		memmove((uchar*)ret+offset+O(Sys_Dir, gid), &empty, sizeof(empty));
		memmove((uchar*)ret+offset+O(Sys_Dir, muid), &empty, sizeof(empty));
	} else
		memmove(ret, &old, sizeof(old));
	expected = mode == 2 || mode == 5 || mode == 12 ? exBounds : mode >= 7 && mode <= 10 ? exHeap : exNilref;
	candidate = old;
	if(mode == 6)candidate = nil;
	if(mode == 7)candidate = (String*)1;
	if(mode == 8)candidate = (String*)((uchar*)old+1);
	if(mode == 9)D2H(old)->ref = 0;
	if(mode >= 6 && mode <= 9)
		memmove((uchar*)ret+offset, &candidate, sizeof(candidate));
	if(mode == 10)
		memmove((uchar*)ret+offset+O(Sys_Dir, uid), &old, sizeof(old));
	memmove(before, result.bytes, sizeof(before));
	if(mode == 3)ret = nil;
	if(mode == 4)ret = H;
	if(mode == 5)ret = (void*)((uintptr)-1-(operation == 2 ? width-1 : sizeof(WORD)+1));
	stat.ret = ret;
	stat.s = (String*)(uintptr)0x1234;
	fstat.ret = ret;
	fstat.fd = &fd;
	path.ret = ret;
	path.fd = &fd;
	if(mode == 11)fstat.fd = path.fd = nil;
	if(mode == 12)fstat.fd = path.fd = (Sys_FD*)((uintptr)-1-1);
	fp = operation == 0 ? (void*)&stat : operation == 1 ? (void*)&fstat : (void*)&path;
	if(mode == 0)fp = nil;
	if(mode == 1)fp = H;
	if(mode == 2)fp = (void*)((uintptr)-1-1);
	nativecalls = allocations = querybadargs = 0;
	queryactive = 0;
	raised = nil;
	ok = kprotect(rawquery, fp);
	stattest(!ok && raised == expected && worker.nerr == 0);
	stattest(!nativecalls && !allocations && !querybadargs && !released && !nativelive);
	stattest(!memcmp(result.bytes, before, sizeof(before)));
	if(mode == 9)D2H(old)->ref = 1;
	destroy(old);
	stattest(!live && Tstring.ref == 1);
	checks++;
}


/* Exercise native failures, aliasing old string owners and every conversion failure. */
static void*
statworker(void *unused)
{
	int available,fail,shared, index, mode;
	vlong numbers[] = {17, 0, -1, 0x7fffffff, -0x7fffffff-1, -1,
		2147483648LL, -2147483649LL, 4294967313LL, -1099511627776LL,
		9223372036854775807LL, -9223372036854775807LL-1};
	char *family = getenv("SYS_STAT_FAMILY");

	USED(unused);
	worker.env=&environment;
	environment.errstr=environment.errbuf0;
	sysinit();
	if(family != nil && !strcmp(family, "preflight")) {
		for(operation=0; operation<3; operation++)
		for(mode=0; mode<13; mode++) {
			if((operation == 0 && mode >= 11) || (operation == 2 && mode == 10))
				continue;
			if(getenv("SYS_STAT_OPERATION") && operation != atoi(getenv("SYS_STAT_OPERATION")))
				continue;
			if(getenv("SYS_STAT_ERROR") && mode != atoi(getenv("SYS_STAT_ERROR")))
				continue;
			querypreflight(mode);
		}
	} else if(family != nil && !strcmp(family, "nativefd")) {
		queryphase = 1;
		for(operation=1; operation<3; operation++)
		for(index=0; index<(sizeof(WORD)>4 ? 12 : 6); index++)
		for(queryframeoffset=0; queryframeoffset<8; queryframeoffset++)
		for(queryfdoffset=0; queryfdoffset<8; queryfdoffset++)
		for(returnoffset=0; returnoffset<8; returnoffset++) {
			if(getenv("SYS_STAT_OPERATION") && operation != atoi(getenv("SYS_STAT_OPERATION")))
				continue;
			if(getenv("SYS_STAT_NUMBER") && index != atoi(getenv("SYS_STAT_NUMBER")))
				continue;
			if(getenv("SYS_STAT_PHASE"))queryphase = atoi(getenv("SYS_STAT_PHASE"));
			querynumber = numbers[index];
			querysentinel = index == 5;
			querybadfd = index >= 6;
			querywantfd = querybadfd ? 17 : (int)numbers[index];
			if(operation == 1)
				for(fail=0; fail<6; fail++)statcase(1, fail, 1);
			else
				for(fail=0; fail<3; fail++)pathcase(1, fail);
		}
	} else {
		for(queryphase=0; queryphase<(getenv("SYS_STAT_CAPTURE") ? 7 : 1); queryphase++)
		for(queryframeoffset=0; queryframeoffset<(getenv("SYS_STAT_CAPTURE") ? 8 : 1); queryframeoffset++)
		for(queryfdoffset=0; queryfdoffset<(getenv("SYS_STAT_CAPTURE") ? 8 : 1); queryfdoffset++)
		for(operation=0;operation<3;operation++)
		for(returnoffset=0;returnoffset<8;returnoffset++)
		for(available=0;available<2;available++){
			if(getenv("SYS_STAT_PHASE") && queryphase != atoi(getenv("SYS_STAT_PHASE")))
				continue;
			if(getenv("SYS_STAT_FRAME") && queryframeoffset != atoi(getenv("SYS_STAT_FRAME")))
				continue;
			if(getenv("SYS_STAT_FD_SHIFT") && queryfdoffset != atoi(getenv("SYS_STAT_FD_SHIFT")))
				continue;
			if(getenv("SYS_STAT_OPERATION") && atoi(getenv("SYS_STAT_OPERATION"))!=operation)
				continue;
			if(getenv("SYS_STAT_ALIGNED") && returnoffset!=0)
				continue;
			if(operation==2){
				for(fail=0;fail<3;fail++)
					pathcase(available,fail);
			}else
				for(pattern=0;pattern<2;pattern++)for(shared=0;shared<2;shared++)
				for(fail=0;fail<6;fail++)
					statcase(available,fail,shared);
		}
	}
	stattest(checks != 0);
	printf("PASS Sys stat/path slots: %d tuple/string/native-buffer/failure cases WORD%zu\n",checks,sizeof(WORD)*8);
	return nil;
}


/* Give ASan a tracked stack while retaining real runtime nonlocal error handling. */
int
main(void)
{
	pthread_t thread;

	stattest(pthread_create(&thread, nil, statworker, nil) == 0);
	stattest(pthread_join(thread, nil) == 0);
	return 0;
}
