#include "lib9.h"

static char qsep[] = " \t\r\n";

/* Compact shell quotes; the fixed separator set contains only ASCII bytes. */
static char*
qtoken(char *s, char *sep)
{
	int quoting;
	char *t;

	quoting = 0;
	t = s;	/* s is output string, t is input string */
	while(*t != '\0' && (quoting || strchr(sep, (uchar)*t) == nil)){
		if(*t != '\''){
			*s++ = *t++;
			continue;
		}
		/* *t is a quote */
		if(!quoting){
			quoting = 1;
			t++;
			continue;
		}
		/* quoting and we're on a quote */
		if(t[1] != '\''){
			/* end of quoted section; absorb closing quote */
			t++;
			quoting = 0;
			continue;
		}
		/* doubled quote; fold two quotes into one */
		t++;
		*s++ = *t++;
	}
	if(*s != '\0'){
		*s = '\0';
		if(t == s)
			t++;
	}
	return t;
}

/* Preserve quoted text while finding a complete UTF-8 delimiter rune. */
static char*
etoken(char *t, char *sep)
{
	Rune r;
	int quoting, n;

	/* move to end of next token */
	quoting = 0;
	while(*t != '\0'){
		n = chartorune(&r, t);
		if(!quoting && utfrune(sep, r) != nil)
			break;
		if(*t != '\''){
			t += n;
			continue;
		}
		/* *t is a quote */
		if(!quoting){
			quoting = 1;
			t++;
			continue;
		}
		/* quoting and we're on a quote */
		if(t[1] != '\''){
			/* end of quoted section; absorb closing quote */
			t++;
			quoting = 0;
			continue;
		}
		/* doubled quote; fold two quotes into one */
		t += 2;
	}
	return t;
}

/* Clear whole delimiter encodings and preserve the historical final remainder. */
int
gettokens(char *s, char **args, int maxargs, char *sep)
{
	Rune r;
	int nargs, n;

	for(nargs = 0; nargs < maxargs; nargs++){
		while(*s != '\0'){
			n = chartorune(&r, s);
			if(utfrune(sep, r) == nil)
				break;
			memset(s, 0, n);
			s += n;
		}
		if(*s == '\0')
			break;
		args[nargs] = s;
		s = etoken(s, sep);
	}

	return nargs;
}

/* Split shell words using ASCII whitespace and remove their quoting syntax. */
int
tokenize(char *s, char **args, int maxargs)
{
	int nargs;

	for(nargs = 0; nargs < maxargs; nargs++){
		while(*s != '\0' && strchr(qsep, (uchar)*s) != nil)
			s++;
		if(*s == '\0')
			break;
		args[nargs] = s;
		s = qtoken(s, qsep);
	}

	return nargs;
}
