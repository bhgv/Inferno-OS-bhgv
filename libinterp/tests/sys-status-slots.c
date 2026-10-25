#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "runt.h"
#include "kernel.h"
#include <assert.h>
#include <stdio.h>

char Ebadarg[]="bad argument";
static int released,calls,entered,bad,operation,wantold,wantnew;
static int nativeint;
static vlong nativebig,wantoff;
static String strings[3];
#define fdchk(x) ((x)==(Sys_FD*)H ? -1 : (x)->fd)
/* Keep address/stack instrumentation without unavailable leak-process inspection. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

/* Each native operation must run outside the interpreter lock. */
void
release(void)
{
	assert(!released);
	released=1;
	entered++;
}

/* A successful native entry must regain the interpreter lock exactly once. */
void
acquire(void)
{
	assert(released && calls==1);
	released=0;
}

/* Record a normal Sys argument error without raising a VM exception. */
void
seterror(char *message,...)
{
	assert(message==Ebadarg);
	bad++;
}

/* Supply stable strings without allocating or consulting a real namespace. */
char*
string2c(String *s)
{
	assert(released);
	if(s==&strings[0])return "first";
	if(s==&strings[1])return "second";
	assert(s==&strings[2]);
	return "spec";
}

/* Verify that the wrapper invokes only its intended native operation. */
static void
nativecall(int op)
{
	assert(released && operation==op && !calls);
	calls++;
}

/* Observe complete descriptor indices reaching native dup. */
int
kdup(int old,int new)
{
	nativecall(0);
	assert(old==wantold && new==wantnew);
	return nativeint;
}

/* Observe the captured remove path. */
int
kremove(char *s)
{
	nativecall(1);
	assert(!strcmp(s,"first"));
	return nativeint;
}

/* Preserve a full 64-bit offset and return through the native seek boundary. */
vlong
kseek(int fd,vlong off,int start)
{
	nativecall(2);
	assert(fd==17 && off==wantoff && start==wantold);
	return nativebig;
}

/* Observe both unmount path arguments before result publication. */
int
kunmount(char *a,char *b)
{
	nativecall(3);
	assert(!strcmp(a,"first") && !strcmp(b,"second"));
	return nativeint;
}

/* Observe the captured working-directory argument. */
int
kchdir(char *s)
{
	nativecall(4);
	assert(!strcmp(s,"first"));
	return nativeint;
}

/* Check the native mount descriptors, flags and strings without mounting anything. */
int
kmount(int fd,int afd,char *on,int flags,char *spec)
{
	nativecall(5);
	assert(fd==17 && afd==-1 && flags==3);
	assert(!strcmp(on,"second") && !strcmp(spec,"spec"));
	return nativeint;
}

/* Check native bind arguments without changing a namespace. */
int
kbind(char *s,char *on,int flags)
{
	nativecall(6);
	assert(flags==3 && !strcmp(s,"first") && !strcmp(on,"second"));
	return nativeint;
}

/* Keep descriptor-query result width independent of the VM WORD model. */
int
kiounit(int fd)
{
	nativecall(7);
	assert(fd==17);
	return nativeint;
}

IMPLEMENTATION
/* Check every return alignment, complete status width and native-int narrowing boundary. */
int main(void)
{
	WORD arguments[]={0,1,-1,17,0x7fffffff,(-0x7fffffff-1),(WORD)((uvlong)1<<32),(WORD)((uvlong)1<<40),(WORD)(-((vlong)1<<40))};
	int results[]={0,1,-1,0x7fffffff,(-0x7fffffff-1)};
	LONG bigresults[]={0,1,-1,9223372036854775807LL,(-9223372036854775807LL-1)};
	F_Sys_dup dup={0};F_Sys_remove remove={0};F_Sys_seek seek={0};F_Sys_unmount unmount={0};
	F_Sys_chdir chdir={0};F_Sys_mount mount={0};F_Sys_bind bind={0};F_Sys_iounit iounit={0};
	Sys_FD fd={17};
	union { uvlong align; uchar bytes[40]; } memory;
	uchar expected[40];
	WORD wordresult,first,second;
	LONG bigresult;
	int i,j,r,offset,valid,width;
	unsigned long checks=0;

	for(operation=0;operation<8;operation++)
	for(i=0;i<(operation==0 || operation==2 ? nelem(arguments) : 1);i++)
	for(j=0;j<(operation==0 ? nelem(arguments) : 1);j++)
	for(r=0;r<nelem(results);r++)for(offset=0;offset<8;offset++){
		first=arguments[i];second=arguments[j];
		valid=(operation!=0 && operation!=2) || ((WORD)(int)first==first && (operation!=0 || (WORD)(int)second==second));
		wantold=(int)first;wantnew=(int)second;wantoff=bigresults[i%nelem(bigresults)];
		nativeint=results[r];nativebig=bigresults[r];
		released=calls=entered=bad=0;
		memset(memory.bytes,0xa5,sizeof(memory.bytes));memmove(expected,memory.bytes,sizeof(expected));
		dup.ret=remove.ret=unmount.ret=chdir.ret=mount.ret=bind.ret=iounit.ret=(void*)(memory.bytes+8+offset);
		seek.ret=(void*)(memory.bytes+8+offset);
		dup.old=first;dup.new=second;remove.s=&strings[0];seek.fd=&fd;seek.off=wantoff;seek.start=first;
		unmount.s1=&strings[0];unmount.s2=&strings[1];chdir.path=&strings[0];
		mount.fd=&fd;mount.afd=H;mount.on=&strings[1];mount.spec=&strings[2];mount.flags=3;
		bind.s=&strings[0];bind.on=&strings[1];bind.flags=3;iounit.fd=&fd;
		switch(operation){case 0:Sys_dup(&dup);break;case 1:Sys_remove(&remove);break;case 2:Sys_seek(&seek);break;
		case 3:Sys_unmount(&unmount);break;case 4:Sys_chdir(&chdir);break;case 5:Sys_mount(&mount);break;
		case 6:Sys_bind(&bind);break;default:Sys_iounit(&iounit);break;}
		wordresult=valid ? (WORD)nativeint : -1;bigresult=valid ? (LONG)nativebig : -1;
		width=operation==2 ? sizeof(LONG) : sizeof(WORD);
		memmove(expected+8+offset,operation==2 ? (void*)&bigresult : (void*)&wordresult,width);
		assert(!memcmp(memory.bytes,expected,sizeof(expected)) && !released);
		assert(calls==valid && entered==valid && bad==!valid);checks++;
	}
	printf("PASS Sys status slots: %lu alignment/native-width/error/scheduler cases WORD%zu\n",checks,sizeof(WORD)*8);
	return 0;
}
