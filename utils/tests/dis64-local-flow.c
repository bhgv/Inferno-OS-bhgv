#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "isa.h"
#include "dis64-local-flow.h"


/* Keep rejection/field/atomic-output checks active with NDEBUG. */
static void
flowcheck(int condition)
{
	if(!condition){
		fprintf(stderr, "local flow predicate failed\n");
		exit(10);
	}
}


/* Read fixture framing independently of production codecs. */
static unsigned int
number(FILE *file)
{
	unsigned char b[4];

	flowcheck(fread(b,1,4,file)==4);
	return ((unsigned int)b[0]<<24)|((unsigned int)b[1]<<16)|((unsigned int)b[2]<<8)|b[3];
}


/* Check an independent graph oracle and unchanged output for every rejection. */
int
main(int argc, char **argv)
{
	Dis64LocalFunction functions[Dis64LocalFunctions+1];
	static Dis64LocalNode nodes[Dis64LocalInstructions+1];
	struct {
		unsigned char before[32]; // Leading publication guard.
		Dis64LocalFlow flow; // Candidate summary, published only on success.
		unsigned char after[32]; // Trailing publication guard.
	} output, saved;
	FILE *file;
	unsigned int cases, c, count, instructions, limit, expected, i, fields[6];
	int accepted;

	flowcheck(argc==2);
	file=fopen(argv[1],"rb");
	flowcheck(file!=0);
	cases=number(file);
	for(c=0; c<cases; c++){
		count=number(file);
		instructions=number(file);
		limit=number(file);
		expected=number(file);
		flowcheck(count<=Dis64LocalFunctions+1 && instructions<=Dis64LocalInstructions+1);
		memset(functions,0,sizeof(functions));
		for(i=0; i<count; i++){
			functions[i].entry=number(file);
			functions[i].end=number(file);
		}
		for(i=0; i<instructions; i++){
			nodes[i].op=number(file);
			nodes[i].target=number(file);
		}
		memset(&output,0xa5,sizeof(output));
		memcpy(&saved,&output,sizeof(saved));
		accepted=dis64localflowcheck(functions,count,nodes,instructions,limit,&output.flow);
		flowcheck(accepted==(int)expected);
		if(!accepted){
			flowcheck(memcmp(&output,&saved,sizeof(output))==0);
			continue;
		}
		flowcheck(memcmp(output.before,saved.before,32)==0 && memcmp(output.after,saved.after,32)==0);
		fields[0]=output.flow.functions;
		fields[1]=output.flow.instructions;
		fields[2]=output.flow.branches;
		fields[3]=output.flow.calls;
		fields[4]=output.flow.depth;
		fields[5]=output.flow.terminals;
		for(i=0; i<6; i++)
			flowcheck(fields[i]==number(file));
	}
	flowcheck(fgetc(file)==EOF && fclose(file)==0);
	printf("PASS experimental local flow: %u independent interval/graph cases; no wire admission\n",cases);
	return 0;
}
