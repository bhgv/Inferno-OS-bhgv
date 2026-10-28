#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "../port/error.h"
#include "isa.h"
#include "interp.h"
#include "runt.h"
#include "kernel.h"
#include "raise.h"

/*
 * here because Sys_FileIO is not public
 */
extern	int	srvf2c(char*, char*, Sys_FileIO*);

/*
 * System types connected to gc
 */
uchar	FDmap[] = Sys_FD_map;
uchar	FileIOmap[] = Sys_FileIO_map;
void	freeFD(Heap*, int);
void	freeFileIO(Heap*, int);
Type*	TFD;
Type*	TFileIO;

static	uchar	rmap[] = Sys_FileIO_read_map;
static	uchar	wmap[] = Sys_FileIO_write_map;
static	Type*	FioTread;
static	Type*	FioTwrite;
static	uchar	dmap[] = Sys_Dir_map;
static	Type*	Tdir;

typedef struct FD FD;
struct FD
{
	Sys_FD	fd;
	Fgrp*	grp;
};

/* Size native records and locate channel-request pointers from their C declarations. */
void
sysinit(void)
{
	TFD = dtype(freeFD, sizeof(FD), FDmap, sizeof(FDmap));
	TFileIO = dtype(freeFileIO, sizeof(Sys_FileIO), FileIOmap, sizeof(FileIOmap));

	/* Support for devsrv.c */
	rmap[0] = 0x80 >> (offsetof(Sys_FileIO_read, t3)/sizeof(void*));
	wmap[0] = (0x80 >> (offsetof(Sys_FileIO_write, t1)/sizeof(void*))) |
		(0x80 >> (offsetof(Sys_FileIO_write, t3)/sizeof(void*)));
	FioTread = dtype(freeheap, sizeof(Sys_FileIO_read), rmap, sizeof(rmap));
	FioTwrite = dtype(freeheap, sizeof(Sys_FileIO_write), wmap, sizeof(wmap));

	/* Support for dirread */
	Tdir = dtype(freeheap, sizeof(Sys_Dir), dmap, sizeof(dmap));
}

void
freeFD(Heap *h, int swept)
{
	FD *handle;

	USED(swept);

	handle = H2D(FD*, h);

	release();
	if(handle->fd.fd >= 0)
		kfgrpclose(handle->grp, handle->fd.fd);
	closefgrp(handle->grp);
	acquire();
}

void
freeFileIO(Heap *h, int swept)
{
	Sys_FileIO *fio;

	if(swept)
		return;

	fio = H2D(Sys_FileIO*, h);
	destroy(fio->read);
	destroy(fio->write);
}

/* Transfer a native descriptor to a VM owner, closing it if allocation fails. */
Sys_FD*
mkfd(int fd)
{
	Heap *h;
	Fgrp *fg;
	FD *handle;
	char saved[ERRMAX];

	fg = up->env->fgrp;
	if(waserror()){
		memmove(saved, up->env->errstr, sizeof(saved));
		if(fd >= 0){
			release();
			kfgrpclose(fg, fd);
			acquire();
		}
		memmove(up->env->errstr, saved, sizeof(saved));
		nexterror();
	}
	h = heap(TFD);
	poperror();
	handle = H2D(FD*, h);
	handle->fd.fd = fd;
	handle->grp = fg;
	incref(&fg->r);
	return (Sys_FD*)handle;
}

typedef struct FDResult FDResult;
struct FDResult
{
	int raw[2];
	Sys_FD *handle[2];
	String *directory;
};

/* Mark a descriptor as transferred before mkfd can raise an allocation error. */
static Sys_FD*
takefd(volatile int *slot)
{
	int fd;

	fd = *slot;
	*slot = -1;
	return fd < 0 ? H : mkfd(fd);
}

/* Close every untransferred descriptor and partial owner without replacing the error. */
static void
freefdresult(volatile FDResult *result)
{
	char saved[ERRMAX];
	int i;

	memmove(saved, up->env->errstr, sizeof(saved));
	for(i = 0; i < 2; i++){
		if(result->raw[i] >= 0){
			release();
			kclose(result->raw[i]);
			acquire();
		}
		destroy(result->handle[i]);
	}
	destroy(result->directory);
	memmove(up->env->errstr, saved, sizeof(saved));
}

/* Derive the connection offset from the generated tuple without aligning its VM slot. */
static uchar*
connectionresultslot(void *result)
{
	F_Sys_dial frame;
	union { Sys_Connection align; uchar bytes[sizeof(*frame.ret)]; } layout;

	frame.ret = (void*)layout.bytes;
	return (uchar*)result + ((uchar*)&frame.ret->t1-layout.bytes);
}


/* Clear the original connection before releasing its captured previous owners. */
static void
clearconnection(void *slot, const Sys_Connection *previous)
{
	Sys_Connection old, empty;
	char saved[ERRMAX];

	memmove(saved, up->env->errstr, sizeof(saved));
	old = *previous;
	empty.dfd = empty.cfd = H;
	empty.dir = H;
	memmove(slot, &empty, sizeof(empty));
	destroy(old.dfd);
	destroy(old.cfd);
	destroy(old.dir);
	memmove(up->env->errstr, saved, sizeof(saved));
}


/* Build a complete connection before replacing the captured previous result. */
static void
makeconnection(int datafd, int controlfd, char *directory, void *slot, const Sys_Connection *previous)
{
	volatile FDResult result;
	Sys_Connection old, replacement;

	old = *previous;
	result.raw[0] = datafd;
	result.raw[1] = controlfd;
	result.handle[0] = result.handle[1] = H;
	result.directory = H;
	if(waserror()){
		freefdresult(&result);
		nexterror();
	}
	result.handle[0] = takefd(&result.raw[0]);
	result.handle[1] = takefd(&result.raw[1]);
	result.directory = c2string(directory, strlen(directory));
	poperror();
	replacement.dfd = result.handle[0];
	replacement.cfd = result.handle[1];
	replacement.dir = result.directory;
	memmove(slot, &replacement, sizeof(replacement));
	destroy(old.dfd);
	destroy(old.cfd);
	destroy(old.dir);
}

#define fdchk(x)	((x) == (Sys_FD*)H ? -1 : (x)->fd)

void
seterror(char *err, ...)
{
	char *estr;
	va_list arg;

	estr = up->env->errstr;
	va_start(arg, err);
	vseprint(estr, estr+ERRMAX, err, arg);
	va_end(arg);
}

char*
syserr(char *s, char *es, Prog *p)
{
	Osenv *o;

	o = p->osenv;
	kstrcpy(s, o->errstr, es - s);
	return s + strlen(s);
}


