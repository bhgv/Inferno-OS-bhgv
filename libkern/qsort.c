/*
 * qsort -- simple quicksort
 */

#include "lib9.h"
typedef
struct
{
	int	(*cmp)(void*, void*);
	void	(*swap)(char*, char*, long);
	long	es;
} Sort;

/* Exchange an element whose address or size needs bytewise access. */
static void
swapb(char *i, char *j, long es)
{
	char c;

	do {
		c = *i;
		*i++ = *j;
		*j++ = c;
		es--;
	} while(es != 0);

}

/* Exchange aligned native-word spans selected by the public entry point. */
static void
swapi(char *ii, char *ij, long es)
{
	long *i, *j, c;

	i = (long*)ii;
	j = (long*)ij;
	do {
		c = *i;
		*i++ = *j;
		*j++ = c;
		es -= sizeof(long);
	} while(es != 0);
}

/* Choose the median of three spread-out elements without moving storage. */
static char*
pivot(char *a, long n, Sort *p)
{
	long j;
	char *pi, *pj, *pk;

	j = n/6 * p->es;
	pi = a + j;	/* 1/6 */
	j += j;
	pj = pi + j;	/* 1/2 */
	pk = pj + j;	/* 5/6 */
	if(p->cmp(pi, pj) < 0) {
		if(p->cmp(pi, pk) < 0) {
			if(p->cmp(pj, pk) < 0)
				return pj;
			return pk;
		}
		return pi;
	}
	if(p->cmp(pj, pk) < 0) {
		if(p->cmp(pi, pk) < 0)
			return pi;
		return pk;
	}
	return pj;
}

/* Recurse on the smaller partition and iterate over the larger one. */
static void
qsorts(char *a, long n, Sort *p)
{
	long j, es;
	char *pi, *pj, *pn;

	es = p->es;
	while(n > 1) {
		if(n > 10) {
			pi = pivot(a, n, p);
		} else
			pi = a + (n>>1)*es;

		p->swap(a, pi, es);
		pi = a;
		pn = a + n*es;
		pj = pn;
		for(;;) {
			do
				pi += es;
			while(pi < pn && p->cmp(pi, a) < 0);
			do
				pj -= es;
			while(pj > a && p->cmp(pj, a) > 0);
			if(pj < pi)
				break;
			p->swap(pi, pj, es);
		}
		p->swap(a, pj, es);
		j = (pj - a) / es;

		n = n-j-1;
		if(j < n) {
			qsorts(a, j, p);
			a += (j+1)*es;
		} else {
			qsorts(a + (j+1)*es, n, p);
			n = j;
		}
	}
}

/* Ignore empty/invalid spans before partitioning or multiplying native counts. */
void
qsort(void *va, long n, long es, int (*cmp)(void*, void*))
{
	Sort s;

	if(n < 2 || es <= 0 || n > (long)(~(ulong)0 >> 1)/es)
		return;
	s.cmp = cmp;
	s.es = es;
	s.swap = swapi;
	if(((uintptr)va | es) % sizeof(long))
		s.swap = swapb;
	qsorts((char*)va, n, &s);
}
