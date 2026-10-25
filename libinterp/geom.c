#include "lib9.h"
#include "interp.h"
#include "isa.h"
#include "draw.h"
#include "runt.h"
#include "raise.h"

/* Add coordinates modulo the active VM word width, without C signed overflow. */
void
Point_add(void *fp)
{
	F_Point_add *f;
	Draw_Point result, *ret;

	f = fp;

	ret = &result;
	ret->x = (UWORD)f->p.x + (UWORD)f->q.x;
	ret->y = (UWORD)f->p.y + (UWORD)f->q.y;
	memmove(f->ret, &result, sizeof(result));
}

/* Subtract coordinates with the same modular arithmetic as VM word subtraction. */
void
Point_sub(void *fp)
{
	F_Point_sub *f;
	Draw_Point result, *ret;

	f = fp;

	ret = &result;
	ret->x = (UWORD)f->p.x - (UWORD)f->q.x;
	ret->y = (UWORD)f->p.y - (UWORD)f->q.y;
	memmove(f->ret, &result, sizeof(result));
}

/* Scale coordinates without overflowing a signed C intermediate. */
void
Point_mul(void *fp)
{
	F_Point_mul *f;
	Draw_Point result, *ret;

	f = fp;

	ret = &result;
	ret->x = (UWORD)f->p.x * (UWORD)f->i;
	ret->y = (UWORD)f->p.y * (UWORD)f->i;
	memmove(f->ret, &result, sizeof(result));
}

/* Raise on zero before writing either coordinate; handle minimum / -1 by wrapping. */
void
Point_div(void *fp)
{
	F_Point_div *f;
	Draw_Point result, *ret;

	f = fp;

	if(f->i == 0)
		error(exZdiv);
	ret = &result;
	if(f->i == -1){
		ret->x = (UWORD)0 - (UWORD)f->p.x;
		ret->y = (UWORD)0 - (UWORD)f->p.y;
	}else{
		ret->x = f->p.x / f->i;
		ret->y = f->p.y / f->i;
	}
	memmove(f->ret, &result, sizeof(result));
}

/* Compute the complete result before copying to an unaligned or aliased VM slot. */
void
Point_eq(void *fp)
{
	F_Point_eq *f;
	WORD result;

	f = fp;
	result = f->p.x == f->q.x && f->p.y == f->q.y;
	memmove(f->ret, &result, sizeof(result));
}

/* Compute the complete result before copying to an unaligned or aliased VM slot. */
void
Point_in(void *fp)
{
	F_Point_in *f;
	WORD result;

	f = fp;
	result = f->p.x >= f->r.min.x && f->p.x < f->r.max.x &&
	       f->p.y >= f->r.min.y && f->p.y < f->r.max.y;
	memmove(f->ret, &result, sizeof(result));
}

/* Compute the complete result before copying to an unaligned or aliased VM slot. */
void
Rect_canon(void *fp)
{
	F_Rect_canon *f;
	Draw_Rect result, *ret;
	WORD t;

	f = fp;

	ret = &result;
	if(f->r.max.x < f->r.min.x){
		t = f->r.max.x;
		ret->max.x = f->r.min.x;
		ret->min.x = t;
	}else{
		t = f->r.max.x;
		ret->min.x = f->r.min.x;
		ret->max.x = t;
	}
	if(f->r.max.y < f->r.min.y){
		t = f->r.max.y;
		ret->max.y = f->r.min.y;
		ret->min.y = t;
	}else{
		t = f->r.max.y;
		ret->min.y = f->r.min.y;
		ret->max.y = t;
	}
	memmove(f->ret, &result, sizeof(result));
}

/* Compute the complete result before copying to an unaligned or aliased VM slot. */
void
Rect_combine(void *fp)
{
	F_Rect_combine *f;
	Draw_Rect result, *ret;

	f = fp;
	ret = &result;
	*ret = f->r;
	if(f->r.min.x > f->s.min.x)
		ret->min.x = f->s.min.x;
	if(f->r.min.y > f->s.min.y)
		ret->min.y = f->s.min.y;
	if(f->r.max.x < f->s.max.x)
		ret->max.x = f->s.max.x;
	if(f->r.max.y < f->s.max.y)
		ret->max.y = f->s.max.y;
	memmove(f->ret, &result, sizeof(result));
}

/* Compute the complete result before copying to an unaligned or aliased VM slot. */
void
Rect_eq(void *fp)
{
	F_Rect_eq *f;
	WORD result;

	f = fp;

	result = f->r.min.x == f->s.min.x
		&& f->r.max.x == f->s.max.x
		&& f->r.min.y == f->s.min.y
		&& f->r.max.y == f->s.max.y;
	memmove(f->ret, &result, sizeof(result));
}

/* Compute the complete result before copying to an unaligned or aliased VM slot. */
void
Rect_Xrect(void *fp)
{
	F_Rect_Xrect *f;
	WORD result;

	f = fp;

	result = f->r.min.x < f->s.max.x
		&& f->s.min.x < f->r.max.x
		&& f->r.min.y < f->s.max.y
		&& f->s.min.y < f->r.max.y;
	memmove(f->ret, &result, sizeof(result));
}

