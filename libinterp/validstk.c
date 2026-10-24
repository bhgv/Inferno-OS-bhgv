#include "lib9.h"
#include "isa.h"
#include "interp.h"

static int depth;

/* Read mapped native owner slots without imposing C pointer alignment. */
void
memchk(void *p, Type *t)
{
	Heap *h;
	int i, j;
	void *v;
	uchar *base;

	if(depth > 100)
		return;
	depth++;
	base = p;
	for(i = 0; i < t->np; i++) {
		for(j = 0; j < 8; j++) {
			if(t->map[i] & (1<<(7-j))) {
				memmove(&v, base+((ulong)i*8+j)*sizeof(v), sizeof(v));
				if(v != H) {
					h = D2H(v);
					hmsize(h);
					if(h->ref <= 0)
						abort();
					if(h->t != nil)
						memchk(v, h->t);
				}
			}
		}
	}
	depth--;
}

/* Follow frame metadata through byte copies, including extension root frames. */
void
validstk(void)
{
	Type *t;
	Frame *f;
	uchar *fp;

	fp = R.FP;
	while(fp != nil) {
		f = (Frame*)fp;
		memmove(&t, fp+O(Frame, t), sizeof(t));
		if(t == nil)
			t = SEXTYPE(f)->reg.TR;

		memchk(f, t);
		memmove(&fp, fp+O(Frame, fp), sizeof(fp));
	}
}
