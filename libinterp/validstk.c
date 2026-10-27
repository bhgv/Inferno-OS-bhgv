#include "lib9.h"
#include "isa.h"
#include "interp.h"

static int depth;


/* Read packed descriptor fields and mapped native slots without C alignment assumptions. */
void
memchk(void *p, Type *t)
{
	Heap *h;
	int i, j, pointers;
	void *v;
	uchar *base, *map;

	if(depth > 100)
		return;
	depth++;
	base = p;
	memmove(&pointers, (uchar*)t+O(Type, np), sizeof(pointers));
	map = (uchar*)t+O(Type, map);
	for(i = 0; i < pointers; i++) {
		for(j = 0; j < 8; j++) {
			if(map[i] & (1<<(7-j))) {
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
