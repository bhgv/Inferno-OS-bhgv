#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
#include <setjmp.h>

typedef int64_t Long;
typedef uint64_t uvlong;
typedef double Real;
typedef int64_t vlong;
#include "isa.h"
enum {Tint, Tbyte, Tbig, Treal, Tfix, Tstring, NumSize = 128};
typedef struct { int line; } Src;
typedef struct { int kind; double factor; struct {int start;} src; } Type;
static Type inttype={Tint,1};
static Type *tint=&inttype;
typedef struct { char *name; } Sym;
typedef struct { Sym *sym; } Decl;
typedef struct Node Node;
struct Node { Type *ty; Src src; Long val; double rval; Decl *decl; };
#define nil NULL // Native null identity.
static int checks;


/* Keep every regression oracle active when NDEBUG is defined. */
static void
check(int condition)
{
	if(!condition){
		fputs("cast folding check failed\n", stderr);
		exit(1);
	}
}

/* LeakSanitizer cannot inspect processes here; the fixture allocates no heap. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

/* Parsing is a bounded dependency; the production numeric lexer has its own tests. */
static Long
strtoi(char *text, int base)
{
	return strtoll(text, nil, base);
}

/* Bind scale lookup while compiling production conversion helpers. */
static double scale(Type *type) { return type->factor; }
static int tequal(Type *a, Type *b) { return a == b; }
static void *allocmem(int size) { (void)size; abort(); }
static char *seprint(char *p, char *end, char *format, ...) { (void)p; (void)end; (void)format; abort(); }
static Decl *enterstring(char *text, int n) { (void)text; (void)n; abort(); }
static Node *mksconst(Src *src, Decl *decl) { (void)src; (void)decl; abort(); }
static Node *foldcasti(Node*, Node*);
/* Valid fixture scales must not generate compiler diagnostics. */
static void compilererror(int line,const char *text) { (void)line; (void)text; abort(); }
/* Unexpected internal selection failures are test failures. */
static void fatal(const char *text) { (void)text; abort(); }
#define error compilererror
SELECTBODY
#undef error
IMPLEMENTATION
/* Use actual VM conversion code with a local error boundary and WORD32 storage. */
#define H ((void*)(uintptr_t)~(uintptr_t)0) // VM nil frame sentinel.
typedef double REAL;
typedef int32_t WORD;
typedef uint32_t UWORD;
typedef int64_t LONG;

/* Model the WORD32 runtime frame used by selected fixed instructions. */
static struct {
	void *s; // Source operand cell.
	void *m; // Middle operand cell.
	void *d; // Destination operand cell.
	char *FP; // Frame containing the selected coefficient.
} R;
static WORD frame[NREG+3];
static jmp_buf boundary;
static int vmerror;
static char exInval[]="invalid",exZdiv[]="zero",exNilref[]="nil frame";

/* Preserve the destination when the VM rejects an exceptional conversion. */
static void
error(char *text)
{
	check(text == exInval);
	vmerror = 1;
	longjmp(boundary, 1);
}
#define OP(n) static void n(void)
#define F(r) (*(REAL*)R.r)
#define W(r) (*(WORD*)R.r)
VMBODY

/* Canonical signed-32 arithmetic uses a wider intermediate as an independent oracle. */
static Long
int32value(Long value)
{
	uint32_t bits = (uint32_t)(uvlong)value;

	return bits & 0x80000000U ? (Long)bits-INT64_C(4294967296) : bits;
}

/* Check source normalization and every integer destination, including real output. */
static void
integers(void)
{
	Long edges[] = {0, 1, -1, 127, 128, 255, 256, -256, 300, -300,
		INT32_MIN, INT32_MAX, INT64_MIN, INT64_MAX, INT64_C(4294967295)};
	uvlong state = UINT64_C(0x6361737473);
	Long input, expected;
	int source, target, i;
	Type types[] = {{Tint}, {Tbyte}, {Tbig}, {Treal}};
	Node left, cast, *result;

	for(i = 0; i < 2048; i++) {
		state = state*UINT64_C(6364136223846793005)+1;
		memcpy(&input, &state, sizeof(input));
		if(i < sizeof(edges)/sizeof(edges[0])) input = edges[i];
		for(source = Tint; source <= Tbig; source++)
			for(target = Tint; target <= Treal; target++) {
				left = (Node){&types[source], {1}, input, 0, nil};
				cast = (Node){&types[target], {99}, 0, 0, nil};
				expected = source == Tint ? int32value(input) : source == Tbyte ? (Long)((uvlong)input&255) : input;
				if(target == Tint) expected = int32value(expected);
				if(target == Tbyte) expected = (uvlong)expected&255;
				result = foldcast(&cast, &left);
				check(result == &left && left.ty == &types[target] && left.src.line == 99);
				if(target == Treal) check(left.rval == (double)expected);
				else check(left.val == expected);
				checks++;
			}
	}
}

