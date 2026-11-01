#define LIMBO_NATIVE64_IMPLEMENTATIONS // Complete trusted callback list.
#include "math-native64.h"

typedef struct Native64MathTuple Native64MathTuple;

struct Native64MathTuple
{
	WORD t0; // Independent int32 tuple component.
	REAL t1; // Naturally aligned double component after ABI padding.
};

#ifdef NATIVE64_NO_LSAN


/* Optional host limitation: process leak inspection is unavailable; allocation checks remain
 * active. */
int
__lsan_is_turned_off(void)
{
	return 1;
}
#endif

Type Tbyte = {.size = 1}; // Canonical primitive identity for borrowed byte arrays.
Type Tword = {.size = 4}; // Canonical int32 identity, independent of host long.
Type Tlong = {.size = 8}; // Canonical big identity.
Type Treal = {.size = 8}; // Canonical double identity.
char exBounds[] = "bounds"; // Observable owning Math error category.
char exInval[] = "invalid"; // Observable tuple conversion error.
char exMathia[] = "Math argument"; // Observable checked-vector error.
char exNilref[] = "nil"; // Observable nil-array error.
char exType[] = "type"; // Observable canonical primitive mismatch.
static int formats; // Count formatter installation after complete module publication.
static int invoked[68]; // Independently require every generated export to execute.
extern void mathmodinit(void);
extern int gfltconv(Fmt *);

/* Observe only formatting installation; the complete owning numeric implementation is linked. */
int
__wrap_fmtinstall(int rune, int (*convert)(Fmt *))
{
	check(modules != &baseline && modules->ntype == 68 && convert == gfltconv);
	check(rune == 'g' || rune == 'G' || rune == 'e' || rune == 0x00c9 || rune == 'f');
	formats++;
	return 0;
}


/* Preserve the real outer VM-error boundary when the initializer reports construction failure. */
static int
startmath(void *unused)
{
	USED(unused);
	mathmodinit();
	return 1;
}


/* Resolve the published export rather than calling a generated C record directly. */
static void
invoke(char *name, void *frame)
{
	int i;

	for (i = 0; i < 68; i++)
		if (strcmp(modules->ext[i].name, name) == 0) {
			invoked[i]++;
			modules->ext[i].u.runt(frame);
			return;
		}
	check(0);
}


/* A fixed tolerance checks known numeric values without invoking a second copy of the same
 * algorithm. */
static int
near(REAL left, REAL right)
{
	REAL delta;

	delta = left - right;
	return delta >= -1e-12 && delta <= 1e-12;
}

