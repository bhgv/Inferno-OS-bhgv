#include "lib9.h"
#include "draw.h"
#include "kernel.h"
#include "drawinfo.h"
#include "interp.h"

#include "emu.h"


int	_drawdebug;

enum {
	CHECKLOCKING = 0
};


/* Retire an exclusively owned attach graph; native display defaults are borrowed aliases. */
static void
windowdiscard(Display *d, Image *window, Screen *screen)
{
	Image *backing;

	backing = screen == nil ? nil : screen->image;
	if(window != nil && window != d->image)
		freeimage(window);
	if(screen != nil)
		freescreen(screen);
	if(backing != nil && backing != window && backing != d->image &&
	   backing != d->white && backing != d->black &&
	   backing != d->opaque && backing != d->transparent)
		freeimage(backing);
}


/* Stage a complete fresh native graph before replacing a caller-owned pair; refusal preserves the old pair. */
int
gengetwindow(Display *d, char *winname, Image **winp, Screen **scrp, int ref)
{
	int n, fd;
	char buf[256+1], reason[ERRMAX];
	Image *backing, *window, *oldwindow;
	Screen *screen, *oldscreen;
	Rectangle rectangle;

	if(d == nil || winname == nil || winp == nil || scrp == nil ||
	   (void*)winp == (void*)scrp || d->closing){
		kwerrstr("getwindow: invalid attach arguments");
		return -1;
	}
	oldwindow = *winp;
	oldscreen = *scrp;
	if((oldwindow != nil && oldwindow->display != d) ||
	   (oldscreen != nil && oldscreen->display != d) ||
	   (oldwindow != nil && oldwindow != d->image && oldwindow->screen != oldscreen)){
		kwerrstr("getwindow: foreign attach graph");
		return -1;
	}
	backing = nil;
	window = nil;
	screen = nil;
	fd = libopen(winname, OREAD);
	n = -1;
	if(fd >= 0){
		n = libread(fd, buf, sizeof(buf)-1);
		libclose(fd);
	}
	if(n <= 0){
		if(d->image == nil || d->image->chan == 0){
			kwerrstr("getwindow: missing display image");
			return -1;
		}
		window = d->image;
	}else{
		if(n > sizeof(buf)-1){
			kwerrstr("getwindow: invalid window-name length");
			return -1;
		}
		buf[n] = '\0';
		backing = namedimage(d, buf);
		if(backing == nil)
			return -1;
		if(backing->chan == 0 || d->white == nil ||
		   (vlong)backing->r.max.x-backing->r.min.x <= 2*Borderwidth ||
		   (vlong)backing->r.max.y-backing->r.min.y <= 2*Borderwidth){
			kwerrstr("getwindow: invalid backing image");
			goto Refuse;
		}
		screen = allocscreen(backing, d->white, 0);
		if(screen == nil)
			goto Refuse;
		rectangle = insetrect(backing->r, Borderwidth);
		window = _allocwindow(nil, screen, rectangle, ref, DWhite);
		if(window == nil)
			goto Refuse;
	}
	*winp = window;
	*scrp = screen;
	windowdiscard(d, oldwindow, oldscreen);
	return 1;

Refuse:
	kgerrstr(reason, sizeof(reason));
	if(window != nil)
		freeimage(window);
	if(screen != nil)
		freescreen(screen);
	freeimage(backing);
	kwerrstr("%s", reason);
	return -1;
}

#define	NINFO	(12*12) // Bound the native display information response.


