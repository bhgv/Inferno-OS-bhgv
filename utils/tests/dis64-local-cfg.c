#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dis64-local-cfg.h"


/* Require semantic oracle/guard failures even when NDEBUG is enabled. */
static void
cfgcheck(int condition)
{
	if(!condition){
		fprintf(stderr, "local CFG predicate failed\n");
		exit(10);
	}
}


/* Read fixture words independently of the production codecs. */
static unsigned int
number(FILE *file)
{
	unsigned char bytes[4];

	cfgcheck(fread(bytes, 1, 4, file) == 4);
	return ((unsigned int)bytes[0]<<24)|((unsigned int)bytes[1]<<16)|
		((unsigned int)bytes[2]<<8)|bytes[3];
}


/* Compare entry-derived CFG summaries and transactional refusal with poisoned scratch. */
int
main(int argc, char **argv)
{
	static Dis64LocalCFGNode nodes[Dis64LocalInstructions+1];
	static Dis64LocalCFGWork scratch[Dis64LocalInstructions+1];
	static unsigned int queue[Dis64LocalInstructions+1];
	Dis64LocalCallee functions[Dis64LocalFunctions+1];
	Dis64LocalPlan plan;
	Dis64LocalEvent *event;

	/* Summary guards are independent of disposable traversal scratch. */
	struct {
		unsigned char before[32]; // Leading publication guard.
		Dis64LocalCFG summary; // Only complete successful analyses may publish here.
		unsigned char after[32]; // Trailing publication guard.
	} output, saved;
	FILE *file;
	unsigned int cases, c, count, base, identity, capacity, limit, i, j, expected, values[7], actual[7];
	int accepted;

	cfgcheck(argc == 2);
	file = fopen(argv[1], "rb");
	cfgcheck(file != 0);
	cases = number(file);
	for(c=0; c<cases; c++){
		plan.framebytes = number(file);
		plan.argumentend = number(file);
		plan.resultcell = number(file);
		plan.result = number(file);
		plan.callees = number(file);
		cfgcheck(plan.callees <= Dis64LocalFunctions+1);
		for(i=0; i<plan.callees; i++){
			functions[i].frame = number(file);
			functions[i].result = number(file);
			functions[i].parameters = number(file);
			for(j=0; j<Dis64LocalParameters; j++)
				functions[i].kinds[j] = number(file);
		}
		plan.functions = functions;
		count = number(file);
		base = number(file);
		identity = number(file);
		capacity = number(file);
		limit = number(file);
		cfgcheck(count <= Dis64LocalInstructions+1);
		for(i=0; i<count; i++){
			event = &nodes[i].event;
			event->action = number(file);
			event->site = number(file);
			event->callee = number(file);
			event->frame = number(file);
			event->index = number(file);
			event->kind = number(file);
			event->boolean = number(file);
			event->source = number(file);
			event->destination = number(file);
			event->width = number(file);
			nodes[i].edge = number(file);
			nodes[i].target = number(file);
			nodes[i].setup = number(file);
		}
		expected = number(file);
		for(i=0; i<7; i++)
			values[i] = number(file);
		memset(scratch, 0xa5, count*sizeof(scratch[0]));
		memset(queue, 0xa5, count*sizeof(queue[0]));
		memset(&output, 0xa5, sizeof(output));
		memcpy(&saved, &output, sizeof(saved));
		accepted = dis64localcfgcheck(&plan, nodes, count, base, identity, limit,
			scratch, queue, capacity, &output.summary);
		cfgcheck(accepted == (int)expected);
		if(!accepted){
			cfgcheck(memcmp(&output, &saved, sizeof(output)) == 0);
			continue;
		}
		actual[0] = output.summary.function;
		actual[1] = output.summary.instructions;
		actual[2] = output.summary.reached;
		actual[3] = output.summary.edges;
		actual[4] = output.summary.returns;
		actual[5] = output.summary.failures;
		actual[6] = output.summary.work;
		cfgcheck(memcmp(actual, values, sizeof(actual)) == 0);
		cfgcheck(memcmp(output.before, saved.before, 32) == 0);
		cfgcheck(memcmp(output.after, saved.after, 32) == 0);
	}
	cfgcheck(fgetc(file) == EOF && fclose(file) == 0);
	printf("PASS experimental local CFG: %u entry-seeded graphs; poisoned scratch and transactional summary; no wire admission\n", cases);
	return 0;
}
