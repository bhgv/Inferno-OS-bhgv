#include "dis64-call-image.h"
#include "dis64-pilot.h"


/* Read whole immutable host files, then prove all sections before constructing a Module.
 * The companion declarations are trusted fixture/compiler input, never file capabilities.
 * Production readmod/parsemod is not replaced or enabled by this private adapter. */
int
main(int argc, char **argv)
{
	static uchar imagebytes[Dis64CallImageBytes];
	static Dis64BoundCall savedproof[Dis64LocalFunctions];
	Dis64FunctionLocals locals[Dis64LocalFunctions];
	Dis64LocalSlot slots[Dis64LocalFunctions][Dis64LocalSlots];
	Dis64CallImage image, beforeimage;
	Dis64Pilot pilot, beforepilot;
	unsigned int vectors, c, bytes, namesize, objectlimit, localcount, workcap, limit;
	unsigned int i, j, valid, kind, failure, chain, records, high, low, accepted, refused;
	uvlong expected, second;
	char path[4096];
	FILE *file, *input;
	int before, status, jit, grow, saved, q, onlyjit, runs, oversized;
	int quanta[] = {1, 7, 1000};

	require(argc == 2 || argc == 3);
	onlyjit = argc == 3 ? atoi(argv[2]) : -1;
	optab[IFRAME] = frame;
	optab[ICALL] = call;
	optab[IRET] = ret;
	optab[ILEA] = lea;
	optab[ICVTWF] = cvtwf;
	optab[IMOVW] = movw;
	optab[IMOVL] = movl;
	optab[IMOVF] = movf;
	optab[IDIVW] = divw;
	optab[IEXIT] = iexit;
	file = fopen(argv[1], "rb");
	require(file != nil);
	vectors = resultnumber(file);
	accepted = refused = runs = 0;
	for(c=0; c<vectors; c++){
		namesize = resultnumber(file);
		require(namesize > 0 && namesize < sizeof(path));
		require(fread(path, 1, namesize, file) == namesize);
		path[namesize] = 0;
		input = fopen(path, "rb");
		require(input != nil);
		bytes = fread(imagebytes, 1, sizeof(imagebytes), input);
		oversized = fgetc(input) != EOF;
		require(fclose(input) == 0);
		objectlimit = resultnumber(file);
		localcount = resultnumber(file);
		workcap = resultnumber(file);
		limit = resultnumber(file);
		require(localcount <= Dis64LocalFunctions && workcap <= ResultCode);
		for(i=0; i<localcount; i++){
			locals[i].symbol = resultnumber(file);
			locals[i].entry = resultnumber(file);
			locals[i].frame = resultnumber(file);
			locals[i].count = resultnumber(file);
			require(locals[i].count <= Dis64LocalSlots);
			for(j=0; j<locals[i].count; j++){
				slots[i][j].offset = resultnumber(file);
				slots[i][j].kind = resultnumber(file);
			}
			locals[i].slots = slots[i];
		}
		valid = resultnumber(file);
		kind = resultnumber(file);
		high = resultnumber(file);
		low = resultnumber(file);
		expected = ((uvlong)high<<32)|low;
		failure = resultnumber(file);
		chain = resultnumber(file);
		high = resultnumber(file);
		low = resultnumber(file);
		second = ((uvlong)high<<32)|low;
		memset(&image, 0x5a, sizeof(image));
		memmove(&beforeimage, &image, sizeof(image));
		memset(resultproof, 0x6b, sizeof(resultproof));
		memmove(savedproof, resultproof, sizeof(resultproof));
		records = 0x7a7a7a7aU;
		before = allocations;
		status = !oversized && dis64callimagecheck(imagebytes, bytes, objectlimit,
		locals, localcount, resultdecoded, resultwork, resultqueue, workcap, limit,
		resultproof, Dis64LocalFunctions, &records, &image);
		resultcheck(status == valid && allocations == before && live == 0, "whole-image decision mismatch");
		if(!status){
			resultcheck(records == 0x7a7a7a7aU && memcmp(&image, &beforeimage, sizeof(image)) == 0 &&
			memcmp(resultproof, savedproof, sizeof(resultproof)) == 0, "whole-image refusal published output");
			refused++;
			continue;
		}
		resultcheck(image.envelope.data == imagebytes && records == image.functions && records == localcount,
		"whole-image identity mismatch");
		/* The current production semantic gate still refuses calls, even for these valid private images. */
		memset(&pilot, 0x3c, sizeof(pilot));
		memmove(&beforepilot, &pilot, sizeof(pilot));
		resultcheck(!dis64pilotcheck(imagebytes, bytes, objectlimit, ResultCode, &pilot) &&
		memcmp(&pilot, &beforepilot, sizeof(pilot)) == 0, "public file-call admission changed");
		resultentry = image.module.entry;
		resultframe = image.module.frametype;
		resultcount = image.instructions;
		resulttypes = image.types;
		for(jit=0; jit<2; jit++){
			if(onlyjit >= 0 && jit != onlyjit) continue;
			for(grow=0; grow<2; grow++)
			for(saved=0; saved<2; saved++)
			for(q=0; q<nelem(quanta); q++){
				resultrun(jit, grow, failure, quanta[q], saved, kind, expected, 0, chain, second);
				runs++;
				if(grow && failure == 0){
					resultrun(jit, grow, failure, quanta[q], saved, kind, expected, 1, chain, second);
					runs++;
				}
			}
		}
		accepted++;
	}
	require(fgetc(file) == EOF && fclose(file) == 0 && live == 0);
	printf("PASS whole-image calls: %u private images, %u atomic refusals, %d actual C/RX/extent/quanta/saved/error runs; public file calls remain closed\n", accepted, refused, runs);
	return 0;
}
