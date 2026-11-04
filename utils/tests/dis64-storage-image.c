#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "isa.h"
#include "dis64-storage-image.h"

static unsigned int checks; // Active structural predicates, independent of build assertions.


/* Every expected refusal must preserve the complete caller-owned output object. */
static void
check(int condition, const char *name, const char *stage)
{
	if(!condition){
		fprintf(stderr,"storage mismatch: %s %s\n",name,stage);
		exit(10);
	}
	checks++;
}


/* Read the independent host-neutral fixture stream without trusting declared extents. */
static unsigned int
word(FILE *input)
{
	unsigned char bytes[4];

	check(fread(bytes,1,4,input) == 4,"fixture","word");
	return (unsigned int)bytes[0]<<24 | (unsigned int)bytes[1]<<16 |
		(unsigned int)bytes[2]<<8 | bytes[3];
}


/* Execute the real joined reader on every immutable raw image and policy row. */
int
main(int argc, char **argv)
{
	unsigned char bytes[16384], before[16384];
	char name[128];
	Dis64StorageLimits limits;
	Dis64StorageImage view, saved;
	FILE *input;
	unsigned int cases, i, length, names, expected, result, policy[8];

	if(argc != 2)
		return 2;
	input = fopen(argv[1],"rb");
	if(input == 0)
		return 2;
	cases = word(input);
	for(i=0; i<cases; i++){
		names = word(input);
		check(names < sizeof(name),"fixture","name extent");
		check(fread(name,1,names,input) == names,"fixture","name bytes");
		name[names] = 0;
		expected = word(input);
		for(length=0; length<8; length++)
			policy[length] = word(input);
		limits.sections = policy[0]; limits.types = policy[1];
		limits.objectbytes = policy[2]; limits.strings = policy[3];
		limits.instructions = policy[4]; limits.bindings = policy[5];
		limits.parameters = policy[6]; limits.initializers = policy[7];
		length = word(input);
		check(length <= sizeof(bytes),name,"input extent");
		check(fread(bytes,1,length,input) == length,name,"input bytes");
		memcpy(before,bytes,length);
		memset(&view,0x5a,sizeof(view));
		memcpy(&saved,&view,sizeof(view));
		result = dis64storageimagecheck(bytes,length,&limits,&view);
		check(result == expected,name,"verdict");
		check(memcmp(bytes,before,length) == 0,name,"immutable source");
		if(!result)
			check(memcmp(&view,&saved,sizeof(view)) == 0,name,"atomic refusal");
		else{
			check(view.envelope.data == bytes && view.envelope.bytes == length,name,"source identity");
			check(view.data.bytes == view.module.databytes,name,"data linkage");
			check(view.types.count <= limits.types && view.code.count <= limits.instructions,name,"policy bounds");
		}
	}
	check(fgetc(input) == EOF,"fixture","trailing input");
	fclose(input);
	memset(&view,0x5a,sizeof(view)); memcpy(&saved,&view,sizeof(view));
	check(!dis64storageimagecheck(0,0,0,&view) && memcmp(&view,&saved,sizeof(view)) == 0,"nil","atomic arguments");
	printf("PASS Dis64 storage image: %u cases, %u checks; no execution admission\n",cases,checks);
	return 0;
}
