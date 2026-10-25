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

/* Clear all connection fields together, including an earlier directory string. */
static void
clearconnection(void *slot)
{
	Sys_Connection old, empty;
	char saved[ERRMAX];

	memmove(saved, up->env->errstr, sizeof(saved));
	memmove(&old, slot, sizeof(old));
	empty.dfd = empty.cfd = H;
	empty.dir = H;
	memmove(slot, &empty, sizeof(empty));
	destroy(old.dfd);
	destroy(old.cfd);
	destroy(old.dir);
	memmove(up->env->errstr, saved, sizeof(saved));
}

/* Build a complete connection before replacing any of the previous managed owners. */
static void
makeconnection(int datafd, int controlfd, char *directory, void *slot)
{
	volatile FDResult result;
	Sys_Connection old, replacement;

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
	memmove(&old, slot, sizeof(old));
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

/* Publish a complete scalar result without assuming VM return-slot alignment. */
void
Sys_millisec(void *fp)
{
	F_Sys_millisec *f;
	WORD result;

	f = fp;
	result = TK2MS(MACHP(0)->ticks);
	memmove(f->ret, &result, sizeof(result));
}

/* Clear an unaligned owner slot before finalization and publish only a constructed FD. */
void
Sys_open(void *fp)
{
	int fd;
	F_Sys_open *f;
	Sys_FD *old, *result;

	f = fp;
	memmove(&old, f->ret, sizeof(old));
	result = H;
	memmove(f->ret, &result, sizeof(result));
	destroy(old);
	release();
	fd = kopen(string2c(f->s), f->mode);
	acquire();
	if(fd == -1)
		return;

	result = mkfd(fd);
	memmove(f->ret, &result, sizeof(result));
}

/* Replace both pipe ends only after both descriptor owners have been constructed. */
void
Sys_pipe(void *fp)
{
	Array *a;
	int fd[2], status;
	WORD outcome;
	uchar *slots;
	Sys_FD *old[2], *next[2];
	F_Sys_pipe *f;
	volatile FDResult result;

	f = fp;
	outcome = -1;
	memmove(f->ret, &outcome, sizeof(outcome));
	a = f->fds;
	if(a == H || a == nil || a->len < 2 || a->data == nil || a->data == H ||
	   (uintptr)a->data > (uintptr)-1-2*sizeof(Sys_FD*))
		return;
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
	slots = a->data;
	memmove(old, slots, sizeof(old));
	next[0] = result.handle[0];
	next[1] = result.handle[1];
	memmove(slots, next, sizeof(next));
	destroy(old[0]);
	destroy(old[1]);
	outcome = 0;
	memmove(f->ret, &outcome, sizeof(outcome));
}

/* Reject high VM argument bits before a native int parameter can silently wrap. */
static int
sysnativeint(WORD value)
{
	return value >= -0x7fffffff-1 && value <= 0x7fffffff;
}

/* Clear an unaligned owner slot before finalization and publish only a constructed FD. */
void
Sys_fildes(void *fp)
{
	F_Sys_fildes *f;
	Sys_FD *old, *result;
	WORD source;
	int fd;

	f = fp;
	source = f->fd;
	memmove(&old, f->ret, sizeof(old));
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

/* Capture the native status before copying into a possibly unaligned VM return slot. */
void
Sys_dup(void *fp)
{
	F_Sys_dup *f;
	WORD result;

	f = fp;
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

/* Clear an unaligned owner slot before finalization and publish only a constructed FD. */
void
Sys_create(void *fp)
{
	int fd;
	F_Sys_create *f;
	Sys_FD *old, *result;

	f = fp;
	memmove(&old, f->ret, sizeof(old));
	result = H;
	memmove(f->ret, &result, sizeof(result));
	destroy(old);
	release();
	fd = kcreate(string2c(f->s), f->mode, f->perm);
	acquire();
	if(fd == -1)
		return;

	result = mkfd(fd);
	memmove(f->ret, &result, sizeof(result));
}

/* Capture the native status before copying into a possibly unaligned VM return slot. */
void
Sys_remove(void *fp)
{
	F_Sys_remove *f;
	WORD result;

	f = fp;
	release();
	result = kremove(string2c(f->s));
	memmove(f->ret, &result, sizeof(result));
	acquire();
}

/* Capture the native status before copying into a possibly unaligned VM return slot. */
void
Sys_seek(void *fp)
{
	F_Sys_seek *f;
	LONG result;

	f = fp;
	if(!sysnativeint(f->start)){
		seterror(Ebadarg);
		result = -1;
		memmove(f->ret, &result, sizeof(result));
		return;
	}
	release();
	result = kseek(fdchk(f->fd), f->off, f->start);
	memmove(f->ret, &result, sizeof(result));
	acquire();
}

/* Capture the native status before copying into a possibly unaligned VM return slot. */
void
Sys_unmount(void *fp)
{
	F_Sys_unmount *f;
	WORD result;

	f = fp;
	release();
	result = kunmount(string2c(f->s1), string2c(f->s2));
	memmove(f->ret, &result, sizeof(result));
	acquire();
}

/* Clamp full VM counts before narrowing and before releasing the interpreter. */
static int
sysiocount(Array *a, WORD count)
{
	if(a == nil || D2H(a)->t != &Tarray || a->t != &Tbyte ||
	   a->len < 0 || (uvlong)a->len > 0x7fffffff || (a->len != 0 && a->data == nil)){
		seterror(Ebadarg);
		return -1;
	}
	if(count > a->len)
		count = a->len;
	return (int)count;
}

/* Validate native I/O spans and copy results without assuming VM slot alignment. */
void
Sys_read(void *fp)
{
	int n;
	F_Sys_read *f;
	WORD result;

	f = fp;
	if(f->buf == (Array*)H || f->n < 0) {
		result = 0;
		memmove(f->ret, &result, sizeof(result));
		return;		
	}
	n = sysiocount(f->buf, f->n);
	if(n < 0){
		result = -1;
		memmove(f->ret, &result, sizeof(result));
		return;
	}

	release();
	result = kread(fdchk(f->fd), f->buf->data, n);
	memmove(f->ret, &result, sizeof(result));
	acquire();
}

/* Validate native I/O spans and copy results without assuming VM slot alignment. */
void
Sys_readn(void *fp)
{
	int fd, m, n, t;
	F_Sys_readn *f;
	WORD result;

	f = fp;
	if(f->buf == (Array*)H || f->n < 0) {
		result = 0;
		memmove(f->ret, &result, sizeof(result));
		return;		
	}
	n = sysiocount(f->buf, f->n);
	if(n < 0){
		result = -1;
		memmove(f->ret, &result, sizeof(result));
		return;
	}
	fd = fdchk(f->fd);

	release();
	for(t = 0; t < n; t += m){
		m = kread(fd, (char*)f->buf->data+t, n-t);
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

/* Validate native I/O spans and copy results without assuming VM slot alignment. */
void
Sys_pread(void *fp)
{
	int n;
	F_Sys_pread *f;
	WORD result;

	f = fp;
	if(f->buf == (Array*)H || f->n < 0) {
		result = 0;
		memmove(f->ret, &result, sizeof(result));
		return;		
	}
	n = sysiocount(f->buf, f->n);
	if(n < 0){
		result = -1;
		memmove(f->ret, &result, sizeof(result));
		return;
	}

	release();
	result = kpread(fdchk(f->fd), f->buf->data, n, f->off);
	memmove(f->ret, &result, sizeof(result));
	acquire();
}

/* Capture the native status before copying into a possibly unaligned VM return slot. */
void
Sys_chdir(void *fp)
{
	F_Sys_chdir *f;
	WORD result;

	f = fp;
	release();
	result = kchdir(string2c(f->path));
	memmove(f->ret, &result, sizeof(result));
	acquire();
}

/* Validate native I/O spans and copy results without assuming VM slot alignment. */
void
Sys_write(void *fp)
{
	int n;
	F_Sys_write *f;
	WORD result;

	f = fp;
	if(f->buf == (Array*)H || f->n < 0) {
		result = 0;
		memmove(f->ret, &result, sizeof(result));
		return;		
	}
	n = sysiocount(f->buf, f->n);
	if(n < 0){
		result = -1;
		memmove(f->ret, &result, sizeof(result));
		return;
	}

	release();
	result = kwrite(fdchk(f->fd), f->buf->data, n);
	memmove(f->ret, &result, sizeof(result));
	acquire();
}

/* Validate native I/O spans and copy results without assuming VM slot alignment. */
void
Sys_pwrite(void *fp)
{
	int n;
	F_Sys_pwrite *f;
	WORD result;

	f = fp;
	if(f->buf == (Array*)H || f->n < 0) {
		result = 0;
		memmove(f->ret, &result, sizeof(result));
		return;		
	}
	n = sysiocount(f->buf, f->n);
	if(n < 0){
		result = -1;
		memmove(f->ret, &result, sizeof(result));
		return;
	}

	release();
	result = kpwrite(fdchk(f->fd), f->buf->data, n, f->off);
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

static Dir*
packdir(Sys_Dir *sd)
{
	char *nm[4], *p;
	int i, n;
	Dir *d;

	nm[0] = string2c(sd->name);
	nm[1] = string2c(sd->uid);
	nm[2] = string2c(sd->gid);
	nm[3] = string2c(sd->muid);
	n = 0;
	for(i=0; i<4; i++)
		n += strlen(nm[i])+1;
	d = smalloc(sizeof(*d)+n);
	p = (char*)d+sizeof(*d);
	for(i=0; i<4; i++){
		n = strlen(nm[i])+1;
		memmove(p, nm[i], n);
		nm[i] = p;
		p += n;
	}
	d->name = nm[0];
	d->uid = nm[1];
	d->gid = nm[2];
	d->muid = nm[3];
	d->qid.path = sd->qid.path;
	d->qid.vers = sd->qid.vers;
	d->qid.type = sd->qid.qtype;
	d->mode = sd->mode;
	d->atime = sd->atime;
	d->mtime = sd->mtime;
	d->length = sd->length;
	d->type = sd->dtype;
	d->dev = sd->dev;
	return d;
}

/* Keep partial directory ownership valid while publishing unaligned status and fields. */
void
Sys_fstat(void *fp)
{
	Dir *d;
	F_Sys_fstat *f;
	F_Sys_fstat layout;
	union { uvlong align; uchar bytes[sizeof(*f->ret)]; } scratch;
	uchar *recordslot;
	WORD status;
	int fd;

	f = fp;
	fd = fdchk(f->fd);
	/* Generated return tuples can contain explicit padding beyond native alignment. */
	layout.ret = (void*)scratch.bytes;
	recordslot = (uchar*)f->ret+((uchar*)&layout.ret->t1-scratch.bytes);
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

/* Keep partial directory ownership valid while publishing unaligned status and fields. */
void
Sys_stat(void *fp)
{
	Dir *d;
	F_Sys_stat *f;
	F_Sys_stat layout;
	union { uvlong align; uchar bytes[sizeof(*f->ret)]; } scratch;
	uchar *recordslot;
	WORD status;
	String *path;

	f = fp;
	path = f->s;
	/* Generated return tuples can contain explicit padding beyond native alignment. */
	layout.ret = (void*)scratch.bytes;
	recordslot = (uchar*)f->ret+((uchar*)&layout.ret->t1-scratch.bytes);
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

/* Clear the string owner before finalization and retain the native path until conversion ends. */
void
Sys_fd2path(void *fp)
{
	F_Sys_fd2path *f;
	char *s;
	String *old, *empty;
	int fd;

	f = fp;
	fd = fdchk(f->fd);
	memmove(&old, f->ret, sizeof(old));
	empty = H;
	memmove(f->ret, &empty, sizeof(empty));
	destroy(old);
	release();
	s = kfd2path(fd);
	acquire();
	if(waserror() == 0){
		retstr(s, f->ret);
		poperror();
	}
	free(s);
}

/* Capture the native status before copying into a possibly unaligned VM return slot. */
void
Sys_mount(void *fp)
{
	F_Sys_mount *f;
	WORD result;

	f = fp;
	if(!sysnativeint(f->flags)){
		seterror(Ebadarg);
		result = -1;
		memmove(f->ret, &result, sizeof(result));
		return;
	}
	release();
	result = kmount(fdchk(f->fd), fdchk(f->afd), string2c(f->on), f->flags, string2c(f->spec));
	memmove(f->ret, &result, sizeof(result));
	acquire();
}

/* Capture the native status before copying into a possibly unaligned VM return slot. */
void
Sys_bind(void *fp)
{
	F_Sys_bind *f;
	WORD result;

	f = fp;
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

/* Publish a complete scalar result without assuming VM return-slot alignment. */
void
Sys_wstat(void *fp)
{
	Dir *d;
	F_Sys_wstat *f;
	WORD result;

	f = fp;
	d = packdir(&f->d);
	release();
	result = kdirwstat(string2c(f->s), d);
	memmove(f->ret, &result, sizeof(result));
	acquire();
	free(d);
}

/* Publish a complete scalar result without assuming VM return-slot alignment. */
void
Sys_fwstat(void *fp)
{
	Dir *d;
	F_Sys_fwstat *f;
	WORD result;

	f = fp;
	d = packdir(&f->d);
	release();
	result = kdirfwstat(fdchk(f->fd), d);
	memmove(f->ret, &result, sizeof(result));
	acquire();
	free(d);
}

/* Stage the scalar status before publishing to an unaligned VM result slot. */
void
Sys_print(void *fp)
{
	int n;
	Prog *p;
	Chan *c;
	char buf[1024], *b = buf;
	F_Sys_print *f;
	WORD result;

	f = fp;
	c = up->env->fgrp->fd[1];
	if(c == nil){
		result = -1;
		memmove(f->ret, &result, sizeof(result));
		return;
	}
	p = currun();

	release();
	n = xprint(p, f, &f->vargs, f->s, buf, sizeof(buf));
	if (n >= sizeof(buf)-UTFmax-2)
		n = bigxprint(p, f, &f->vargs, f->s, &b, sizeof(buf));
	result = kwrite(1, b, n);
	memmove(f->ret, &result, sizeof(result));
	if (b != buf)
		free(b);
	acquire();
}

/* Stage the scalar status before publishing to an unaligned VM result slot. */
void
Sys_fprint(void *fp)
{
	int n;
	Prog *p;
	char buf[1024], *b = buf;
	F_Sys_fprint *f;
	WORD result;

	f = fp;
	p = currun();
	release();
	n = xprint(p, f, &f->vargs, f->s, buf, sizeof(buf));
	if (n >= sizeof(buf)-UTFmax-2)
		n = bigxprint(p, f, &f->vargs, f->s, &b, sizeof(buf));
	result = kwrite(fdchk(f->fd), b, n);
	memmove(f->ret, &result, sizeof(result));
	if (b != buf)
		free(b);
	acquire();
}

/* Stage the scalar status before publishing to an unaligned VM result slot. */
void
Sys_werrstr(void *fp)
{
	F_Sys_werrstr *f;
	WORD result;

	f = fp;
	kstrcpy(up->env->errstr, string2c(f->s), ERRMAX);
	result = 0;
	memmove(f->ret, &result, sizeof(result));
}

/* Adopt both dial descriptors as one result, closing them on conversion failure. */
void
Sys_dial(void *fp)
{
	int datafd, controlfd;
	char dir[NETPATHLEN], *address, *local;
	F_Sys_dial *f;
	WORD status;

	f = fp;
	address = string2c(f->addr);
	local = string2c(f->local);
	release();
	datafd = kdial(address, local, dir, &controlfd);
	acquire();
	status = -1;
	memmove(f->ret, &status, sizeof(status));
	if(datafd < 0){
		clearconnection(connectionresultslot(f->ret));
		return;
	}
	makeconnection(datafd, controlfd, dir, connectionresultslot(f->ret));
	status = 0;
	memmove(f->ret, &status, sizeof(status));
}

/* Adopt the announce control descriptor only after the complete result is ready. */
void
Sys_announce(void *fp)
{
	char dir[NETPATHLEN], *address;
	int fd;
	F_Sys_announce *f;
	WORD status;

	f = fp;
	address = string2c(f->addr);
	release();
	fd = kannounce(address, dir);
	acquire();
	status = -1;
	memmove(f->ret, &status, sizeof(status));
	if(fd < 0){
		clearconnection(connectionresultslot(f->ret));
		return;
	}
	makeconnection(-1, fd, dir, connectionresultslot(f->ret));
	status = 0;
	memmove(f->ret, &status, sizeof(status));
}

/* Keep earlier connection owners valid until the new listening result is complete. */
void
Sys_listen(void *fp)
{
	F_Sys_listen *f;
	WORD status;
	char dir[NETPATHLEN], *directory;
	int fd;

	f = fp;
	directory = string2c(f->c.dir);
	release();
	fd = klisten(directory, dir);
	acquire();
	status = -1;
	memmove(f->ret, &status, sizeof(status));
	if(fd < 0){
		clearconnection(connectionresultslot(f->ret));
		return;
	}
	makeconnection(-1, fd, dir, connectionresultslot(f->ret));
	status = 0;
	memmove(f->ret, &status, sizeof(status));
}

/* Define the no-sleep result and copy it only after regaining the interpreter lock. */
void
Sys_sleep(void *fp)
{
	F_Sys_sleep *f;
	WORD result = 0;

	f = fp;
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
	int done, wrote;

	for(done = 0; done < n; done += wrote){
		wrote = kwrite(fd, buf+done, n-done);
		if(wrote <= 0)
			return -1;
		*total += (UWORD)wrote;
	}
	return 0;
}

/* Preserve stream progress and copy the VM-width result into any slot alignment. */
void
Sys_stream(void *fp)
{
	Prog *p;
	uchar *buf;
	int src, dst, size, t, n;
	UWORD nbytes;
	F_Sys_stream *f;
	WORD result;

	f = fp;
	if(f->bufsiz <= 0 || (uvlong)f->bufsiz > 0x7fffffff){
		seterror(Ebadarg);
		result = -1;
		memmove(f->ret, &result, sizeof(result));
		return;
	}
	size = (int)f->bufsiz;
	buf = malloc(size);
	if(buf == nil){
		kwerrstr(Enomem);
		result = -1;
		memmove(f->ret, &result, sizeof(result));
		return;
	}
	src = fdchk(f->src);
	dst = fdchk(f->dst);
	p = currun();

	release();
	t = 0;
	nbytes = 0;
	while(p->kill == nil){
		n = kread(src, buf+t, size-t);
		if(n <= 0)
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

/* Capture the native export status before publishing an unaligned VM result. */
void
Sys_export(void *fp)
{
	F_Sys_export *f;
	WORD result;

	f = fp;
	release();
	result = export(fdchk(f->c), string2c(f->dir), f->flag&Sys_EXPASYNC);
	memmove(f->ret, &result, sizeof(result));
	acquire();
}

/* Build both channels before registration, releasing partial results on every error. */
void
Sys_file2chan(void *fp)
{
	F_Sys_file2chan *f;
	volatile struct { Sys_FileIO *file; int released; } result;
	Sys_FileIO *old, *published;
	char *directory, *name, saved[ERRMAX];
	int status;

	f = fp;
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
	memmove(&old, f->ret, sizeof(old));
	published = result.file;
	memmove(f->ret, &published, sizeof(published));
	destroy(old);
	if(status < 0)
		memmove(up->env->errstr, saved, sizeof(saved));
}

enum
{
	/* the following pctl calls can block and must release the virtual machine */
	BlockingPctl=	Sys_NEWFD|Sys_FORKFD|Sys_NEWNS|Sys_FORKNS|Sys_NEWENV|Sys_FORKENV
};

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
		*f->ret = -1;
		return;
	}

	o = p->osenv;
	if(f->flags & Sys_NEWFD) {
		ofg = o->fgrp;
		nfg = newfgrp(ofg);
		lock(ofg);
		/* file descriptors to preserve */
		for(l = f->movefd; l != H; l = l->tail) {
			fd = *(int*)l->data;
			if(fd >= 0 && fd <= ofg->maxfd) {
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
		/* file descriptors to close */
		for(l = f->movefd; l != H; l = l->tail)
			kclose(*(int*)l->data);
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

	*f->ret = p->pid;
}

/* Retain ownership of native entries and publish tuple fields without alignment assumptions. */
void
Sys_dirread(void *fp)
{
	Dir *entries;
	Sys_Dir *record;
	F_Sys_dirread *f;
	volatile struct { Heap *array; } result;
	struct { WORD status; Array *array; } layout;
	Array *old, *published;
	uchar *arrayslot;
	WORD status;
	int i, count, fd;

	f = fp;
	fd = fdchk(f->fd);
	arrayslot = (uchar*)f->ret+((uchar*)&layout.array-(uchar*)&layout);
	status = -1;
	memmove(&old, arrayslot, sizeof(old));
	published = H;
	memmove(f->ret, &status, sizeof(status));
	memmove(arrayslot, &published, sizeof(published));
	destroy(old);
	release();
	count = kdirread(fd, &entries);
	acquire();
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

/* Capture the native status before copying into a possibly unaligned VM return slot. */
void
Sys_iounit(void *fp)
{
	F_Sys_iounit *f;
	WORD result;

	f = fp;
	release();
	result = kiounit(fdchk(f->fd));
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
