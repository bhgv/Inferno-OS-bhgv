#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dis64-local-state.h"


/* Keep every oracle, refusal and publication check active under NDEBUG. */
static void
statecheck(int condition)
{
	if(!condition){
		fprintf(stderr, "local state predicate failed\n");
		exit(10);
	}
}


/* Decode independent big-endian fixture integers, without production wire codecs. */
static unsigned int
number(FILE *file)
{
	unsigned char bytes[4];

	statecheck(fread(bytes, 1, 4, file) == 4);
	return ((unsigned int)bytes[0]<<24)|((unsigned int)bytes[1]<<16)|
		((unsigned int)bytes[2]<<8)|bytes[3];
}


/* Read fields explicitly; fixture layout never depends on C padding or host endianness. */
static void
readstate(FILE *file, Dis64LocalState *state)
{
	state->phase = number(file);
	state->site = number(file);
	state->callee = number(file);
	state->bound = number(file);
	state->ready = number(file);
	state->assigned = number(file);
	state->arguments[0] = number(file);
	state->arguments[1] = number(file);
}


/* Compare fields rather than object bytes, which can contain unspecified padding. */
static int
samestate(const Dis64LocalState *left, const Dis64LocalState *right)
{
	return left->phase == right->phase && left->site == right->site &&
		left->callee == right->callee && left->bound == right->bound &&
		left->ready == right->ready && left->assigned == right->assigned &&
		left->arguments[0] == right->arguments[0] && left->arguments[1] == right->arguments[1];
}


/* Exercise transitions/joins, guards and in-place publication for each oracle record. */
int
main(int argc, char **argv)
{
	Dis64LocalCallee functions[Dis64LocalFunctions+1];
	Dis64LocalPlan plan;
	Dis64LocalState left, right, expected, alias;
	Dis64LocalEvent event;

	/* Guard both sides of the transactionally published candidate. */
	struct {
		unsigned char before[32]; // Leading publication guard.
		Dis64LocalState state; // Candidate output, untouched on refusal.
		unsigned char after[32]; // Trailing publication guard.
	} output, saved;
	FILE *file;
	unsigned int groups, records, group, record, operation, accepted, i, j, total;
	int result;

	statecheck(argc == 2);
	file = fopen(argv[1], "rb");
	statecheck(file != 0);
	groups = number(file);
	total = 0;
	for(group=0; group<groups; group++){
		plan.framebytes = number(file);
		plan.argumentend = number(file);
		plan.resultcell = number(file);
		plan.result = number(file);
		plan.callees = number(file);
		statecheck(plan.callees <= Dis64LocalFunctions+1);
		for(i=0; i<plan.callees; i++){
			functions[i].frame = number(file);
			functions[i].result = number(file);
			functions[i].parameters = number(file);
			for(j=0; j<Dis64LocalParameters; j++)
				functions[i].kinds[j] = number(file);
		}
		plan.functions = functions;
		records = number(file);
		for(record=0; record<records; record++){
			operation = number(file);
			readstate(file, &left);
			readstate(file, &right);
			event.action = number(file);
			event.site = number(file);
			event.callee = number(file);
			event.frame = number(file);
			event.index = number(file);
			event.kind = number(file);
			event.boolean = number(file);
			event.source = number(file);
			event.destination = number(file);
			event.width = number(file);
			accepted = number(file);
			readstate(file, &expected);
			memset(&output, 0xa5, sizeof(output));
			memcpy(&saved, &output, sizeof(saved));
			if(operation == 0)
				result = dis64localstatestep(&plan, &left, &event, &output.state);
			else{
				statecheck(operation == 1);
				result = dis64localstatemerge(&plan, &left, &right, &output.state);
			}
			statecheck(result == (int)accepted);
			if(result){
				statecheck(samestate(&output.state, &expected));
				statecheck(memcmp(output.before, saved.before, 32) == 0);
				statecheck(memcmp(output.after, saved.after, 32) == 0);
			}else
				statecheck(memcmp(&output, &saved, sizeof(output)) == 0);
			alias = left;
			if(operation == 0)
				result = dis64localstatestep(&plan, &alias, &event, &alias);
			else
				result = dis64localstatemerge(&plan, &alias, &right, &alias);
			statecheck(result == (int)accepted);
			statecheck(samestate(&alias, result ? &expected : &left));
			if(operation == 1){
				alias = right;
				result = dis64localstatemerge(&plan, &left, &alias, &alias);
				statecheck(result == (int)accepted);
				statecheck(samestate(&alias, result ? &expected : &right));
			}
			total++;
		}
	}
	statecheck(fgetc(file) == EOF && fclose(file) == 0);
	printf("PASS experimental local state: %u transition/join records; disjoint and in-place outputs; no wire admission\n", total);
	return 0;
}
