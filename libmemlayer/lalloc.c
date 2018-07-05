#include "lib9.h"
#include "draw.h"
#include "memdraw.h"
#include "memlayer.h"


#ifdef EXT_WIN
extern void* attach_clutter_actor(char* buffer, int min_x, int min_y, int max_x, int max_y);
#endif


Memimage*
memlalloc(Memscreen *s, Rectangle screenr, Refreshfn refreshfn, void *refreshptr, ulong val)
{
	Memlayer *l;
	Memimage *n;
	static Memimage *paint;

	if(paint == nil){
		paint = allocmemimage(Rect(0,0,1,1), RGBA32);
		if(paint == nil)
			return nil;
		paint->flags |= Frepl;
		paint->clipr = Rect(-0x3FFFFFF, -0x3FFFFFF, 0x3FFFFFF, 0x3FFFFFF);
	}

#ifndef EXT_WIN
	n = allocmemimaged(screenr, s->image->chan, s->image->data);
#else
	n = allocmemimage(screenr, s->image->chan);
	if(n && n->data && n->data->bdata /*&& !n->ext_win*/){
		n->ext_win = attach_clutter_actor(n->data->bdata, 
										n->r.min.x, n->r.min.y, 
										n->r.max.x, n->r.max.y);
	}else{
		n->ext_win = NULL;
	}
print("clutter new window chan=%d\n", chantodepth(s->image->chan) );
#endif
	if(n == nil)
		return nil;
	l = malloc(sizeof(Memlayer));
	if(l == nil){
		free(n);
		return nil;
	}

	l->screen = s;
	if(refreshfn)
		l->save = nil;
	else{
		l->save = allocmemimage(screenr, s->image->chan);
		if(l->save == nil){
			free(l);
			free(n);
			return nil;
		}
		/* allocmemimage doesn't initialize memory; this paints save area */
		if(val != DNofill)
			memfillcolor(l->save, val);
	}
	l->refreshfn = refreshfn;
	l->refreshptr = nil;	/* don't set it until we're done */
	l->screenr = screenr;
	l->delta = Pt(0,0);

	n->data->ref++;
	n->zero = s->image->zero;
	n->width = s->image->width;
	n->layer = l;

	/* start with new window behind all existing ones */
	l->front = s->rearmost;
	l->rear = nil;
	if(s->rearmost)
		s->rearmost->layer->rear = n;
	s->rearmost = n;
	if(s->frontmost == nil)
		s->frontmost = n;
	l->clear = 0;

	/* now pull new window to front */
	_memltofrontfill(n, val != DNofill);
	l->refreshptr = refreshptr;

	/*
	 * paint with requested color; previously exposed areas are already right
	 * if this window has backing store, but just painting the whole thing is simplest.
	 */
	if(val != DNofill){
		memsetchan(paint, n->chan);
		memfillcolor(paint, val);
		memdraw(n, n->r, paint, n->r.min, nil, n->r.min, S);
	}
	return n;
}
