#include "lib9.h"
#include "draw.h"

/*
 * Easy versions of the cache routines; may be substituted by fancier ones for other purposes
 */

static char	*lastname;
Subfont	*lastsubfont;

/* Find a cached subfont and retain the cache hit for its caller. */
Subfont*
lookupsubfont(Display *d, char *name)
{
	if(strcmp(name, "*default*") == 0)
		return d->defaultsubfont;
	if(lastname && strcmp(name, lastname)==0 && d==lastsubfont->bits->display){
		lastsubfont->ref++;
		return lastsubfont;
	}
	return 0;
}

/* Retain the existing cache entry when a replacement name cannot be copied. */
void
installsubfont(char *name, Subfont *subfont)
{
	char *copy;

	copy = strdup(name);
	if(copy == nil)
		return;
	free(lastname);
	lastname = copy;
	lastsubfont = subfont;	/* notice we don't free the old one; that's your business */
}

/* Remove the weak cache reference before its subfont storage is released. */
void
uninstallsubfont(Subfont *subfont)
{
	if(subfont == lastsubfont){
		free(lastname);
		lastname = 0;
		lastsubfont = 0;
	}
}
