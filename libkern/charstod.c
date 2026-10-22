#include "lib9.h"

/*
 * Reads a floating-point number by interpreting successive characters
 * returned by (*f)(vp).  The last call it makes to f terminates the
 * scan, so is not a character in the number.  It may therefore be
 * necessary to back up the input stream up one byte after calling charstod.
 */

/* Continue scanning after storage fills so the last callback still ends the token. */
#define ADVANCE do { \
	if(s < e) \
		*s++ = c; \
	else \
		overflow = 1; \
	c = (*f)(vp); \
} while(0)

/* Bound stored text while consuming the entire number and one terminator. */
double
charstod(int(*f)(void*), void *vp)
{
	char str[400], *s, *e, *start;
	int c, overflow;

	overflow = 0;
	s = str;
	e = str + sizeof str - 1;
	c = (*f)(vp);
	while(c == ' ' || c == '\t')
		c = (*f)(vp);
	if(c == '-' || c == '+'){
		ADVANCE;
	}
	start = s;
	while(c >= '0' && c <= '9'){
		ADVANCE;
	}
	if(c == '.'){
		ADVANCE;
		while(c >= '0' && c <= '9'){
			ADVANCE;
		}
	}
	if(s > start && (c == 'e' || c == 'E')){
		ADVANCE;
		if(c == '-' || c == '+'){
			ADVANCE;
		}
		while(c >= '0' && c <= '9'){
			ADVANCE;
		}
	}else if(s == start && (c == 'i' || c == 'I')){
		ADVANCE;
		if(c != 'n' && c != 'N')
			return NaN();
		ADVANCE;
		if(c != 'f' && c != 'F')
			return NaN();
		ADVANCE;
		if(c != 'i' && c != 'I')
			return NaN();
		ADVANCE;
		if(c != 'n' && c != 'N')
			return NaN();
		ADVANCE;
		if(c != 'i' && c != 'I')
			return NaN();
		ADVANCE;
		if(c != 't' && c != 'T')
			return NaN();
		ADVANCE;
		if(c != 'y' && c != 'Y')
			return NaN();
		ADVANCE;  /* so caller can back up uniformly */
	}else if(s == start && (c == 'n' || c == 'N')){
		ADVANCE;
		if(c != 'a' && c != 'A')
			return NaN();
		ADVANCE;
		if(c != 'n' && c != 'N')
			return NaN();
		ADVANCE;  /* so caller can back up uniformly */
	}
	if(overflow)
		return NaN();
	*s = 0;
	return strtod(str, &s);
}
