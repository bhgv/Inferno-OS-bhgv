#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "isa.h"
#include "dis64-local-operands.h"


/* Semantic predicates stay active under NDEBUG and distinguish controls from crashes. */
static void
operandcheck(int condition)
{
	if(!condition){
		fprintf(stderr, "local operand predicate failed\n");
		exit(10);
	}
}


/* Independent fixture framing, not the production operand decoder. */
static unsigned int
number(FILE *file)
{
	unsigned char b[4];

	operandcheck(fread(b, 1, 4, file) == 4);
	return ((unsigned int)b[0]<<24)|((unsigned int)b[1]<<16)|((unsigned int)b[2]<<8)|b[3];
}


/* Exercise scalar helper regressions before any control could reach undefined modulo. */
static void
slotregressions(void)
{
	Dis64Instruction instruction;
	Dis64LocalSlot slot = {64, Dis64Int};
	Dis64LocalOperands output, saved;
	long wide;

	operandcheck(!dis64codeslot(AIMM, 0, 0, 128, 128, 1));
	operandcheck(!dis64codeslot(AFP, 64, 0, 128, 128, 0));
	operandcheck(!dis64codeslot(AFP, 66, 3, 128, 128, 0));
	operandcheck(dis64codeslot(AFP, 64, 4, 0, 128, 0));
	if(sizeof(long) > 4){
		wide = (long)0x100000040ULL;
		operandcheck(!dis64codeslot(AFP, wide, 4, 0, 128, 0));
		operandcheck(!dis64codeslot(AMP, wide, 4, 128, 128, 0));
		instruction.op = IMOVW;
		instruction.mode = SRC(AFP)|DST(AFP)|AXNON;
		instruction.source = wide;
		instruction.middle = 0;
		instruction.destination = 64;
		operandcheck(!dis64instructioncheck(&instruction, 1, 0, 128));
		memset(&output, 0xa5, sizeof(output));
		memcpy(&saved, &output, sizeof(saved));
		operandcheck(!dis64localoperandscheck(&instruction, 1, 0, 1, &slot, 1, 128, &output));
		operandcheck(memcmp(&output, &saved, sizeof(output)) == 0);
	}
	instruction.op = IJMP;
	instruction.mode = SRC(AXXX)|DST(AIMM)|AXNON;
	instruction.source = instruction.middle = 0;
	instruction.destination = 0;
	memset(&output, 0xa5, sizeof(output));
	memcpy(&saved, &output, sizeof(saved));
	operandcheck(!dis64localoperandscheck(&instruction, 4, 2, 4, 0, 0, 72, &output));
	operandcheck(memcmp(&output, &saved, sizeof(output)) == 0);
	instruction.destination = 2;
	operandcheck(dis64localoperandscheck(&instruction, 4, 2, 4, 0, 0, 72, &output));
	operandcheck(output.target == 2 && output.edge == Dis64CFGJump);
	instruction.op = IEXIT;
	instruction.mode = SRC(AXXX)|DST(AXXX)|AXNON;
	instruction.source = 1;
	instruction.destination = 0;
	memset(&output, 0xa5, sizeof(output));
	memcpy(&saved, &output, sizeof(saved));
	operandcheck(!dis64localoperandscheck(&instruction, 1, 0, 1, 0, 0, 72, &output));
	operandcheck(memcmp(&output, &saved, sizeof(output)) == 0);
}


/* Compare every fact, written count, unused output and refusal bytes against the oracle. */
int
main(int argc, char **argv)
{
	static unsigned char data[Dis64LocalBytes+1];
	static Dis64LocalOperands scratch[Dis64LocalInstructions+1];
	Dis64LocalSlot slots[Dis64LocalSlots+1];

	/* Include one sentinel record beyond each advertised logical output capacity. */
	static struct {
		unsigned char before[32]; // Leading output guard.
		unsigned int written; // Transactional published count.
		Dis64LocalOperands facts[Dis64LocalInstructions+1]; // Accessible backing for capacity controls.
		unsigned char after[32]; // Trailing output guard.
	} output, saved;
	FILE *file;
	unsigned int cases, c, frame, count, bytes, capacity, expected, written, i, j, wanted[8], actual[8], span;
	int accepted;

	operandcheck(argc == 2);
	slotregressions();
	file = fopen(argv[1], "rb");
	operandcheck(file != 0);
	cases = number(file);
	for(c=0; c<cases; c++){
		frame = number(file);
		count = number(file);
		operandcheck(count <= Dis64LocalSlots+1);
		for(i=0; i<count; i++){
			slots[i].offset = number(file);
			slots[i].kind = number(file);
		}
		bytes = number(file);
		capacity = number(file);
		operandcheck(bytes <= Dis64LocalBytes+1 && capacity <= Dis64LocalInstructions);
		operandcheck(fread(data, 1, bytes, file) == bytes);
		expected = number(file);
		written = number(file);
		span = capacity+1;
		memset(output.before, 0xa5, sizeof(output.before));
		memset(output.after, 0xa5, sizeof(output.after));
		output.written = 0xa5a5a5a5U;
		memset(output.facts, 0xa5, span*sizeof(output.facts[0]));
		/* Different poison makes copying an unproduced extra record observable. */
		memset(scratch, 0x5a, span*sizeof(scratch[0]));
		memcpy(saved.before, output.before, sizeof(output.before));
		memcpy(saved.after, output.after, sizeof(output.after));
		saved.written = output.written;
		memcpy(saved.facts, output.facts, span*sizeof(output.facts[0]));
		accepted = dis64localoperandsdecode(data, bytes, slots, count, frame,
			scratch, output.facts, capacity, &output.written);
		operandcheck(accepted == (int)expected);
		if(!accepted){
			operandcheck(output.written == saved.written);
			operandcheck(memcmp(output.facts, saved.facts, span*sizeof(output.facts[0])) == 0);
		}else{
			operandcheck(output.written == written && written <= capacity);
			for(i=0; i<written; i++){
				for(j=0; j<8; j++)
					wanted[j] = number(file);
				actual[0] = output.facts[i].op;
				actual[1] = output.facts[i].reads;
				actual[2] = output.facts[i].source;
				actual[3] = output.facts[i].middle;
				actual[4] = output.facts[i].write;
				actual[5] = output.facts[i].kind;
				actual[6] = output.facts[i].target;
				actual[7] = output.facts[i].edge;
				operandcheck(memcmp(actual, wanted, sizeof(actual)) == 0);
			}
			operandcheck(memcmp(output.facts+written, saved.facts+written,
				(span-written)*sizeof(output.facts[0])) == 0);
		}
		operandcheck(memcmp(output.before, saved.before, sizeof(output.before)) == 0);
		operandcheck(memcmp(output.after, saved.after, sizeof(output.after)) == 0);
	}
	operandcheck(fgetc(file) == EOF && fclose(file) == 0);
	printf("PASS experimental local operands: %u raw payload/layout cases; exact typed facts and transactional output\n", cases);
	printf("PASS scalar slot guards: invalid widths and non-narrowing offsets; long-bytes=%u\n", (unsigned int)sizeof(long));
	return 0;
}
