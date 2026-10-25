/* This fixture uses the guarded heap/native adapters shared with sys-dirread.c. */
static int operation, returnoffset, nativecalls;
static Osenv environment;
static String *watchedstring;
static uchar *stringownerslot;

/* Reuse independently populated native metadata for a single descriptor query. */
Dir*
kdirfstat(int fd)
{
	Dir *record;

	assert(released && fd==17 && !nativecalls++);
	assert(kdirread(fd,&record)==nativecount);
	return record;
}

/* Check the path before entering the same controlled native query. */
Dir*
kdirstat(char *path)
{
	assert(!strcmp(path,"lookup"));
	return kdirfstat(17);
}

/* Return a separately owned UTF-8 native path or a native failure. */
char*
kfd2path(int fd)
{
	char *path;

	assert(released && fd==17 && !nativecalls++ && !nativelive);
	if(!nativecount)
		return nil;
	path=malloc(strlen(texts[1])+1);
	assert(path);
	strcpy(path,texts[1]);
	nativebuffer=path;
	nativelive=1;
	return path;
}

/* Keep conversion of the query argument independent of allocation-failure budgets. */
char*
string2c(String *string)
{
	assert(released && string==(String*)(uintptr)0x1234);
	return "lookup";
}

/* Observe publication before finalizing an old fd2path result. */
static void
watchstring(void *value)
{
	String *published;

	if(value!=H && value==watchedstring){
		memmove(&published,stringownerslot,sizeof(published));
		assert(published==H);
		watchedstring=H;
	}
}

/* Invoke each actual wrapper under a real outer runtime protection frame. */
static int
querystat(void *frame)
{
	if(operation==0)
		Sys_stat(frame);
	else if(operation==1)
		Sys_fstat(frame);
	else
		Sys_fd2path(frame);
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
	assert(directoryoffset==__builtin_offsetof(__typeof__(*fstat.ret),t1));
	assert(kprotect(querystat,operation ? (void*)&fstat : (void*)&stat));
	assert(!released && !nativelive && nativecalls==1 && worker.nerr==0);
	failure=0;
	memmove(&actual.status,memory.bytes+16+returnoffset,sizeof(actual.status));
	memmove(&actual.dir,memory.bytes+16+returnoffset+directoryoffset,sizeof(actual.dir));
	current[0]=actual.dir.name; current[1]=actual.dir.uid;
	current[2]=actual.dir.gid; current[3]=actual.dir.muid;
	ordinal=1;
	reached=available;
	for(i=0;i<4;i++){
		newtext=i==1 && pattern ? nil : texts[i];
		if(reached && newtext && fail==ordinal)
			reached=0;
		if(reached)
			checkstring(current[i],newtext);
		else{
			assert(current[i]==old[i]);
			checkstring(current[i],oldtext[shared ? 0 : i]);
		}
		if(newtext)
			ordinal++;
	}
	budget=pattern ? 3 : 4;
	success=available && !(fail>0 && fail<=budget);
	assert((raised==exHeap)==(available && fail>0 && fail<=budget));
	status=success ? 0 : -1;
	assert(actual.status==status);
	wanted=initial.dir;
	wanted.name=current[0]; wanted.uid=current[1];
	wanted.gid=current[2]; wanted.muid=current[3];
	if(success)
		metadata(&wanted);
	memmove(expected+16+returnoffset,&status,sizeof(status));
	memmove(expected+16+returnoffset+directoryoffset,&wanted,sizeof(wanted));
	assert(!memcmp(expected,memory.bytes,sizeof(expected)));
	for(i=0;i<4;i++)
		destroy(current[i]);
	assert(!live && Tstring.ref==1);
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
	assert(kprotect(querystat,&frame));
	assert(!released && !nativelive && nativecalls==1 && watchedstring==H && worker.nerr==0);
	failure=0;
	memmove(&result,memory.bytes+8+returnoffset,sizeof(result));
	assert((raised==exHeap)==(available && fail==1));
	if(available && fail!=1)
		checkstring(result,texts[1]);
	else
		assert(result==H);
	memmove(expected+8+returnoffset,&result,sizeof(result));
	assert(!memcmp(expected,memory.bytes,sizeof(expected)));
	destroy(result);
	assert(!live && Tstring.ref==1);
	checks++;
}

/* Exercise native failures, aliasing old string owners and every conversion failure. */
int
main(void)
{
	int available,fail,shared;

	worker.env=&environment;
	environment.errstr=environment.errbuf0;
	sysinit();
	for(operation=0;operation<3;operation++)
	for(returnoffset=0;returnoffset<8;returnoffset++)
	for(available=0;available<2;available++){
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
	printf("PASS Sys stat/path slots: %d tuple/string/native-buffer/failure cases WORD%zu\n",checks,sizeof(WORD)*8);
	return 0;
}
