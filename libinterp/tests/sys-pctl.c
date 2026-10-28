#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "runt.h"
#include "kernel.h"
#include <pthread.h>
#include <stdio.h>

#define pctltest(test) pctlpredicate(!!(test), __LINE__) // Keep ownership predicates active with NDEBUG.


/* Diagnose explicit invariants independently of the compiler assertion mode. */
static void
pctlpredicate(int valid, int line)
{
	if(!valid){
		fprintf(stderr, "Sys pctl predicate failed at line %d\n", line);
		abort();
	}
}

char exNilref[] = "nil", exBounds[] = "bounds", exHeap[] = "heap";
char Ebadarg[]="bad argument";
static char injected[]="injected";
static char *raised;
static Proc worker;
static Prog program;
static Osenv environment;
static Fgrp oldgroup,newgroup;
static Chan channels[8];
static Chan *oldslots[8],*newslots[8];
static int released,enters,leaves,locked,failgroup,created,badargs,closes,cleanup;
static int emptypgrp,emptyegrp,currentcalls;
static int capturephase, armed, mutated, frameoffset, nodeoffset;
static union {
	uvlong align; // Host alignment for byte-shifted borrowed frames.
	uchar bytes[sizeof(F_Sys_pctl)+32]; // Complete frame and both guards.
} frameslot;
static uchar frameimage[sizeof(frameslot.bytes)];
static void *activeframe;


/* Change the original frame once at an actual native or scheduler boundary. */
static void
capture(int phase)
{
	if(!armed || mutated || capturephase != phase)
		return;
	memset(activeframe, 0xa7, sizeof(F_Sys_pctl));
	memset(frameimage+8+frameoffset, 0xa7, sizeof(F_Sys_pctl));
	mutated = 1;
}


/* Use the real nested runtime error-frame protocol without starting a scheduler. */
Proc*
getup(void)
{
	return &worker;
}


/* Retain the VM error identity across the pctl error frame and its outer protector. */
void
nexterror(void)
{
	pctltest(worker.nerr>0);
	siglongjmp(worker.estack[--worker.nerr],1);
}


/* Native group allocation errors are handled by the production pctl frame. */
void
error(char *message)
{
	raised=message;
	nexterror();
}


/* Keep address/stack instrumentation without unavailable leak-process inspection. */
int
__lsan_is_turned_off(void)
{
	return 1;
}


/* Select the program while the VM lock is still held. */
Prog*
currun(void)
{
	pctltest(!released);
	currentcalls++;
	capture(1);
	return &program;
}


/* Native descriptor-group operations must use exactly one release/acquire pair. */
void
release(void)
{
	pctltest(!released);
	released=1;enters++;
	capture(2);
}


/* Error paths must regain the interpreter lock before publishing status. */
void
acquire(void)
{
	pctltest(released && !locked);
	released=0;leaves++;
	capture(4);
}


/* Preserve the expected old-group locking discipline for selective copying. */
void
lock(Lock *lock)
{
	pctltest(released && lock==&oldgroup.l && !locked);
	locked=1;
}


/* The old group must be unlocked before replacing its owner. */
void
unlock(Lock *lock)
{
	pctltest(released && lock==&oldgroup.l && locked);
	locked=0;
}


/* Observe exactly one retained native channel reference for each copied descriptor. */
int
incref(Ref *ref)
{
	pctltest(released && locked);
	return ++ref->ref;
}


/* Record rejected high VM bits without invoking a native close on their truncated value. */
void
seterror(char *message,...)
{
	pctltest(released && message==Ebadarg);
	badargs++;
}


/* Create an empty group of the production-compatible descriptor size. */
Fgrp*
newfgrp(Fgrp *old)
{
	pctltest(!getenv("PCTL_PREFLIGHT"));
	pctltest(released && old==&oldgroup && !created);
	capture(3);
	if(failgroup)error(injected);
	created=1;newgroup.fd=newslots;newgroup.nfd=8;newgroup.maxfd=old->maxfd;newgroup.r.ref=1;
	return &newgroup;
}


