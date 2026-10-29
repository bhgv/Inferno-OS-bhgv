#include "lib9.h"
#include "interp.h"
#include "isa.h"
#include "runt.h"
#include "raise.h"
#include "mathi.h"
#include "mathmod.h"

typedef union MathBits64 MathBits64;
union MathBits64
{
	double x;
	uvlong u;
};

typedef union MathBits32 MathBits32;
union MathBits32
{
	float x;
	unsigned int u;
};

/* Match a borrowed primitive array descriptor from a checked module call.
 * actual may be canonical or module-local; expected is a canonical byte/word/real
 * descriptor. Local descriptors retain only size/map layout, so the caller's
 * checked Limbo signature supplies primitive semantics. Reject pointer-bearing
 * layouts and wrong canonical primitives even when native widths coincide.
 * Return one on a compatible layout; no ownership, allocation or blocking. */
static int
matharraytype(Type *actual, Type *expected)
{
    if (actual == nil || actual == H || expected == nil || expected == H)
    {
        return 0;
    }
    if (actual == &Tbyte || actual == &Tword || actual == &Tlong || actual == &Treal)
    {
        return actual == expected;
    }
    return actual->size == expected->size && actual->np == 0;
}


/* Check full VM lengths before multiplication, narrowing or touching array data. */
static int
matharraycount(Array *bytes, Array *numbers, int width, Type *type)
{
	if(bytes == H || bytes == nil || numbers == H || numbers == nil)
		error(exNilref);
	if(!matharraytype(bytes->t, &Tbyte) || !matharraytype(numbers->t, type))
		error(exType);
	if(bytes->len < 0 || numbers->len < 0 ||
	   (uvlong)numbers->len > 0x7fffffff/width ||
	   (uvlong)numbers->len > 0x7fffffff/type->size ||
	   bytes->len != numbers->len*width)
		error(exMathia);
	return (int)numbers->len;
}

/* Sign-extend an external 32-bit integer without overflowing a 32-bit VM word. */
static WORD
mathsigned32(unsigned int bits)
{
	if(bits & 0x80000000U)
		return -1-(WORD)(~bits);
	return (WORD)bits;
}

void
mathmodinit(void)
{
	builtinmod("$Math", Mathmodtab, Mathmodlen);
	fmtinstall('g', gfltconv);
	fmtinstall('G', gfltconv);
	fmtinstall('e', gfltconv);
	/* fmtinstall('E', gfltconv); */	/* avoid clash with ether address */
	fmtinstall(0x00c9, gfltconv);	/* L'É' */
	fmtinstall('f', gfltconv);
}

/* Decode big-endian signed 32-bit values into complete VM-word slots. */
void
Math_import_int(void *fp)
{
	F_Math_import_int *f;
	int i, n;
	unsigned int u;
	unsigned char *bp;
	WORD *x;

	f = fp;
	n = matharraycount(f->b, f->x, 4, &Tword);
	bp = (unsigned char *)(f->b->data);
	x = (WORD*)(f->x->data);
	for(i=0; i<n; i++){
		u = *bp++;
		u = (u<<8) | *bp++;
		u = (u<<8) | *bp++;
		u = (u<<8) | *bp++;
		x[i] = mathsigned32(u);
	}
}

/* Decode external binary32 values through a local representation union. */
void
Math_import_real32(void *fp)
{
	F_Math_import_real32 *f;
	int i, n;
	unsigned int u;
	unsigned char *bp;
	double *x;
	MathBits32 bits32;

	f = fp;
	n = matharraycount(f->b, f->x, 4, &Treal);
	bp = (unsigned char *)(f->b->data);
	x = (double*)(f->x->data);
	for(i=0; i<n; i++){
		u = *bp++;
		u = (u<<8) | *bp++;
		u = (u<<8) | *bp++;
		u = (u<<8) | *bp++;
		bits32.u = u;
		x[i] = bits32.x;
	}
}

