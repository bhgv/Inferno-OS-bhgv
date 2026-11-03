#include "lib9.h"
#include "isa.h"
#include "interp.h"
#include "draw-native64.h"
#include <stdio.h>
#include <setjmp.h>

typedef struct GeometryCase GeometryCase;
struct GeometryCase {
	u32int input[8]; // Independent input coordinate bits, in source argument order.
	u32int output[5]; // Independent Python integer/boolean/tuple result bits.
	int failure; // Whether division must raise before changing any return bytes.
};

typedef struct GeometrySpec GeometrySpec;
struct GeometrySpec {
	char *name; // Owning C entry identity.
	void (*call)(void*); // Actual complete production consumer body.
	int bytes; // Generated native Frame64 extent.
	int result; // Generated result-address field offset.
	int resultbytes; // Complete generated result object extent.
	int arguments; // Logical source argument count.
	int offset[2]; // Actual C offsets of source arguments, excluding padding.
	int words[2]; // Number of int32 cells in each argument.
	GeometryCase *cases; // Independently generated arithmetic/geometry oracle.
	int count; // Exact oracle population.
};

static jmp_buf recovery; // Existing VM exception transport, bounded to one actual call.
static int expecting; // An unexpected error must fail instead of masking a result mismatch.
static unsigned long checks; // Active predicates, independent of NDEBUG.
char exZdiv[] = "zero divide"; // Existing exception identity used by the owning body.


/* Stop at the semantic predicate, not a compiler, signal or sanitizer failure. */
static void
check(int condition)
{
	if(!condition){
		fprintf(stderr, "Native64 geometry mismatch at %lu\n", checks);
		exit(10);
	}
	checks++;
}


/* Require the real zero-divide identity before unwinding the isolated call. */
void
error(char *message)
{
	check(expecting && message == exZdiv);
	longjmp(recovery, 1);
}

GEOMETRY_ORACLES


/* Keep loop state outside the setjmp activation modified by the production entry. */
static int
invoke(void (*call)(void*), void *frame)
{
	if(setjmp(recovery))
		return 0;
	call(frame);
	return 1;
}


/* Compare every result byte, every surrounding byte and every partial argument alias. */
static void
exercise(GeometrySpec *spec)
{
	union {
		uvlong alignment; // Keep the generated Frame64 base naturally aligned.
		uchar bytes[256]; // Back both valid frames and bounded input controls.
	} frame;
	union {
		uvlong alignment; // Keep surrounding result storage naturally aligned.
		uchar bytes[64]; // Include every deliberately unaligned result and guard byte.
	} output;
	uchar wantedframe[256], wantedoutput[64], *destination;
	GeometryCase *value;
	int row, alias, offset, argument, word, cursor, ok;

	check(spec->bytes <= sizeof(frame.bytes) && spec->result == 32);
	check(spec->resultbytes <= 20 && spec->arguments <= 2);
	for(row=0; row<spec->count; row++)
	for(alias=0; alias<=spec->arguments; alias++)
	for(offset=0; offset<8; offset++){
		value = &spec->cases[row];
		memset(frame.bytes, 0xa5, sizeof(frame.bytes));
		memset(output.bytes, 0x5a, sizeof(output.bytes));
		cursor = 0;
		for(argument=0; argument<spec->arguments; argument++){
			check(spec->offset[argument] >= 64);
			for(word=0; word<spec->words[argument]; word++){
				memmove(frame.bytes+spec->offset[argument]+4*word, &value->input[cursor], 4);
				cursor++;
			}
		}
		destination = alias ? frame.bytes+spec->offset[alias-1]+offset : output.bytes+16+offset;
		memmove(frame.bytes+spec->result, &destination, sizeof(destination));
		memmove(wantedframe, frame.bytes, sizeof(frame.bytes));
		memmove(wantedoutput, output.bytes, sizeof(output.bytes));
		if(!value->failure){
			if(alias)
				memmove(wantedframe+spec->offset[alias-1]+offset, value->output, spec->resultbytes);
			else
				memmove(wantedoutput+16+offset, value->output, spec->resultbytes);
		}
		expecting = value->failure;
		ok = invoke(spec->call, frame.bytes);
		check(ok == !value->failure);
		check(memcmp(frame.bytes, wantedframe, sizeof(frame.bytes)) == 0);
		check(memcmp(output.bytes, wantedoutput, sizeof(output.bytes)) == 0);
	}
}


/* Full Draw geometry uses real generated declarations, without registering unrelated Draw entries. */
int
main(void)
{
	unsigned int i;

	check(limbo_native64_Draw_layout());
	check(sizeof(WORD) == 4 && sizeof(Draw_Point) == 8 && sizeof(Draw_Rect) == 16);
	for(i=0; i<nelem(specifications); i++)
		exercise(&specifications[i]);
	printf("PASS native64 Draw geometry: %u actual entries; %lu predicates; independent results, byte offsets and partial aliases\n", (unsigned)nelem(specifications), checks);
	return 0;
}