/* Reject high VM argument bits before a native int parameter can silently wrap. */
static int
sysnativeint(WORD value)
{
	return value >= -0x7fffffff-1 && value <= 0x7fffffff;
}


/* Capture a packed FD number and reject narrowing before releasing the VM. */
static int
sysiofd(Sys_FD *handle, int *descriptor)
{
	WORD number;

	if(handle == (Sys_FD*)H)
		number = -1;
	else {
		if(handle == nil)
			error(exNilref);
		if((uintptr)handle > (uintptr)-1-(O(Sys_FD, fd)+sizeof(number)))
			error(exBounds);
		memmove(&number, (uchar*)handle+O(Sys_FD, fd), sizeof(number));
	}
	if(!sysnativeint(number)) {
		seterror(Ebadarg);
		return 0;
	}
	*descriptor = (int)number;
	return 1;
}


/* Copy fixed builtin operands and validate their complete original result cell. */
static void
sysstatusframe(void *fp, void *copy, uintptr size, uintptr retoffset, uintptr resultbytes)
{
	void *ret;

	if(fp == nil || fp == H)
		error(exNilref);
	if((uintptr)fp > (uintptr)-1-size)
		error(exBounds);
	memmove(copy, fp, size);
	memmove(&ret, (uchar*)copy+retoffset, sizeof(ret));
	if(ret == nil || ret == H)
		error(exNilref);
	if((uintptr)ret > (uintptr)-1-resultbytes)
		error(exBounds);
}


/* Validate an old result owner before callbacks can make its destruction unsafe. */
static void*
sysresultowner(void *slot)
{
	void *old;
	uintptr address;
	ulong references;

	memmove(&old, slot, sizeof(old));
	if(old == nil)
		error(exNilref);
	if(old != H) {
		address = (uintptr)old;
		if(address <= sizeof(Heap) || (address-sizeof(Heap))%sizeof(void*) != 0)
			error(exHeap);
		memmove(&references, (uchar*)old-sizeof(Heap)+O(Heap, ref), sizeof(references));
		if(references == 0)
			error(exHeap);
	}
	return old;
}


/* Validate captured multi-owner results before any native call or partial construction. */
static void
sysfdresultowners(void *slots, int count)
{
	void *owners[3];
	ulong references;
	int i, j, copies;

	if(count < 1 || count > 3)
		error(exBounds);
	for(i = 0; i < count; i++)
		owners[i] = sysresultowner((uchar*)slots+i*sizeof(void*));
	for(i = 0; i < count; i++) {
		if(owners[i] == H)
			continue;
		copies = 0;
		for(j = 0; j < count; j++)
			if(owners[j] == owners[i])
				copies++;
		memmove(&references, (uchar*)owners[i]-sizeof(Heap)+O(Heap, ref), sizeof(references));
		if(references < copies)
			error(exHeap);
	}
}


/* Check every directory owner, including the references consumed by duplicate slots. */
static void
sysdirowners(void *slot)
{
	void *owners[4];
	uintptr offsets[4];
	ulong references;
	int i, j, copies;

	offsets[0] = O(Sys_Dir, name);
	offsets[1] = O(Sys_Dir, uid);
	offsets[2] = O(Sys_Dir, gid);
	offsets[3] = O(Sys_Dir, muid);
	for(i = 0; i < 4; i++)
		owners[i] = sysresultowner((uchar*)slot+offsets[i]);
	for(i = 0; i < 4; i++) {
		if(owners[i] == H)
			continue;
		copies = 0;
		for(j = 0; j < 4; j++)
			if(owners[j] == owners[i])
				copies++;
		memmove(&references, (uchar*)owners[i]-sizeof(Heap)+O(Heap, ref), sizeof(references));
		if(references < copies)
			error(exHeap);
	}
}


/* Capture fixed operands before callbacks and retain the original scalar result cell. */
void
Sys_millisec(void *fp)
{
	F_Sys_millisec local, *f;
	WORD result;

	sysstatusframe(fp, &local, sizeof(local), O(F_Sys_millisec, ret), sizeof(*local.ret));
	f = &local;
	result = TK2MS(MACHP(0)->ticks);
	memmove(f->ret, &result, sizeof(result));
}


/* Snapshot creation operands and the original owner slot before finalization callbacks. */
void
Sys_open(void *fp)
{
	int fd;
	F_Sys_open local, *f;
	Sys_FD *old, *result;

	sysstatusframe(fp, &local, sizeof(local), O(F_Sys_open, ret), sizeof(*local.ret));
	f = &local;
	old = sysresultowner(f->ret);
	result = H;
	memmove(f->ret, &result, sizeof(result));
	destroy(old);
	if(!sysnativeint(f->mode)) {
		seterror(Ebadarg);
		return;
	}
	release();
	fd = kopen(string2c(f->s), f->mode);
	acquire();
	if(fd == -1)
		return;

	result = mkfd(fd);
	memmove(f->ret, &result, sizeof(result));
}


/* Capture packed pipe operands and both old owners before constructing replacements. */
void
Sys_pipe(void *fp)
{
	Array *a, view;
	Type element;
	int fd[2], status;
	WORD outcome;
	uchar *slots;
	Sys_FD *old[2], *next[2];
	F_Sys_pipe local, *f;
	volatile FDResult result;

	sysstatusframe(fp, &local, sizeof(local), O(F_Sys_pipe, ret), sizeof(*local.ret));
	f = &local;
	outcome = -1;
	a = f->fds;
	if(a == H || a == nil)
		goto Empty;
	if((uintptr)a < sizeof(void*) || (uintptr)a > (uintptr)-1-sizeof(view))
		error(exBounds);
	memmove(&view, a, sizeof(view));
	if(view.len < 2 || view.data == nil || view.data == H ||
	   (uintptr)view.data > (uintptr)-1-2*sizeof(Sys_FD*))
		goto Empty;
	/* [GC:] A byte or scalar array cannot own two pointer-sized replacement slots. */
	if(view.t == nil || view.t == (Type*)H ||
	   (uintptr)view.t < sizeof(void*) || (uintptr)view.t > (uintptr)-1-sizeof(element))
		goto Empty;
	memmove(&element, view.t, sizeof(element));
	if(element.size != sizeof(Sys_FD*) || element.np != 1 || element.map[0] != 0x80)
		goto Empty;
	slots = view.data;
	memmove(old, slots, sizeof(old));
	sysfdresultowners(old, 2);
	memmove(f->ret, &outcome, sizeof(outcome));
	release();
	status = kpipe(fd);
	acquire();
	if(status < 0)
		return;
	result.raw[0] = fd[0];
	result.raw[1] = fd[1];
	result.handle[0] = result.handle[1] = H;
	result.directory = H;
	if(waserror()){
		freefdresult(&result);
		nexterror();
	}
	result.handle[0] = takefd(&result.raw[0]);
	result.handle[1] = takefd(&result.raw[1]);
	poperror();
	next[0] = result.handle[0];
	next[1] = result.handle[1];
	memmove(slots, next, sizeof(next));
	destroy(old[0]);
	destroy(old[1]);
	outcome = 0;
Empty:
	memmove(f->ret, &outcome, sizeof(outcome));
}