/* Decode external binary64 values after validating both array sizes. */
void
Math_import_real(void *fp)
{
	F_Math_import_real *f;
	int i, n;
	uvlong u;
	unsigned char *bp;
	double *x;
	MathBits64 bits64;

	f = fp;
	n = matharraycount(f->b, f->x, 8, &Treal);
	bp = f->b->data;
	x = (double*)(f->x->data);
	for(i=0; i<n; i++){
		u = *bp++;
		u = (u<<8) | *bp++;
		u = (u<<8) | *bp++;
		u = (u<<8) | *bp++;
		u = (u<<8) | *bp++;
		u = (u<<8) | *bp++;
		u = (u<<8) | *bp++;
		u = (u<<8) | *bp++;
		bits64.u = u;
		x[i] = bits64.x;
	}
}

/* Encode the low 32 bits of each VM word using its actual array stride. */
void
Math_export_int(void *fp)
{
	F_Math_export_int *f;
	int i, n;
	unsigned int u;
	unsigned char *bp;
	WORD *x;

	f = fp;
	n = matharraycount(f->b, f->x, 4, &Tword);
	bp = (unsigned char *)(f->b->data);
	x = (WORD*)(f->x->data);
	for(i=0; i<n; i++){
		u = x[i];
		*bp++ = u>>24;
		*bp++ = u>>16;
		*bp++ = u>>8;
		*bp++ = u;
	}
}

/* Narrow real values to binary32 after checking the complete destination. */
void
Math_export_real32(void *fp)
{
	F_Math_export_real32 *f;
	int i, n;
	unsigned int u;
	unsigned char *bp;
	double *x;
	MathBits32 bits32;

	f = fp;
	n = matharraycount(f->b, f->x, 4, &Treal);
	bp = (unsigned char *)(f->b->data);
	x = (double*)(f->x->data);
	for(i=0; i<n; i++){
		bits32.x = x[i];
		u = bits32.u;
		*bp++ = u>>24;
		*bp++ = u>>16;
		*bp++ = u>>8;
		*bp++ = u;
	}
}

/* Export exact binary64 representations in big-endian byte order. */
void
Math_export_real(void *fp)
{
	F_Math_export_real *f;
	int i, n;
	uvlong u;
	unsigned char *bp;
	double *x;
	MathBits64 bits64;

	f = fp;
	n = matharraycount(f->b, f->x, 8, &Treal);
	bp = (unsigned char *)(f->b->data);
	x = (double*)(f->x->data);
	for(i=0; i<n; i++){
		bits64.x = x[i];
		u = bits64.u;
		*bp++ = u>>56;
		*bp++ = u>>48;
		*bp++ = u>>40;
		*bp++ = u>>32;
		*bp++ = u>>24;
		*bp++ = u>>16;
		*bp++ = u>>8;
		*bp++ = u;
	}
}


/* Convert a scalar bit representation without shared mutable scratch state. */
void
Math_bits32real(void *fp)
{
	F_Math_bits32real *f;
	REAL result;
	MathBits32 bits32;

	f = fp;
	bits32.u = f->b;
	result = bits32.x;
	memmove(f->ret, &result, sizeof(result));
}

/* Convert a scalar bit representation without shared mutable scratch state. */
void
Math_bits64real(void *fp)
{
	F_Math_bits64real *f;
	REAL result;
	MathBits64 bits64;

	f = fp;
	bits64.u = f->b;
	result = bits64.x;
	memmove(f->ret, &result, sizeof(result));
}

/* Convert a scalar bit representation without shared mutable scratch state. */
void
Math_realbits32(void *fp)
{
	F_Math_realbits32 *f;
	WORD result;
	MathBits32 bits32;

	f = fp;
	bits32.x = f->x;
	result = mathsigned32(bits32.u);
	memmove(f->ret, &result, sizeof(result));
}

/* Convert a scalar bit representation without shared mutable scratch state. */
void
Math_realbits64(void *fp)
{
	F_Math_realbits64 *f;
	LONG result;
	MathBits64 bits64;

	f = fp;
	bits64.x = f->x;
	result = bits64.u;
	memmove(f->ret, &result, sizeof(result));
}


