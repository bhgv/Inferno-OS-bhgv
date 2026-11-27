#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "isa.h"
#include "dis64-bound-numeric.h"


/* Keep oracle, guard and atomic-refusal checks active with NDEBUG. */
static void
boundcheck(int condition)
{
	if(!condition){
		fprintf(stderr, "bound numeric predicate failed\n");
		exit(10);
	}
}


/* Read independent fixture words without production metadata readers. */
static unsigned int
number(FILE *file)
{
	unsigned char b[4];

	boundcheck(fread(b, 1, 4, file) == 4);
	return ((unsigned int)b[0]<<24)|((unsigned int)b[1]<<16)|((unsigned int)b[2]<<8)|b[3];
}


/* Compare catalog identity and every conditional numeric summary, including late refusal. */
int
main(int argc, char **argv)
{
	static unsigned char data[4][Dis64LocalBytes+1];
	static Dis64InitWork scratch[Dis64LocalInstructions+1];
	static unsigned int queue[Dis64LocalInstructions+1];
	Dis64FunctionLocals locals[Dis64LocalFunctions+1];
	Dis64LocalSlot slots[Dis64LocalFunctions+1][Dis64LocalSlots+1];

	/* One extra physical output record permits safe logical-capacity controls. */
	struct {
		unsigned char before[32]; // Leading publication guard.
		Dis64BoundNumeric functions[Dis64LocalFunctions+1]; // Final numeric-only records.
		unsigned int written; // Final record count, unchanged on refusal.
		unsigned char after[32]; // Trailing publication guard.
	} output, saved;
	FILE *file;
	unsigned int cases, c, sizes[4], objectlimit, entry, frame, localcount, workcap, capacity, limit;
	unsigned int expected, functions, i, j, values[13], actual[13];
	int accepted;

	boundcheck(argc == 2);
	file = fopen(argv[1], "rb");
	boundcheck(file != 0);
	cases = number(file);
	for(c=0; c<cases; c++){
		for(i=0; i<4; i++){
			sizes[i] = number(file);
			boundcheck(sizes[i] <= Dis64LocalBytes+1);
		}
		objectlimit = number(file);
		entry = number(file);
		frame = number(file);
		localcount = number(file);
		workcap = number(file);
		capacity = number(file);
		limit = number(file);
		boundcheck(localcount <= Dis64LocalFunctions+1 && workcap <= Dis64LocalInstructions);
		for(i=0; i<4; i++)
			boundcheck(fread(data[i], 1, sizes[i], file) == sizes[i]);
		for(i=0; i<localcount; i++){
			locals[i].symbol = number(file);
			locals[i].entry = number(file);
			locals[i].frame = number(file);
			locals[i].count = number(file);
			boundcheck(locals[i].count <= Dis64LocalSlots+1);
			for(j=0; j<locals[i].count; j++){
				slots[i][j].offset = number(file);
				slots[i][j].kind = number(file);
			}
			locals[i].slots = slots[i];
		}
		expected = number(file);
		functions = number(file);
		memset(&output, 0xa5, sizeof(output));
		memcpy(&saved, &output, sizeof(saved));
		memset(scratch, 0x5a, (workcap+1)*sizeof(scratch[0]));
		memset(queue, 0x5a, (workcap+1)*sizeof(queue[0]));
		accepted = dis64boundnumericcheck(data[0], sizes[0], data[1], sizes[1], data[2], sizes[2],
			data[3], sizes[3], objectlimit, entry, frame, locals, localcount, scratch, queue, workcap,
			limit, output.functions, capacity, &output.written);
		if(accepted != (int)expected)
			fprintf(stderr, "bound numeric case %u: accepted=%d expected=%u\n", c, accepted, expected);
		boundcheck(accepted == (int)expected);
		if(!accepted){
			boundcheck(memcmp(&output, &saved, sizeof(output)) == 0);
			continue;
		}
		boundcheck(output.written == functions && functions <= Dis64LocalFunctions);
		for(i=0; i<functions; i++){
			for(j=0; j<13; j++)
				values[j] = number(file);
			actual[0] = output.functions[i].symbol;
			actual[1] = output.functions[i].entry;
			actual[2] = output.functions[i].end;
			actual[3] = output.functions[i].frame;
			actual[4] = output.functions[i].numeric.instructions;
			actual[5] = output.functions[i].numeric.slots;
			actual[6] = output.functions[i].numeric.parameters;
			actual[7] = output.functions[i].numeric.reached;
			actual[8] = output.functions[i].numeric.edges;
			actual[9] = output.functions[i].numeric.missing;
			actual[10] = output.functions[i].numeric.work;
			actual[11] = output.functions[i].numeric.reads;
			actual[12] = output.functions[i].numeric.writes;
			boundcheck(memcmp(actual, values, sizeof(actual)) == 0);
		}
		boundcheck(memcmp(output.functions+functions, saved.functions+functions,
			(Dis64LocalFunctions+1-functions)*sizeof(output.functions[0])) == 0);
		boundcheck(memcmp(output.before, saved.before, sizeof(output.before)) == 0);
		boundcheck(memcmp(output.after, saved.after, sizeof(output.after)) == 0);
	}
	boundcheck(fgetc(file) == EOF && fclose(file) == 0);
	printf("PASS catalog-bound numeric: %u metadata/actual-code cases; canonical intervals and atomic numeric-only outputs\n", cases);
	return 0;
}