/* Snapshot creation operands and the original owner slot before finalization callbacks. */
void
Sys_fildes(void *fp)
{
	F_Sys_fildes local, *f;
	Sys_FD *old, *result;
	WORD source;
	int fd;

	sysstatusframe(fp, &local, sizeof(local), O(F_Sys_fildes, ret), sizeof(*local.ret));
	f = &local;
	source = f->fd;
	old = sysresultowner(f->ret);
	result = H;
	memmove(f->ret, &result, sizeof(result));
	destroy(old);
	if(!sysnativeint(source)){
		seterror(Ebadarg);
		return;
	}
	release();
	fd = kdup(source, -1);
	acquire();
	if(fd == -1)
		return;
	result = mkfd(fd);
	memmove(f->ret, &result, sizeof(result));
}


/* Capture fixed operands before callbacks and retain the original scalar result cell. */
void
Sys_dup(void *fp)
{
	F_Sys_dup local, *f;
	WORD result;

	sysstatusframe(fp, &local, sizeof(local), O(F_Sys_dup, ret), sizeof(*local.ret));
	f = &local;
	if(!sysnativeint(f->old) || !sysnativeint(f->new)){
		seterror(Ebadarg);
		result = -1;
		memmove(f->ret, &result, sizeof(result));
		return;
	}
	release();
	result = kdup(f->old, f->new);
	memmove(f->ret, &result, sizeof(result));
	acquire();
}


/* Snapshot creation operands and the original owner slot before finalization callbacks. */
void
Sys_create(void *fp)
{
	int fd;
	F_Sys_create local, *f;
	Sys_FD *old, *result;

	sysstatusframe(fp, &local, sizeof(local), O(F_Sys_create, ret), sizeof(*local.ret));
	f = &local;
	old = sysresultowner(f->ret);
	result = H;
	memmove(f->ret, &result, sizeof(result));
	destroy(old);
	if(!sysnativeint(f->mode)) {
		seterror(Ebadarg);
		return;
	}
	release();
	fd = kcreate(string2c(f->s), f->mode, f->perm);
	acquire();
	if(fd == -1)
		return;

	result = mkfd(fd);
	memmove(f->ret, &result, sizeof(result));
}


/* Capture fixed operands before callbacks and retain the original scalar result cell. */
void
Sys_remove(void *fp)
{
	F_Sys_remove local, *f;
	WORD result;

	sysstatusframe(fp, &local, sizeof(local), O(F_Sys_remove, ret), sizeof(*local.ret));
	f = &local;
	release();
	result = kremove(string2c(f->s));
	memmove(f->ret, &result, sizeof(result));
	acquire();
}


/* Capture fixed operands before callbacks and retain the original scalar result cell. */
void
Sys_seek(void *fp)
{
	F_Sys_seek local, *f;
	LONG result;
	int fd;

	sysstatusframe(fp, &local, sizeof(local), O(F_Sys_seek, ret), sizeof(*local.ret));
	f = &local;
	if(!sysnativeint(f->start)){
		seterror(Ebadarg);
		result = -1;
		memmove(f->ret, &result, sizeof(result));
		return;
	}
	if(!sysiofd(f->fd, &fd)) {
		result = -1;
		memmove(f->ret, &result, sizeof(result));
		return;
	}
	release();
	result = kseek(fd, f->off, f->start);
	memmove(f->ret, &result, sizeof(result));
	acquire();
}


/* Capture fixed operands before callbacks and retain the original scalar result cell. */
void
Sys_unmount(void *fp)
{
	F_Sys_unmount local, *f;
	WORD result;

	sysstatusframe(fp, &local, sizeof(local), O(F_Sys_unmount, ret), sizeof(*local.ret));
	f = &local;
	release();
	result = kunmount(string2c(f->s1), string2c(f->s2));
	memmove(f->ret, &result, sizeof(result));
	acquire();
}


/* Capture a borrowed byte-array view without typed reads from packed headers. */
static int
sysiocount(Array *a, WORD count, uchar **data)
{
	Heap owner;
	Array view;
	uintptr address;

	address = (uintptr)a;
	if(a == nil || address <= sizeof(Heap) ||
	   address > (uintptr)-1-sizeof(view))
		goto Invalid;
	memmove(&owner, (uchar*)a-sizeof(Heap), sizeof(owner));
	if(owner.t != &Tarray)
		goto Invalid;
	memmove(&view, a, sizeof(view));
	if(view.t != &Tbyte || view.len < 0 ||
	   (uvlong)view.len > 0x7fffffff ||
	   (view.len != 0 && (view.data == nil || view.data == H ||
	   (uintptr)view.data > (uintptr)-1-(uintptr)view.len)))
		goto Invalid;
	if(count > view.len)
		count = view.len;
	*data = view.data;
	return (int)count;

Invalid:
	seterror(Ebadarg);
	return -1;
}


/* Validate complete frame and result spans before touching native I/O operands. */
static void
sysioframe(void *fp, void *copy, uintptr size, uintptr retoffset)
{
	sysstatusframe(fp, copy, size, retoffset, sizeof(WORD));
}


/* Snapshot I/O operands before callbacks and publish to the original result cell. */
void
Sys_read(void *fp)
{
	F_Sys_read local, *f;
	uchar *data;
	int fd, n;
	WORD result;

	sysioframe(fp, &local, sizeof(local), O(F_Sys_read, ret));
	f = &local;
	if(f->buf == (Array*)H || f->n < 0) {
		result = 0;
		memmove(f->ret, &result, sizeof(result));
		return;
	}
	n = sysiocount(f->buf, f->n, &data);
	if(n < 0 || !sysiofd(f->fd, &fd)) {
		result = -1;
		memmove(f->ret, &result, sizeof(result));
		return;
	}

	release();
	result = kread(fd, data, n);
	memmove(f->ret, &result, sizeof(result));
	acquire();
}