/* Evaluate the scalar result before copying it into an unaligned VM return slot. */
void
Math_getFPcontrol(void *fp)
{
	F_Math_getFPcontrol *f;
	WORD result;

	f = fp;

	result = getFPcontrol();
	memmove(f->ret, &result, sizeof(result));
}

/* Evaluate the scalar result before copying it into an unaligned VM return slot. */
void
Math_getFPstatus(void *fp)
{
	F_Math_getFPstatus *f;
	WORD result;

	f = fp;

	result = getFPstatus();
	memmove(f->ret, &result, sizeof(result));
}

/* Evaluate the scalar result before copying it into an unaligned VM return slot. */
void
Math_finite(void *fp)
{
	F_Math_finite *f;
	WORD result;

	f = fp;

	result = finite(f->x);
	memmove(f->ret, &result, sizeof(result));
}

/* Evaluate the scalar result before copying it into an unaligned VM return slot. */
void
Math_ilogb(void *fp)
{
	F_Math_ilogb *f;
	WORD result;

	f = fp;

	result = ilogb(f->x);
	memmove(f->ret, &result, sizeof(result));
}

/* Evaluate the scalar result before copying it into an unaligned VM return slot. */
void
Math_isnan(void *fp)
{
	F_Math_isnan *f;
	WORD result;

	f = fp;

	result = isNaN(f->x);
	memmove(f->ret, &result, sizeof(result));
}

/* Evaluate the scalar result before copying it into an unaligned VM return slot. */
void
Math_acos(void *fp)
{
	F_Math_acos *f;
	REAL result;

	f = fp;

	result = __ieee754_acos(f->x);
	memmove(f->ret, &result, sizeof(result));
}

/* Evaluate the scalar result before copying it into an unaligned VM return slot. */
void
Math_acosh(void *fp)
{
	F_Math_acosh *f;
	REAL result;

	f = fp;

	result = __ieee754_acosh(f->x);
	memmove(f->ret, &result, sizeof(result));
}

/* Evaluate the scalar result before copying it into an unaligned VM return slot. */
void
Math_asin(void *fp)
{
	F_Math_asin *f;
	REAL result;

	f = fp;

	result = __ieee754_asin(f->x);
	memmove(f->ret, &result, sizeof(result));
}

/* Evaluate the scalar result before copying it into an unaligned VM return slot. */
void
Math_asinh(void *fp)
{
	F_Math_asinh *f;
	REAL result;

	f = fp;

	result = asinh(f->x);
	memmove(f->ret, &result, sizeof(result));
}

/* Evaluate the scalar result before copying it into an unaligned VM return slot. */
void
Math_atan(void *fp)
{
	F_Math_atan *f;
	REAL result;

	f = fp;

	result = atan(f->x);
	memmove(f->ret, &result, sizeof(result));
}

/* Evaluate the scalar result before copying it into an unaligned VM return slot. */
void
Math_atanh(void *fp)
{
	F_Math_atanh *f;
	REAL result;

	f = fp;

	result = __ieee754_atanh(f->x);
	memmove(f->ret, &result, sizeof(result));
}

/* Evaluate the scalar result before copying it into an unaligned VM return slot. */
void
Math_cbrt(void *fp)
{
	F_Math_cbrt *f;
	REAL result;

	f = fp;

	result = cbrt(f->x);
	memmove(f->ret, &result, sizeof(result));
}

/* Evaluate the scalar result before copying it into an unaligned VM return slot. */
void
Math_ceil(void *fp)
{
	F_Math_ceil *f;
	REAL result;

	f = fp;

	result = ceil(f->x);
	memmove(f->ret, &result, sizeof(result));
}

/* Evaluate the scalar result before copying it into an unaligned VM return slot. */
void
Math_cos(void *fp)
{
	F_Math_cos *f;
	REAL result;

	f = fp;

	result = cos(f->x);
	memmove(f->ret, &result, sizeof(result));
}

/* Evaluate the scalar result before copying it into an unaligned VM return slot. */
void
Math_cosh(void *fp)
{
	F_Math_cosh *f;
	REAL result;

	f = fp;

	result = __ieee754_cosh(f->x);
	memmove(f->ret, &result, sizeof(result));
}