#define UNARY(name, input, expected)                                                               \
	do {                                                                                           \
		F_Math_##name f = {0};                                                                     \
		REAL result;                                                                               \
		f.ret = &result;                                                                           \
		f.x = input;                                                                               \
		invoke(#name, &f);                                                                         \
		check(near(result, expected));                                                             \
	} while (0) // Actual unary frame and a fixed numeric oracle.
#define BINARY(name, left, right, expected)                                                        \
	do {                                                                                           \
		F_Math_##name f = {0};                                                                     \
		REAL result;                                                                               \
		f.ret = &result;                                                                           \
		f.x = left;                                                                                \
		f.y = right;                                                                               \
		invoke(#name, &f);                                                                         \
		check(near(result, expected));                                                             \
	} while (0) // Actual binary frame with x/y fields.
#define INTEGER(name, input, expected)                                                             \
	do {                                                                                           \
		F_Math_##name f = {0};                                                                     \
		WORD result;                                                                               \
		f.ret = &result;                                                                           \
		f.x = input;                                                                               \
		invoke(#name, &f);                                                                         \
		check(result == expected);                                                                 \
	} while (0) // Actual int32 return through its generated slot.

/* Execute scalar, mixed-width and tuple exports through generated frames and owned runtime links.
 */
static void
scalars(void)
{
	UNARY(acos, 1, 0);
	UNARY(acosh, 1, 0);
	UNARY(asin, 0, 0);
	UNARY(asinh, 0, 0);
	UNARY(atan, 0, 0);
	UNARY(atanh, 0, 0);
	UNARY(cbrt, 8, 2);
	UNARY(ceil, 1.25, 2);
	UNARY(cos, 0, 1);
	UNARY(cosh, 0, 1);
	UNARY(erf, 0, 0);
	UNARY(erfc, 0, 1);
	UNARY(exp, 0, 1);
	UNARY(expm1, 0, 0);
	UNARY(fabs, -2, 2);
	UNARY(floor, 1.25, 1);
	UNARY(j0, 0, 1);
	UNARY(j1, 0, 0);
	UNARY(log, 1, 0);
	UNARY(log10, 1, 0);
	UNARY(log1p, 0, 0);
	UNARY(rint, 2, 2);
	UNARY(sin, 0, 0);
	UNARY(sinh, 0, 0);
	UNARY(sqrt, 4, 2);
	UNARY(tan, 0, 0);
	UNARY(tanh, 0, 0);
	UNARY(y0, 1, 0.08825696421567696);
	UNARY(y1, 1, -0.7812128213002887);
	BINARY(fdim, 5, 2, 3);
	BINARY(fmin, 5, 2, 2);
	BINARY(fmax, 5, 2, 5);
	BINARY(fmod, 5, 2, 1);
	BINARY(hypot, 3, 4, 5);
	BINARY(nextafter, 2, 2, 2);
	BINARY(pow, 2, 3, 8);
	INTEGER(finite, 2, 1);
	INTEGER(isnan, 2, 0);
	INTEGER(ilogb, 8, 3);
	{
		F_Math_atan2 f = {0};
		REAL r;
		f.ret = &r;
		f.y = 0;
		f.x = 1;
		invoke("atan2", &f);
		check(r == 0);
	}
	{
		F_Math_copysign f = {0};
		REAL r;
		f.ret = &r;
		f.x = 2;
		f.s = -1;
		invoke("copysign", &f);
		check(r == -2);
	}
	{
		F_Math_remainder f = {0};
		REAL r;
		f.ret = &r;
		f.x = 5;
		f.p = 2;
		invoke("remainder", &f);
		check(r == 1);
	}
	{
		F_Math_scalbn f = {0};
		REAL r;
		f.ret = &r;
		f.x = 2;
		f.n = 3;
		invoke("scalbn", &f);
		check(r == 16);
	}
	{
		F_Math_pow10 f = {0};
		REAL r;
		f.ret = &r;
		f.p = 2;
		invoke("pow10", &f);
		check(r == 100);
	}
	{
		F_Math_jn f = {0};
		REAL r;
		f.ret = &r;
		f.n = 0;
		f.x = 0;
		invoke("jn", &f);
		check(r == 1);
	}
	{
		F_Math_yn f = {0};
		REAL r;
		f.ret = &r;
		f.n = 0;
		f.x = 1;
		invoke("yn", &f);
		check(near(r, 0.08825696421567696));
	}
	{
		F_Math_lgamma f = {0};
		Native64MathTuple r = {0};
		f.ret = (void *)&r;
		f.x = 1;
		invoke("lgamma", &f);
		check(r.t0 == 1 && r.t1 == 0);
	}
	{
		F_Math_modf f = {0};
		Native64MathTuple r = {0};
		f.ret = (void *)&r;
		f.x = 3.25;
		invoke("modf", &f);
		check(r.t0 == 3 && r.t1 == 0.25);
	}
	{
		F_Math_bits32real f = {0};
		REAL r;
		f.ret = &r;
		f.b = 0x3f800000;
		invoke("bits32real", &f);
		check(r == 1);
	}
	{
		F_Math_bits64real f = {0};
		REAL r;
		f.ret = &r;
		f.b = 0x3ff0000000000000LL;
		invoke("bits64real", &f);
		check(r == 1);
	}
	{
		F_Math_realbits32 f = {0};
		WORD r;
		f.ret = &r;
		f.x = 1;
		invoke("realbits32", &f);
		check(r == 0x3f800000);
	}
	{
		F_Math_realbits64 f = {0};
		LONG r;
		f.ret = &r;
		f.x = 1;
		invoke("realbits64", &f);
		check(r == 0x3ff0000000000000LL);
	}
	{
		F_Math_getFPcontrol f = {0};
		WORD r;
		f.ret = &r;
		invoke("getFPcontrol", &f);
	}
	{
		F_Math_getFPstatus f = {0};
		WORD r;
		f.ret = &r;
		invoke("getFPstatus", &f);
	}
	{
		F_Math_FPcontrol f = {0};
		WORD r;
		f.ret = &r;
		f.r = 0;
		f.mask = 0;
		invoke("FPcontrol", &f);
	}
	{
		F_Math_FPstatus f = {0};
		WORD r;
		f.ret = &r;
		f.r = 0;
		f.mask = 0;
		invoke("FPstatus", &f);
	}
}


/* Borrow ordinary C payloads at the checked array boundary; this fixture does not substitute a
 * collector. */
static Array
array(void *data, int length, Type *type)
{
	Array result = {0};

	result.t = type;
	result.len = length;
	result.data = data;
	result.root = H;
	return result;
}


/* Exercise all vector/conversion exports and a real two-by-two column-major GEMM. */
static void
vectors(void)
{
	REAL values[] = {3, 4}, other[] = {2, 1}, matrix[] = {1, 2, 3, 4}, identity[] = {1, 0, 0, 1},
		 output[4] = {0};
	WORD indices[] = {0, 1}, integers[] = {-1, 2};
	uchar bytes[16] = {0};
	Array x = array(values, 2, &Treal), y = array(other, 2, &Treal), pi = array(indices, 2, &Tword);
	Array words = array(integers, 2, &Tword), raw = array(bytes, 8, &Tbyte);
	REAL result;
	WORD index;
	{
		F_Math_dot f = {0};
		f.ret = &result;
		f.x = &x;
		f.y = &y;
		invoke("dot", &f);
		check(result == 10);
	}
	{
		F_Math_norm1 f = {0};
		f.ret = &result;
		f.x = &x;
		invoke("norm1", &f);
		check(result == 7);
	}
	{
		F_Math_norm2 f = {0};
		f.ret = &result;
		f.x = &x;
		invoke("norm2", &f);
		check(result == 5);
	}
	{
		F_Math_iamax f = {0};
		f.ret = &index;
		f.x = &x;
		invoke("iamax", &f);
		check(index == 1);
	}
	{
		F_Math_sort f = {0};
		indices[0] = 1;
		indices[1] = 0;
		f.x = &x;
		f.pi = &pi;
		invoke("sort", &f);
		check(indices[0] == 0 && indices[1] == 1);
	}
	{
		F_Math_export_int f = {0};
		f.b = &raw;
		f.x = &words;
		invoke("export_int", &f);
		check(bytes[0] == 255 && bytes[7] == 2);
	}
	{
		F_Math_import_int f = {0};
		integers[0] = integers[1] = 0;
		f.b = &raw;
		f.x = &words;
		invoke("import_int", &f);
		check(integers[0] == -1 && integers[1] == 2);
	}
	{
		F_Math_export_real32 f = {0};
		f.b = &raw;
		f.x = &x;
		invoke("export_real32", &f);
		check(bytes[0] == 0x40 && bytes[1] == 0x40);
	}
	{
		F_Math_import_real32 f = {0};
		f.b = &raw;
		f.x = &y;
		invoke("import_real32", &f);
		check(other[0] == 3 && other[1] == 4);
	}
	raw.len = 16;
	{
		F_Math_export_real f = {0};
		f.b = &raw;
		f.x = &x;
		invoke("export_real", &f);
		check(bytes[0] == 0x40 && bytes[1] == 0x08);
	}
	{
		F_Math_import_real f = {0};
		f.b = &raw;
		f.x = &y;
		invoke("import_real", &f);
		check(other[0] == 3 && other[1] == 4);
	}
	{
		F_Math_gemm f = {0};
		Array a = array(matrix, 4, &Treal), b = array(identity, 4, &Treal),
			  c = array(output, 4, &Treal);
		f.transa = f.transb = 'N';
		f.m = f.n = f.k = 2;
		f.alpha = 1;
		f.beta = 0;
		f.a = &a;
		f.b = &b;
		f.c = &c;
		f.lda = f.ldb = f.ldc = 2;
		invoke("gemm", &f);
		check(memcmp(matrix, output, sizeof(matrix)) == 0);
	}
}


/* Inject every construction failure before checking every actual exported callback. */
int
main(void)
{
	Module *module;
	int cutoff, success, i, byte, failures = 0;

	baseline.ref = 7;
	baseline.path = "$Baseline";
	check(limbo_native64_Math_implementation_count == 68 && limbo_native64_Math_spec_count == 68);
	for (cutoff = 0; cutoff < 143; cutoff++) {
		formats = 0;
		fail_after = cutoff;
		success = kprotect(startmath, nil);
		fail_after = -1;
		if (success) {
			module = modules;
			check(formats == 5 && module->ntype == 68 && module->ext[68].name == nil);
			for (i = 0; i < 68; i++) {
				const LimboNative64Entry *entry = limbo_native64_Math_implementations[i];
				check(strcmp(module->ext[i].name, entry->name) == 0);
				check(module->ext[i].u.runt == entry->function &&
					  module->type[i]->size == entry->frame_size);
				check(module->type[i]->np == entry->map_bytes);
				for (byte = 0; byte < entry->map_bytes; byte++)
					check(module->type[i]->map[byte] == entry->map[byte]);
			}
			unload(module);
		} else {
			failures++;
			check(formats == 0);
		}
		check(modules == &baseline && live == 0 && worker.nerr == 0 && baseline.ref == 7);
	}
	check(failures == 141);
	formats = 0;
	check(kprotect(startmath, nil));
	scalars();
	vectors();
	for (i = 0; i < 68; i++)
		check(invoked[i] == 1);
	unload(modules);
	check(modules == &baseline && live == 0 && worker.nerr == 0);
	printf("PASS complete native64 Math: 68 registered and invoked exports, 141 allocation "
		   "refusals, maps and formatter ordering\n");
	return 0;
}