/* Snapshot I/O operands before callbacks and publish to the original result cell. */
void
Sys_readn(void *fp)
{
	F_Sys_readn local, *f;
	uchar *data;
	int fd, n, m, t;
	WORD result;

	sysioframe(fp, &local, sizeof(local), O(F_Sys_readn, ret));
	f = &local;
	if(f->buf == (Array*)H || f->n < 0) {
		result = 0;
		memmove(f->ret, &result, sizeof(result));
		return;
	}
	n = sysiocount(f->buf, f->n, &data);
	if(n < 0 || !sysiofd(f->fd, &fd)) {
		result = -1;
		memmove(f->ret, &result, sizeof(result));
		return;
	}

	release();
	for(t = 0; t < n; t += m){
		m = kread(fd, data+t, n-t);
		if(m <= 0){
			if(t == 0)
				t = m;
			break;
		}
	}
	result = t;
	memmove(f->ret, &result, sizeof(result));
	acquire();
}


/* Snapshot I/O operands before callbacks and publish to the original result cell. */
void
Sys_pread(void *fp)
{
	F_Sys_pread local, *f;
	uchar *data;
	int fd, n;
	WORD result;

	sysioframe(fp, &local, sizeof(local), O(F_Sys_pread, ret));
	f = &local;
	if(f->buf == (Array*)H || f->n < 0) {
		result = 0;
		memmove(f->ret, &result, sizeof(result));
		return;
	}
	n = sysiocount(f->buf, f->n, &data);
	if(n < 0 || !sysiofd(f->fd, &fd)) {
		result = -1;
		memmove(f->ret, &result, sizeof(result));
		return;
	}

	release();
	result = kpread(fd, data, n, f->off);
	memmove(f->ret, &result, sizeof(result));
	acquire();
}


/* Capture fixed operands before callbacks and retain the original scalar result cell. */
void
Sys_chdir(void *fp)
{
	F_Sys_chdir local, *f;
	WORD result;

	sysstatusframe(fp, &local, sizeof(local), O(F_Sys_chdir, ret), sizeof(*local.ret));
	f = &local;
	release();
	result = kchdir(string2c(f->path));
	memmove(f->ret, &result, sizeof(result));
	acquire();
}


/* Snapshot I/O operands before callbacks and publish to the original result cell. */
void
Sys_write(void *fp)
{
	F_Sys_write local, *f;
	uchar *data;
	int fd, n;
	WORD result;

	sysioframe(fp, &local, sizeof(local), O(F_Sys_write, ret));
	f = &local;
	if(f->buf == (Array*)H || f->n < 0) {
		result = 0;
		memmove(f->ret, &result, sizeof(result));
		return;
	}
	n = sysiocount(f->buf, f->n, &data);
	if(n < 0 || !sysiofd(f->fd, &fd)) {
		result = -1;
		memmove(f->ret, &result, sizeof(result));
		return;
	}

	release();
	result = kwrite(fd, data, n);
	memmove(f->ret, &result, sizeof(result));
	acquire();
}


/* Snapshot I/O operands before callbacks and publish to the original result cell. */
void
Sys_pwrite(void *fp)
{
	F_Sys_pwrite local, *f;
	uchar *data;
	int fd, n;
	WORD result;

	sysioframe(fp, &local, sizeof(local), O(F_Sys_pwrite, ret));
	f = &local;
	if(f->buf == (Array*)H || f->n < 0) {
		result = 0;
		memmove(f->ret, &result, sizeof(result));
		return;
	}
	n = sysiocount(f->buf, f->n, &data);
	if(n < 0 || !sysiofd(f->fd, &fd)) {
		result = -1;
		memmove(f->ret, &result, sizeof(result));
		return;
	}

	release();
	result = kpwrite(fd, data, n, f->off);
	memmove(f->ret, &result, sizeof(result));
	acquire();
}

/* Update owned strings in place, then copy scalar metadata without alignment assumptions. */
static void
unpackdir(Dir *d, void *slot)
{
	Sys_Dir result;
	uchar *bytes;

	bytes = slot;
	retstr(d->name, bytes+offsetof(Sys_Dir, name));
	retstr(d->uid, bytes+offsetof(Sys_Dir, uid));
	retstr(d->gid, bytes+offsetof(Sys_Dir, gid));
	retstr(d->muid, bytes+offsetof(Sys_Dir, muid));
	/* String publication remains visible to GC during subsequent allocations. */
	memmove(&result, slot, sizeof(result));
	result.qid.path = d->qid.path;
	result.qid.vers = d->qid.vers;
	result.qid.qtype = d->qid.type;
	result.mode = d->mode;
	result.atime = d->atime;
	result.mtime = d->mtime;
	result.length = d->length;
	result.dtype = d->type;
	result.dev = d->dev;
	memmove(slot, &result, sizeof(result));
}

/* Capture unaligned VM metadata and check every native string/allocation size before copying. */
static Dir*
packdir(void *slot)
{
	char *nm[4], *p;
	Sys_Dir source;
	ulong lengths[4], size, length;
	int i;
	Dir *d;

	memmove(&source, slot, sizeof(source));
	nm[0] = string2c(source.name);
	nm[1] = string2c(source.uid);
	nm[2] = string2c(source.gid);
	nm[3] = string2c(source.muid);
	size = sizeof(*d);
	for(i = 0; i < 4; i++){
		length = strlen(nm[i]);
		if(length >= (ulong)-1-size)
			error(exHeap);
		lengths[i] = length+1;
		size += lengths[i];
	}
	d = smalloc(size);
	p = (char*)d+sizeof(*d);
	for(i = 0; i < 4; i++){
		memmove(p, nm[i], lengths[i]);
		nm[i] = p;
		p += lengths[i];
	}
	d->name = nm[0];
	d->uid = nm[1];
	d->gid = nm[2];
	d->muid = nm[3];
	d->qid.path = source.qid.path;
	d->qid.vers = source.qid.vers;
	d->qid.type = source.qid.qtype;
	d->mode = source.mode;
	d->atime = source.atime;
	d->mtime = source.mtime;
	d->length = source.length;
	d->type = source.dtype;
	d->dev = source.dev;
	return d;
}


/* Capture query operands and validate old owners before native or allocation callbacks. */
void
Sys_fstat(void *fp)
{
	Dir *d;
	F_Sys_fstat local, *f;
	F_Sys_fstat layout;
	union {
		uvlong align; // Align only the scratch layout used to compute tuple offsets.
		uchar bytes[sizeof(*f->ret)]; // Preserve the generated tuple's complete extent.
	} scratch;
	uchar *recordslot;
	WORD status;
	int fd;

	sysstatusframe(fp, &local, sizeof(local), O(F_Sys_fstat, ret), sizeof(*local.ret));
	f = &local;
	/* Generated return tuples can contain explicit padding beyond native alignment. */
	layout.ret = (void*)scratch.bytes;
	recordslot = (uchar*)f->ret+((uchar*)&layout.ret->t1-scratch.bytes);
	if(!sysiofd(f->fd, &fd)) {
		status = -1;
		memmove(f->ret, &status, sizeof(status));
		return;
	}
	sysdirowners(recordslot);
	status = -1;
	memmove(f->ret, &status, sizeof(status));
	release();
	d = kdirfstat(fd);
	acquire();
	if(d == nil)
		return;
	if(waserror() == 0){
		unpackdir(d, recordslot);
		status = 0;
		memmove(f->ret, &status, sizeof(status));
		poperror();
	}
	free(d);
}