/* Evaluate the scalar result before copying it into an unaligned VM return slot. */
void
Math_erf(void *fp)
{
	F_Math_erf *f;
	REAL result;

	f = fp;

	result = erf(f->x);
	memmove(f->ret, &result, sizeof(result));
}

/* Evaluate the scalar result before copying it into an unaligned VM return slot. */
void
Math_erfc(void *fp)
{
	F_Math_erfc *f;
	REAL result;

	f = fp;

	result = erfc(f->x);
	memmove(f->ret, &result, sizeof(result));
}

/* Evaluate the scalar result before copying it into an unaligned VM return slot. */
void
Math_exp(void *fp)
{
	F_Math_exp *f;
	REAL result;

	f = fp;

	result = __ieee754_exp(f->x);
	memmove(f->ret, &result, sizeof(result));
}

/* Evaluate the scalar result before copying it into an unaligned VM return slot. */
void
Math_expm1(void *fp)
{
	F_Math_expm1 *f;
	REAL result;

	f = fp;

	result = expm1(f->x);
	memmove(f->ret, &result, sizeof(result));
}

/* Evaluate the scalar result before copying it into an unaligned VM return slot. */
void
Math_fabs(void *fp)
{
	F_Math_fabs *f;
	REAL result;

	f = fp;

	result = fabs(f->x);
	memmove(f->ret, &result, sizeof(result));
}

/* Evaluate the scalar result before copying it into an unaligned VM return slot. */
void
Math_floor(void *fp)
{
	F_Math_floor *f;
	REAL result;

	f = fp;

	result = floor(f->x);
	memmove(f->ret, &result, sizeof(result));
}

/* Evaluate the scalar result before copying it into an unaligned VM return slot. */
void
Math_j0(void *fp)
{
	F_Math_j0 *f;
	REAL result;

	f = fp;

	result = __ieee754_j0(f->x);
	memmove(f->ret, &result, sizeof(result));
}

/* Evaluate the scalar result before copying it into an unaligned VM return slot. */
void
Math_j1(void *fp)
{
	F_Math_j1 *f;
	REAL result;

	f = fp;

	result = __ieee754_j1(f->x);
	memmove(f->ret, &result, sizeof(result));
}

/* Evaluate the scalar result before copying it into an unaligned VM return slot. */
void
Math_log(void *fp)
{
	F_Math_log *f;
	REAL result;

	f = fp;

	result = __ieee754_log(f->x);
	memmove(f->ret, &result, sizeof(result));
}

/* Evaluate the scalar result before copying it into an unaligned VM return slot. */
void
Math_log10(void *fp)
{
	F_Math_log10 *f;
	REAL result;

	f = fp;

	result = __ieee754_log10(f->x);
	memmove(f->ret, &result, sizeof(result));
}

/* Evaluate the scalar result before copying it into an unaligned VM return slot. */
void
Math_log1p(void *fp)
{
	F_Math_log1p *f;
	REAL result;

	f = fp;

	result = log1p(f->x);
	memmove(f->ret, &result, sizeof(result));
}

/* Evaluate the scalar result before copying it into an unaligned VM return slot. */
void
Math_rint(void *fp)
{
	F_Math_rint *f;
	REAL result;

	f = fp;

	result = rint(f->x);
	memmove(f->ret, &result, sizeof(result));
}

/* Evaluate the scalar result before copying it into an unaligned VM return slot. */
void
Math_sin(void *fp)
{
	F_Math_sin *f;
	REAL result;

	f = fp;

	result = sin(f->x);
	memmove(f->ret, &result, sizeof(result));
}

/* Evaluate the scalar result before copying it into an unaligned VM return slot. */
void
Math_sinh(void *fp)
{
	F_Math_sinh *f;
	REAL result;

	f = fp;

	result = __ieee754_sinh(f->x);
	memmove(f->ret, &result, sizeof(result));
}