/* Compute the complete result before copying to an unaligned or aliased VM slot. */
void
Rect_clip(void *fp)
{
	F_Rect_clip *f, output;
	union { WORD align; uchar bytes[sizeof(*output.ret)]; } result;
	Draw_Rect *r, *s, *ret;

	f = fp;

	r = &f->r;
	s = &f->s;
	output = *f;
	output.ret = (void*)result.bytes;
	memmove(result.bytes, f->ret, sizeof(result.bytes));
	ret = &output.ret->t0;

	/*
	 * Expand rectXrect() in line for speed
	 */
	if(!(r->min.x<s->max.x && s->min.x<r->max.x
	&& r->min.y<s->max.y && s->min.y<r->max.y)){
		*ret = *r;
		output.ret->t1 = 0;
		memmove(f->ret, result.bytes, sizeof(result.bytes));
		return;
	}

	/* They must overlap */
	if(r->min.x < s->min.x)
		ret->min.x = s->min.x;
	else
		ret->min.x = r->min.x;
	if(r->min.y < s->min.y)
		ret->min.y = s->min.y;
	else
		ret->min.y = r->min.y;
	if(r->max.x > s->max.x)
		ret->max.x = s->max.x;
	else
		ret->max.x = r->max.x;
	if(r->max.y > s->max.y)
		ret->max.y = s->max.y;
	else
		ret->max.y = r->max.y;
	output.ret->t1 = 1;
	memmove(f->ret, result.bytes, sizeof(result.bytes));
}

/* Compute the complete result before copying to an unaligned or aliased VM slot. */
void
Rect_inrect(void *fp)
{
	F_Rect_inrect *f;
	WORD result;

	f = fp;

	result = f->s.min.x <= f->r.min.x
		&& f->r.max.x <= f->s.max.x
		&& f->s.min.y <= f->r.min.y
		&& f->r.max.y <= f->s.max.y;
	memmove(f->ret, &result, sizeof(result));
}

/* Compute the complete result before copying to an unaligned or aliased VM slot. */
void
Rect_contains(void *fp)
{
	F_Rect_contains *f;
	WORD result;
	WORD x, y;

	f = fp;

	x = f->p.x;
	y = f->p.y;
	result = x >= f->r.min.x && x < f->r.max.x
		&& y >= f->r.min.y && y < f->r.max.y;
	memmove(f->ret, &result, sizeof(result));
}

/* Translate both corners with VM-width unsigned addition. */
void
Rect_addpt(void *fp)
{
	F_Rect_addpt *f;
	Draw_Rect result, *ret;
	WORD n;

	f = fp;

	ret = &result;
	n = f->p.x;
	ret->min.x = (UWORD)f->r.min.x + (UWORD)n;
	ret->max.x = (UWORD)f->r.max.x + (UWORD)n;
	n = f->p.y;
	ret->min.y = (UWORD)f->r.min.y + (UWORD)n;
	ret->max.y = (UWORD)f->r.max.y + (UWORD)n;
	memmove(f->ret, &result, sizeof(result));
}

/* Translate both corners with VM-width unsigned subtraction. */
void
Rect_subpt(void *fp)
{
	WORD n;
	F_Rect_subpt *f;
	Draw_Rect result, *ret;

	f = fp;

	ret = &result;
	n = f->p.x;
	ret->min.x = (UWORD)f->r.min.x - (UWORD)n;
	ret->max.x = (UWORD)f->r.max.x - (UWORD)n;
	n = f->p.y;
	ret->min.y = (UWORD)f->r.min.y - (UWORD)n;
	ret->max.y = (UWORD)f->r.max.y - (UWORD)n;
	memmove(f->ret, &result, sizeof(result));
}

/* Inset corners without signed overflow, preserving noncanonical results. */
void
Rect_inset(void *fp)
{
	WORD n;
	Draw_Rect result, *ret;
	F_Rect_inset *f;

	f = fp;

	ret = &result;
	n = f->n;
	ret->min.x = (UWORD)f->r.min.x + (UWORD)n;
	ret->min.y = (UWORD)f->r.min.y + (UWORD)n;
	ret->max.x = (UWORD)f->r.max.x - (UWORD)n;
	ret->max.y = (UWORD)f->r.max.y - (UWORD)n;
	memmove(f->ret, &result, sizeof(result));
}

/* Measure width using the VM word subtraction rules. */
void
Rect_dx(void *fp)
{
	F_Rect_dx *f;
	WORD result;

	f = fp;

	result = (UWORD)f->r.max.x - (UWORD)f->r.min.x;
	memmove(f->ret, &result, sizeof(result));
}

/* Measure height using the VM word subtraction rules. */
void
Rect_dy(void *fp)
{
	F_Rect_dy *f;
	WORD result;

	f = fp;

	result = (UWORD)f->r.max.y - (UWORD)f->r.min.y;
	memmove(f->ret, &result, sizeof(result));
}

/* Measure both dimensions without signed C overflow. */
void
Rect_size(void *fp)
{
	F_Rect_size *f;
	Draw_Point result, *ret;

	f = fp;

	ret = &result;
	ret->x = (UWORD)f->r.max.x - (UWORD)f->r.min.x;
	ret->y = (UWORD)f->r.max.y - (UWORD)f->r.min.y;
	memmove(f->ret, &result, sizeof(result));
}