/* Copy each old descriptor once, retaining the independent native channel ownership. */
Fgrp*
dupfgrp(Fgrp *old)
{
	int i;
	Fgrp *result;

	result=newfgrp(old);
	for(i=0;i<8;i++)if(oldslots[i]){
		newslots[i]=oldslots[i];
		newslots[i]->r.ref++;
	}
	return result;
}


/* Close within an explicit group while the new group is still unpublished. */
int
kfgrpclose(Fgrp *group,int fd)
{
	pctltest(released && group==&newgroup && environment.fgrp==&oldgroup);
	closes++;
	if(fd<0 || fd>=8 || !group->fd[fd])return -1;
	pctltest(group->fd[fd]->r.ref>0);
	group->fd[fd]->r.ref--;
	group->fd[fd]=nil;
	return 0;
}


/* The previous implementation's implicit-environment close must not select the old group. */
int
kclose(int fd)
{
	return kfgrpclose(environment.fgrp,fd);
}


/* Drop a group reference, releasing its channel references only at the last owner. */
void
closefgrp(Fgrp *group)
{
	int i;

	if(!cleanup){
		capture(5);
		pctltest(released && group==&oldgroup && environment.fgrp==&newgroup && !locked);
		pctltest(group->r.ref==2);
	}
	pctltest(group->r.ref>0);
	if(--group->r.ref)return;
	for(i=0;i<group->nfd;i++)if(group->fd[i]){
		pctltest(group->fd[i]->r.ref>0);
		group->fd[i]->r.ref--;
		group->fd[i]=nil;
	}
}


/* No namespace group is created by the descriptor-only scenarios. */
void
closepgrp(Pgrp *group)
{
	pctltest(released && group==nil);
	emptypgrp++;
}


/* Error cleanup sees only the initialized nil environment-group placeholder. */
void
closeegrp(Egrp *group)
{
	pctltest(released && group==nil);
	emptyegrp++;
}


/* Fail visibly if this descriptor fixture unexpectedly executes an unrelated pctl branch. */
static void*
unexpected(void)
{
	pctltest(0);
	return nil;
}

#define newpgrp(...) unexpected() // Namespace branches are outside this descriptor fixture.
#define cclone(...) unexpected() // Reject untested channel cloning.
#define cnameclose(...) unexpected() // Reject untested name release.
#define newcname(...) unexpected() // Reject untested name allocation.
#define pgrpcpy(...) unexpected() // Reject untested namespace copying.
#define newegrp(...) unexpected() // Reject untested environment allocation.
#define egrpcpy(...) unexpected() // Reject untested environment copying.
#define newgrp(...) unexpected() // Reject untested process grouping.
IMPLEMENTATION


/* Invoke the actual complete builtin under a real outer protection frame. */
static int
invoke(void *frame)
{
	Sys_pctl(frame);
	return 1;
}


/* Check unaligned VM words independently of descriptor-group mutation. */
static unsigned long
wordcases(void)
{
	WORD values[]={0,1,-1,0x7fffffff,(-0x7fffffff-1),(WORD)((uvlong)1<<32),
		(WORD)((uvlong)1<<40),(WORD)(-((vlong)1<<40)),(WORD)((uvlong)1<<63)};
	union {
		uvlong align; // Native alignment for shifted scalar words.
		uchar bytes[32]; // Word payload and complete guard regions.
	} memory;
	int i,offset,fd,valid;
	unsigned long checks=0;

	for(i=0;i<nelem(values);i++)for(offset=0;offset<8;offset++){
		memset(memory.bytes,0xa5,sizeof(memory.bytes));
		memmove(memory.bytes+8+offset,&values[i],sizeof(WORD));
		fd=12345;valid=(WORD)(int)values[i]==values[i];
		pctltest(sysfdword(memory.bytes+8+offset,&fd)==valid);
		pctltest(fd==(valid ? (int)values[i] : 12345));checks++;
	}
	return checks;
}


