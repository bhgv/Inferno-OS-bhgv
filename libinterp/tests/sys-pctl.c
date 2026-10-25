#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "runt.h"
#include "kernel.h"
#include <assert.h>
#include <stdio.h>

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
static int emptypgrp,emptyegrp;

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
	assert(worker.nerr>0);
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
	assert(!released);
	return &program;
}

/* Native descriptor-group operations must use exactly one release/acquire pair. */
void
release(void)
{
	assert(!released);
	released=1;enters++;
}

/* Error paths must regain the interpreter lock before publishing status. */
void
acquire(void)
{
	assert(released && !locked);
	released=0;leaves++;
}

/* Preserve the expected old-group locking discipline for selective copying. */
void
lock(Lock *lock)
{
	assert(released && lock==&oldgroup.l && !locked);
	locked=1;
}

/* The old group must be unlocked before replacing its owner. */
void
unlock(Lock *lock)
{
	assert(released && lock==&oldgroup.l && locked);
	locked=0;
}

/* Observe exactly one retained native channel reference for each copied descriptor. */
int
incref(Ref *ref)
{
	assert(released && locked);
	return ++ref->ref;
}

/* Record rejected high VM bits without invoking a native close on their truncated value. */
void
seterror(char *message,...)
{
	assert(released && message==Ebadarg);
	badargs++;
}

/* Create an empty group of the production-compatible descriptor size. */
Fgrp*
newfgrp(Fgrp *old)
{
	assert(released && old==&oldgroup && !created);
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
	assert(released && group==&newgroup && environment.fgrp==&oldgroup);
	closes++;
	if(fd<0 || fd>=8 || !group->fd[fd])return -1;
	assert(group->fd[fd]->r.ref>0);
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
		assert(released && group==&oldgroup && environment.fgrp==&newgroup && !locked);
		assert(group->r.ref==2);
	}
	assert(group->r.ref>0);
	if(--group->r.ref)return;
	for(i=0;i<group->nfd;i++)if(group->fd[i]){
		assert(group->fd[i]->r.ref>0);
		group->fd[i]->r.ref--;
		group->fd[i]=nil;
	}
}

/* No namespace group is created by the descriptor-only scenarios. */
void
closepgrp(Pgrp *group)
{
	assert(released && group==nil);
	emptypgrp++;
}

/* Error cleanup sees only the initialized nil environment-group placeholder. */
void
closeegrp(Egrp *group)
{
	assert(released && group==nil);
	emptyegrp++;
}

/* Fail visibly if this descriptor fixture unexpectedly executes an unrelated pctl branch. */
static void*
unexpected(void)
{
	assert(0);
	return nil;
}

#define newpgrp(...) unexpected()
#define cclone(...) unexpected()
#define cnameclose(...) unexpected()
#define newcname(...) unexpected()
#define pgrpcpy(...) unexpected()
#define newegrp(...) unexpected()
#define egrpcpy(...) unexpected()
#define newgrp(...) unexpected()
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
	union { uvlong align; uchar bytes[32]; } memory;
	int i,offset,fd,valid;
	unsigned long checks=0;

	for(i=0;i<nelem(values);i++)for(offset=0;offset<8;offset++){
		memset(memory.bytes,0xa5,sizeof(memory.bytes));
		memmove(memory.bytes+8+offset,&values[i],sizeof(WORD));
		fd=12345;valid=(WORD)(int)values[i]==values[i];
		assert(sysfdword(memory.bytes+8+offset,&fd)==valid);
		assert(fd==(valid ? (int)values[i] : 12345));checks++;
	}
	return checks;
}

/* Protect shared old groups while checking descriptor selection, refs, errors and every return slot. */
int
main(void)
{
	WORD lists[][8]={{0},{0,2,7},{1,1,-1,8},
		{(WORD)(((uvlong)1<<32)|2),(WORD)(((uvlong)1<<40)|7),(WORD)(-((vlong)1<<40)+1)},
		{0x7fffffff,(-0x7fffffff-1),0},{-1,17,(WORD)((uvlong)1<<63)},{2}};
	int lengths[]={0,3,4,3,3,3,1};
	int flags[]={0,Sys_NEWFD,Sys_FORKFD};
	int pids[]={0,1,-1,0x7fffffff,(-0x7fffffff-1)};
	union Node { uvlong align; uchar bytes[sizeof(List)+sizeof(WORD)]; } nodes[8];
	union { uvlong align; uchar bytes[40]; } memory;
	uchar expected[40];
	Chan *wanted[8];
	F_Sys_pctl frame={0};
	List *node;
	WORD result;
	int mode,profile,offset,failure,pid,i,fd,valid,failed,wantbad,wantclose;
	unsigned long checks=0;

	worker.env=&environment;program.osenv=&environment;
	checks=wordcases();
	for(mode=0;mode<3;mode++)for(profile=0;profile<nelem(lists);profile++)
	for(offset=0;offset<8;offset++)for(failure=0;failure<2;failure++)for(pid=0;pid<nelem(pids);pid++){
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
		for(i=lengths[profile]-1;i>=0;i--){
			node=(List*)nodes[i].bytes;node->tail=frame.movefd;
			memmove(node->data,&lists[profile][i],sizeof(WORD));frame.movefd=node;
		}
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
		assert(kprotect(invoke,&frame));
		failed=mode && failure;
		assert(!released && !locked && !worker.nerr);
		assert(enters==(mode!=0) && leaves==(mode!=0));
		assert((raised==injected)==failed);
		assert(emptypgrp==failed && emptyegrp==failed);
		result=failed ? -1 : pids[pid];
		memmove(expected+8+offset,&result,sizeof(result));
		assert(!memcmp(expected,memory.bytes,sizeof(expected)));
		assert(environment.fgrp==(mode && !failed ? &newgroup : &oldgroup));
		assert(oldgroup.r.ref==(mode && !failed ? 1 : 2));
		assert(badargs==(mode==2 && !failed ? wantbad : 0));
		assert(closes==(mode==2 && !failed ? wantclose : 0));
		for(i=0;i<8;i++){
			assert(oldslots[i]==(i==3 ? nil : &channels[i]));
			if(mode && !failed)assert(newslots[i]==wanted[i]);
			assert(channels[i].r.ref==(oldslots[i]!=nil)+(mode && !failed && wanted[i]!=nil));
		}
		cleanup=1;
		if(mode && !failed)closefgrp(&newgroup);
		while(oldgroup.r.ref)closefgrp(&oldgroup);
		for(i=0;i<8;i++)assert(channels[i].r.ref==0);
		checks++;
	}
	printf("PASS Sys pctl: %lu descriptor-group/width/alignment/error/ref cases WORD%zu\n",checks,sizeof(WORD)*8);
	return 0;
}