/* Capture query operands and validate old owners before native or allocation callbacks. */
void
Sys_stat(void *fp)
{
	Dir *d;
	F_Sys_stat local, *f;
	F_Sys_stat layout;
	union {
		uvlong align; // Align only the scratch layout used to compute tuple offsets.
		uchar bytes[sizeof(*f->ret)]; // Preserve the generated tuple's complete extent.
	} scratch;
	uchar *recordslot;
	WORD status;
	String *path;

	sysstatusframe(fp, &local, sizeof(local), O(F_Sys_stat, ret), sizeof(*local.ret));
	f = &local;
	path = f->s;
	/* Generated return tuples can contain explicit padding beyond native alignment. */
	layout.ret = (void*)scratch.bytes;
	recordslot = (uchar*)f->ret+((uchar*)&layout.ret->t1-scratch.bytes);
	sysdirowners(recordslot);
	status = -1;
	memmove(f->ret, &status, sizeof(status));
	release();
	d = kdirstat(string2c(path));
	acquire();
	if(d == nil)
		return;
	if(waserror() == 0){
		unpackdir(d, recordslot);
		status = 0;
		memmove(f->ret, &status, sizeof(status));
		poperror();
	}
	free(d);
}


/* Capture query operands and validate old owners before native or allocation callbacks. */
void
Sys_fd2path(void *fp)
{
	F_Sys_fd2path local, *f;
	char *s;
	String *old, *empty;
	int fd, valid;

	sysstatusframe(fp, &local, sizeof(local), O(F_Sys_fd2path, ret), sizeof(*local.ret));
	f = &local;
	valid = sysiofd(f->fd, &fd);
	old = sysresultowner(f->ret);
	empty = H;
	memmove(f->ret, &empty, sizeof(empty));
	destroy(old);
	if(!valid)
		return;
	release();
	s = kfd2path(fd);
	acquire();
	if(waserror() == 0){
		retstr(s, f->ret);
		poperror();
	}
	free(s);
}


/* Capture fixed operands before callbacks and retain the original scalar result cell. */
void
Sys_mount(void *fp)
{
	F_Sys_mount local, *f;
	WORD result;
	int fd, afd;

	sysstatusframe(fp, &local, sizeof(local), O(F_Sys_mount, ret), sizeof(*local.ret));
	f = &local;
	if(!sysnativeint(f->flags)){
		seterror(Ebadarg);
		result = -1;
		memmove(f->ret, &result, sizeof(result));
		return;
	}
	if(!sysiofd(f->fd, &fd) || !sysiofd(f->afd, &afd)) {
		result = -1;
		memmove(f->ret, &result, sizeof(result));
		return;
	}
	release();
	result = kmount(fd, afd, string2c(f->on), f->flags, string2c(f->spec));
	memmove(f->ret, &result, sizeof(result));
	acquire();
}


/* Capture fixed operands before callbacks and retain the original scalar result cell. */
void
Sys_bind(void *fp)
{
	F_Sys_bind local, *f;
	WORD result;

	sysstatusframe(fp, &local, sizeof(local), O(F_Sys_bind, ret), sizeof(*local.ret));
	f = &local;
	if(!sysnativeint(f->flags)){
		seterror(Ebadarg);
		result = -1;
		memmove(f->ret, &result, sizeof(result));
		return;
	}
	release();
	result = kbind(string2c(f->s), string2c(f->on), f->flags);
	memmove(f->ret, &result, sizeof(result));
	acquire();
}


/* Capture fixed operands before callbacks and retain the original scalar result cell. */
void
Sys_wstat(void *fp)
{
	Dir *d;
	F_Sys_wstat local, *f;
	WORD result;

	sysstatusframe(fp, &local, sizeof(local), O(F_Sys_wstat, ret), sizeof(*local.ret));
	f = &local;
	d = packdir(&f->d);
	release();
	result = kdirwstat(string2c(f->s), d);
	memmove(f->ret, &result, sizeof(result));
	acquire();
	free(d);
}


/* Capture fixed operands before callbacks and retain the original scalar result cell. */
void
Sys_fwstat(void *fp)
{
	Dir *d;
	F_Sys_fwstat local, *f;
	WORD result;
	int fd;

	sysstatusframe(fp, &local, sizeof(local), O(F_Sys_fwstat, ret), sizeof(*local.ret));
	f = &local;
	if(!sysiofd(f->fd, &fd)) {
		result = -1;
		memmove(f->ret, &result, sizeof(result));
		return;
	}
	d = packdir(&f->d);
	release();
	result = kdirfwstat(fd, d);
	memmove(f->ret, &result, sizeof(result));
	acquire();
	free(d);
}


/* Capture the original format/result before releasing the VM for output. */
void
Sys_print(void *fp)
{
	int n;
	Prog *p;
	Chan *c;
	char buf[1024], *b = buf;
	String *format;
	void *ret, *args;
	WORD result;

	if(fp == nil || fp == H)
		error(exNilref);
	if((uintptr)fp > (uintptr)-1-O(F_Sys_print, vargs))
		error(exBounds);
	memmove(&ret, (uchar*)fp+O(F_Sys_print, ret), sizeof(ret));
	memmove(&format, (uchar*)fp+O(F_Sys_print, s), sizeof(format));
	args = (uchar*)fp+O(F_Sys_print, vargs);
	if(ret == nil || ret == H)
		error(exNilref);
	if((uintptr)ret > (uintptr)-1-sizeof(result))
		error(exBounds);
	c = up->env->fgrp->fd[1];
	if(c == nil){
		result = -1;
		memmove(ret, &result, sizeof(result));
		return;
	}
	p = currun();

	release();
	n = xprint(p, fp, args, format, buf, sizeof(buf));
	if (n >= sizeof(buf)-UTFmax-2)
		n = bigxprint(p, fp, args, format, &b, sizeof(buf));
	result = kwrite(1, b, n);
	memmove(ret, &result, sizeof(result));
	if (b != buf)
		free(b);
	acquire();
}