/* Evaluate the scalar result before copying it into an unaligned VM return slot. */
void
Math_sqrt(void *fp)
{
	F_Math_sqrt *f;
	REAL result;

	f = fp;

	result = __ieee754_sqrt(f->x);
	memmove(f->ret, &result, sizeof(result));
}

/* Evaluate the scalar result before copying it into an unaligned VM return slot. */
void
Math_tan(void *fp)
{
	F_Math_tan *f;
	REAL result;

	f = fp;

	result = tan(f->x);
	memmove(f->ret, &result, sizeof(result));
}

/* Evaluate the scalar result before copying it into an unaligned VM return slot. */
void
Math_tanh(void *fp)
{
	F_Math_tanh *f;
	REAL result;

	f = fp;

	result = tanh(f->x);
	memmove(f->ret, &result, sizeof(result));
}

/* Evaluate the scalar result before copying it into an unaligned VM return slot. */
void
Math_y0(void *fp)
{
	F_Math_y0 *f;
	REAL result;

	f = fp;

	result = __ieee754_y0(f->x);
	memmove(f->ret, &result, sizeof(result));
}

/* Evaluate the scalar result before copying it into an unaligned VM return slot. */
void
Math_y1(void *fp)
{
	F_Math_y1 *f;
	REAL result;

	f = fp;

	result = __ieee754_y1(f->x);
	memmove(f->ret, &result, sizeof(result));
}

/* Evaluate the scalar result before copying it into an unaligned VM return slot. */
void
Math_fdim(void *fp)
{
	F_Math_fdim *f;
	REAL result;

	f = fp;

	result = fdim(f->x, f->y);
	memmove(f->ret, &result, sizeof(result));
}

/* Evaluate the scalar result before copying it into an unaligned VM return slot. */
void
Math_fmax(void *fp)
{
	F_Math_fmax *f;
	REAL result;

	f = fp;

	result = fmax(f->x, f->y);
	memmove(f->ret, &result, sizeof(result));
}

/* Evaluate the scalar result before copying it into an unaligned VM return slot. */
void
Math_fmin(void *fp)
{
	F_Math_fmin *f;
	REAL result;

	f = fp;

	result = fmin(f->x, f->y);
	memmove(f->ret, &result, sizeof(result));
}

/* Evaluate the scalar result before copying it into an unaligned VM return slot. */
void
Math_fmod(void *fp)
{
	F_Math_fmod *f;
	REAL result;

	f = fp;

	result = __ieee754_fmod(f->x, f->y);
	memmove(f->ret, &result, sizeof(result));
}

/* Evaluate the scalar result before copying it into an unaligned VM return slot. */
void
Math_hypot(void *fp)
{
	F_Math_hypot *f;
	REAL result;

	f = fp;

	result = __ieee754_hypot(f->x, f->y);
	memmove(f->ret, &result, sizeof(result));
}

/* Evaluate the scalar result before copying it into an unaligned VM return slot. */
void
Math_nextafter(void *fp)
{
	F_Math_nextafter *f;
	REAL result;

	f = fp;

	result = nextafter(f->x, f->y);
	memmove(f->ret, &result, sizeof(result));
}

/* Evaluate the scalar result before copying it into an unaligned VM return slot. */
void
Math_pow(void *fp)
{
	F_Math_pow *f;
	REAL result;

	f = fp;

	result = __ieee754_pow(f->x, f->y);
	memmove(f->ret, &result, sizeof(result));
}



/* Evaluate the scalar result before copying it into an unaligned VM return slot. */
void
Math_FPcontrol(void *fp)
{
	F_Math_FPcontrol *f;
	WORD result;

	f = fp;

	result = FPcontrol(f->r, f->mask);
	memmove(f->ret, &result, sizeof(result));
}

/* Evaluate the scalar result before copying it into an unaligned VM return slot. */
void
Math_FPstatus(void *fp)
{
	F_Math_FPstatus *f;
	WORD result;

	f = fp;

	result = FPstatus(f->r, f->mask);
	memmove(f->ret, &result, sizeof(result));
}

