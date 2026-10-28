#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "runt.h"
#include "kernel.h"
#include "raise.h"
#include <stdio.h>


/* Keep every inherited status/call/ownership predicate active under NDEBUG. */
static void
statuspredicate(int test)
{
	if(!test) {
		fprintf(stderr, "Sys status predicate failed\n");
		abort();
	}
}
#define statuscheck(test) statuspredicate(!!(test)) // Normalize pointer and scalar predicates.

char exNilref[] = "nil", exBounds[] = "bounds";
char Ebadarg[]="bad argument";
static int released,calls,entered,bad,operation,wantold,wantnew,wantflags;
static int nativeint,printmode;
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
	statuscheck(!released);
	released=1;
	entered++;
}

/* A successful native entry must regain the interpreter lock exactly once. */
void
acquire(void)
{
	statuscheck(released && calls<=1);
	released=0;
}

/* Record a normal Sys argument error without raising a VM exception. */
void
seterror(char *message,...)
{
	statuscheck(message==Ebadarg);
	bad++;
}

/* Supply stable strings without allocating or consulting a real namespace. */
char*
string2c(String *s)
{
	statuscheck(released || operation==14);
	if(s==&strings[0])return "first";
	if(s==&strings[1])return "second";
	statuscheck(s==&strings[2]);
	return "spec";
}

/* Verify that the wrapper invokes only its intended native operation. */
static void
nativecall(int op)
{
	statuscheck(released && operation==op && !calls);
	calls++;
}

/* Observe complete descriptor indices reaching native dup. */
int
kdup(int old,int new)
{
	nativecall(0);
	statuscheck(old==wantold && new==wantnew);
	return nativeint;
}

/* Observe the captured remove path. */
int
kremove(char *s)
{
	nativecall(1);
	statuscheck(!strcmp(s,"first"));
	return nativeint;
}

/* Preserve a full 64-bit offset and return through the native seek boundary. */
vlong
kseek(int fd,vlong off,int start)
{
	nativecall(2);
	statuscheck(fd==17 && off==wantoff && start==wantold);
	return nativebig;
}

/* Observe both unmount path arguments before result publication. */
int
kunmount(char *a,char *b)
{
	nativecall(3);
	statuscheck(!strcmp(a,"first") && !strcmp(b,"second"));
	return nativeint;
}

/* Observe the captured working-directory argument. */
int
kchdir(char *s)
{
	nativecall(4);
	statuscheck(!strcmp(s,"first"));
	return nativeint;
}

/* Check the native mount descriptors, flags and strings without mounting anything. */
int
kmount(int fd,int afd,char *on,int flags,char *spec)
{
	nativecall(5);
	statuscheck(fd==17 && afd==-1 && flags==wantflags);
	statuscheck(!strcmp(on,"second") && !strcmp(spec,"spec"));
	return nativeint;
}

/* Check native bind arguments without changing a namespace. */
int
kbind(char *s,char *on,int flags)
{
	nativecall(6);
	statuscheck(flags==wantflags && !strcmp(s,"first") && !strcmp(on,"second"));
	return nativeint;
}

/* Keep descriptor-query result width independent of the VM WORD model. */
int
kiounit(int fd)
{
	nativecall(7);
	statuscheck(fd==17);
	return nativeint;
}

static Dir *packed;
static Proc process;
static int osentered;
static WORD wantperiod;

/* Model packing before releasing the interpreter lock, retaining native ownership. */
static Dir*
packdir(Sys_Dir *d)
{
	USED(d);
	statuscheck(!released);
	packed=malloc(sizeof(*packed));
	statuscheck(packed);
	return packed;
}

/* Verify that a packed directory remains live throughout the native update. */
int
kdirwstat(char *path,Dir *d)
{
	nativecall(8);
	statuscheck(d==packed && !strcmp(path,"first"));
	return nativeint;
}

/* Preserve native descriptor selection for directory updates. */
int
kdirfwstat(int fd,Dir *d)
{
	nativecall(9);
	statuscheck(fd==17 && d==packed);
	return nativeint;
}

/* Millisecond queries run while retaining the interpreter lock. */
long
osmillisec(void)
{
	statuscheck(!released && operation==10 && !calls);
	calls++;
	return nativeint;
}

/* Use a controlled clock expression for the kernel-side wrapper as well. */
#define MACHP(n) (&process)
#define TK2MS(ticks) osmillisec()
#undef up
#define up (&process)
#undef waserror
#define waserror() 0
#undef poperror
#define poperror() ((void)0)

/* Enter the host sleep adapter exactly once on the positive-period path. */
void
osenter(void)
{
	statuscheck(released && !osentered);
	osentered=1;
}

