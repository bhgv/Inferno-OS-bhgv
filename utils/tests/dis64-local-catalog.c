#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dis64-local-catalog.h"

#define CatalogBytes 16384 // Maximum independently encoded fixture payload extent.

struct CatalogOutput {
	unsigned char before[32]; // Leading write guard.
	Dis64LocalFunction functions[Dis64LocalFunctions]; // Proposed checked output records.
	unsigned char after[32]; // Trailing write guard.
};


/* Keep test failures active in optimized and NDEBUG builds. */
static void
catalogcheck(int condition)
{
	if(!condition){
		fprintf(stderr, "local catalog predicate failed\n");
		exit(10);
	}
}


/* Read fixture framing independently of the production BE reader. */
static unsigned int
number(FILE *file)
{
	unsigned char bytes[4];

	catalogcheck(fread(bytes, 1, 4, file) == 4);
	return ((unsigned int)bytes[0]<<24)|((unsigned int)bytes[1]<<16)|
		((unsigned int)bytes[2]<<8)|bytes[3];
}


/* Compare every proposed interval, argument span and signature against Python's oracle. */
int
main(int argc, char **argv)
{
	unsigned char exports[CatalogBytes], strings[CatalogBytes], types[CatalogBytes];
	struct CatalogOutput output, saved;
	FILE *file;
	unsigned int cases, c, ne, ns, nt, limit, code, entry, frame, cap, expected, count;
	unsigned int i, j, value, fields[8];
	int accepted;

	catalogcheck(argc == 2);
	file = fopen(argv[1], "rb");
	catalogcheck(file != 0);
	cases = number(file);
	for(c=0; c<cases; c++){
		ne = number(file);
		ns = number(file);
		nt = number(file);
		limit = number(file);
		code = number(file);
		entry = number(file);
		frame = number(file);
		cap = number(file);
		expected = number(file);
		catalogcheck(ne<=CatalogBytes && ns<=CatalogBytes && nt<=CatalogBytes);
		catalogcheck(fread(exports,1,ne,file)==ne && fread(strings,1,ns,file)==ns && fread(types,1,nt,file)==nt);
		memset(&output, 0xa5, sizeof(output));
		memcpy(&saved, &output, sizeof(saved));
		count = 0xc0de;
		accepted = dis64localcatalogcheck(exports,ne,strings,ns,types,nt,limit,
			code,entry,frame,output.functions,cap,&count);
		catalogcheck(accepted == (expected != 0));
		if(!accepted){
			catalogcheck(count == 0xc0de && memcmp(&saved,&output,sizeof(output)) == 0);
			continue;
		}
		catalogcheck(count == expected);
		catalogcheck(memcmp(output.functions+count,saved.functions+count,
			(Dis64LocalFunctions-count)*sizeof(Dis64LocalFunction))==0);
		catalogcheck(memcmp(output.before,saved.before,32)==0 && memcmp(output.after,saved.after,32)==0);
		for(i=0; i<count; i++){
			Dis64LocalFunction *function;

			function = output.functions+i;
			fields[0] = function->symbol;
			fields[1] = function->entry;
			fields[2] = function->end;
			fields[3] = function->frame;
			fields[4] = function->argumentend;
			fields[5] = function->resultcell;
			fields[6] = function->signature.result;
			fields[7] = function->signature.parameters;
			for(j=0; j<8; j++)
				catalogcheck(fields[j] == number(file));
			for(j=0; j<function->signature.parameters; j++){
				value = number(file);
				catalogcheck(dis64u32(function->signature.data+12+4*j) == value);
			}
		}
	}
	catalogcheck(fgetc(file)==EOF && fclose(file)==0);
	printf("PASS experimental local catalog: %u independent metadata/geometry cases; no executable admission\n",cases);
	return 0;
}