/* Evaluate the scalar result before copying it into an unaligned VM return slot. */
void
Math_atan2(void *fp)
{
	F_Math_atan2 *f;
	REAL result;

	f = fp;

	result = __ieee754_atan2(f->y, f->x);
	memmove(f->ret, &result, sizeof(result));
}

/* Evaluate the scalar result before copying it into an unaligned VM return slot. */
void
Math_copysign(void *fp)
{
	F_Math_copysign *f;
	REAL result;

	f = fp;

	result = copysign(f->x, f->s);
	memmove(f->ret, &result, sizeof(result));
}

/* Bessel order is a discrete index: reject high VM bits instead of changing its parity. */
static int
mathorder(WORD order)
{
	if(order < -0x7fffffff-1 || order > 0x7fffffff)
		error(exInval);
	return (int)order;
}

/* Validate the complete order before passing it to the native recurrence. */
void
Math_jn(void *fp)
{
	F_Math_jn *f;
	REAL result;

	f = fp;

	result = __ieee754_jn(mathorder(f->n), f->x);
	memmove(f->ret, &result, sizeof(result));
}

/* Widen the native sign output before storing it in a possibly wider VM slot. */
void
Math_lgamma(void *fp)
{
	F_Math_lgamma *f, output;
	union { REAL align; uchar bytes[sizeof(*output.ret)]; } result;
	int sign;
	double value;

	f = fp;
	value = __ieee754_lgamma_r(f->x, &sign);
	/* Stage the tuple at native alignment while preserving its ABI padding. */
	output = *f;
	output.ret = (void*)result.bytes;
	memmove(result.bytes, f->ret, sizeof(result.bytes));
	output.ret->t0 = sign;
	output.ret->t1 = value;
	memmove(f->ret, result.bytes, sizeof(result.bytes));
}

/* Publish both components only when the integral part fits the active VM word. */
void
Math_modf(void *fp)
{
	F_Math_modf *f, output;
	union { REAL align; uchar bytes[sizeof(*output.ret)]; } result;
	double ipart, fraction;

	f = fp;
	fraction = modf(f->x, &ipart);
	if(sizeof(WORD) == 4){
		if(!(ipart >= -2147483648.0 && ipart < 2147483648.0))
			error(exInval);
	}else{
		if(!(ipart >= -9223372036854775808.0 && ipart < 9223372036854775808.0))
			error(exInval);
	}
	/* Stage the tuple at native alignment while preserving its ABI padding. */
	output = *f;
	output.ret = (void*)result.bytes;
	memmove(result.bytes, f->ret, sizeof(result.bytes));
	output.ret->t0 = (WORD)ipart;
	output.ret->t1 = fraction;
	memmove(f->ret, result.bytes, sizeof(result.bytes));
}

/* Preserve extreme VM exponents when the native math API takes a narrower int. */
static int
mathexponent(WORD n)
{
	if(n > 0x7fffffff)
		return 0x7fffffff;
	if(n < -0x7fffffff-1)
		return -0x7fffffff-1;
	return (int)n;
}

/* Decimal powers saturate in the original direction before narrowing the exponent. */
void
Math_pow10(void *fp)
{
	F_Math_pow10 *f;
	REAL result;

	f = fp;

	result = ipow10(mathexponent(f->p));
	memmove(f->ret, &result, sizeof(result));
}

/* Evaluate the scalar result before copying it into an unaligned VM return slot. */
void
Math_remainder(void *fp)
{
	F_Math_remainder *f;
	REAL result;

	f = fp;

	result = __ieee754_remainder(f->x, f->p);
	memmove(f->ret, &result, sizeof(result));
}

/* Scale with a bounded native exponent while preserving zero, infinity and NaN. */
void
Math_scalbn(void *fp)
{
	F_Math_scalbn *f;
	REAL result;

	f = fp;

	result = scalbn(f->x, mathexponent(f->n));
	memmove(f->ret, &result, sizeof(result));
}

/* Preserve native order bounds without silently wrapping a wider VM word. */
void
Math_yn(void *fp)
{
	F_Math_yn *f;
	REAL result;

	f = fp;

	result = __ieee754_yn(mathorder(f->n), f->x);
	memmove(f->ret, &result, sizeof(result));
}