/* The host sleep adapter must leave before reacquiring the VM lock. */
void
osleave(void)
{
	statuscheck(released && osentered);
	osentered=0;
}

/* Observe the emulator sleep result without delaying the test. */
int
limbosleep(ulong period)
{
	nativecall(11);
	statuscheck(osentered && period==(ulong)wantperiod);
	return nativeint;
}

/* The kernel wait predicate is passed through unchanged. */
int
return0(void *arg)
{
	USED(arg);
	return 0;
}

/* Observe a kernel sleep without touching a real scheduler. */
void
tsleep(Rendez *r,int (*predicate)(void*),void *arg,int period)
{
	nativecall(11);
	statuscheck(r==&process.sleep && predicate==return0 && arg==0 && period==wantperiod);
}

/* Unexpected error-frame paths are failures rather than silent fixture exits. */
void
error(char *message)
{
	USED(message);
	statuscheck(0);
}

static Prog program;
static Chan stdoutchan;
static Chan *descriptors[2];
static Fgrp group;

/* Formatting retains the running program selected while the VM lock was held. */
Prog*
currun(void)
{
	statuscheck(!released);
	return &program;
}

/* Supply a bounded short result or request the real wrapper's allocated path. */
int
xprint(Prog *p,void *frame,void *args,String *s,char *buf,int size)
{
	USED(frame);USED(args);
	statuscheck(released && p==&program && s==&strings[0] && size==1024);
	memset(buf,'x',3);
	return printmode==1 ? size : 3;
}

/* Allocate a formatted buffer that the production wrapper must release. */
int
bigxprint(Prog *p,void *frame,void *args,String *s,char **buf,int size)
{
	USED(frame);USED(args);
	statuscheck(released && p==&program && s==&strings[0] && size==1024 && printmode==1);
	*buf=malloc(2048);
	statuscheck(*buf);
	memset(*buf,'x',2048);
	return 2048;
}

/* Record both formatted write sizes and native failure/status values. */
long
kwrite(int fd,void *buf,long count)
{
	nativecall(operation);
	statuscheck(operation==12 || operation==13);
	statuscheck(fd==(operation==12 ? 1 : printmode==2 ? -1 : 17));
	statuscheck(count==(printmode==1 ? 2048 : 3));
	statuscheck(((char*)buf)[0]=='x' && ((char*)buf)[count-1]=='x');
	return nativeint;
}

/* Observe error-string copying without changing the fixture's native statuses. */
void
kstrcpy(char *dst,char *src,int size)
{
	statuscheck(!released && operation==14 && dst==process.env->errstr && size==ERRMAX);
	statuscheck(!strcmp(src,"first"));
	calls++;
	strcpy(dst,src);
}

/* Preserve the export flag mask and scalar native result without exporting a namespace. */
int
export(int fd,char *directory,int flags)
{
	nativecall(15);
	statuscheck(fd==17 && !strcmp(directory,"first") && flags==(3&Sys_EXPASYNC));
	return nativeint;
}