/* Validate early failures and ignored lists before any program or native-group action. */
static unsigned long
preflight(void)
{
	union {
		uvlong align; // Aligned backing for packed result words.
		uchar bytes[40]; // Result and complete guard regions.
	} output;
	union {
		uvlong align; // Aligned backing for borrowed list nodes.
		uchar bytes[sizeof(List)+16]; // Node span plus all byte shifts.
	} nodes[3];
	uchar nodeimage[sizeof(nodes)];
	F_Sys_pctl frame;
	Fgrp before;
	List *links[3], *tail;
	uchar expected[sizeof(output.bytes)];
	char *expectederror;
	WORD result, value = 0;
	int mode, groupmode, offset, i, ok, ignored;
	unsigned long checks = 0;

	for(groupmode = 0; groupmode < 2; groupmode++)
	for(mode = 0; mode < 16; mode++)
	for(frameoffset = 0; frameoffset < 8; frameoffset++)
	for(nodeoffset = 0; nodeoffset < 8; nodeoffset++)
	for(offset = 0; offset < 8; offset++){
		if(getenv("PCTL_PREFLIGHT_MODE") && mode != atoi(getenv("PCTL_PREFLIGHT_MODE")))
			continue;
		armed = 0;
		memset(&frame, 0, sizeof(frame));
		memset(&oldgroup, 0, sizeof(oldgroup));
		oldgroup.r.ref = 2;
		before = oldgroup;
		environment.fgrp = &oldgroup;
		program.pid = 42;
		frame.flags = groupmode ? Sys_FORKFD : Sys_NEWFD;
		frame.ret = (void*)(output.bytes+8+offset);
		for(i = 0; i < 3; i++){
			memset(nodes[i].bytes, 0xa5, sizeof(nodes[i].bytes));
			links[i] = (void*)(nodes[i].bytes+nodeoffset);
			tail = H;
			memmove((uchar*)links[i]+O(List, tail), &tail, sizeof(tail));
			memmove((uchar*)links[i]+O(List, data), &value, sizeof(value));
		}
		frame.movefd = H;
		expectederror = mode == 2 || mode == 5 || mode == 7 || mode == 8 || mode == 10 ? exBounds : exNilref;
		if(mode >= 11)
			expectederror = exHeap;
		if(mode == 3)
			frame.ret = nil;
		if(mode == 4)
			frame.ret = H;
		if(mode == 5)
			frame.ret = (void*)((uintptr)-1-sizeof(WORD)+1);
		if(mode == 6 || mode == 14)
			frame.movefd = nil;
		if(mode == 7)
			frame.movefd = (void*)1;
		if(mode == 8)
			frame.movefd = (void*)((uintptr)-1-O(List, data)-sizeof(WORD)+1);
		if(mode >= 9 && mode <= 13){
			frame.movefd = links[0];
			tail = mode == 9 ? nil : mode == 10 ?
				(void*)((uintptr)-1-O(List, data)-sizeof(WORD)+1) : links[0];
			if(mode >= 12)
				tail = links[1];
			memmove((uchar*)links[0]+O(List, tail), &tail, sizeof(tail));
			if(mode == 12){
				tail = links[0];
				memmove((uchar*)links[1]+O(List, tail), &tail, sizeof(tail));
			}
			if(mode == 13){
				tail = links[2];
				memmove((uchar*)links[1]+O(List, tail), &tail, sizeof(tail));
				tail = links[1];
				memmove((uchar*)links[2]+O(List, tail), &tail, sizeof(tail));
			}
		}
		if(mode == 15){
			frame.movefd = links[0];
			tail = links[0];
			memmove((uchar*)links[0]+O(List, tail), &tail, sizeof(tail));
		}
		memmove(nodeimage, nodes, sizeof(nodeimage));
		ignored = mode >= 14;
		if(ignored)
			frame.flags = 0;
		memset(output.bytes, 0xa5, sizeof(output.bytes));
		memmove(expected, output.bytes, sizeof(expected));
		memset(frameslot.bytes, 0xa5, sizeof(frameslot.bytes));
		activeframe = frameslot.bytes+8+frameoffset;
		memmove(activeframe, &frame, sizeof(frame));
		memmove(frameimage, frameslot.bytes, sizeof(frameimage));
		if(mode == 0)
			activeframe = nil;
		if(mode == 1)
			activeframe = H;
		if(mode == 2)
			activeframe = (void*)((uintptr)-1-sizeof(frame)+1);
		released=enters=leaves=locked=created=badargs=closes=cleanup=emptypgrp=emptyegrp=currentcalls=0;
		raised = nil;
		ok = kprotect(invoke, activeframe);
		pctltest(ok == ignored && raised == (ignored ? nil : expectederror));
		pctltest(!memcmp(nodeimage, nodes, sizeof(nodeimage)));
		if(ignored){
			result = program.pid;
			memmove(expected+8+offset, &result, sizeof(result));
		}
		pctltest(!memcmp(expected, output.bytes, sizeof(expected)));
		pctltest(!memcmp(frameimage, frameslot.bytes, sizeof(frameimage)));
		pctltest(!memcmp(&before, &oldgroup, sizeof(before)) && environment.fgrp == &oldgroup);
		pctltest(!released && !enters && !leaves && !locked && !created && !badargs && !closes);
		pctltest(!emptypgrp && !emptyegrp && !worker.nerr && currentcalls == ignored);
		checks++;
	}
	return checks;
}


