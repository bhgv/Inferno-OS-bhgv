implement Sum;

include "sum.m";

# Add the integers from one through n without allocating temporary objects.
sum(n: int): int
{
	total := 0;
	for(i := 1; i <= n; i++)
		total += i;
	return total;
}