/* Bound native vector counts without losing the high bits of a VM length. */
static int
mathvectorcount(Array *array, Type *type)
{
	if(array == H || array == nil)
		error(exNilref);
	if(!matharraytype(array->t, type))
		error(exType);
	if(array->len < 0 || (uvlong)array->len > 0x7fffffff/type->size)
		error(exMathia);
	return (int)array->len;
}

/* Put NaNs after numeric values so comparisons remain consistent during sort. */
static int
cmp(WORD *u, WORD *v, double *x)
{
	double a, b;

	a = x[*u];
	b = x[*v];
	if(a != a)
		return b != b ? 0 : 1;
	if(b != b)
		return -1;
	return a < b ? -1 : a > b ? 1 : 0;
}

/* Swap complete VM-word index slots rather than native C ints. */
static void
swap(WORD *a, WORD *b)
{
	WORD value;

	value = *a;
	*a = *b;
	*b = value;
}

/* Exchange disjoint index blocks without narrowing their stored values. */
static void
vecswap(WORD *a, WORD *b, int n)
{
	while(n > 0){
		n--;
		swap(a++, b++);
	}
}

#define minimum(x, y) ((x)<=(y) ? (x) : (y))

/* Select the median index under the same NaN ordering as the partition. */
static WORD*
med3(WORD *a, WORD *b, WORD *c, double *x)
{	return cmp(a, b, x) < 0 ?
		  (cmp(b, c, x) < 0 ? b : (cmp(a, c, x) < 0 ? c : a ) )
		: (cmp(b, c, x) > 0 ? b : (cmp(a, c, x) < 0 ? a : c ) );
}

/* Bound stack use by recursing on the smaller numeric-index partition. */
static void
rqsort(WORD *a, int n, double *x)
{
	WORD *pa, *pb, *pc, *pd, *pl, *pm, *pn;
	int d, r, left, right;

	while(n > 1){
		if(n < 7){ /* Insertion sort on small arrays */
			for(pm = a + 1; pm < a + n; pm++)
				for(pl = pm; pl > a && cmp(pl-1, pl, x) > 0; pl--)
					swap(pl, pl-1);
			return;
		}
		pm = a + (n/2);
		if(n > 7){
			pl = a;
			pn = a + (n-1);
			if(n > 40){ /* On big arrays, pseudomedian of 9 */
				d = (n/8);
				pl = med3(pl, pl+d, pl+2*d, x);
				pm = med3(pm-d, pm, pm+d, x);
				pn = med3(pn-2*d, pn-d, pn, x);
			}
			pm = med3(pl, pm, pn, x); /* On mid arrays, med of 3 */
		}
		swap(a, pm); /* On tiny arrays, partition around middle */
		pa = pb = a + 1;
		pc = pd = a + (n-1);
		for(;;){
			while(pb <= pc && (r = cmp(pb, a, x)) <= 0){
				if(r == 0){
					swap(pa, pb);
					pa++;
				}
				pb++;
			}
			while(pb <= pc && (r = cmp(pc, a, x)) >= 0){
				if(r == 0){
					swap(pc, pd);
					pd--;
				}
				pc--;
			}
			if(pb > pc)
				break;
			swap(pb, pc);
			pb++;
			pc--;
		}
		pn = a + n;
		r = minimum(pa-a, pb-pa);
		vecswap(a, pb-r, r);
		r = minimum(pd-pc, pn-pd-1);
		vecswap(pb, pn-r, r);
		left = pb-pa;
		right = pd-pc;
		if(left < right){
			if(left > 1)
				rqsort(a, left, x);
			a = pn-right;
			n = right;
		}else{
			if(right > 1)
				rqsort(pn-right, right, x);
			n = left;
		}
	}
}

