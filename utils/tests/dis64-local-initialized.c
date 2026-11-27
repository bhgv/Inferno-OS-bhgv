#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "isa.h"
#include "dis64-local-initialized.h"


/* Always-active semantic/refusal predicates, including optimized NDEBUG builds. */
static void
initcheck(int condition)
{
	if(!condition){
		fprintf(stderr, "local initialized predicate failed\n");
		exit(10);
	}
}


/* Independent fixture framing, without using production code/signature codecs. */
static unsigned int
number(FILE *file)
{
	unsigned char b[4];

	initcheck(fread(b, 1, 4, file) == 4);
	return ((unsigned int)b[0]<<24)|((unsigned int)b[1]<<16)|((unsigned int)b[2]<<8)|b[3];
}


/* Check complete summaries and each reached fixed-point set against a synchronous oracle. */
int
main(int argc, char **argv)
{
	static unsigned char code[Dis64LocalBytes+1], signature[512];
	static Dis64InitWork scratch[Dis64LocalInstructions+1];
	static unsigned int queue[Dis64LocalInstructions+1];
	Dis64LocalSlot slots[Dis64LocalSlots+1];

	/* Summary is the only published output; traversal scratch stays disposable. */
	struct {
		unsigned char before[32]; // Leading publication guard.
		Dis64Initialized summary; // Completed analysis summary only.
		unsigned char after[32]; // Trailing publication guard.
	} output, saved;
	FILE *file;
	unsigned int cases, c, frame, slotcount, sigbytes, codebytes, capacity, limit, expected;
	unsigned int i, j, values[9], actual[9], seen, bits[4];
	int accepted;

	initcheck(argc == 2);
	file = fopen(argv[1], "rb");
	initcheck(file != 0);
	cases = number(file);
	for(c=0; c<cases; c++){
		frame = number(file);
		slotcount = number(file);
		initcheck(slotcount <= Dis64LocalSlots+1);
		for(i=0; i<slotcount; i++){
			slots[i].offset = number(file);
			slots[i].kind = number(file);
		}
		sigbytes = number(file);
		codebytes = number(file);
		capacity = number(file);
		limit = number(file);
		initcheck(sigbytes <= sizeof(signature) && codebytes <= sizeof(code) && capacity <= Dis64LocalInstructions);
		initcheck(fread(signature, 1, sigbytes, file) == sigbytes);
		initcheck(fread(code, 1, codebytes, file) == codebytes);
		expected = number(file);
		for(i=0; i<9; i++)
			values[i] = number(file);
		memset(scratch, 0x5a, (capacity+1)*sizeof(scratch[0]));
		memset(queue, 0x5a, (capacity+1)*sizeof(queue[0]));
		memset(&output, 0xa5, sizeof(output));
		memcpy(&saved, &output, sizeof(saved));
		accepted = dis64initializedcheck(code, codebytes, signature, sigbytes, slots, slotcount,
			frame, scratch, queue, capacity, limit, &output.summary);
		if(accepted != (int)expected)
			fprintf(stderr, "initialized case %u: accepted=%d expected=%u\n", c, accepted, expected);
		initcheck(accepted == (int)expected);
		if(!accepted){
			initcheck(memcmp(&output, &saved, sizeof(output)) == 0);
			continue;
		}
		actual[0] = output.summary.instructions;
		actual[1] = output.summary.slots;
		actual[2] = output.summary.parameters;
		actual[3] = output.summary.reached;
		actual[4] = output.summary.edges;
		actual[5] = output.summary.missing;
		actual[6] = output.summary.work;
		actual[7] = output.summary.reads;
		actual[8] = output.summary.writes;
		initcheck(memcmp(actual, values, sizeof(actual)) == 0);
		initcheck(memcmp(output.before, saved.before, sizeof(output.before)) == 0);
		initcheck(memcmp(output.after, saved.after, sizeof(output.after)) == 0);
		for(i=0; i<output.summary.instructions; i++){
			seen = number(file);
			for(j=0; j<4; j++)
				bits[j] = number(file);
			initcheck(scratch[i].seen == seen);
			if(seen)
				initcheck(memcmp(scratch[i].incoming, bits, sizeof(bits)) == 0);
		}
	}
	initcheck(fgetc(file) == EOF && fclose(file) == 0);
	printf("PASS experimental numeric initialized: %u raw CFG/signature cases; entry seeds, intersections and exact reached sets\n", cases);
	return 0;
}