/* Protect shared old groups while checking descriptor selection, refs, errors and every return slot. */
static int
pctlmatrix(void)
{
	WORD lists[][8]={{0},{0,2,7},{1,1,-1,8},
		{(WORD)(((uvlong)1<<32)|2),(WORD)(((uvlong)1<<40)|7),(WORD)(-((vlong)1<<40)+1)},
		{0x7fffffff,(-0x7fffffff-1),0},{-1,17,(WORD)((uvlong)1<<63)},{2}};
	int lengths[]={0,3,4,3,3,3,1};
	int flags[]={0,Sys_NEWFD,Sys_FORKFD};
	int pids[]={0,1,-1,0x7fffffff,(-0x7fffffff-1)};
	union {
		uvlong align; // Native backing alignment without requiring aligned views.
		uchar bytes[sizeof(List)+sizeof(WORD)+8]; // List header, scalar and all shifts.
	} nodes[8];
	uchar nodeimage[sizeof(nodes)];
	union {
		uvlong align; // Native alignment for shifted return words.
		uchar bytes[40]; // Complete result and both surrounding guards.
	} memory;
	uchar expected[40];
	Chan *wanted[8];
	F_Sys_pctl frame={0};
	List *node;
	WORD result;
	int mode,profile,offset,failure,pid,i,fd,valid,failed,wantbad,wantclose;
	unsigned long checks=0;

	worker.env=&environment;program.osenv=&environment;
	checks=wordcases();
	if(getenv("PCTL_PREFLIGHT")){
		checks += preflight();
		printf("PASS Sys pctl preflight: %lu cases WORD%zu\n", checks, sizeof(WORD)*8);
		return 0;
	}
	for(mode=0;mode<3;mode++)for(profile=0;profile<nelem(lists);profile++)
	for(frameoffset = 0; frameoffset < (getenv("PCTL_CAPTURE") ? 8 : 1); frameoffset++)
	for(nodeoffset = 0; nodeoffset < (getenv("PCTL_CAPTURE") ? 8 : 1); nodeoffset++)
	for(capturephase = 0; capturephase < (getenv("PCTL_CAPTURE") ? 6 : 1); capturephase++)
	for(offset=0;offset<8;offset++)for(failure=0;failure<2;failure++)for(pid=0;pid<nelem(pids);pid++){
		if(getenv("PCTL_FRAME") && frameoffset != atoi(getenv("PCTL_FRAME")))continue;
		if(getenv("PCTL_NODE") && nodeoffset != atoi(getenv("PCTL_NODE")))continue;
		if(getenv("PCTL_PHASE") && capturephase != atoi(getenv("PCTL_PHASE")))continue;
		if(getenv("PCTL_MODE") && mode!=atoi(getenv("PCTL_MODE")))continue;
		if(getenv("PCTL_PROFILE") && profile!=atoi(getenv("PCTL_PROFILE")))continue;
		if(getenv("PCTL_OFFSET") && offset!=atoi(getenv("PCTL_OFFSET")))continue;
		memset(&oldgroup,0,sizeof(oldgroup));memset(&newgroup,0,sizeof(newgroup));
		memset(channels,0,sizeof(channels));memset(newslots,0,sizeof(newslots));
		for(i=0;i<8;i++){
			oldslots[i]=i==3 ? nil : &channels[i];
			channels[i].r.ref=oldslots[i] ? 1 : 0;
			wanted[i]=mode==2 ? oldslots[i] : nil;
		}
		oldgroup.fd=oldslots;oldgroup.nfd=8;oldgroup.maxfd=7;oldgroup.r.ref=2;
		environment.fgrp=&oldgroup;program.pid=pids[pid];
		frame.flags=flags[mode];frame.movefd=H;
		memset(nodes, 0xa5, sizeof(nodes));
		for(i=lengths[profile]-1;i>=0;i--){
			node=(List*)(nodes[i].bytes+nodeoffset);
			memmove((uchar*)node+O(List, tail), &frame.movefd, sizeof(frame.movefd));
			memmove((uchar*)node+O(List, data), &lists[profile][i], sizeof(WORD));
			frame.movefd=node;
		}
		memmove(nodeimage, nodes, sizeof(nodeimage));
		wantbad=wantclose=0;
		for(i=0;i<lengths[profile];i++){
			valid=(WORD)(int)lists[profile][i]==lists[profile][i];fd=(int)lists[profile][i];
			if(mode==1 && valid && fd>=0 && fd<8)wanted[fd]=oldslots[fd];
			if(mode==2){
				if(valid){wantclose++;if(fd>=0 && fd<8)wanted[fd]=nil;}
				else wantbad++;
			}
		}
		memset(memory.bytes,0xa5,sizeof(memory.bytes));memmove(expected,memory.bytes,sizeof(expected));
		frame.ret=(void*)(memory.bytes+8+offset);
		released=enters=leaves=locked=created=badargs=closes=cleanup=emptypgrp=emptyegrp=0;
		failgroup=failure;raised=nil;
		memset(frameslot.bytes, 0xa5, sizeof(frameslot.bytes));
		activeframe = frameslot.bytes+8+frameoffset;
		memmove(activeframe, &frame, sizeof(frame));
		memmove(frameimage, frameslot.bytes, sizeof(frameimage));
		armed = 1;
		mutated = 0;
		pctltest(kprotect(invoke, activeframe));
		armed = 0;
		pctltest(!memcmp(nodeimage, nodes, sizeof(nodeimage)));
		pctltest(!memcmp(frameimage, frameslot.bytes, sizeof(frameimage)));
		failed=mode && failure;
		pctltest(!released && !locked && !worker.nerr);
		pctltest(enters==(mode!=0) && leaves==(mode!=0));
		pctltest((raised==injected)==failed);
		pctltest(emptypgrp==failed && emptyegrp==failed);
		result=failed ? -1 : pids[pid];
		memmove(expected+8+offset,&result,sizeof(result));
		pctltest(!memcmp(expected,memory.bytes,sizeof(expected)));
		pctltest(environment.fgrp==(mode && !failed ? &newgroup : &oldgroup));
		pctltest(oldgroup.r.ref==(mode && !failed ? 1 : 2));
		pctltest(badargs==(mode==2 && !failed ? wantbad : 0));
		pctltest(closes==(mode==2 && !failed ? wantclose : 0));
		for(i=0;i<8;i++){
			pctltest(oldslots[i]==(i==3 ? nil : &channels[i]));
			if(mode && !failed)pctltest(newslots[i]==wanted[i]);
			pctltest(channels[i].r.ref==(oldslots[i]!=nil)+(mode && !failed && wanted[i]!=nil));
		}
		cleanup=1;
		if(mode && !failed)closefgrp(&newgroup);
		while(oldgroup.r.ref)closefgrp(&oldgroup);
		for(i=0;i<8;i++)pctltest(channels[i].r.ref==0);
		checks++;
	}
	printf("PASS Sys pctl: %lu descriptor-group/width/alignment/error/ref cases WORD%zu\n",checks,sizeof(WORD)*8);
	return 0;
}


/* Execute real nested VM errors on a tracked pthread stack. */
static void*
pctlworker(void *unused)
{
	(void)unused;
	pctltest(pctlmatrix() == 0);
	return nil;
}


/* Join before returning so every native ownership predicate completes. */
int
main(void)
{
	pthread_t thread;

	pctltest(pthread_create(&thread, nil, pctlworker, nil) == 0);
	pctltest(pthread_join(thread, nil) == 0);
	return 0;
}