/* Validate the native FD and capture packed operands before output side effects. */
void
Sys_fprint(void *fp)
{
	int n;
	Prog *p;
	char buf[1024], *b = buf;
	String *format;
	void *ret, *args;
	WORD result, number;
	Sys_FD *handle;
	int descriptor;

	if(fp == nil || fp == H)
		error(exNilref);
	if((uintptr)fp > (uintptr)-1-O(F_Sys_fprint, vargs))
		error(exBounds);
	memmove(&ret, (uchar*)fp+O(F_Sys_fprint, ret), sizeof(ret));
	memmove(&format, (uchar*)fp+O(F_Sys_fprint, s), sizeof(format));
	args = (uchar*)fp+O(F_Sys_fprint, vargs);
	if(ret == nil || ret == H)
		error(exNilref);
	if((uintptr)ret > (uintptr)-1-sizeof(result))
		error(exBounds);
	memmove(&handle, (uchar*)fp+O(F_Sys_fprint, fd), sizeof(handle));
	if(handle == (Sys_FD*)H)
		number = -1;
	else {
		if(handle == nil)
			error(exNilref);
		if((uintptr)handle > (uintptr)-1-(O(Sys_FD, fd)+sizeof(number)))
			error(exBounds);
		memmove(&number, (uchar*)handle+O(Sys_FD, fd), sizeof(number));
	}
	if(!sysnativeint(number)) {
		seterror(Ebadarg);
		result = -1;
		memmove(ret, &result, sizeof(result));
		return;
	}
	descriptor = (int)number;
	p = currun();
	release();
	n = xprint(p, fp, args, format, buf, sizeof(buf));
	if (n >= sizeof(buf)-UTFmax-2)
		n = bigxprint(p, fp, args, format, &b, sizeof(buf));
	result = kwrite(descriptor, b, n);
	memmove(ret, &result, sizeof(result));
	if (b != buf)
		free(b);
	acquire();
}


/* Validate and capture both error-text and scalar result publication addresses. */
void
Sys_werrstr(void *fp)
{
	String *source;
	void *ret;
	char *text, *destination;
	WORD result;

	if(fp == nil || fp == H)
		error(exNilref);
	if((uintptr)fp > (uintptr)-1-(O(F_Sys_werrstr, s)+sizeof(source)))
		error(exBounds);
	memmove(&ret, (uchar*)fp+O(F_Sys_werrstr, ret), sizeof(ret));
	memmove(&source, (uchar*)fp+O(F_Sys_werrstr, s), sizeof(source));
	if(ret == nil || ret == H)
		error(exNilref);
	if((uintptr)ret > (uintptr)-1-sizeof(result))
		error(exBounds);
	/* Conversion may allocate; keep both original output addresses authoritative. */
	destination = up->env->errstr;
	text = string2c(source);
	kstrcpy(destination, text, ERRMAX);
	result = 0;
	memmove(ret, &result, sizeof(result));
}


/* Capture the complete frame and previous connection before native and owner callbacks. */
void
Sys_dial(void *fp)
{
	int datafd, controlfd;
	char dir[NETPATHLEN], *address, *local;
	F_Sys_dial localframe, *f;
	Sys_Connection previous;
	uchar *resultslot;
	WORD status;

	sysstatusframe(fp, &localframe, sizeof(localframe), O(F_Sys_dial, ret), sizeof(*localframe.ret));
	f = &localframe;
	resultslot = connectionresultslot(f->ret);
	memmove(&previous, resultslot, sizeof(previous));
	sysfdresultowners(&previous, 3);
	address = string2c(f->addr);
	local = string2c(f->local);
	release();
	datafd = kdial(address, local, dir, &controlfd);
	acquire();
	status = -1;
	memmove(f->ret, &status, sizeof(status));
	if(datafd < 0){
		clearconnection(resultslot, &previous);
		return;
	}
	makeconnection(datafd, controlfd, dir, resultslot, &previous);
	status = 0;
	memmove(f->ret, &status, sizeof(status));
}


/* Capture the complete frame and previous connection before native and owner callbacks. */
void
Sys_announce(void *fp)
{
	char dir[NETPATHLEN], *address;
	int fd;
	F_Sys_announce localframe, *f;
	Sys_Connection previous;
	uchar *resultslot;
	WORD status;

	sysstatusframe(fp, &localframe, sizeof(localframe), O(F_Sys_announce, ret), sizeof(*localframe.ret));
	f = &localframe;
	resultslot = connectionresultslot(f->ret);
	memmove(&previous, resultslot, sizeof(previous));
	sysfdresultowners(&previous, 3);
	address = string2c(f->addr);
	release();
	fd = kannounce(address, dir);
	acquire();
	status = -1;
	memmove(f->ret, &status, sizeof(status));
	if(fd < 0){
		clearconnection(resultslot, &previous);
		return;
	}
	makeconnection(-1, fd, dir, resultslot, &previous);
	status = 0;
	memmove(f->ret, &status, sizeof(status));
}


/* Capture the complete frame and previous connection before native and owner callbacks. */
void
Sys_listen(void *fp)
{
	F_Sys_listen localframe, *f;
	Sys_Connection previous;
	uchar *resultslot;
	WORD status;
	char dir[NETPATHLEN], *directory;
	int fd;

	sysstatusframe(fp, &localframe, sizeof(localframe), O(F_Sys_listen, ret), sizeof(*localframe.ret));
	f = &localframe;
	resultslot = connectionresultslot(f->ret);
	memmove(&previous, resultslot, sizeof(previous));
	sysfdresultowners(&previous, 3);
	directory = string2c(f->c.dir);
	release();
	fd = klisten(directory, dir);
	acquire();
	status = -1;
	memmove(f->ret, &status, sizeof(status));
	if(fd < 0){
		clearconnection(resultslot, &previous);
		return;
	}
	makeconnection(-1, fd, dir, resultslot, &previous);
	status = 0;
	memmove(f->ret, &status, sizeof(status));
}


/* Capture fixed operands and complete result slots before native and allocation callbacks. */
void
Sys_sleep(void *fp)
{
	F_Sys_sleep local, *f;
	WORD result = 0;

	sysstatusframe(fp, &local, sizeof(local), O(F_Sys_sleep, ret), sizeof(*local.ret));
	f = &local;
	if(!sysnativeint(f->period)){
		seterror(Ebadarg);
		result = -1;
		memmove(f->ret, &result, sizeof(result));
		return;
	}
	release();
	if(f->period > 0){
		if(waserror()){
			acquire();
			error("");
		}
		tsleep(&up->sleep, return0, 0, f->period);
		poperror();
	}
	acquire();
	memmove(f->ret, &result, sizeof(result));
}


