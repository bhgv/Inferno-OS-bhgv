#include "lib9.h"
#include "draw.h"
#include "kernel.h"
#include "pixelspan.h"


/* Validate complete source storage before clipping, pointer arithmetic or output. */
int
loadimage(Image *i, Rectangle r, uchar *data, int ndata)
{
	Rectangle dstr;
	vlong offset;
	int bpl, total, chunk, llen, clipped, dy, n, written, y;
	uchar *a;

	if(i == nil || i->display == nil ||
	   !drawpixelspan(r, i->depth, ndata, &bpl, &total)){
		kwerrstr("loadimage: invalid rectangle or insufficient data");
		return -1;
	}
	if(total == 0)
		return 0;
	if(data == nil){
		kwerrstr("loadimage: nil data");
		return -1;
	}
	dstr = r;
	if(!rectclip(&dstr, i->r) || !rectclip(&dstr, i->clipr))
		return 0;
	if(!drawpixelspan(dstr, i->depth, ndata, &llen, &clipped)){
		kwerrstr("loadimage: invalid clipped rectangle");
		return -1;
	}
	offset = ((vlong)dstr.min.y-r.min.y)*bpl+
		drawpixelbyte(dstr.min.x, i->depth)-drawpixelbyte(r.min.x, i->depth);
	if(offset < 0 || offset > total || llen > total-offset){
		kwerrstr("loadimage: invalid clipped offset");
		return -1;
	}
	if(i->display->bufsize <= 64 || llen > i->display->bufsize-64){
		kwerrstr("loadimage: image too wide for buffer");
		return -1;
	}
	chunk = i->display->bufsize-64;
	data += offset;
	written = 0;
	while(dstr.max.y > dstr.min.y){
		dy = (vlong)dstr.max.y-dstr.min.y;
		if(dy > chunk/llen)
			dy = chunk/llen;
		n = dy*llen;
		a = bufimage(i->display, 21+n);
		if(a == nil){
			kwerrstr("bufimage failed");
			return -1;
		}
		a[0] = 'y';
		BPLONG(a+1, i->id);
		BPLONG(a+5, dstr.min.x);
		BPLONG(a+9, dstr.min.y);
		BPLONG(a+13, dstr.max.x);
		BPLONG(a+17, dstr.min.y+dy);
		a += 21;
		for(y=0; y<dy; y++){
			memmove(a, data, llen);
			a += llen;
			written += llen;
			/* The last source row needs no pointer to a nonexistent next row. */
			if((vlong)dstr.min.y+y+1 < dstr.max.y)
				data += bpl;
		}
		dstr.min.y += dy;
	}
	if(flushimage(i->display, 0) < 0)
		return -1;
	return written;
}