/* Fold portable string values and retain target-dependent prefixes as runtime casts. */
static void
strings(void)
{
	Long values[] = {0, 255, 256, 300, -1, -300, INT32_MIN, INT32_MAX,
		INT64_C(4294967295), INT64_MIN, INT64_MAX};
	Type source = {Tstring}, target;
	char text[64];
	Sym symbol = {text};
	Decl decl = {&symbol};
	Node left, cast, original;
	Long expected;
	unsigned int i;
	int kind;

	for(i = 0; i < sizeof(values)/sizeof(values[0]); i++)
		for(kind = Tint; kind <= Tbig; kind++) {
			snprintf(text, sizeof(text), "%lld", (long long)values[i]);
			target.kind = kind;
			left = (Node){&source, {1}, 0, 0, &decl};
			cast = (Node){&target, {99}, 0, 0, nil};
			expected = kind == Tint ? int32value(values[i]) : kind == Tbyte ? (Long)((uvlong)values[i]&255) : values[i];
			original = left;
			if(kind != Tbig && (values[i] < INT32_MIN || values[i] > INT32_MAX)){
				check(foldcast(&cast, &left) == &cast);
				check(memcmp(&left, &original, sizeof left) == 0);
			}else{
				check(foldcast(&cast, &left) == &left);
				check(left.val == expected && left.ty == &target && left.src.line == 99);
			}
			checks++;
		}
}

/* Preserve the original AST when a native rounded conversion is not representable. */
static void
realcase(double value, int kind)
{
	Type source = {Treal}, target = {kind};
	Node left = {&source, {1}, 123, value, nil}, original, cast = {&target, {99}, 0, 0, nil};
	Node *result;
	double adjusted;
	long double rounded;
	Long expected;
	int valid;

	memcpy(&original, &left, sizeof(left));
	adjusted = value < 0 ? value-.5 : value+.5;
	rounded = truncl((long double)adjusted);
	valid = isfinite(adjusted) && rounded >= -9223372036854775808.0L && rounded < 9223372036854775808.0L;
	if(kind != Tbig) valid = valid && rounded >= -2147483648.0L && rounded < 2147483648.0L;
	result = foldcast(&cast, &left);
	if(!valid) {
		check(result == &cast && memcmp(&original, &left, sizeof(left)) == 0);
	}else {
		expected = (Long)rounded;
		if(kind == Tbyte) expected = (uvlong)expected&255;
		check(result == &left && left.val == expected && left.ty == &target && left.src.line == 99);
	}
	checks++;
}

/* Match exact fixed instruction rounding while retaining compile-time constants. */
static void
fixed(void)
{
	Type source = {Tfix,.25}, target;
	Node left, cast;
	Long expected;
	int kind, value;

	for(kind = Tint; kind <= Tfix; kind++)
		for(value = -4096; value <= 4096; value++) {
			if(kind == Treal) continue;
			target = (Type){kind, .5};
			left = (Node){&source, {1}, value, 0, nil};
			cast = (Node){&target, {99}, 0, 0, nil};
			expected = kind==Tfix ? value>>1 : value>>2;
			if(kind==Tbig) expected=(Long)(value*.25+(value<0?-.5:.5));
			if(kind==Tbyte) expected &= 255;
			check(foldcast(&cast,&left) == &left);
			check(left.val==expected && left.ty==&target && left.src.line==99);
			checks++;
		}
	left = (Node){&source,{1},-1,0,nil};
	cast = (Node){&source,{99},0,0,nil};
	check(foldcast(&cast,&left) == &left); checks++;
}

/* Int/byte inputs use fixed instructions; big inputs follow the real precast. */
static void
fixedinputs(void)
{
	Type source, target={Tfix,.25};
	Node left,cast,original;
	int kind, value;

	for(kind=Tint;kind<=Tbig;kind++) for(value=-255;value<=255;value++) {
		source=(Type){kind,1};
		left=(Node){&source,{1},kind==Tbyte ? value&255 : value,0,nil};
		original=left;cast=(Node){&target,{99},0,0,nil};
		check(foldcast(&cast,&left)==&left && left.val==original.val*4);
		checks++;
	}
}