IMPLEMENTATION
/* Check every return alignment, complete status width and native-int narrowing boundary. */
int
main(void)
{
	WORD arguments[]={0,1,-1,17,0x7fffffff,(-0x7fffffff-1),(WORD)((uvlong)1<<32),(WORD)((uvlong)1<<40),(WORD)(-((vlong)1<<40))};
	int results[]={0,1,-1,0x7fffffff,(-0x7fffffff-1)};
	LONG bigresults[]={0,1,-1,9223372036854775807LL,(-9223372036854775807LL-1)};
	F_Sys_export exportframe={0};
	F_Sys_dup dup={0};
	F_Sys_remove remove={0};
	F_Sys_seek seek={0};
	F_Sys_unmount unmount={0};
	F_Sys_print print={0};
	F_Sys_fprint fprint={0};
	F_Sys_werrstr werrstr={0};
	F_Sys_wstat wstat={0};
	F_Sys_fwstat fwstat={0};
	F_Sys_millisec millisec={0};
	F_Sys_sleep sleep={0};
	F_Sys_chdir chdir={0};
	F_Sys_mount mount={0};
	F_Sys_bind bind={0};
	F_Sys_iounit iounit={0};
	Sys_FD fd={17};
	union {
		uvlong align;
		uchar bytes[40];
	} memory;
	uchar expected[40];
	char errorbuffer[ERRMAX];
	WORD wordresult,first,second;
	LONG bigresult;
	int i,j,r,offset,valid,width;
	unsigned long checks=0;

	process.env=&process.defenv;
	process.env->errstr=errorbuffer;
	process.env->fgrp=&group;
	group.fd=descriptors;
	group.nfd=2;

	for(operation=0;operation<16;operation++)
	for(i=0;i<(operation==0 || operation==2 || operation==5 || operation==6 || operation==11 ? nelem(arguments) : 1);i++)
	for(j=0;j<(operation==0 ? nelem(arguments) : operation==12 || operation==13 ? 3 : 1);j++)
	for(r=0;r<nelem(results);r++)for(offset=0;offset<8;offset++){
		if(getenv("SYS_STATUS_OPERATION") && atoi(getenv("SYS_STATUS_OPERATION"))!=operation)
			continue;
		if(getenv("SYS_STATUS_ZERO_SLEEP") && i!=0)
			continue;
		if(getenv("SYS_STATUS_PRINT_MODE") && j!=atoi(getenv("SYS_STATUS_PRINT_MODE")))
			continue;
		first=arguments[i];
		second=arguments[j];
		valid=(operation!=0 && operation!=2 && operation!=5 && operation!=6 && operation!=11) || ((WORD)(int)first==first && (operation!=0 || (WORD)(int)second==second));
		wantflags=(int)first;
		wantperiod=first;
		wantold=(int)first;
		wantnew=(int)second;
		wantoff=bigresults[i%nelem(bigresults)];
		nativeint=results[r];
		nativebig=bigresults[r];
		released=calls=entered=bad=0;
		printmode=j;
		descriptors[1]=printmode==2 ? nil : &stdoutchan;
		memset(memory.bytes,0xa5,sizeof(memory.bytes));
		memmove(expected,memory.bytes,sizeof(expected));
		dup.ret=remove.ret=unmount.ret=chdir.ret=mount.ret=bind.ret=iounit.ret=(void*)(memory.bytes+8+offset);
		seek.ret=(void*)(memory.bytes+8+offset);
		exportframe.ret=print.ret=fprint.ret=werrstr.ret=wstat.ret=fwstat.ret=millisec.ret=sleep.ret=(void*)(memory.bytes+8+offset);
		exportframe.dir=print.s=fprint.s=werrstr.s=&strings[0];
		fprint.fd=printmode==2 ? H : &fd;
		exportframe.c=&fd;exportframe.flag=3;
		wstat.s=&strings[0];
		fwstat.fd=&fd;
		sleep.period=first;
		dup.old=first;
		dup.new=second;
		remove.s=&strings[0];
		seek.fd=&fd;
		seek.off=wantoff;
		seek.start=first;
		unmount.s1=&strings[0];
		unmount.s2=&strings[1];
		chdir.path=&strings[0];
		mount.fd=&fd;
		mount.afd=H;
		mount.on=&strings[1];
		mount.spec=&strings[2];
		mount.flags=first;
		bind.s=&strings[0];
		bind.on=&strings[1];
		bind.flags=first;
		iounit.fd=&fd;
		switch(operation){
		case 0:Sys_dup(&dup);break;
		case 1:Sys_remove(&remove);break;
		case 2:Sys_seek(&seek);break;
		case 3:Sys_unmount(&unmount);break;
		case 4:Sys_chdir(&chdir);break;
		case 5:Sys_mount(&mount);break;
		case 6:Sys_bind(&bind);break;
		case 7:Sys_iounit(&iounit);break;
		case 8:Sys_wstat(&wstat);break;
		case 9:Sys_fwstat(&fwstat);break;
		case 10:Sys_millisec(&millisec);break;
		case 11:Sys_sleep(&sleep);break;
		case 12:Sys_print(&print);break;
		case 13:Sys_fprint(&fprint);break;
		case 14:Sys_werrstr(&werrstr);break;
		default:Sys_export(&exportframe);break;
		}
		wordresult=valid ? (WORD)nativeint : -1;
		bigresult=valid ? (LONG)nativebig : -1;
		if(valid && operation==11 && (TEST_OS || first<=0))wordresult=0;
		if(operation==12 && printmode==2)wordresult=-1;
		if(operation==14)wordresult=0;
		width=operation==2 ? sizeof(LONG) : sizeof(WORD);
		memmove(expected+8+offset,operation==2 ? (void*)&bigresult : (void*)&wordresult,width);
		statuscheck(!memcmp(memory.bytes,expected,sizeof(expected)) && !released);
		statuscheck(calls==(valid && (operation!=11 || first>0) && (operation!=12 || printmode!=2)));
		statuscheck(entered==(valid && operation!=10 && operation!=14 && (operation!=12 || printmode!=2)) && bad==!valid && !osentered);
		checks++;
	}
	printf("PASS Sys status slots: %lu alignment/native-width/error/scheduler cases WORD%zu\n",checks,sizeof(WORD)*8);
	return 0;
}
