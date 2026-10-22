#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "kernel.h"
#include "runt.h"
#include "raise.h"
#include <assert.h>
#include <stdio.h>

static Proc worker;
static char *raised;
static int checks, live, allocations, failalloc, vmfail, vmlive;
static void *owners[16];
char exInval[] = "invalid", exNomem[] = "memory";

/* Preserve the real emulator error stack across protected formatting calls. */
Proc *getup(void) { return &worker; }
void nexterror(void) {
	assert(worker.nerr > 0);
	siglongjmp(worker.estack[--worker.nerr], 1);
}
void error(char *message) { raised = message; nexterror(); }
/* Native error text is deterministic and respects an empty destination. */
char *syserr(char *p, char *end, Prog *prog) {
	(void)prog;
	if(p < end)*p++ = '!';
	return p;
}
void rerrstr(char *p, uint n) { if(n)snprintf(p, n, "fixture"); }
/* Count only the buffers owned by production bigxprint, with bounded allocation. */
static void *outputmalloc(size_t n) {
	void *p;
	allocations++;
	if(failalloc || n > 16384)return nil;
	p = malloc(n); assert(p); live++;
	return p;
}
static void outputfree(void *p) { if(p){ assert(live > 0); live--; free(p); } }
/* LeakSanitizer cannot inspect processes in this sandbox. */
int __lsan_is_turned_off(void) { return 1; }
/* Bound VM result allocation while retaining actual Array/String declarations. */
static void *vmalloc(size_t n) {
	void *p;
	int i;
	if(vmfail)error(exNomem);
	p = calloc(1, n); assert(p);
	for(i = 0; i < 16 && owners[i]; i++);
	assert(i < 16); owners[i] = p; vmlive++;
	return p;
}
String *c2string(char *text, int n) {
	String *s;
	if(n == 0)return H;
	s = vmalloc(sizeof(String)+n+1); s->len = n;
	memmove(s->Sascii, text, n); return s;
}
Array *mem2array(void *text, int n) {
	Array *a = vmalloc(sizeof(Array)+n);
	a->len = n; a->data = (uchar*)(a+1);
	memmove(a->data, text, n); return a;
}
void destroy(void *p) {
	int i;
	if(p == H)return;
	for(i = 0; i < 16 && owners[i] != p; i++);
	assert(i < 16); owners[i] = nil; vmlive--; free(p);
}
Prog *currun(void) { return nil; }
GUARD
#define malloc outputmalloc
#define free outputfree
IMPLEMENTATION
#undef malloc
#undef free

/* Construct both actual VM string representations with exact payload bounds. */
static String*
string(char *text, int wide)
{
	String *s;
	int n, i;

	n = strlen(text);
	s = calloc(1, sizeof(String)+(n+1)*sizeof(Rune)); assert(s);
	s->len = wide ? -n : n;
	for(i = 0; i < n; i++){
		if(wide)s->Srune[i] = (uchar)text[i];
		else s->Sascii[i] = text[i];
	}
	return s;
}

/* Literal/argument processing must not move before a full output span. */
static void
bounded(void)
{
	char output[70], format[600];
	String *s;
	int args[512];
	int capacity, wide, n, i;

	memset(args, 0, sizeof(args));
	for(i = 0; i < 512; i++)args[i] = 7;
	for(i = 0; i < 100; i++)memmove(format+3*i, "a%d", 3);
	format[300] = 0;
	for(wide = 0; wide < 2; wide++){
		s = string(format, wide);
		for(capacity = 0; capacity <= 64; capacity++){
			memset(output, 0xa5, sizeof(output));
			n = xprint(nil, args, args, s, output+1, capacity);
			assert(n >= 0 && n <= (capacity > 0 ? capacity-1 : 0));
			for(i = 0; i < n; i++)assert(output[i+1] == (i%2 ? '7' : 'a'));
			assert((uchar)output[0] == 0xa5);
			for(i = capacity+1; i < sizeof(output); i++)assert((uchar)output[i] == 0xa5);
			checks++;
		}
		free(s);
		s = string("plain%", wide);
		n = xprint(nil, args, args, s, output, sizeof(output));
		assert(n == 5 && !memcmp(output, "plain", 5));
		free(s);
		s = string("plain%123", wide);
		n = xprint(nil, args, args, s, output, sizeof(output));
		assert(n == 5 && !memcmp(output, "plain", 5));
		free(s);
	}
	assert(xprint(nil, nil, nil, H, nil, 0) == 0);
	assert(xprint(nil, nil, nil, H, nil, -1) == 0);
}

/* Compare ordinary literals, dynamic widths and aligned integer arguments. */
static void
ordinary(void)
{
	String *s;
	char output[256];
	int args[8] = {5, 7, 8};
	int wide, n;

	for(wide = 0; wide < 2; wide++){
		s = string("[%*d:%d:%%:%r]", wide);
		n = xprint(nil, args, args, s, output, sizeof(output));
		assert(n == 13 && !memcmp(output, "[    7:8:%:!]", 13));
		free(s); checks++;
	}
}

