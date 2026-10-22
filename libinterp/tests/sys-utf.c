#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "runt.h"
#include <assert.h>
#include <stdio.h>
#include <setjmp.h>

char exBounds[] = "bounds", exType[] = "type";
Type Tarray = {1}, Tbyte = {1}, other = {1};
static jmp_buf boundary;
static int checks;
static char *raised;

/* Deliver VM errors to a bounded fixture without mutating the output. */
void
error(char *s)
{
	raised = s;
	longjmp(boundary, 1);
}

/* The sandbox cannot provide LeakSanitizer process inspection. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

IMPLEMENTATION

/* Allocate real array/heap declarations with a separate guarded byte span. */
static Array*
array(uchar *data, int length)
{
	Heap *h;
	Array *a;

	h = calloc(1, sizeof(Heap)+sizeof(Array));
	assert(h);
	h->t = &Tarray;
	a = H2D(Array*, h);
	a->t = &Tbyte;
	a->data = data;
	a->len = length;
	return a;
}

/* Encode scalar values independently of the linked production rune routines. */
static int
encode(unsigned c, uchar *p)
{
	if(c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff))
		c = 0xfffd;
	if(c < 0x80){ p[0] = c; return 1; }
	if(c < 0x800){ p[0] = 0xc0 | (c>>6); p[1] = 0x80 | (c&63); return 2; }
	if(c < 0x10000){
		p[0] = 0xe0 | (c>>12); p[1] = 0x80 | ((c>>6)&63); p[2] = 0x80 | (c&63); return 3;
	}
	p[0] = 0xf0 | (c>>18); p[1] = 0x80 | ((c>>12)&63);
	p[2] = 0x80 | ((c>>6)&63); p[3] = 0x80 | (c&63);
	return 4;
}

/* Compare every Unicode scalar and surrogate with an independent byte encoder. */
static void
scalars(void)
{
	F_Sys_char2byte f = {0};
	F_Sys_byte2char g = {0};
	F_Sys_utfbytes u = {0};
	uchar data[6], expected[4];
	unsigned c;
	int n, i;
	WORD result;
	Array *a;

	a = array(data+1, 4);
	f.buf = g.buf = u.buf = a;
	f.ret = &result;
	g.ret = calloc(1, sizeof(*g.ret));
	assert(g.ret);
	u.ret = &result;
	for(c = 0; c <= 0x110000; c++){
		memset(data, 0xa5, sizeof(data));
		n = encode(c, expected);
		f.c = c;
		Sys_char2byte(&f);
		assert(result == n && !memcmp(data+1, expected, n));
		assert(data[0] == 0xa5);
		for(i = n+1; i < sizeof(data); i++)
			assert(data[i] == 0xa5);
		a->len = n;
		Sys_byte2char(&g);
		assert(g.ret->t0 == (c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff) ? 0xfffd : c));
		assert(g.ret->t1 == n && g.ret->t2 == 1);
		for(i = 0; i <= n; i++){
			u.n = i;
			Sys_utfbytes(&u);
			assert(result == (i == n ? n : 0));
		}
		if(n > 1){
			a->len = n-1;
			Sys_byte2char(&g);
			assert(g.ret->t0 == Runeerror && g.ret->t1 == 0 && g.ret->t2 == 0);
			memset(data, 0xa5, sizeof(data));
			Sys_char2byte(&f);
			assert(result == 0);
			for(i = 0; i < sizeof(data); i++)
				assert(data[i] == 0xa5);
		}
		a->len = 4;
		checks++;
	}
	free(g.ret);
	free(D2H(a));
}