/* Stage native descriptors, channels and images; one refusal path retires every acquired owner. */
Display*
initdisplay(char *dev, char *win, void(*error)(Display*, char*))
{
	char buf[128], info[NINFO+1], reason[ERRMAX], *windowdir, *devdir;
	int datafd, ctlfd, reffd, directory, locked, received;
	Image parsed;
	Display *disp;
	Image *image;
	Dir *dir;
	void *q;

	fmtinstall('P', Pfmt);
	fmtinstall('R', Rfmt);
	if(dev == nil)
		dev = "/dev";
	if(win == nil)
		win = "/dev";
	if(strlen(dev)>sizeof(buf)-25 || strlen(win)>sizeof(buf)-25){
		kwerrstr("initdisplay: directory name too long");
		return nil;
	}
	windowdir = nil;
	devdir = nil;
	q = nil;
	disp = nil;
	image = nil;
	dir = nil;
	ctlfd = datafd = reffd = -1;
	locked = 0;
	windowdir = strdup(win);
	if(windowdir == nil)
		goto Refuse;
	q = libqlalloc();
	if(q == nil)
		goto Refuse;
	if(_drawdebug)
		print("initdisplay: prog=%p, proc=%p\n", (void*)currun(), (void*)up);
	sprint(buf, "%s/draw/new", dev);
	ctlfd = libopen(buf, ORDWR);
	if(ctlfd < 0){
		if(libbind("#i", dev, MBEFORE) < 0)
			goto Refuse;
		ctlfd = libopen(buf, ORDWR);
		if(ctlfd < 0)
			goto Refuse;
	}
	received = libread(ctlfd, info, sizeof(info));
	if(received != NINFO){
		if(received >= 0)
			kwerrstr("invalid display image information length");
		goto Refuse;
	}
	if(!drawimageinfo(info, &parsed, &directory) || parsed.chan == 0){
		kwerrstr("invalid display image information");
		goto Refuse;
	}
	sprint(buf, "%s/draw/%d/data", dev, directory);
	datafd = libopen(buf, ORDWR);
	if(datafd < 0)
		goto Refuse;
	sprint(buf, "%s/draw/%d/refresh", dev, directory);
	reffd = libopen(buf, OREAD);
	if(reffd < 0)
		goto Refuse;
	disp = malloc(sizeof(*disp));
	if(disp == nil)
		goto Refuse;
	memset(disp, 0, sizeof(*disp));
	image = malloc(sizeof(*image));
	if(image == nil)
		goto Refuse;
	memset(image, 0, sizeof(*image));
	devdir = strdup(dev);
	if(devdir == nil)
		goto Refuse;
	image->display = disp;
	image->id = 0;
	image->chan = parsed.chan;
	image->depth = parsed.depth;
	image->repl = parsed.repl;
	image->r = parsed.r;
	image->clipr = parsed.clipr;
	disp->dirno = directory;
	disp->image = image;
	disp->bufp = disp->buf;
	disp->bufsize = Displaybufsize;
	disp->error = error;
	disp->chan = image->chan;
	disp->depth = image->depth;
	disp->windir = windowdir;
	disp->devdir = devdir;
	disp->qlock = q;
	disp->datachan = libfdtochan(datafd, ORDWR);
	if(disp->datachan == nil)
		goto Refuse;
	disp->refchan = libfdtochan(reffd, OREAD);
	if(disp->refchan == nil)
		goto Refuse;
	disp->ctlchan = libfdtochan(ctlfd, ORDWR);
	if(disp->ctlchan == nil)
		goto Refuse;
	libqlock(q);
	locked = 1;
	disp->white = allocimage(disp, Rect(0,0,1,1), GREY1, 1, DWhite);
	if(disp->white == nil)
		goto Refuse;
	disp->black = allocimage(disp, Rect(0,0,1,1), GREY1, 1, DBlack);
	if(disp->black == nil)
		goto Refuse;
	disp->opaque = allocimage(disp, Rect(0,0,1,1), GREY1, 1, DWhite);
	if(disp->opaque == nil)
		goto Refuse;
	disp->transparent = allocimage(disp, Rect(0,0,1,1), GREY1, 1, DBlack);
	if(disp->transparent == nil)
		goto Refuse;
	dir = libdirfstat(ctlfd);
	if(dir != nil && dir->type == 'i'){
		disp->local = 1;
		disp->dataqid = dir->qid.path;
	}
	free(dir);
	libclose(reffd);
	libclose(datafd);
	libclose(ctlfd);
	if(CHECKLOCKING)
		disp->local = 0;
	/* The complete display adopts every staged owner and retains its initial exclusion. */
	return disp;

Refuse:
	kgerrstr(reason, sizeof(reason));
	if(disp != nil){
		freeimage(disp->white);
		freeimage(disp->black);
		freeimage(disp->opaque);
		freeimage(disp->transparent);
		if(locked)
			libqunlock(q);
		libchanclose(disp->datachan);
		libchanclose(disp->refchan);
		libchanclose(disp->ctlchan);
	}
	free(image);
	free(disp);
	if(reffd >= 0)
		libclose(reffd);
	if(datafd >= 0)
		libclose(datafd);
	if(ctlfd >= 0)
		libclose(ctlfd);
	if(q != nil)
		libqlfree(q);
	free(devdir);
	free(windowdir);
	kwerrstr("initdisplay: %s", reason[0] != 0 ? reason : "native initialization failed");
	return nil;
}


