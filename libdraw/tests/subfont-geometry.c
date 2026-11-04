#include "lib9.h"
#include "draw.h"
#include <stdio.h>
#include <limits.h>

static int checks, calls; // Active predicates and bounded draw calls.
static Rectangle observed; // Last complete rectangle passed to the native boundary.
static Point maskpoint; // Corresponding source-mask origin.


/* Stop on semantic errors without relying on assertions or debug build flags. */
static void
check(int ok, char *stage)
{
	if(!ok){
		fprintf(stderr, "Subfont geometry mismatch: %s\n", stage);
		exit(10);
	}
	checks++;
}


/* Observe drawing arguments without claiming renderer or guest-heap coverage. */
void
draw(Image *dst, Rectangle r, Image *src, Image *mask, Point p)
{
	USED(dst); USED(src); USED(mask);
	calls++;
	observed = r;
	maskpoint = p;
}

IMPLEMENTATION


/* Compare real glyph placement against wide independent coordinate arithmetic. */
int
main(void)
{
	Fontchar info[257];
	Subfont f;
	Image image;
	Point p, q;
	vlong left, right, top, bottom, next;
	int x, y, bearing, width, accepted, before;
	char *large;
	size_t count;
	static int edges[] = {INT_MIN, INT_MIN+127, -1, 0, 1, INT_MAX-255, INT_MAX-15, INT_MAX}; // Int32 placement boundaries.

	memset(info, 0, sizeof info);
	memset(&f, 0, sizeof f);
	memset(&image, 0, sizeof image);
	f.n = 256; f.height = 15; f.info = info; f.bits = &image;
	info['A'].x = 100; info['A'+1].x = 110;
	info['A'].top = 2; info['A'].bottom = 14;
	for(x = 0; x < nelem(edges); x++)
	for(y = 0; y < nelem(edges); y++)
	for(bearing = -128; bearing <= 127; bearing++)
	for(width = 0; width <= 255; width += 85){
		info['A'].left = bearing; info['A'].width = width;
		p = Pt(edges[x], edges[y]);
		left = (vlong)p.x+bearing; right = left+10;
		top = (vlong)p.y+2; bottom = (vlong)p.y+14;
		next = (vlong)p.x+width;
		accepted = left >= INT_MIN && right <= INT_MAX && top <= INT_MAX && bottom <= INT_MAX && next <= INT_MAX;
		before = calls;
		q = stringsubfont(&image, p, &image, &f, "A");
		check(calls-before == accepted, "placement admission");
		check(q.x == (accepted ? next : p.x) && q.y == p.y, "pen position");
		if(accepted){
			check(observed.min.x == left && observed.max.x == right && observed.min.y == top && observed.max.y == bottom, "wide rectangle");
			check(maskpoint.x == 100 && maskpoint.y == 2, "mask origin");
		}
	}
	info['A'].left = -2; info['A'].width = 7;
	info[233].x = 120; info[234].x = 130;
	info[233].top = 1; info[233].bottom = 14; info[233].width = 5;
	q = stringsubfont(&image, Pt(5, 8), &image, &f, "A\xc3\xa9" "A");
	check(q.x == 24 && q.y == 8, "UTF placement");
	q = strsubfontwidth(&f, "A\xc3\xa9" "A");
	check(q.x == 19 && q.y == 15, "UTF width");
	check(strsubfontwidth(&f, "A\xc4\x80" "A").x == 14, "outside subfont");
	check(strsubfontwidth(&f, "A\xc3").x == 7, "truncated UTF");
	before = calls;
	q = stringsubfont(&image, Pt(INT_MAX-12, 0), &image, &f, "AA");
	check(q.x == INT_MAX-5 && calls == before+1, "complete prefix");
	before = calls;
	info['A'].width = 255;
	q = stringsubfont(&image, Pt(INT_MAX-20, 0), &image, &f, "A");
	check(q.x == INT_MAX-20 && calls == before, "pen-only overflow");
	info['A'].width = 7;
	info['A'+1].x = 99;
	before = calls;
	q = stringsubfont(&image, Pt(5, 8), &image, &f, "A");
	check(q.x == 5 && calls == before, "metric refusal");
	check(strsubfontwidth(&f, "A").x == 0, "metric width");
	info['A'+1].x = 110; info['A'].top = 15;
	check(strsubfontwidth(&f, "A").x == 0, "vertical metrics");
	info['A'].top = 2;
	check(strsubfontwidth(nil, "A").x == 0, "nil font");
	check(strsubfontwidth(&f, nil).x == 0, "nil text");
	check(stringsubfont(nil, Pt(4, 5), &image, &f, "A").x == 4, "nil image");
	info['A'].width = 255;
	count = (size_t)INT_MAX/255+2;
	large = malloc(count+1);
	check(large != nil, "fixture allocation");
	memset(large, 'A', count); large[count] = 0;
	q = strsubfontwidth(&f, large);
	free(large);
	check(q.x == (INT_MAX/255)*255, "width boundary");
	printf("PASS subfont geometry: %d active checks\n", checks);
	return 0;
}