/* Complete short stream writes and count only bytes acknowledged by the destination. */
static int
sysstreamwrite(int fd, uchar *buf, int n, UWORD *total)
{
	int done;
	long wrote;

	for(done = 0; done < n; done += wrote){
		wrote = kwrite(fd, buf+done, n-done);
		if(wrote <= 0 || wrote > n-done)
			return -1;
		*total += (UWORD)wrote;
	}
	return 0;
}


/* Capture fixed operands and complete result slots before native and allocation callbacks. */
void
Sys_stream(void *fp)
{
	Prog *p;
	uchar *buf;
	int src, dst, size, t;
	long n;
	UWORD nbytes;
	F_Sys_stream local, *f;
	WORD result;

	sysstatusframe(fp, &local, sizeof(local), O(F_Sys_stream, ret), sizeof(*local.ret));
	f = &local;
	if(f->bufsiz <= 0 || (uvlong)f->bufsiz > 0x7fffffff){
		seterror(Ebadarg);
		result = -1;
		memmove(f->ret, &result, sizeof(result));
		return;
	}
	if(!sysiofd(f->src, &src) || !sysiofd(f->dst, &dst)) {
		result = -1;
		memmove(f->ret, &result, sizeof(result));
		return;
	}
	p = currun();
	size = (int)f->bufsiz;
	buf = malloc(size);
	if(buf == nil){
		kwerrstr(Enomem);
		result = -1;
		memmove(f->ret, &result, sizeof(result));
		return;
	}

	release();
	t = 0;
	nbytes = 0;
	while(p->kill == nil){
		n = kread(src, buf+t, size-t);
		if(n <= 0 || n > size-t)
			break;
		t += n;
		if(t == size){
			if(sysstreamwrite(dst, buf, t, &nbytes) < 0){
				t = 0;
				break;
			}
			t = 0;
		}
	}
	if(t != 0)
		sysstreamwrite(dst, buf, t, &nbytes);
	acquire();
	free(buf);
	/* Interpret the modular byte count at the VM's width without signed overflow. */
	if(nbytes <= (~(UWORD)0 >> 1))
		result = (WORD)nbytes;
	else
		result = -1-(WORD)(~nbytes);
	memmove(f->ret, &result, sizeof(result));
}


/* Capture fixed operands and complete result slots before native and allocation callbacks. */
void
Sys_export(void *fp)
{
	F_Sys_export local, *f;
	WORD result;
	int fd;

	sysstatusframe(fp, &local, sizeof(local), O(F_Sys_export, ret), sizeof(*local.ret));
	f = &local;
	if(!sysiofd(f->c, &fd)) {
		result = -1;
		memmove(f->ret, &result, sizeof(result));
		return;
	}
	release();
	result = export(fd, string2c(f->dir), f->flag&Sys_EXPASYNC);
	memmove(f->ret, &result, sizeof(result));
	acquire();
}


/* Capture the result owner and inputs before constructing or registering either channel. */
void
Sys_file2chan(void *fp)
{
	F_Sys_file2chan local, *f;
	volatile struct {
		Sys_FileIO *file; // Partial channel pair owned until publication.
		int released; // Restore VM ownership when registration raises.
	} result;
	Sys_FileIO *old, *published;
	char *directory, *name, saved[ERRMAX];
	int status;

	sysstatusframe(fp, &local, sizeof(local), O(F_Sys_file2chan, ret), sizeof(*local.ret));
	f = &local;
	old = sysresultowner(f->ret);
	/* String conversion can allocate and must run while the VM is acquired. */
	directory = string2c(f->dir);
	name = string2c(f->file);
	result.file = H;
	result.released = 0;
	if(waserror()){
		if(result.released)
			acquire();
		memmove(saved, up->env->errstr, sizeof(saved));
		destroy(result.file);
		memmove(up->env->errstr, saved, sizeof(saved));
		nexterror();
	}
	result.file = H2D(Sys_FileIO*, heap(TFileIO));
	result.file->read = cnewc(FioTread, movtmp, 16);
	result.file->write = cnewc(FioTwrite, movtmp, 16);
	release();
	result.released = 1;
	status = srvf2c(directory, name, result.file);
	acquire();
	result.released = 0;
	poperror();
	if(status < 0){
		memmove(saved, up->env->errstr, sizeof(saved));
		destroy(result.file);
		result.file = H;
	}
	published = result.file;
	memmove(f->ret, &published, sizeof(published));
	destroy(old);
	if(status < 0)
		memmove(up->env->errstr, saved, sizeof(saved));
}

/* Decode a VM list integer without narrowing an unrelated high-bit descriptor into native int. */
static int
sysfdword(void *slot, int *fd)
{
	WORD value;

	memmove(&value, slot, sizeof(value));
	if(!sysnativeint(value))
		return 0;
	*fd = value;
	return 1;
}

enum
{
	/* the following pctl calls can block and must release the virtual machine */
	BlockingPctl=	Sys_NEWFD|Sys_FORKFD|Sys_NEWNS|Sys_FORKNS|Sys_NEWENV|Sys_FORKENV
};

