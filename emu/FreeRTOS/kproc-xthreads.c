#include	"dat.h"
#include	"fns.h"
#include	"error.h"

#undef _POSIX_C_SOURCE 
#undef getwd

#include	<unistd.h>
#include	<signal.h>
#include 	<pthread.h>
#include	<limits.h>
#include	<errno.h>
//{}#include	<semaphore.h>


#include <FreeRTOS.h>

#include <semphr.h>

void* pvGetLuaState();
void  uxSetLuaState(void*);


typedef SemaphoreHandle_t sem_t;

typedef struct Osdep Osdep;
struct Osdep {
	sem_t	sem;
	pthread_t	self;
};

static pthread_key_t  prdakey;

extern int dflag;

Proc*
getup(void)
{
	return pvGetLuaState();
//	return pthread_getspecific(prdakey);
}

void
pexit(char *msg, int t)
{
	Osenv *e;
	Proc *p;
	Osdep *os;

	USED(t);

	lock(&procs.l);
	p = up;
	if(p->prev)
		p->prev->next = p->next;
	else
		procs.head = p->next;

	if(p->next)
		p->next->prev = p->prev;
	else
		procs.tail = p->prev;
	unlock(&procs.l);

	if(0)
		print("pexit: %s: %s\n", p->text, msg);

	e = p->env;
	if(e != nil) {
		closefgrp(e->fgrp);
		closepgrp(e->pgrp);
		closeegrp(e->egrp);
		closesigs(e->sigs);
		free(e->user);
	}
	free(p->prog);
	os = p->os;
	if(os != nil){
//		sem_destroy(&os->sem);
		vSemaphoreDelete(os->sem);
		os->sem = NULL;
		free(os);
	}
	free(p);
	pthread_exit(0);
}

static void*
tramp(void *arg)
{
	Proc *p;
	Osdep *os;

	p = arg;
	os = p->os;
	os->self = pthread_self();

	uxSetLuaState(arg);
//	if(pthread_setspecific(prdakey, arg))
//		panic("set specific data failed in tramp\n");
#if 0 //{}
	if(0){
		pthread_attr_t attr;
		memset(&attr, 0, sizeof(attr));
		pthread_getattr_np(pthread_self(), &attr);
		size_t s;
		pthread_attr_getstacksize(&attr, &s);
		print("stack size = %d\n", s);
	}
#endif //{}
	p->func(p->arg);
	pexit("{Tramp}", 0);
	return nil;
}

void
kproc(char *name, void (*func)(void*), void *arg, int flags)
{
	pthread_t thread;
	Proc *p;
	Pgrp *pg;
	Fgrp *fg;
	Egrp *eg;
	pthread_attr_t attr;
	Osdep *os;

	p = newproc();
	if(p == nil)
		panic("kproc: no memory");

	os = malloc(sizeof(*os));
	if(os == nil)
		panic("kproc: no memory");
	os->self = 0;	/* set by tramp */
//	sem_init(&os->sem, 0, 0);
	os->sem = xSemaphoreCreateMutex();
//	os->sem = xSemaphoreCreateCounting(1024, 0);
	p->os = os;

	if(flags & KPDUPPG) {
		pg = up->env->pgrp;
		incref(&pg->r);
		p->env->pgrp = pg;
	}
	if(flags & KPDUPFDG) {
		fg = up->env->fgrp;
		incref(&fg->r);
		p->env->fgrp = fg;
	}
	if(flags & KPDUPENVG) {
		eg = up->env->egrp;
		incref(&eg->r);
		p->env->egrp = eg;
	}

	p->env->uid = up->env->uid;
	p->env->gid = up->env->gid;
	kstrdup(&p->env->user, up->env->user);

	strcpy(p->text, name);

	p->func = func;
	p->arg = arg;

	lock(&procs.l);
	if(procs.tail != nil) {
		p->prev = procs.tail;
		procs.tail->next = p;
	} else {
		procs.head = p;
		p->prev = nil;
	}
	procs.tail = p;
	unlock(&procs.l);

	memset(&attr, 0, sizeof(attr));
	if(pthread_attr_init(&attr) == -1)
		panic("pthread_attr_init failed");
	if(flags & KPX11)
		pthread_attr_setstacksize(&attr, 512*1024);	/* could be a parameter */
	else if(KSTACK > 0)
		pthread_attr_setstacksize(&attr, (KSTACK < /*PTHREAD_STACK_MIN*/configMINIMAL_STACK_SIZE ? /*PTHREAD_STACK_MIN*/configMINIMAL_STACK_SIZE : KSTACK)+1024);
#ifndef ANDROID
//	pthread_attr_setinheritsched(&attr, PTHREAD_INHERIT_SCHED);
#endif
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	if(pthread_create(&thread, &attr, tramp, p))
		panic("thr_create failed\n");
	pthread_attr_destroy(&attr);
}

/* called to wake up kproc blocked on a syscall */
void
oshostintr(Proc *p)
{
	Osdep *os;

	os = p->os;
	if(os != nil && os->self != 0)
		pthread_kill(os->self, SIGUSR1);
}

void
osblock(void)
{
	Osdep *os;

	os = up->os;
//	while(sem_wait(&os->sem))
	while(xSemaphoreTake(os->sem, ( TickType_t )512) != pdTRUE)
		{}	/* retry on signals (which shouldn't happen) */
}

void
osready(Proc *p)
{
	Osdep *os;

	os = p->os;
//	sem_post(&os->sem);
	xSemaphoreGive(os->sem);
}

void
kprocinit(Proc *p)
{
	uxSetLuaState(p);
#if 0 //{}
	if(pthread_key_create(&prdakey, NULL))
		panic("key_create failed");
	if(pthread_setspecific(prdakey, p))
		panic("set specific thread data failed");
#endif
}

#ifdef ANDROID
#include <sched.h>
#endif
void
osyield(void)
{
//	pthread_yield_np();
	/* define pthread_yield to be sched_yield or pthread_yield_np if required */
#ifndef ANDROID
	pthread_yield();
#else
	sched_yield();
#endif
}

void
ospause(void)
{
	/* main just wants this thread to go away */
	pthread_exit(0);
}

void
oslopri(void)
{
#if 0 //{}
	struct sched_param param;
	int policy;
	pthread_t self;

	self = pthread_self();
	pthread_getschedparam(self, &policy, &param);
	param.sched_priority = sched_get_priority_min(policy);
	pthread_setschedparam(self,  policy, &param);
#endif //{}
}
