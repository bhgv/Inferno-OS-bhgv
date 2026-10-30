#include "lib9.h"
#include "isa.h"
#include "interp.h"
#define LIMBO_NATIVE64_TABLES // Request private full-map records, not runtime Runtab registration.
#include "math-native64.h"
#include "native64-bind.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int registration_errors; // Count explicit refusal before any legacy table is touched.
extern void mathmodinit(void); // Actual production initializer from the selected translation unit.

static unsigned int checks; // Active checks remain enabled under NDEBUG.


/* Reject a wrong generated record or an unexpected actual C result. */
static void
check(int condition)
{
	if(!condition)
		abort();
	checks++;
}


/* Observe the explicit registration refusal; this fixture owns no VM error stack. */
void
error(char *message)
{
	check(strcmp(message, "experimental native64 Math registration is not supported") == 0);
	registration_errors++;
}


/* Match actual selected entry points, legacy hashes and full fixed-frame geometry. */
static void
entry(const LimboNative64Entry *record, char *name, void (*function)(void*), UWORD signature, unsigned int bytes)
{
	unsigned int i;

	check(strcmp(record->name, name) == 0 && record->function == function);
	check(record->legacy_signature == signature && record->frame_size == bytes);
	check(record->map_bytes == (bytes+63)/64);
	for(i=0; i<record->map_bytes; i++)
		check(record->map[i] == 0);
}


/* Invoke actual Math C code through emitted entries; inspect every return-byte alignment. */
int
main(void)
{
	F_Math_bits32real from32;
	F_Math_realbits32 to32;
	F_Math_bits64real from64;
	F_Math_realbits64 to64;
	uchar output[32], expected[8];
	unsigned int words[] = {0, 0x80000000U, 0x3f800000U, 0xbf800000U, 0x7f800000U, 0xff800000U};
	uvlong longs[] = {0, 0x8000000000000000ULL, 0x3ff0000000000000ULL, 0xbff0000000000000ULL, 0x7ff0000000000000ULL, 0xfff0000000000000ULL};
	REAL value;
	float narrow;
	WORD word;
	LONG big;
	unsigned int i, offset, byte;
	const LimboNative64Entry *selected[] = {
		&limbo_native64_Math_bits32real_entry,
		&limbo_native64_Math_realbits32_entry,
		&limbo_native64_Math_bits64real_entry,
		&limbo_native64_Math_realbits64_entry,
	};
	const LimboNative64Entry *bound[4];

	check(limbo_native64_Math_layout());
	check(native64bind(limbo_native64_Math_specs, limbo_native64_Math_spec_count,
		selected, nelem(selected), bound, nelem(bound)));
	mathmodinit();
	check(registration_errors == 1);
	entry(&limbo_native64_Math_bits32real_entry, "bits32real", Math_bits32real, 0x40a58596U, sizeof(from32));
	entry(&limbo_native64_Math_realbits32_entry, "realbits32", Math_realbits32, 0x7b2e5f52U, sizeof(to32));
	entry(&limbo_native64_Math_bits64real_entry, "bits64real", Math_bits64real, 0x5463c72cU, sizeof(from64));
	entry(&limbo_native64_Math_realbits64_entry, "realbits64", Math_realbits64, 0x9589d476U, sizeof(to64));
	for(offset=0; offset<8; offset++)
		for(i=0; i<nelem(words); i++){
			memmove(&word, &words[i], 4);
			memmove(&narrow, &words[i], 4);
			value = narrow;
			memmove(expected, &value, 8);
			memset(output, 0xa5, sizeof(output));
			memset(&from32, 0, sizeof(from32));
			from32.b = word;
			from32.ret = (REAL*)(output+offset);
			bound[0]->function(&from32);
			check(memcmp(output+offset, expected, 8) == 0);
			for(byte=0; byte<sizeof(output); byte++)
				if(byte < offset || byte >= offset+8)
					check(output[byte] == 0xa5);
			memset(&to32, 0, sizeof(to32));
			to32.x = value;
			to32.ret = (WORD*)(output+offset);
			memset(output, 0xa5, sizeof(output));
			bound[1]->function(&to32);
			check(memcmp(output+offset, &word, 4) == 0);
			for(byte=0; byte<sizeof(output); byte++)
				if(byte < offset || byte >= offset+4)
					check(output[byte] == 0xa5);
			memmove(&big, &longs[i], 8);
			memmove(&value, &longs[i], 8);
			memset(&from64, 0, sizeof(from64));
			from64.b = big;
			from64.ret = (REAL*)(output+offset);
			memset(output, 0xa5, sizeof(output));
			bound[2]->function(&from64);
			check(memcmp(output+offset, &value, 8) == 0);
			memset(&to64, 0, sizeof(to64));
			to64.x = value;
			to64.ret = (LONG*)(output+offset);
			bound[3]->function(&to64);
			check(memcmp(output+offset, &big, 8) == 0);
		}
	printf("PASS native64 actual Math: %u checks, four generated entries, 192 calls, eight result alignments\n", checks);
	return 0;
}
