#include "lib9.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>

static int checks;

/* Build an independent lookup oracle for ASCII-only case equivalence. */
static void
mapping(int *fold)
{
	int i;

	for(i = 0; i < 256; i++)
		fold[i] = i;
	for(i = 0; i < 26; i++)
		fold['A'+i] = 'a'+i;
}

/* Compare ordering by sign; exact magnitude is not part of the public contract. */
static int
sign(int n)
{
	return (n > 0)-(n < 0);
}

/* Check every leading byte pair, both termination positions and count bounds. */
static void
bytepairs(void)
{
	int fold[256], a, b, bound, expected, counts[] = {0, 1, 2, 3, INT_MAX};
	char left[3], right[3];

	mapping(fold);
	for(a = 0; a < 256; a++)
	for(b = 0; b < 256; b++){
		left[0] = a;
		left[1] = 'X';
		left[2] = 0;
		right[0] = b;
		right[1] = 'x';
		right[2] = 0;
		expected = sign(fold[a]-fold[b]);
		assert(sign(cistrcmp(left, right)) == expected);
		assert(sign(cistrcmp(right, left)) == -expected);
		for(bound = 0; bound < nelem(counts); bound++){
			assert(sign(cistrncmp(left, right, counts[bound])) ==
				(counts[bound] ? expected : 0));
			checks++;
		}
	}
}

/* Exercise bounded prefixes, different lengths, and bytes beyond the bound. */
static void
prefixes(void)
{
	char left[34], right[34];
	int length, other, bound, i, expected;

	for(length = 0; length <= 32; length++)
	for(other = 0; other <= 32; other++){
		for(i = 0; i < length; i++)
			left[i] = 'A'+i%26;
		left[length] = 0;
		for(i = 0; i < other; i++)
			right[i] = 'a'+i%26;
		right[other] = 0;
		assert(sign(cistrcmp(left, right)) == sign(length-other));
		for(bound = 0; bound <= 33; bound++){
			expected = bound <= length && bound <= other ? 0 : sign(length-other);
			assert(sign(cistrncmp(left, right, bound)) == expected);
			checks++;
		}
	}
}

/* A nonpositive count must not access either pointer or decrement INT_MIN. */
static void
emptybounds(void)
{
	int counts[] = {INT_MIN, INT_MIN+1, -1, 0}, i;

	for(i = 0; i < nelem(counts); i++){
		assert(cistrncmp("a", "b", counts[i]) == 0);
		assert(cistrncmp(nil, nil, counts[i]) == 0);
		checks++;
	}
}

/* Run identical unsigned-byte semantics in both native libraries and char modes. */
int
main(int argc, char **argv)
{
	if(argc == 1 || strcmp(argv[1], "pairs") == 0)
		bytepairs();
	if(argc == 1 || strcmp(argv[1], "prefixes") == 0)
		prefixes();
	if(argc == 1 || strcmp(argv[1], "bounds") == 0)
		emptybounds();
	printf("native case comparison: %d scenarios passed\n", checks);
	return 0;
}