/* Validate every index before sorting so errors leave the permutation intact. */
void
Math_sort(void *fp)
{
	F_Math_sort *f;
	WORD *indices;
	int i, count, length;

	f = fp;
	count = mathvectorcount(f->pi, &Tword);
	length = mathvectorcount(f->x, &Treal);
	indices = (WORD*)f->pi->data;
	for(i = 0; i < count; i++)
		if(indices[i] < 0 || indices[i] >= length)
			error(exMathia);
	rqsort(indices, count, (double*)f->x->data);
}


/************ BLAS ***************/

/* Validate both vectors before calling native dot with an int count. */
void
Math_dot(void *fp)
{
	F_Math_dot *f;
	REAL result;
	int n;

	f = fp;
	n = mathvectorcount(f->x, &Treal);
	if(mathvectorcount(f->y, &Treal) != n)
		error(exMathia);
	result = dot(n, (double*)f->x->data, (double*)f->y->data);
	memmove(f->ret, &result, sizeof(result));
}

/* Return the maximum-magnitude index only after checking the full VM length. */
void
Math_iamax(void *fp)
{
	F_Math_iamax *f;
	WORD result;
	int n;

	f = fp;
	n = mathvectorcount(f->x, &Treal);
	result = iamax(n, (double*)f->x->data);
	memmove(f->ret, &result, sizeof(result));
}

/* Reject invalid real arrays before computing their Euclidean norm. */
void
Math_norm2(void *fp)
{
	F_Math_norm2 *f;
	REAL result;
	int n;

	f = fp;
	n = mathvectorcount(f->x, &Treal);
	result = norm2(n, (double*)f->x->data);
	memmove(f->ret, &result, sizeof(result));
}

/* Reject invalid real arrays before computing their absolute-value sum. */
void
Math_norm1(void *fp)
{
	F_Math_norm1 *f;
	REAL result;
	int n;

	f = fp;
	n = mathvectorcount(f->x, &Treal);
	result = norm1(n, (double*)f->x->data);
	memmove(f->ret, &result, sizeof(result));
}

/* Include the final active row and widen before multiplying leading dimensions. */
static void
mathmatrix(Array *array, int rows, int columns, WORD stride)
{
	int count;
	uvlong required;

	count = mathvectorcount(array, &Treal);
	if(stride < rows || stride < 0 || (uvlong)stride > 0x7fffffff)
		error(exBounds);
	if(rows == 0 || columns == 0)
		return;
	required = (uvlong)(columns-1)*(uvlong)stride+rows;
	if(required > (uvlong)count)
		error(exBounds);
}

/* Check complete matrix extents and native parameter widths before GEMM. */
void
Math_gemm(void *fp)
{
	F_Math_gemm *f;
	int m, n, k, nota, notb, active;
	double *a, *b;

	f = fp;
	if((f->transa != 'N' && f->transa != 'T' && f->transa != 'C') ||
	   (f->transb != 'N' && f->transb != 'T' && f->transb != 'C') ||
	   f->m < 0 || f->n < 0 || f->k < 0 ||
	   (uvlong)f->m > 0x7fffffff || (uvlong)f->n > 0x7fffffff ||
	   (uvlong)f->k > 0x7fffffff)
		error(exMathia);
	m = (int)f->m;
	n = (int)f->n;
	k = (int)f->k;
	nota = f->transa == 'N';
	notb = f->transb == 'N';
	active = m != 0 && n != 0 && k != 0 && f->alpha != 0;
	a = nil;
	b = nil;
	if(f->a != H && f->a != nil){
		mathmatrix(f->a, nota ? m : k, nota ? k : m, f->lda);
		a = (double*)f->a->data;
	}else if(active && m != k)
		error(exMathia);	/* A nil left matrix denotes a square identity. */
	if(f->b != H && f->b != nil){
		mathmatrix(f->b, notb ? k : n, notb ? n : k, f->ldb);
		b = (double*)f->b->data;
	}else if(active)
		error(exNilref);
	mathmatrix(f->c, m, n, f->ldc);
	gemm((int)f->transa, (int)f->transb, m, n, k, f->alpha,
		a, a == nil ? 0 : (int)f->lda, b, b == nil ? 0 : (int)f->ldb,
		f->beta, (double*)f->c->data, (int)f->ldc);
}
