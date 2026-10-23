#include "lib9.h"
#include "fmtdef.h"
#include <assert.h>
#include <stdio.h>
#include <limits.h>

void _quotesetup(char*, Rune*, int, int, Quoteinfo*, int, int);

/* Native formatting diagnostics do not need an emulator process. */
void
rerrstr(char *p, uint n)
{
	if(n)
		snprintf(p, n, "fixture");
}

/* Enable independent ASCII shell punctuation quoting in selected vectors. */
static int
special(int c)
{
	return c == ';';
}

/* Count a bounded prefix with independent scalar widths and quote expansion. */
static Quoteinfo
expected(Rune *r, int nin, int nout, int sharp, int runesout)
{
	Quoteinfo q = {0};
	int width, needquote, i, bytes, runes;
	Rune c;

	if(nout < 0)
		return q;
	if(sharp || nin == 0 || *r == 0){
		if(nout < 2)
			return q;
		q.quoted = 1;
		q.nbytesout = q.nrunesout = 2;
	}
	for(i = 0; r[i] != 0; i++){
		c = r[i];
		width = c < 0x80 ? 1 : c < 0x800 ? 2 : c < 0x10000 ? 3 : 4;
		if(nin >= 0 && q.nbytesin+width > nin)
			break;
		bytes = q.nbytesout+width;
		runes = q.nrunesout+1;
		if((runesout ? runes : bytes) > nout)
			break;
		needquote = c <= 32 || c == '\'' || (doquote != nil && special(c));
		if(needquote && !q.quoted){
			if((runesout ? runes+2 : bytes+2) > nout)
				break;
			q.quoted = 1;
			q.nbytesout += 2;
			q.nrunesout += 2;
			bytes += 2;
			runes += 2;
		}
		if(c == '\''){
			if((runesout ? runes+1 : bytes+1) > nout)
				break;
			bytes++;
			runes++;
		}
		q.nbytesout = bytes;
		q.nrunesout = runes;
		q.nbytesin += width;
		q.nrunesin++;
	}
	return q;
}

/* Sweep input-byte limits, output-byte/rune limits and forced quoting. */
static void
counts(void)
{
	Rune cases[][16] = {{0}, {'a','b',0}, {'a',' ',0}, {'\'',0},
		{'a','\'','\'',0}, {0xa2,0x20ac,0x1f600,' ', ';',0},
		{';','a','\'',0}};
	char text[128], *p, *bounded;
	Quoteinfo actual, want;
	int i, j, nin, nout, sharp, runesout, custom, length;

	for(custom = 0; custom < 2; custom++){
		doquote = custom ? special : nil;
		for(i = 0; i < nelem(cases); i++){
			p = text;
			for(j = 0; cases[i][j]; j++)
				p += runetochar(p, &cases[i][j]);
			*p = 0;
			length = p-text;
			for(nin = -1; nin <= length+1; nin++)
			for(nout = -1; nout <= length*2+5; nout++)
			for(sharp = 0; sharp < 2; sharp++)
			for(runesout = 0; runesout < 2; runesout++){
				want = expected(cases[i], nin, nout, sharp, runesout);
				_quotesetup(text, nil, nin, nout, &actual, sharp, runesout);
				assert(memcmp(&actual, &want, sizeof(want)) == 0);
				_quotesetup(nil, cases[i], nin, nout, &actual, sharp, runesout);
				assert(memcmp(&actual, &want, sizeof(want)) == 0);
				if(nin > 0 && nin <= length){
					/* No terminator or readable spare byte beyond the declared span. */
					bounded = malloc(nin);
					assert(bounded != nil);
					memmove(bounded, text, nin);
					_quotesetup(bounded, nil, nin, nout, &actual, sharp, runesout);
					assert(memcmp(&actual, &want, sizeof(want)) == 0);
					free(bounded);
				}
			}
			_quotesetup(text, nil, INT_MIN, INT_MAX, &actual, 0, 0);
			want = expected(cases[i], -1, INT_MAX, 0, 0);
			assert(memcmp(&actual, &want, sizeof(want)) == 0);
		}
	}
	doquote = nil;
}

/* Exercise the real formatter and feed quoted UTF-8 back into the tokenizer. */
static void
roundtrip(void)
{
	char *cases[] = {"", "abc", "a b", "'", "a''b", "¢€😀 'x'"};
	char buffer[256], copy[256], *args[2];
	Rune runes[64], output[256], *end;
	char *p;
	int i, n, j;
	Rune scalar;

	quotefmtinstall();
	for(i = 0; i < nelem(cases); i++){
		p = cases[i];
		j = 0;
		while(*p)
			p += chartorune(&runes[j++], p);
		runes[j] = 0;
		n = snprint(buffer, sizeof(buffer), "%q", cases[i]);
		assert(n >= 0 && n == strlen(buffer));
		strcpy(copy, buffer);
		assert(tokenize(copy, args, 2) == 1 && strcmp(args[0], cases[i]) == 0);
		snprint(copy, sizeof(copy), "%Q", runes);
		assert(strcmp(copy, buffer) == 0);
		end = runeseprint(output, output+nelem(output), "%q", cases[i]);
		assert(end != nil);
		p = copy;
		for(j = 0; output+j < end; j++)
			p += runetochar(p, &output[j]);
		*p = 0;
		assert(strcmp(copy, buffer) == 0);
		end = runeseprint(output, output+nelem(output), "%Q", runes);
		p = copy;
		for(j = 0; output+j < end; j++)
			p += runetochar(p, &output[j]);
		*p = 0;
		assert(strcmp(copy, buffer) == 0);
		/* Short outputs must remain bounded, NUL terminated and valid UTF-8. */
		for(j = 1; j < n+3; j++){
			memset(copy, 0xa5, sizeof(copy));
			snprint(copy, j, "%q", cases[i]);
			assert(memchr(copy, 0, j) != nil && (uchar)copy[j] == 0xa5);
			for(p = copy; *p; ){
				p += chartorune(&scalar, p);
				assert(scalar != Runeerror);
			}
		}
	}
}

/* Test bounded and NUL-terminated input through actual formatter dispatch. */
int
main(void)
{
	counts();
	roundtrip();
	puts("quote formatting: finite spans, limits, signed extrema and UTF roundtrips passed");
	return 0;
}