/* Cross-check fixed constant results with the exact selected VM instruction. */
static void
fixedpipeline(void)
{
	double factors[]={.03125,.1,.25,.5,1,1.5,3,17};
	Type source={Tfix,1},target={Tfix,1};
	Node left,cast;
	WORD input,count,output;
	int i,j,k,op,p,a;

	for(i=0;i<8;i++) for(j=0;j<8;j++) for(k=-1024;k<=1024;k++) {
		source.factor=factors[i];target.factor=factors[j];
		left=(Node){&source,{1},k,0,nil};cast=(Node){&target,{99},0,0,nil};
		check(foldcast(&cast,&left)==&left);
		op=fixop(ICVTXX,&source,tint,&target,&p,&a);
		input=k;count=p;output=123;
		R.FP=(char*)frame;
		frame[NREG]=a;
		R.s=&input;R.m=&count;R.d=&output;
		if(op==IMOVW) output=0;
		else if(op==ICVTXX) cvtxx();
		else if(op==ICVTXX0) cvtxx0();
		else { check(op==ICVTXX1);cvtxx1(); }
		check(left.val==output);checks++;
	}
}

/* Compare compiler real/string folding directly with the production WORD32 VM. */
static void
fixedtargets(void)
{
	Type source={Treal,1},target={Tfix,1};
	double scales[]={1,.25,.5,1.5,3,1e-300,1e300,0x1p-1074};
	double edges[]={NAN,INFINITY,-INFINITY,0,-0.,.125,-.125,.5,-.5,
		2147483647.,2147483647.5,-2147483648.,-2147483648.5,1e300,-1e300};
	Node left,original,cast,*result;
	char text[64]; Sym symbol={text}; Decl decl={&symbol};
	double input,factor,rounded;
	int32_t output;
	int i,j,mode,valid;

	for(j=0;j<8;j++) for(i=-4096;i<(int)(sizeof(edges)/sizeof(edges[0]));i++) {
		input=i<0?i*.125:edges[i];target.factor=scales[j];factor=1/scales[j];
		for(mode=0;mode<2;mode++) {
			source.kind=mode?Tstring:Treal;
			if(mode) snprintf(text,sizeof(text),"%.17g",input);
			left=(Node){&source,{1},123,input,mode?&decl:nil};
			original=left;cast=(Node){&target,{99},0,0,nil};
			rounded=input*factor;rounded=rounded<0?rounded-.5:rounded+.5;
			valid=isfinite(rounded) && truncl((long double)rounded)>=INT32_MIN &&
				truncl((long double)rounded)<=INT32_MAX;
			result=foldcast(&cast,&left);
			output=12345; R.s=&input;R.m=&factor;R.d=&output;vmerror=0;
			if(setjmp(boundary)==0) cvtfx();
			if(valid) {
				check(!vmerror && result==&left && left.val==output);
				check(left.ty==&target && left.src.line==99);
			}else {
				check(vmerror && output==12345 && result==&cast);
				check(!memcmp(&original,&left,sizeof(left)));
			}
			checks++;
		}
	}
}

/* Exercise ties, byte wrapping, signed boundaries, NaNs and infinities. */
int
main(void)
{
	double edges[] = {NAN, INFINITY, -INFINITY, 0, -0.0, 255.5, -255.5,
		2147483647.0, 2147483647.5, -2147483648.0, -2147483648.5,
		-2147483648.49, -2147483649.0, 9223372036854775808.0,
		-9223372036854775808.0, 1e300, -1e300};
	unsigned int i;
	int kind, step;

	integers();
	strings();
	fixed();
	fixedinputs();
	fixedtargets();
	fixedpipeline();
	for(kind = Tint; kind <= Tbig; kind++) {
		for(step = -4096; step <= 4096; step++) realcase(step*.25, kind);
		for(i = 0; i < sizeof(edges)/sizeof(edges[0]); i++) realcase(edges[i], kind);
		realcase(nextafter(9223372036854775808.0, 0), kind);
		realcase(nextafter(-9223372036854775808.0, -INFINITY), kind);
	}
	printf("PASS compiler casts: %d source/target/narrowing/rounding/deferred-AST cases\n", checks);
	return 0;
}
