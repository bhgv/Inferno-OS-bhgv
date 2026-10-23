#include <u.h>
#include <libc.h>
#include <mp.h>
#include <libsec.h>

/* Match a complete boundary line without reading beyond its terminating NUL. */
static char*
pemboundary(char *s, char *prefix, char *type)
{
	char *e, *next;
	ulong n, np, nt;

	e = strchr(s, '\n');
	if(e == nil)
		e = s+strlen(s);
	next = e;
	if(*next == '\n')
		next++;
	if(e > s && e[-1] == '\r')
		e--;
	n = e-s;
	np = strlen(prefix);
	nt = strlen(type);
	if(n < np+5 || nt != n-np-5)
		return nil;
	if(memcmp(s, prefix, np) != 0 || memcmp(s+np, type, nt) != 0 ||
	   memcmp(e-5, "-----", 5) != 0)
		return nil;
	return next;
}

/* Bound decoder lengths and publish the continuation only after successful decoding. */
uchar*
decodePEM(char *s, char *type, int *len, char **new_s)
{
	uchar *d;
	char *t, *e, *next;
	ulong size;
	int n, capacity;

	*len = 0;
	t = nil;
	for(e = s; *e != 0; e = next){
		next = pemboundary(e, "-----BEGIN ", type);
		if(next != nil){
			t = next;
			break;
		}
		next = strchr(e, '\n');
		if(next == nil)
			break;
		next++;
	}
	if(t == nil)
		goto badformat;
	for(e = t; *e != 0; e = next){
		next = pemboundary(e, "-----END ", type);
		if(next != nil)
			break;
		next = strchr(e, '\n');
		if(next == nil)
			goto badformat;
		next++;
	}
	if(*e == 0)
		goto badformat;

	size = e-t;
	if(size > 0x7fffffff){
		werrstr("incorrect .pem file format: section too large");
		return nil;
	}
	n = size;
	/* Division first avoids overflowing even when the input length is INT_MAX. */
	capacity = (n/4)*3 + ((n%4)*3+3)/4;
	d = malloc(capacity != 0 ? capacity : 1);
	if(d == nil){
		werrstr("out of memory");
		return nil;
	}
	n = dec64(d, capacity, t, n);
	if(n < 0){
		free(d);
		werrstr("incorrect .pem file format: bad base64 encoded data");
		return nil;
	}
	*len = n;
	if(new_s != nil)
		*new_s = next;
	return d;

badformat:
	werrstr("incorrect .pem file format: bad header or trailer");
	return nil;
}

/* Return the decoded prefix and release unlinked data if a chain node cannot be allocated. */
PEMChain*
decodepemchain(char *s, char *type)
{
	PEMChain *first, *last, *chp;
	uchar *d;
	int n;

	first = last = nil;
	while(*s != 0){
		d = decodePEM(s, type, &n, &s);
		if(d == nil)
			break;
		chp = malloc(sizeof(PEMChain));
		if(chp == nil){
			free(d);
			werrstr("out of memory");
			break;
		}
		chp->next = nil;
		chp->pem = d;
		chp->pemlen = n;
		if(first == nil)
			first = chp;
		else
			last->next = chp;
		last = chp;
	}
	return first;
}
