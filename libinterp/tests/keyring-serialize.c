#include "lib9.h"
#include "mp.h"
#include "libsec.h"
#include "isa.h"
#include "interp.h"
#define DigestState_copy Keyring_DigestState_copy
#define IPint_random Keyring_IPint_random
#include "keyringif.h"
#include "keys.h"
#include <assert.h>
#include <stdio.h>
#include <limits.h>

static int live, attempts, failat, checks;
static char formatted[8192];

/* Count every temporary export/formatter buffer while injecting allocation failures. */
static void*
serialmalloc(ulong n)
{
	void *p;

	if(++attempts == failat)
		return nil;
	assert(n > 0 && n < 1000000);
	p = malloc(n);
	assert(p != nil);
	memset(p, 0xa5, n);
	live++;
	return p;
}

/* Formatter failures and successful exports must release every temporary owner. */
static void
serialfree(void *p)
{
	if(p != nil){
		assert(live > 0);
		live--;
		free(p);
	}
}

/* Native formatting uses no running interpreter error state in this fixture. */
void rerrstr(char *buf, uint n) { if(n)snprintf(buf, n, "fixture"); }
int __lsan_is_turned_off(void) { return 1; }
#define malloc serialmalloc
#define free serialfree
IMPLEMENTATION
#undef malloc
#undef free
VECTORS

/* Build native digits independently from the expected Python byte/base64 representation. */
static void
number(mpint *b, mpdigit *digits, int n)
{
	int i, top;
	uchar c;

	memset(b, 0, sizeof(*b));
	memset(digits, 0, 64*sizeof(*digits));
	for(i = 0; i < n; i++){
		c = (n-1-i)*37+n*11;
		digits[i/Dbytes] |= (mpdigit)c<<((i%Dbytes)*8);
	}
	top = (n+Dbytes-1)/Dbytes;
	while(top > 0 && digits[top-1] == 0)
		top--;
	b->p = digits;
	b->size = 64;
	b->top = top;
	b->sign = 1;
}

static int formatnumber(mpint*);

/* Require complete text or a terminated marker at every output capacity. */
static void
integers(void)
{
	mpint b;
	mpdigit digits[64];
	char output[256];
	int i, capacity, n, needed;

	for(i = 0; i < nelem(vectors); i++){
		number(&b, digits, vectors[i].n);
		needed = strlen(vectors[i].expected);
		assert(formatnumber(&b) == 0);
		assert(memcmp(formatted, vectors[i].expected, needed) == 0);
		assert((uchar)formatted[needed] == 0xa5);
		checks++;
		for(capacity = 0; capacity <= needed+2; capacity++){
			memset(output, 0xa5, sizeof output);
			n = bigtobase64(&b, output+1, capacity);
			if(capacity == 0)
				assert(n == 0 && (uchar)output[1] == 0xa5);
			else if(capacity == 1)
				assert(n == 0 && output[1] == 0);
			else if(capacity <= needed)
				assert(n == 1 && strcmp(output+1, "*") == 0);
			else
				assert(n == needed && strcmp(output+1, vectors[i].expected) == 0);
			assert((uchar)output[0] == 0xa5 && (uchar)output[capacity+1] == 0xa5);
			assert(live == 0);
			checks++;
		}
	}
	assert(bigtobase64(nil, nil, 0) == 0);
	checks++;
}

/* Native fmtprint exposes callback errors, unlike snprintf's intentional truncation result. */
static int
formatnumber(mpint *b)
{
	Fmt fmt;

	memset(&fmt, 0, sizeof fmt);
	memset(formatted, 0xa5, sizeof formatted);
	fmt.start = fmt.to = formatted;
	fmt.stop = formatted+sizeof(formatted)-1;
	return fmtprint(&fmt, "%U", b);
}

/* Fault both the formatter allocation and its nested magnitude export. */
static void
failures(void)
{
	mpint b;
	mpdigit digits[64];
	char output[32];
	int i;

	number(&b, digits, 8);
	for(i = 1; i <= 2; i++){
		attempts = 0;
		failat = i;
		assert(formatnumber(&b) < 0 && live == 0);
		checks++;
	}
	attempts = 0;
	failat = 1;
	memset(output, 0xa5, sizeof output);
	assert(bigtobase64(&b, output, sizeof output) == 1 && !strcmp(output, "*"));
	assert((uchar)output[2] == 0xa5 && live == 0);
	checks++;
	failat = 0;
}

/* Bad native metadata must not overflow sizing or read nonexistent digit storage. */
static void
bounds(void)
{
	mpint b;
	char output[8];
	int mode;
	mpdigit fake = 0;

	for(mode = 0; mode < 4; mode++){
		memset(&b, 0, sizeof b);
		b.top = mode == 0 ? -1 : mode == 1 ? INT_MAX : 1;
		b.size = mode == 1 ? INT_MAX : mode == 2 ? 0 : 1;
		b.p = mode == 1 ? &fake : nil;
		memset(output, 0xa5, sizeof output);
		assert(bigtobase64(&b, output, sizeof output) == 1 && !strcmp(output, "*"));
		assert(formatnumber(&b) < 0 && live == 0);
		checks++;
	}
}

/* Exercise real codecs, native Fmt dispatch, output guards and failure ownership. */
int
main(int argc, char **argv)
{
	fmtinstall('U', big64conv);
	if(argc == 1 || strcmp(argv[1], "integers") == 0)
		integers();
	if(argc == 1 || strcmp(argv[1], "failure") == 0)
		failures();
	if(argc == 1 || strcmp(argv[1], "bounds") == 0)
		bounds();
	printf("Keyring integer serialization: %d codec/capacity/failure cases passed\n", checks);
	return 0;
}
