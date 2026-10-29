/* Inspect or canonically rewrite only the agreed Dis64 container envelope. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "isa.h"
#include "dis64-pilot.h"

#define InspectLimit (64U*1024U*1024U) // Explicit host-tool file resource limit.


/* Read a stable bounded image before any envelope or payload interpretation. */
static unsigned char*
readimage(const char *path, unsigned int *bytes)
{
	FILE *file;
	unsigned char *data;
	long length;
	int extra, bad;

	file = fopen(path, "rb");
	if(file == 0)
		return 0;
	if(fseek(file, 0, SEEK_END) != 0 || (length = ftell(file)) < 0 ||
	   (unsigned long)length > InspectLimit || fseek(file, 0, SEEK_SET) != 0){
		fclose(file);
		return 0;
	}
	data = malloc(length == 0 ? 1 : (unsigned long)length);
	if(data == 0){
		fclose(file);
		return 0;
	}
	bad = fread(data, 1, (unsigned long)length, file) != (unsigned long)length;
	extra = fgetc(file);
	bad |= extra != EOF || ferror(file);
	bad |= fclose(file) != 0;
	if(bad){
		free(data);
		return 0;
	}
	*bytes = (unsigned int)length;
	return data;
}


/* Rewrite an extension-free checked envelope; preserve opaque payload bytes. */
static int
rewrite(const char *path, const Dis64View *view)
{
	Dis64Payload *payloads;
	Dis64Section section;
	unsigned char *output;
	unsigned int i, bytes;
	FILE *file;
	int ok;

	payloads = calloc(view->sections, sizeof(*payloads));
	output = malloc(view->bytes);
	if(payloads == 0 || output == 0){
		free(payloads);
		free(output);
		return 0;
	}
	for(i=0; i<view->sections; i++){
		if(!dis64section(view, i, &section)){
			free(payloads);
			free(output);
			return 0;
		}
		payloads[i].kind = section.kind;
		payloads[i].flags = section.flags;
		payloads[i].data = view->data+section.offset;
		payloads[i].bytes = section.bytes;
	}
	ok = dis64write(output, view->bytes, 0, 0, payloads, view->sections, &bytes);
	if(ok){
		file = fopen(path, "wb");
		if(file == 0)
			ok = 0;
		else{
			ok = fwrite(output, 1, bytes, file) == bytes;
			if(fclose(file) != 0)
				ok = 0;
		}
	}
	free(payloads);
	free(output);
	return ok;
}


/* Report structural acceptance explicitly without promising executable support. */
int
main(int argc, char **argv)
{
	unsigned char *data;
	unsigned int bytes, i;
	int pilotmode, rewriteindex;
	Dis64Pilot pilot = {0};
	Dis64View view;
	Dis64Section section;

	/* Flags have one unambiguous order and never execute container code. */
	pilotmode = argc > 2 && strcmp(argv[2], "--pilot") == 0;
	rewriteindex = pilotmode ? 3 : 2;
	if(argc != rewriteindex && !(argc == rewriteindex+2 &&
	   strcmp(argv[rewriteindex], "--rewrite") == 0)){
		fprintf(stderr, "usage: dis64inspect input [--pilot] [--rewrite output]\n");
		return 2;
	}
	data = readimage(argv[1], &bytes);
	if(data == 0){
		fprintf(stderr, "cannot read bounded input image\n");
		return 1;
	}
	/* No extension implementation is claimed by this envelope-only inspector. */
	if(!dis64check(data, bytes, 1024, 4096, 0, 0, &view)){
		fprintf(stderr, "invalid envelope or unsupported mandatory extension\n");
		free(data);
		return 1;
	}
	/* Semantic rejection precedes opening the optional rewrite destination. */
	if(pilotmode && !dis64pilotcheck(data, bytes, InspectLimit, 1048576, &pilot)){
		fprintf(stderr, "unsupported or invalid scalar pilot payloads\n");
		free(data);
		return 1;
	}
	if(argc == rewriteindex+2 && !rewrite(argv[rewriteindex+1], &view)){
		fprintf(stderr, "cannot write canonical envelope\n");
		free(data);
		return 1;
	}
	printf("{\"envelope_only\":%s,\"execution_supported\":false,\"format_major\":%u,"
		"\"format_minor\":%u,\"abi_id\":%u,\"bytes\":%u,\"header_bytes\":%u,\"sections\":[",
		pilotmode ? "false" : "true", Dis64Major, Dis64Minor, Dis64Abi, view.bytes, view.headerbytes);
	for(i=0; i<view.sections; i++){
		if(!dis64section(&view, i, &section)){
			free(data);
			return 1;
		}
		printf("%s{\"kind\":%u,\"flags\":%u,\"offset\":%u,\"bytes\":%u}",
			i == 0 ? "" : ",", section.kind, section.flags, section.offset, section.bytes);
	}
	printf("]");
	if(pilotmode){
		printf(",\"scalar_pilot\":true,\"data_bytes\":%u,\"frame_bytes\":%u,"
			"\"entry_pc\":%u,\"instruction_count\":%u,\"string_count\":%u,\"type_count\":%u,\"initializer_count\":%u",
			pilot.module.databytes, pilot.frame.bytes, pilot.module.entry,
			pilot.code.count, pilot.strings.count, pilot.types.count, pilot.initializers.count);
	}
	printf("}\n");
	free(data);
	return 0;
}
