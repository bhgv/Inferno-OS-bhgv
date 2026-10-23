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

enum { Maxalg = 3 };
static Type types[5];
static Type *TSigAlg = &types[0], *TSK = &types[1], *TPK = &types[2], *TCertificate = &types[3];
static SigAlgVec *algs[Maxalg];
static int nalg, errors, checks;
static char exType[] = "type", exBadSA[] = "algorithm", exBadSK[] = "secret";
static char exBadPK[] = "public", exBadCert[] = "certificate";

/* Observe validation failures without requiring a running VM error stack. */
void
errorf(char *format, ...)
{
	USED(format);
	errors++;
}

/* The sandbox cannot expose /proc task metadata required by LeakSanitizer. */
int __lsan_is_turned_off(void) { return 1; }

IMPLEMENTATION

/* Invoke the real validator selected by the fixture's matching heap type. */
static void*
validate(int kind, void *data)
{
	switch(kind){
	case 0: return checkSigAlg(data);
	case 1: return checkSK(data);
	case 2: return checkPK(data);
	default: return checkCertificate(data);
	}
}

/* Nil, VM nil and an alien zero-payload heap must be rejected before any field read. */
static void
invalidtypes(void)
{
	Heap *h;
	int kind;

	h = malloc(sizeof(*h));
	assert(h != nil);
	memset(h, 0, sizeof(*h));
	h->t = &types[4];
	for(kind = 0; kind < 4; kind++){
		errors = 0;
		assert(validate(kind, nil) == nil && errors == 1);
		errors = 0;
		assert(validate(kind, H) == nil && errors == 1);
		errors = 0;
		assert(validate(kind, H2D(void*, h)) == nil && errors == 1);
		checks += 3;
	}
	free(h);
}

/* Matching heap types still require initialized native payloads and registered vectors. */
static void
validtypes(void)
{
	Heap *h;
	SigAlg *sa;
	SigAlgVec registered, foreign;
	void *data;
	int kind;

	for(kind = 0; kind < 4; kind++){
		h = malloc(sizeof(*h)+sizeof(SK)+sizeof(PK)+sizeof(Certificate)+sizeof(SigAlg));
		assert(h != nil);
		memset(h, 0, sizeof(*h)+sizeof(SK)+sizeof(PK)+sizeof(Certificate)+sizeof(SigAlg));
		h->t = &types[kind];
		data = H2D(void*, h);
		errors = 0;
		assert(validate(kind, data) == nil && errors == 1);
		switch(kind){
		case 0:
			sa = data;
			sa->vec = &foreign;
			errors = 0;
			assert(validate(kind, data) == nil && errors == 1);
			algs[1] = &foreign;
			errors = 0;
			assert(validate(kind, data) == nil && errors == 1);
			sa->vec = &registered;
			algs[0] = &registered;
			nalg = 1;
			checks += 2;
			break;
		case 1: ((SK*)data)->key = data; break;
		case 2: ((PK*)data)->key = data; break;
		default: ((Certificate*)data)->signa = data; break;
		}
		errors = 0;
		assert(validate(kind, data) == data && errors == 0);
		checks += 2;
		free(h);
	}
}

/* ASan makes even a single field access beyond the alien heap allocation observable. */
int
main(int argc, char **argv)
{
	if(argc == 1 || strcmp(argv[1], "invalid") == 0)
		invalidtypes();
	if(argc == 1 || strcmp(argv[1], "valid") == 0)
		validtypes();
	printf("Keyring object validation: %d heap/type/registry cases passed\n", checks);
	return 0;
}
