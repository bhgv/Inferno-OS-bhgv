#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "draw.h"
#include "runt.h"
#include "raise.h"
#include <assert.h>
#include <stdio.h>
#include <setjmp.h>

static jmp_buf boundary;
static int expects, checks;
static char *operation;
char exZdiv[] = "zero divide";

/* Geometry must deliver the existing VM exception before changing the result. */
void
error(char *message)
{
	assert(expects && message == exZdiv);
	longjmp(boundary, 1);
}

/* No fixture allocation needs process inspection by LeakSanitizer. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

/* Formatting is outside the arithmetic fixture and must never be invoked. */
int
fmtprint(Fmt *format, char *text, ...)
{
	(void)format;
	(void)text;
	abort();
	return 0;
}

IMPLEMENTATION

typedef struct Test Test;
struct Test {
	UWORD in[7];
	UWORD out[24];
};
VECTORS

/* Inspect all coordinate bits without assuming a host signed overflow result. */
static void
pointresult(Draw_Point p, const UWORD *expected)
{
	assert((UWORD)p.x == expected[0] && (UWORD)p.y == expected[1]);
	checks++;
}

/* Check each rectangle field rather than assuming padding or representation. */
static void
rectresult(Draw_Rect r, const UWORD *expected)
{
	pointresult(r.min, expected);
	pointresult(r.max, expected+2);
}

/* Allow independent parent-source probes for each arithmetic entry point. */
static int
selected(char *name)
{
	return operation == nil || strcmp(operation, name) == 0;
}

/* Execute real generated builtin frames with separate and exact input-alias results. */
static void
vmcase(const Test *t)
{
	F_Point_add add = {0};
	F_Point_sub sub = {0};
	F_Point_mul mul = {0};
	F_Point_div div = {0};
	F_Rect_addpt radd = {0};
	F_Rect_subpt rsub = {0};
	F_Rect_inset inset = {0};
	F_Rect_dx dx = {0};
	F_Rect_dy dy = {0};
	F_Rect_size size = {0};
	Draw_Point p, q, result = {123, -456};
	Draw_Rect r, rectangle;
	WORD scalar;
	int alias;

	p.x = t->in[0]; p.y = t->in[1];
	r.min = p; r.max.x = t->in[2]; r.max.y = t->in[3];
	q.x = t->in[5]; q.y = t->in[6];
	for(alias = 0; alias < 3; alias++){
		add.p = sub.p = p; add.q = sub.q = q;
		add.ret = alias == 0 ? &result : alias == 1 ? &add.p : &add.q;
		sub.ret = alias == 0 ? &result : alias == 1 ? &sub.p : &sub.q;
		if(selected("Point_add")){
			Point_add(&add);
			pointresult(*add.ret, t->out);
		}
		if(selected("Point_sub")){
			Point_sub(&sub);
			pointresult(*sub.ret, t->out+2);
		}
	}
	for(alias = 0; alias < 2; alias++){
		mul.p = div.p = p; mul.i = div.i = t->in[4];
		mul.ret = alias ? &mul.p : &result;
		div.ret = alias ? &div.p : &result;
		if(selected("Point_mul")){
			Point_mul(&mul);
			pointresult(*mul.ret, t->out+4);
		}
		if(selected("Point_div")){
			if(div.i){
				Point_div(&div); pointresult(*div.ret, t->out+6);
			}else{
				Draw_Point saved = *div.ret;
				expects = 1;
				if(setjmp(boundary) == 0){ Point_div(&div); assert(0); }
				expects = 0;
				assert(div.ret->x == saved.x && div.ret->y == saved.y);
				checks++;
			}
		}
		radd.r = rsub.r = inset.r = dx.r = dy.r = size.r = r;
		radd.p = rsub.p = q; inset.n = t->in[4];
		radd.ret = alias ? &radd.r : &rectangle;
		rsub.ret = alias ? &rsub.r : &rectangle;
		inset.ret = alias ? &inset.r : &rectangle;
		if(selected("Rect_addpt")){
			Rect_addpt(&radd);
			rectresult(*radd.ret, t->out+8);
		}
		if(selected("Rect_subpt")){
			Rect_subpt(&rsub);
			rectresult(*rsub.ret, t->out+12);
		}
		if(selected("Rect_inset")){
			Rect_inset(&inset);
			rectresult(*inset.ret, t->out+16);
		}
		dx.ret = alias ? &dx.r.min.x : &scalar;
		dy.ret = alias ? &dy.r.min.y : &scalar;
		if(selected("Rect_dx")){
			Rect_dx(&dx);
			assert((UWORD)*dx.ret == t->out[20]);
			checks++;
		}
		if(selected("Rect_dy")){
			Rect_dy(&dy);
			assert((UWORD)*dy.ret == t->out[21]);
			checks++;
		}
		size.ret = alias ? &size.r.min : &result;
		if(selected("Rect_size")){
			Rect_size(&size);
			pointresult(*size.ret, t->out+22);
		}
	}
}

/* Native libdraw coordinates stay int-sized even when the VM uses wider words. */
static void
nativecase(const Test *t)
{
	Point p, q, result;
	Rectangle r, rectangle;
	int n;

	p = Pt(t->in[0], t->in[1]);
	q = Pt(t->in[5], t->in[6]);
	r = Rpt(p, Pt(t->in[2], t->in[3]));
	n = t->in[4];
#define POINT(expr, at) do { \
	result = (expr); \
	assert((uint)result.x == (uint)t->out[at] && (uint)result.y == (uint)t->out[(at)+1]); \
	checks++; \
} while(0)
#define RECT(expr, at) do { \
	rectangle = (expr); \
	assert((uint)rectangle.min.x == (uint)t->out[at] && (uint)rectangle.min.y == (uint)t->out[(at)+1]); \
	assert((uint)rectangle.max.x == (uint)t->out[(at)+2] && (uint)rectangle.max.y == (uint)t->out[(at)+3]); \
	checks++; \
} while(0)
	if(selected("addpt")) POINT(addpt(p, q), 0);
	if(selected("subpt")) POINT(subpt(p, q), 2);
	if(selected("mulpt")) POINT(mulpt(p, n), 4);
	if(n && selected("divpt")) POINT(divpt(p, n), 6);
	if(selected("rectaddpt")) RECT(rectaddpt(r, q), 8);
	if(selected("rectsubpt")) RECT(rectsubpt(r, q), 12);
	if(selected("insetrect")) RECT(insetrect(r, n), 16);
#undef POINT
#undef RECT
}

/* Compare all production results to independently generated Python integer vectors. */
int
main(int argc, char **argv)
{
	unsigned int i;
	int vm = argc == 1 || strcmp(argv[1], "vm") == 0;
	int native = argc == 1 || strcmp(argv[1], "native") == 0;

	operation = getenv("GEOMETRY_OP");
	for(i = 0; i < sizeof(vectors)/sizeof(vectors[0]); i++){
		if(vm) vmcase(&vectors[i]);
		if(native && sizeof(WORD) == sizeof(int)) nativecase(&vectors[i]);
	}
	printf("PASS geometry arithmetic: %d result/error checks, WORD%zu\n", checks, sizeof(WORD)*8);
	return 0;
}