/* Keep packed Dis words and aligned 64-bit integer arguments in distinct slots. */
static void
mixedintegers(void)
{
	union { vlong align; uchar bytes[32]; } frame;
	vlong big;
	int first = -7, last = 9, n;
	String *s;
	char output[128], expected[128];

	memset(&frame, 0, sizeof(frame));
	big = (-9223372036854775807LL-1);
	memmove(frame.bytes, &first, 4);
	memmove(frame.bytes+8, &big, 8);
	memmove(frame.bytes+16, &last, 4);
	s = string("%d:%bd:%d", 0);
	snprintf(expected, sizeof(expected), "%d:%lld:%d", first, (long long)big, last);
	n = xprint(nil, frame.bytes, frame.bytes, s, output, sizeof(output));
	assert(n == strlen(expected) && !memcmp(output, expected, n));
	free(s); checks++;
}

/* Reject overlong specifications and overflowing growth without losing owners. */
static void
failures(void)
{
	String *s;
	char format[256], output[64], *result;
	int args[128];
	int mode, wide, before;

	memset(args, 0, sizeof(args));
	for(wide = 0; wide < 2; wide++) for(mode = 0; mode < 8; mode++){
		memset(format, '0', 200); format[0] = '%'; format[200] = 'd'; format[201] = 0;
		if(mode == 1)memset(format+1, 'b', 199);
		if(mode == 2){ strcpy(format, "%00000000000000000000*d"); args[0] = 1; }
		if(mode >= 3 && mode != 6)strcpy(format, "ordinary");
		if(mode == 7){ strcpy(format, "%*d"); args[0] = 0x7fffffff; }
		s = string(format, wide); result = (char*)1;
		before = live; raised = nil;
		if(waserror()){
			assert(live == before && result == (char*)1);
			assert(raised == (mode == 3 || mode == 4 || mode == 7 ? exNomem : exInval));
		}else{
			if(mode == 0 || mode == 1)xprint(nil, args, args, s, output, sizeof(output));
			if(mode == 2)xprint(nil, args, args, s, output, sizeof(output));
			if(mode == 3)bigxprint(nil, args, args, s, &result, 0x7fffffff);
			if(mode == 4){ failalloc = 1; bigxprint(nil, args, args, s, &result, 16); }
			if(mode == 5)bigxprint(nil, args, args, s, &result, 0);
			if(mode == 6)bigxprint(nil, args, args, s, &result, 16);
			if(mode == 7)bigxprint(nil, args, args, s, &result, 16);
			assert(0);
		}
		failalloc = 0; free(s); checks++;
	}
}

/* Dynamic output grows through multiple retries and transfers one final owner. */
static void
growing(void)
{
	String *s;
	char text[1025], *result;
	int args[1] = {0};
	int n;

	memset(text, 'x', 1024); text[1024] = 0;
	s = string(text, 0); result = nil;
	n = bigxprint(nil, args, args, s, &result, 16);
	assert(n == 1024 && !memcmp(result, text, n));
	assert(live == 1 && allocations > 1);
	outputfree(result); free(s); checks++;
}

/* Fail VM conversion after both stack and grown formatting without losing owners. */
static void
results(void)
{
	F_Sys_sprint f = {0};
	F_Sys_aprint g = {0};
	String *format;
	static String *text;
	static Array *bytes;
	char input[1025];
	void *old;
	int op, grown, fail, n;

	for(op = 0; op < 2; op++) for(grown = 0; grown < 2; grown++) for(fail = 0; fail < 2; fail++){
		n = grown ? 1024 : 8;
		memset(input, 'a', n); input[n] = 0;
		format = string(input, 0);
		if(op == 0){ text = c2string("old", 3); old = text; f.ret = &text; f.s = format; }
		else { bytes = mem2array("old", 3); old = bytes; g.ret = &bytes; g.s = format; }
		vmfail = fail;
		if(waserror()){
			assert(fail && raised == exNomem && live == 0 && vmlive == 1);
			assert((op == 0 ? (void*)text : (void*)bytes) == old);
		}else{
			if(op == 0)Sys_sprint(&f); else Sys_aprint(&g);
			poperror();
			assert(!fail && live == 0 && vmlive == 1);
			if(op == 0)assert(text->len == n && !memcmp(text->Sascii, input, n));
			else assert(bytes->len == n && !memcmp(bytes->data, input, n));
		}
		vmfail = 0;
		destroy(op == 0 ? (void*)text : (void*)bytes);
		free(format); checks++;
	}
}

/* Use real formatting and VM errors rather than a printf approximation. */
int
main(void)
{
	bounded(); ordinary(); mixedintegers(); failures(); growing(); results();
	assert(worker.nerr == 0 && live == 0 && vmlive == 0);
	printf("PASS Sys print: %d bounds/format/growth/ownership cases, WORD%zu\n", checks, sizeof(WORD)*8);
	return 0;
}
