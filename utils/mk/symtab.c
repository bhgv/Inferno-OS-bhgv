#include	"mk.h"

#define	NHASH	4099 // Number of private symbol buckets.
#define	HASHMUL	79UL // Unsigned arithmetic defines native-width wrap.
static Symtab *hash[NHASH];


/* Hash every byte with defined unsigned wrap on both ILP32 and LP64 hosts. */
static uint
symhash(char *name, int space)
{
	ulong h;
	uchar *p;

	h = (uint)space;
	for(p = (uchar*)name; *p; p++)
		h = h*HASHMUL+*p;
	return h%NHASH;
}


/* Release all entries before reusing the private table. */
void
syminit(void)
{
	Symtab **s, *ss, *next;

	for(s = hash; s < &hash[NHASH]; s++){
		for(ss = *s; ss; ss = next){
			next = ss->next;
			free((char *)ss);
		}
		*s = 0;
	}
}


/* Look up a byte-exact name or install its borrowed name/value once. */
Symtab *
symlook(char *sym, int space, void *install)
{
	uint h;
	Symtab *s;

	h = symhash(sym, space);
	for(s = hash[h]; s; s = s->next)
		if((s->space == space) && (strcmp(s->name, sym) == 0))
			return(s);
	if(install == 0)
		return(0);
	s = (Symtab *)Malloc(sizeof(Symtab));
	s->space = space;
	s->name = sym;
	s->value = install;
	s->next = hash[h];
	hash[h] = s;
	return(s);
}


/* Remove the unique matching entry without following its freed next link. */
void
symdel(char *sym, int space)
{
	uint h;
	Symtab *s, *ls;

	h = symhash(sym, space);
	for(s = hash[h], ls = 0; s; ls = s, s = s->next)
		if((s->space == space) && (strcmp(s->name, sym) == 0)){
			if(ls)
				ls->next = s->next;
			else
				hash[h] = s->next;
			free((char *)s);
			return;
		}
}


/* Visit borrowed entries in one symbol namespace without changing ownership. */
void
symtraverse(int space, void (*fn)(Symtab*))
{
	Symtab **s, *ss;

	for(s = hash; s < &hash[NHASH]; s++)
		for(ss = *s; ss; ss = ss->next)
			if(ss->space == space)
				(*fn)(ss);
}


/* Saturate the final diagnostic bin instead of indexing past long collision chains. */
void
symstat(void)
{
	Symtab **s, *ss;
	int n;
	int l[1000];

	memset((char *)l, 0, sizeof(l));
	for(s = hash; s < &hash[NHASH]; s++){
		for(ss = *s, n = 0; ss; ss = ss->next)
			n++;
		if(n >= nelem(l))
			n = nelem(l)-1;
		l[n]++;
	}
	for(n = 0; n < 1000; n++)
		if(l[n]) Bprint(&bout, "%d of length %d%s\n", l[n], n, n == nelem(l)-1 ? "+" : "");
}