/* Discard private notifications under display exclusion without invoking their payloads. */
static void
discardrefresh(Display *disp)
{
	Refreshq *entry, *next;

	entry = disp->refhead;
	disp->refhead = disp->reftail = nil;
	while(entry != nil){
		next = entry->next;
		free(entry);
		entry = next;
	}
}


/* Caller owns an unlocked display and has excluded or stopped its other users.
 * Cancel notifications before resource teardown. Default fonts remain caller-owned.
 * This function does not itself join the refresh worker or grant a lifetime pin. */
void
closedisplay(Display *disp)
{
	int fd;
	char buf[128];

	if(disp == nil)
		return;
	libqlock(disp->qlock);
	disp->closing = 1;
	discardrefresh(disp);
	if(disp->oldlabel[0]){
		snprint(buf, sizeof buf, "%s/label", disp->windir);
		fd = libopen(buf, OWRITE);
		if(fd >= 0){
			libwrite(fd, disp->oldlabel, strlen(disp->oldlabel));
			libclose(fd);
		}
	}

	free(disp->devdir);
	free(disp->windir);
	freeimage(disp->white);
	freeimage(disp->black);
	freeimage(disp->opaque);
	freeimage(disp->transparent);
	free(disp->image);
	libchanclose(disp->datachan);
	libchanclose(disp->refchan);
	libchanclose(disp->ctlchan);
	/* Channel closure requests worker termination; caller must still exclude its users. */
	discardrefresh(disp);
	libqunlock(disp->qlock);
	libqlfree(disp->qlock);
	free(disp);
}

int
lockdisplay(Display *disp)
{
	if(disp->local)
		return 0;
	if(libqlowner(disp->qlock) != currun()){
		libqlock(disp->qlock);
		return 1;
	}
	return 0;
}

void
unlockdisplay(Display *disp)
{
	if(disp->local)
		return;
	libqunlock(disp->qlock);
}

/* use static buffer to avoid stack bloat */
int
_drawprint(int fd, char *fmt, ...)
{
	int n;
	va_list arg;
	char buf[128];
//	static QLock l;

//	qlock(&l);
	va_start(arg, fmt);
	vseprint(buf, buf+sizeof buf, fmt, arg);
	va_end(arg);
	n = libwrite(fd, buf, strlen(buf));
//	qunlock(&l);
	return n;
}

#ifdef YYY
void
drawerror(Display *d, char *s)
{
	char err[ERRMAX];

	if(d->error)
		d->error(d, s);
	else{
		err[0] = 0;
		errstr(err, sizeof err);
		_drawprint(2, "draw: %s: %s\n", s, err);
		exits(s);
	}
}

static
int
doflush(Display *d)
{
	int n;

	n = d->bufp-d->buf;
	if(n <= 0)
		return 1;

	if(kchanio(d->datachan, d->buf, n, OWRITE) != n){
		if(_drawdebug)
			_drawprint(2, "flushimage fail: d=%p: %r\n", d); /**/
		d->bufp = d->buf;	/* might as well; chance of continuing */
		return -1;
	}
	d->bufp = d->buf;
	return 1;
}

int
flushimage(Display *d, int visible)
{
	if(visible)
		*d->bufp++ = 'v';	/* one byte always reserved for this */
	return doflush(d);
}

uchar*
bufimage(Display *d, int n)
{
	uchar *p;

	if(n<0 || n>Displaybufsize){
		kwerrstr("bad count in bufimage");
		return 0;
	}
	if(d->bufp+n > d->buf+Displaybufsize)
		if(doflush(d) < 0)
			return 0;
	p = d->bufp;
	d->bufp += n;
	return p;
}

#endif