/* Publish complete pctl status and preserve descriptor-group ownership during list operations. */
void
Sys_pctl(void *fp)
{
	int fd;
	Prog *p;
	List *l;
	Chan *c;
	volatile struct {Pgrp *np;} np;
	Pgrp *opg;
	Chan *dot;
	Osenv *o;
	F_Sys_pctl *f;
	Fgrp *fg, *ofg, *nfg;
	volatile struct {Egrp *ne;} ne;
	Egrp *oe;
	WORD result;

	f = fp;

	p = currun();
	if(f->flags & BlockingPctl)
		release();

	np.np = nil;
	ne.ne = nil;
	if(waserror()) {
		closepgrp(np.np);
		closeegrp(ne.ne);
		if(f->flags & BlockingPctl)
			acquire();
		result = -1;
		memmove(f->ret, &result, sizeof(result));
		return;
	}

	o = p->osenv;
	if(f->flags & Sys_NEWFD) {
		ofg = o->fgrp;
		nfg = newfgrp(ofg);
		lock(ofg);
		/* file descriptors to preserve */
		for(l = f->movefd; l != H; l = l->tail) {
			if(sysfdword(l->data, &fd) && fd >= 0 && fd <= ofg->maxfd) {
				c = ofg->fd[fd];
				if(c != nil && fd < nfg->nfd && nfg->fd[fd] == nil) {
					incref(c);
					nfg->fd[fd] = c;
					if(nfg->maxfd < fd)
						nfg->maxfd = fd;
				}
			}
		}
		unlock(ofg);
		o->fgrp = nfg;
		closefgrp(ofg);
	}
	else
	if(f->flags & Sys_FORKFD) {
		ofg = o->fgrp;
		fg = dupfgrp(ofg);
		/* Close only in the unpublished copy; the old group may belong to other programs. */
		for(l = f->movefd; l != H; l = l->tail){
			if(sysfdword(l->data, &fd))
				kfgrpclose(fg, fd);
			else
				seterror(Ebadarg);
		}
		o->fgrp = fg;
		closefgrp(ofg);
	}

	if(f->flags & Sys_NEWNS) {
		np.np = newpgrp();
		dot = o->pgrp->dot;
		np.np->dot = cclone(dot);
		np.np->slash = cclone(dot);
		cnameclose(np.np->slash->name);
		np.np->slash->name = newcname("/");
		np.np->nodevs = o->pgrp->nodevs;
		opg = o->pgrp;
		o->pgrp = np.np;
		np.np = nil;
		closepgrp(opg);
	}
	else
	if(f->flags & Sys_FORKNS) {
		np.np = newpgrp();
		pgrpcpy(np.np, o->pgrp);
		opg = o->pgrp;
		o->pgrp = np.np;
		np.np = nil;
		closepgrp(opg);
	}

	if(f->flags & Sys_NEWENV) {
		oe = o->egrp;
		o->egrp = newegrp();
		closeegrp(oe);
	}
	else
	if(f->flags & Sys_FORKENV) {
		ne.ne = newegrp();
		egrpcpy(ne.ne, o->egrp);
		oe = o->egrp;
		o->egrp = ne.ne;
		ne.ne = nil;
		closeegrp(oe);
	}

	if(f->flags & Sys_NEWPGRP)
		newgrp(p);

	if(f->flags & Sys_NODEVS)
		o->pgrp->nodevs = 1;

	poperror();

	if(f->flags & BlockingPctl)
		acquire();

	result = p->pid;
	memmove(f->ret, &result, sizeof(result));
}


/* Capture fixed operands and complete result slots before native and allocation callbacks. */
void
Sys_dirread(void *fp)
{
	Dir *entries;
	Sys_Dir *record;
	F_Sys_dirread local, *f;
	volatile struct {
		Heap *array; // Own only the partially constructed VM result during cleanup.
	} result;
	struct {
		WORD status; // Match the generated leading status field.
		Array *array; // Match the generated pointer-aligned array field.
	} layout;
	Array *old, *published;
	uchar *arrayslot;
	WORD status;
	int i, count, fd, valid;
	long received;

	sysstatusframe(fp, &local, sizeof(local), O(F_Sys_dirread, ret), sizeof(*local.ret));
	f = &local;
	valid = sysiofd(f->fd, &fd);
	arrayslot = (uchar*)f->ret+((uchar*)&layout.array-(uchar*)&layout);
	status = -1;
	old = sysresultowner(arrayslot);
	published = H;
	memmove(f->ret, &status, sizeof(status));
	memmove(arrayslot, &published, sizeof(published));
	destroy(old);
	if(!valid)
		return;
	release();
	entries = nil;
	received = kdirread(fd, &entries);
	acquire();
	if(received < -0x7fffffff-1 || received > 0x7fffffff) {
		seterror(Ebadarg);
		free(entries);
		return;
	}
	count = (int)received;
	if(count <= 0){
		status = count;
		memmove(f->ret, &status, sizeof(status));
		free(entries);
		return;
	}
	result.array = nil;
	if(waserror()){
		if(result.array != nil)
			destroy(H2D(Array*, result.array));
		free(entries);
		return;
	}
	result.array = heaparray(Tdir, count);
	record = (Sys_Dir*)H2D(Array*, result.array)->data;
	for(i = 0; i < count; i++)
		unpackdir(&entries[i], &record[i]);
	poperror();
	status = count;
	published = H2D(Array*, result.array);
	memmove(f->ret, &status, sizeof(status));
	memmove(arrayslot, &published, sizeof(published));
	free(entries);
}

void
Sys_fauth(void *fp)
{

	int fd;
	F_Sys_fauth *f;
	void *r;

	f = fp;
	r = *f->ret;
	*f->ret = H;
	destroy(r);
	release();
	fd = kfauth(fdchk(f->fd), string2c(f->aname));
	acquire();
	if(fd >= 0)
		*f->ret = mkfd(fd);
}

void
Sys_fversion(void *fp)
{
	void *r;
	F_Sys_fversion *f;
	int n;
	char buf[20], *s;

	f = fp;
	f->ret->t0 = -1;
	r = f->ret->t1;
	f->ret->t1 = H;
	destroy(r);
	s = string2c(f->version);
	n = strlen(s);
	if(n >= sizeof(buf)-1)
		n = sizeof(buf)-1;
	memmove(buf, s, n);
	buf[n] = 0;
	release();
	n = kfversion(fdchk(f->fd), f->msize, buf, sizeof(buf));
	acquire();
	if(n >= 0){
		f->ret->t0 = f->msize;
		retnstr(buf, n, &f->ret->t1);
	}
}


/* Capture fixed operands before callbacks and retain the original scalar result cell. */
void
Sys_iounit(void *fp)
{
	F_Sys_iounit local, *f;
	WORD result;
	int fd;

	sysstatusframe(fp, &local, sizeof(local), O(F_Sys_iounit, ret), sizeof(*local.ret));
	f = &local;
	if(!sysiofd(f->fd, &fd)) {
		result = -1;
		memmove(f->ret, &result, sizeof(result));
		return;
	}
	release();
	result = kiounit(fd);
	memmove(f->ret, &result, sizeof(result));
	acquire();
}

/* Cover queue allocation as well as blocking with channel-registration cleanup. */
void
ccom(Progq **cl, Prog *p)
{
	volatile struct {Progq **cl;} vcl;

	vcl.cl = cl;
	if(waserror()) {
		if(p->ptr != nil) {	/* no killcomm */
			cqdelp(vcl.cl, p);
			p->ptr = nil;
		}
		nexterror();
	}
	cqadd(cl, p);
	cblock(p);
	poperror();
}

void
crecv(Channel *c, void *ip)
{
	Prog *p;
	REG rsav;

	if(c->send->prog == nil && c->size == 0) {
		p = currun();
		p->ptr = ip;
		ccom(&c->recv, p);
		return;
	}

	rsav = R;
	R.s = &c;
	R.d = ip;
	irecv();
	R = rsav;
}

void
csend(Channel *c, void *ip)
{
 	Prog *p;
	REG rsav;

	if(c->recv->prog == nil && (c->buf == H || c->size == c->buf->len)) {
		p = currun();
		p->ptr = ip;
		ccom(&c->send, p);
		return;
	}

	rsav = R;
	R.s = ip;
	R.d = &c;
	isend();
	R = rsav;
}
