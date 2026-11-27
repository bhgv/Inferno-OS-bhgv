#include "lib9.h"
#include "freetype.h"
#include <stdio.h>


/* Fail explicitly if a vendored engine invariant reaches its native exit hook. */
void
exits(char *message)
{
	fprintf(stderr, "unexpected Freetype exit: %s\n", message);
	exit(1);
}


/* Keep the real-engine smoke oracle independent of NDEBUG. */
static void
check(int truth)
{
	if(!truth){
		fprintf(stderr, "Freetype engine mismatch\n");
		exit(1);
	}
}


/* Render real repository fonts with the vendored engine and production bridge.
 * This executable uses the vendored libc file adapter, not the Inferno namespace. */
int
main(int argc, char **argv)
{
	FTface face;
	FTfaceinfo info;
	FTglyph base, moved, rotated;
	FTmatrix matrix = {0, -65536, 65536, 0};
	FTvector delta = {128, 192};
	int i;

	check(argc > 1);
	for(i=1; i<argc; i++){
		memset(&face, 0, sizeof(face));
		check(ftnewface(argv[i], 0, &face, &info) == nil);
		check(ftsetcharsize(face, 16*64, 72, 72, &info) == nil);
		check(fthaschar(face, 'A'));
		ftsettransform(face, nil, nil);
		check(ftloadglyph(face, 'A', &base) == nil);
		check(base.width > 0 && base.height > 0 && base.bitmap != nil && base.advx > 0);
		ftsettransform(face, nil, &delta);
		check(ftloadglyph(face, 'A', &moved) == nil);
		check(moved.width == base.width && moved.height == base.height);
		check(moved.left == base.left+2 && moved.top == base.top+3);
		check(moved.advx == base.advx && moved.advy == base.advy);
		ftsettransform(face, &matrix, nil);
		check(ftloadglyph(face, 'A', &rotated) == nil);
		check(rotated.width > 0 && rotated.height > 0 && rotated.bitmap != nil);
		check(rotated.advx == -base.advy && rotated.advy == base.advx);
		ftdoneface(face);
	}
	printf("PASS vendored Freetype engine: %d fonts; identity, translation and rotation\n", argc-1);
	return 0;
}
