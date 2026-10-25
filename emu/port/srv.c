#include	"dat.h"
#include	"fns.h"
#include	"error.h"
#include	"raise.h"
#include	<interp.h>
#include	<isa.h>
#include	"ip.h"
#include	"srv.h"
#include	"srvm.h"

static	QLock	dbq;
enum { SrvMaxHosts = 10 };

void
Srv_init(void *fp)
{
	USED(fp);
}

/* Publish nil before releasing a previous result in a possibly unaligned slot. */
static void
srvclear(void *slot)
{
	void *old, *empty;

	memmove(&old, slot, sizeof(old));
	empty = H;
	memmove(slot, &empty, sizeof(empty));
	destroy(old);
}

/* Release every native answer, including partially populated resolver vectors. */
static void
srvfreehosts(char **hosts, int count)
{
	int i;

	for(i = 0; i < count; i++)
		free(hosts[i]);
	free(hosts);
}

/* Root nodes before string allocation and roll back both VM and native owners on error. */
static void
srvhostlist(char **hosts, int count, int capacity, void *slot)
{
	List *list;
	String *text;
	ulong length;
	int i;

	if(waserror()){
		srvclear(slot);
		srvfreehosts(hosts, capacity);
		nexterror();
	}
	if(count < 0 || count > capacity)
		error(exHeap);
	/* Prepending in reverse preserves the resolver's original answer order. */
	for(i = count; i > 0; i--){
		if(hosts[i-1] == nil)
			error(exHeap);
		length = strlen(hosts[i-1]);
		if(length > 0x7fffffff)
			error(exHeap);
		list = cons(sizeof(text), slot);
		text = newstring(length);
		memmove(text->Sascii, hosts[i-1], length);
		memmove(list->data, &text, sizeof(text));
		list->t = &Tptr;
		Tptr.ref++;
	}
	poperror();
	srvfreehosts(hosts, capacity);
}

/* Resolve native answers outside the VM lock, then publish an owned list transactionally. */
void
Srv_iph2a(void *fp)
{
	F_Srv_iph2a *f;
	char **hosts;
	int count;

	f = fp;
	srvclear(f->ret);
	/* The vector survives longjmp without indeterminate automatic array contents. */
	hosts = malloc(SrvMaxHosts*sizeof(*hosts));
	if(hosts == nil)
		error(exHeap);
	memset(hosts, 0, SrvMaxHosts*sizeof(*hosts));
	release();
	qlock(&dbq);
	if(waserror()){
		qunlock(&dbq);
		acquire();
		srvfreehosts(hosts, SrvMaxHosts);
		return;
	}
	count = so_gethostbyname(string2c(f->host), hosts, SrvMaxHosts);
	poperror();
	qunlock(&dbq);
	acquire();
	srvhostlist(hosts, count, SrvMaxHosts, f->ret);
}

/* Resolve native answers outside the VM lock, then publish an owned list transactionally. */
void
Srv_ipa2h(void *fp)
{
	F_Srv_ipa2h *f;
	char **hosts;
	int count;

	f = fp;
	srvclear(f->ret);
	/* The vector survives longjmp without indeterminate automatic array contents. */
	hosts = malloc(SrvMaxHosts*sizeof(*hosts));
	if(hosts == nil)
		error(exHeap);
	memset(hosts, 0, SrvMaxHosts*sizeof(*hosts));
	release();
	qlock(&dbq);
	if(waserror()){
		qunlock(&dbq);
		acquire();
		srvfreehosts(hosts, SrvMaxHosts);
		return;
	}
	count = so_gethostbyaddr(string2c(f->addr), hosts, SrvMaxHosts);
	poperror();
	qunlock(&dbq);
	acquire();
	srvhostlist(hosts, count, SrvMaxHosts, f->ret);
}

/* Clear a native string owner slot without assuming pointer alignment. */
void
Srv_ipn2p(void *fp)
{
	int n;
	char buf[16];
	F_Srv_ipn2p *f;

	f = fp;
	srvclear(f->ret);
	release();
	qlock(&dbq);
	if(waserror()){
		qunlock(&dbq);
		acquire();
		return;
	}
	n = so_getservbyname(string2c(f->service), string2c(f->net), buf);
	poperror();
	qunlock(&dbq);
	acquire();
	if(n >= 0)
		retstr(buf, f->ret);
}

void
srvmodinit(void)
{
	builtinmod("$Srv", Srvmodtab, Srvmodlen);
}