/* Malformed encodings consume one byte; offsets preserve surrounding bytes. */
static void
malformed(void)
{
	static uchar invalid[][4] = {
		{0x80, 0, 0, 0}, {0xc0, 0x80, 0, 0}, {0xc2, 0x41, 0, 0},
		{0xe0, 0x80, 0x80, 0}, {0xed, 0xa0, 0x80, 0},
		{0xf0, 0x80, 0x80, 0x80}, {0xf4, 0x90, 0x80, 0x80}, {0xff, 0, 0, 0}
	};
	F_Sys_byte2char g = {0};
	F_Sys_char2byte f = {0};
	uchar data[8];
	Array *a;
	WORD result;
	int i;

	a = array(data, sizeof(data));
	g.buf = f.buf = a;
	g.n = f.n = 2;
	g.ret = calloc(1, sizeof(*g.ret));
	assert(g.ret);
	f.ret = &result;
	for(i = 0; i < sizeof(invalid)/sizeof(invalid[0]); i++){
		memset(data, 0xa5, sizeof(data));
		memmove(data+2, invalid[i], 4);
		Sys_byte2char(&g);
		assert(g.ret->t0 == Runeerror && g.ret->t1 == 1 && g.ret->t2 == 0);
		checks++;
	}
	memset(data, 0xa5, sizeof(data));
	f.c = 0x10ffff;
	Sys_char2byte(&f);
	assert(result == 4 && !memcmp(data+2, "\xf4\x8f\xbf\xbf", 4));
	assert(data[0] == 0xa5 && data[1] == 0xa5 && data[6] == 0xa5 && data[7] == 0xa5);
	Sys_byte2char(&g);
	assert(g.ret->t0 == 0x10ffff && g.ret->t1 == 4 && g.ret->t2 == 1);
	free(g.ret);
	free(D2H(a));
}

/* Invalid full-word counts and array metadata preserve all outputs and bytes. */
static void
bounds(void)
{
	F_Sys_char2byte f = {0};
	F_Sys_byte2char g = {0};
	F_Sys_utfbytes u = {0};
	Array *a;
	uchar data[8];
	WORD result, offset;
	int op, mode, i;

	a = array(data, 8);
	g.ret = calloc(1, sizeof(*g.ret));
	assert(g.ret);
	f.ret = u.ret = &result;
	for(op = 0; op < 3; op++) for(mode = 0; mode < 11; mode++){
		a->len = 8; a->t = &Tbyte; D2H(a)->t = &Tarray; a->data = data;
		f.buf = g.buf = u.buf = a;
		offset = 0;
		if(mode == 0)offset = -1;
		if(mode == 1)offset = sizeof(WORD)>4 ? (WORD)((uvlong)1<<32) : -1;
		if(mode == 2)a->len = -1;
		if(mode == 3)a->len = sizeof(WORD)>4 ? (WORD)((uvlong)1<<32) : -1;
		if(mode == 4)a->t = &other;
		if(mode == 5)D2H(a)->t = &other;
		if(mode == 6)a->data = nil;
		if(mode == 7)f.buf = g.buf = u.buf = H;
		if(mode == 8)f.buf = g.buf = u.buf = nil;
		if(mode == 9)offset = 9;
		if(mode == 10)offset = sizeof(WORD)>4 ? (WORD)(((uvlong)1<<32)+1) : -1;
		f.n = g.n = u.n = offset;
		result = 77; g.ret->t0 = g.ret->t1 = g.ret->t2 = 77;
		memset(data, 0xa5, sizeof(data)); raised = nil;
		if(setjmp(boundary) == 0){
			if(op == 0)Sys_char2byte(&f);
			if(op == 1)Sys_byte2char(&g);
			if(op == 2)Sys_utfbytes(&u);
			assert(0);
		}
		assert(raised && result == 77);
		assert(g.ret->t0 == 77 && g.ret->t1 == 77 && g.ret->t2 == 77);
		for(i = 0; i < sizeof(data); i++)assert(data[i] == 0xa5);
		checks++;
	}
	a->len = 0; a->data = nil; a->t = &Tbyte; D2H(a)->t = &Tarray;
	u.buf = a; u.n = 0; Sys_utfbytes(&u); assert(result == 0);
	/* A full-width invalid rune must not wrap to an ASCII character. */
	a->data = data; a->len = 8; f.buf = a; f.n = 2;
	f.c = sizeof(WORD)>4 ? (WORD)(((uvlong)1<<32)+65) : -1;
	Sys_char2byte(&f);
	assert(result == 3 && !memcmp(data+2, "\xef\xbf\xbd", 3));
	free(g.ret); free(D2H(a));
}

/* Exercise real generated builtin frames and the real UTF decoder/encoder. */
int
main(void)
{
	scalars();
	malformed();
	bounds();
	printf("PASS Sys UTF: %d scalar/bounds cases, WORD%zu\n", checks, sizeof(WORD)*8);
	return 0;
}
