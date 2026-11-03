#include "lib9.h"
#include "draw.h"
#include "kernel.h"

typedef struct Memimage Memimage;

static int	screenid;


/* Reserve only valid same-display screens, with bounded positive native IDs and complete refusal cleanup. */
Screen*
allocscreen(Image *image, Image *fill, int public)
{
	uchar *a;
	Screen *s;
	int id, try;
	Display *d;

	if(image == nil || fill == nil || image->display == nil)
		return nil;
	d = image->display;
	if(d != fill->display){
		kwerrstr("allocscreen: image and fill on different displays");
		return 0;
	}
	s = malloc(sizeof(Screen));
	if(s == 0)
		return 0;
	SET(id);
	for(try=0; try<25; try++){
		/* loop until find a free id */
		a = bufimage(d, 1+4+4+4+1);
		if(a == 0){
			free(s);
			return 0;
		}
		if(screenid >= 2147483647)
			screenid = 0;
		id = ++screenid;
		a[0] = 'A';
		BPLONG(a+1, id);
		BPLONG(a+5, image->id);
		BPLONG(a+9, fill->id);
		a[13] = public;
		if(flushimage(d, 0) != -1)
			break;
	}
	if(try == 25){
		free(s);
		return nil;
	}
	s->display = d;
	s->id = id;
	s->image = image;

	s->fill = fill;
	return s;
}

Screen*
publicscreen(Display *d, int id, ulong chan)
{
	uchar *a;
	Screen *s;

	s = malloc(sizeof(Screen));
	if(s == 0)
		return 0;
	a = bufimage(d, 1+4+4);
	if(a == 0){
    Error:
		free(s);
		return 0;
	}
	a[0] = 'S';
	BPLONG(a+1, id);
	BPLONG(a+5, chan);
	if(flushimage(d, 0) < 0)
		goto Error;

	s->display = d;
	s->id = id;
	s->image = 0;
	s->fill = 0;
	return s;
}

/* Consume the local Screen even if its best-effort server-side release cannot be sent. */
int
freescreen(Screen *s)
{
	uchar *a;
	Display *display;
	int result;

	if(s == nil)
		return 0;
	display = s->display;
	a = bufimage(display, 5);
	result = -1;
	if(a != nil){
		a[0] = 'F';
		BPLONG(a+1, s->id);
		if(flushimage(display, 1) >= 0)
			result = 1;
	}
	free(s);
	return result;
}


Image*
allocwindow(Screen *s, Rectangle r, int ref, ulong val)
{
	Image *i = _allocwindow(nil, s, r, ref, val);
	if(i)
		attachtowindow(i);
	return i;
}

Image*
_allocwindow(Image *i, Screen *s, Rectangle r, int ref, ulong val)
{
	Display *d;

	d = s->display;
	i = _allocimage(i, d, r, s->image->chan, 0, val, s->id, ref);
	if(i == 0)
		return 0;
	i->screen = s;
	i->next = s->display->windows;
	s->display->windows = i;
	return i;
}


/* Validate the complete native vector before reserving its one ordered window packet. */
static void
topbottom(Image **w, int n, int top)
{
	int i;
	uchar *b;
	Display *d;

	if(n < 0 || n > (Displaybufsize-100)/4 || (n != 0 && w == nil)){
		kwerrstr("top/bottom: invalid window vector");
		return;
	}
	if(n == 0)
		return;
	if(w[0] == nil || w[0]->display == nil)
		return;
	d = w[0]->display;
	for(i = 1; i < n; i++)
		if(w[i] == nil || w[i]->display != d){
			kwerrstr("top/bottom: windows not on same display");
			return;
		}
	b = bufimage(d, 1+1+2+4*n);
	if(b == nil)
		return;
	b[0] = 't';
	b[1] = top;
	BPSHORT(b+2, n);
	for(i = 0; i < n; i++)
		BPLONG(b+4+4*i, w[i]->id);
}


/* Send one window to the bottom without changing caller ownership. */
void
bottomwindow(Image *w)
{
	topbottom(&w, 1, 0);
}


/* Send one window to the top without changing caller ownership. */
void
topwindow(Image *w)
{
	topbottom(&w, 1, 1);
}


/* Preserve caller order in a multi-window bottom request. */
void
bottomnwindows(Image **w, int n)
{
	topbottom(w, n, 0);
}


/* Preserve caller order in a multi-window top request. */
void
topnwindows(Image **w, int n)
{
	topbottom(w, n, 1);
}


/* Translate into checked local rectangles before issuing any server command or mutating the image. */
static int
windoworiginrect(Rectangle source, vlong dx, vlong dy, Rectangle *result)
{
	vlong values[4];
	int i;

	values[0] = (vlong)source.min.x+dx;
	values[1] = (vlong)source.min.y+dy;
	values[2] = (vlong)source.max.x+dx;
	values[3] = (vlong)source.max.y+dy;
	for(i = 0; i < 4; i++)
		if(values[i] < (vlong)-2147483647-1 || values[i] > 2147483647)
			return 0;
	result->min.x = values[0];
	result->min.y = values[1];
	result->max.x = values[2];
	result->max.y = values[3];
	return 1;
}


/* Return failure for every transport refusal; publish checked local geometry only after the final flush. */
int
originwindow(Image *w, Point log, Point scr)
{
	uchar *b;
	vlong dx, dy;
	Rectangle r, clip;

	if(w == nil || w->display == nil)
		return -1;
	dx = (vlong)log.x-w->r.min.x;
	dy = (vlong)log.y-w->r.min.y;
	if(!windoworiginrect(w->r, dx, dy, &r) ||
	   !windoworiginrect(w->clipr, dx, dy, &clip)){
		kwerrstr("originwindow: coordinate overflow");
		return -1;
	}
	if(flushimage(w->display, 0) < 0)
		return -1;
	b = bufimage(w->display, 1+4+2*4+2*4);
	if(b == nil)
		return -1;
	b[0] = 'o';
	BPLONG(b+1, w->id);
	BPLONG(b+5, log.x);
	BPLONG(b+9, log.y);
	BPLONG(b+13, scr.x);
	BPLONG(b+17, scr.y);
	if(flushimage(w->display, 1) < 0)
		return -1;
	w->r = r;
	w->clipr = clip;
	return 1;
}
