#include "lib9.h"
#include "draw.h"
#include "kernel.h"
#include "interp.h"
#include "pixelspan.h"


/* Refuse impossible extents before output and require a complete read per request. */
int
unloadimage(Image *i, Rectangle r, uchar *data, int ndata)
{
	int bpl, total, ntot, dy, n, expected, received;
	uchar *a;
	Display *d;

	if(i == nil || i->display == nil ||
	   !drawpixelspan(r, i->depth, ndata, &bpl, &total)){
		kwerrstr("unloadimage: invalid rectangle or insufficient data");
		return -1;
	}
	if(total == 0)
		return 0;
	if(data == nil || !rectinrect(r, i->r)){
		kwerrstr("unloadimage: bad rectangle or nil data");
		return -1;
	}
	if(bpl > 8000){
		kwerrstr("unloadimage: image too wide");
		return -1;
	}
	d = i->display;
	if(flushimage(d, 0) < 0)
		return -1;
	ntot = 0;
	while(r.min.y < r.max.y){
		dy = 8000/bpl;
		if(dy > (vlong)r.max.y-r.min.y)
			dy = (vlong)r.max.y-r.min.y;
		expected = dy*bpl;
		a = bufimage(d, 21);
		if(a == nil){
			kwerrstr("unloadimage: %r");
			return -1;
		}
		a[0] = 'r';
		BPLONG(a+1, i->id);
		BPLONG(a+5, r.min.x);
		BPLONG(a+9, r.min.y);
		BPLONG(a+13, r.max.x);
		BPLONG(a+17, r.min.y+dy);
		if(flushimage(d, 0) < 0)
			return -1;
		received = 0;
		while(received < expected){
			if(d->local == 0)
				release();
			n = kchanio(d->datachan, data+ntot+received, expected-received, OREAD);
			if(d->local == 0)
				acquire();
			if(n < 0)
				return n;
			if(n == 0 || n > expected-received){
				kwerrstr("unloadimage: incomplete pixel response");
				return -1;
			}
			received += n;
		}
		ntot += received;
		r.min.y += dy;
	}
	return ntot;
}
