#include "lib9.h"
#include "draw.h"
#include "kernel.h"
#include "drawinfo.h"


/* Refuse invalid image handles and failed pending output before requesting attachment. */
void
attachtowindow(Image *image)
{
	uchar *command;
	Display *display;

	if(image == nil || image->display == nil || image->display->closing){
		kwerrstr("attachtowindow: invalid image descriptor");
		return;
	}
	display = image->display;
	if(flushimage(display, 0) < 0)
		return;
	command = bufimage(display, 1+4);
	if(command == nil)
		return;
	command[0] = 'W';
	BPLONG(command+1, image->id);
	flushimage(display, 0);
}


Image*
allocimage(Display *d, Rectangle r, ulong chan, int repl, ulong val)
{
	return _allocimage(nil, d, r, chan, repl, val, 0, 0);
}


/* Initialize fresh callback state while preserving the callback identity of an explicitly reused header. */
Image*
_allocimage(Image *ai, Display *d, Rectangle r, ulong chan, int repl, ulong val, int screenid, int refresh)
{
	uchar *a;
	char *err;
	Image *i;
	Rectangle clipr;
	int id;
	int depth;

	err = 0;
	i = 0;

	if(d == nil || d->closing || d->imageid < 0 || d->imageid == 2147483647){
		kwerrstr("allocimage: unavailable image ID");
		return nil;
	}
	if(chan == 0){
		kwerrstr("bad channel descriptor");
		return nil;
	}

	depth = chantodepth(chan);
	if(depth == 0){
		err = "bad channel descriptor";
    Error:
		if(err)
			kwerrstr("allocimage: %s", err);
		else
			kwerrstr("allocimage: %r");
		free(i);
		return 0;
	}

	/* Refuse before issuing another request if pending output failed. */
	if(flushimage(d, 0) < 0)
		goto Error;
	a = bufimage(d, 1+4+4+1+4+1+4*4+4*4+4);
	if(a == 0)
		goto Error;
	d->imageid++;
	id = d->imageid;
	a[0] = 'b';
	BPLONG(a+1, id);
	BPLONG(a+5, screenid);
	a[9] = refresh;
	BPLONG(a+10, chan);
	a[14] = repl;
	BPLONG(a+15, r.min.x);
	BPLONG(a+19, r.min.y);
	BPLONG(a+23, r.max.x);
	BPLONG(a+27, r.max.y);
	if(repl)
		/* huge but not infinite, so various offsets will leave it huge, not overflow */
		clipr = Rect(-0x3FFFFFFF, -0x3FFFFFFF, 0x3FFFFFFF, 0x3FFFFFFF);
	else
		clipr = r;
	BPLONG(a+31, clipr.min.x);
	BPLONG(a+35, clipr.min.y);
	BPLONG(a+39, clipr.max.x);
	BPLONG(a+43, clipr.max.y);
	BPLONG(a+47, val);
	if(flushimage(d, 0) < 0)
		goto Error;

	if(ai)
		i = ai;
	else{
		i = malloc(sizeof(Image));
		if(i == nil){
			a = bufimage(d, 1+4);
			if(a){
				a[0] = 'f';
				BPLONG(a+1, id);
				flushimage(d, 0);
			}
			goto Error;
		}
	}
	if(ai == nil){
		i->reffn = nil;
		i->refptr = nil;
	}
	i->display = d;
	i->id = id;
	i->depth = depth;
	i->chan = chan;
	i->r = r;
	i->clipr = clipr;
	i->repl = repl;
	i->screen = 0;
	i->next = 0;
	i->fpath = nil;
	return i;
}


/* A newly named native image starts with no path or refresh callback owner. */
Image*
namedimage(Display *d, char *name)
{
	uchar *a;
	char *err, buf[12*12+1], reason[ERRMAX];
	Image *i;
	int id, n;
	uvlong length;

	err = 0;
	i = 0;

	if(d == nil || d->closing || d->imageid < 0 || d->imageid == 2147483647 || name == nil){
		kwerrstr("namedimage: invalid display, image ID or name");
		return nil;
	}
	length = strlen(name);
	if(length >= 256){
		err = "name too long";
    Error:
		if(err)
			kwerrstr("namedimage: %s", err);
		else{
			kgerrstr(reason, sizeof(reason));
			kwerrstr("namedimage: %s", reason);
		}
		if(i)
			free(i);
		return 0;
	}
	n = length;
	/* Refuse before issuing another request if pending output failed. */
	if(flushimage(d, 0) < 0)
		goto Error;
	a = bufimage(d, 1+4+1+n);
	if(a == 0)
		goto Error;
	d->imageid++;
	id = d->imageid;
	a[0] = 'n';
	BPLONG(a+1, id);
	a[5] = n;
	memmove(a+6, name, n);
	if(flushimage(d, 0) < 0)
		goto Error;

	if(kchanio(d->ctlchan, buf, sizeof buf, OREAD) != 12*12){
		kwerrstr("invalid image information length");
		goto Error1;
	}
	buf[12*12] = '\0';

	i = malloc(sizeof(Image));
	if(i == nil){
	Error1:
		kgerrstr(reason, sizeof(reason));
		a = bufimage(d, 1+4);
		if(a){
			a[0] = 'f';
			BPLONG(a+1, id);
			flushimage(d, 0);
		}
		kwerrstr("%s", reason);
		goto Error;
	}
	i->display = d;
	i->id = id;
	if(!drawimageinfo(buf, i, nil)){
		kwerrstr("invalid image information fields");
		goto Error1;
	}
	i->reffn = nil;
	i->refptr = nil;
	i->fpath = nil;
	i->screen = 0;
	i->next = 0;
	return i;
}


/* Keep the one-byte name length exact; refusal must not reserve or send a truncated packet. */
int
nameimage(Image *image, char *name, int in)
{
	uchar *command;
	uvlong length;
	int count;

	if(image == nil || image->display == nil || image->display->closing || name == nil){
		kwerrstr("nameimage: invalid image descriptor or name");
		return 0;
	}
	length = strlen(name);
	if(length >= 256){
		kwerrstr("nameimage: name too long");
		return 0;
	}
	count = length;
	command = bufimage(image->display, 1+4+1+1+count);
	if(command == nil)
		return 0;
	command[0] = 'N';
	BPLONG(command+1, image->id);
	command[5] = in != 0;
	command[6] = count;
	memmove(command+7, name, count);
	if(flushimage(image->display, 0) < 0)
		return 0;
	return 1;
}


/* Unlink a retiring window before any blocking or refused I/O can leave a stale Display.windows edge. */
int
_freeimage1(Image *i)
{
	uchar *a;
	Display *d;
	Image *w;

	if(i == 0)
		return 0;
	/* make sure no refresh events occur on this if we block in the write */
	d = i->display;
	if(i->screen){
		w = d->windows;
		if(w == i)
			d->windows = i->next;
		else
			while(w){
				if(w->next == i){
					w->next = i->next;
					break;
				}
				w = w->next;
			}
	}
	i->next = nil;
	/* flush pending data so we don't get error deleting the image */
	flushimage(d, 0);
	a = bufimage(d, 1+4);
	if(a == 0)
		return -1;
	a[0] = 'f';
	BPLONG(a+1, i->id);
	if(flushimage(d, i->screen!=0) < 0)
		return -1;

	return 0;
}

int
freeimage(Image *i)
{
	int ret;

	ret = _freeimage1(i);
	if(i && i->fpath)
		free(i->fpath);
	free(i);
	return ret;
}
