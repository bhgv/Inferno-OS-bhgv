#include "lib9.h"
#include "freetype/freetype.h"
#include "freetype/internal/ftstream.h"
#include "freetype.h"
#include <stdio.h>

static int checks, saw_matrix, saw_vector; // Active native-ABI observations.
static FT_Matrix observed_matrix; // Native long-width coefficients passed by production.
static FT_Vector observed_vector; // Native long-width deltas passed by production.
static FT_GlyphSlotRec slot; // A real FreeType slot declaration, with bounded synthetic renderer data.
static FT_FaceRec face; // Native face identity passed through the production bridge.


/* Keep all predicates active in optimized builds. */
static void
check(int truth)
{
	if(!truth){
		fprintf(stderr, "Freetype native mismatch at check %d\n", checks);
		exit(10);
	}
	checks++;
}


/* Capture real FreeType argument layouts, without replacing bridge conversion. */
void
FT_Set_Transform(FT_Face f, FT_Matrix *m, FT_Vector *v)
{
	check(f == &face);
	saw_matrix = m != nil;
	saw_vector = v != nil;
	if(m != nil)
		observed_matrix = *m;
	if(v != nil)
		observed_vector = *v;
}


/* The test controls only character lookup and native rasterizer output. */
FT_UInt
FT_Get_Char_Index(FT_Face f, FT_ULong c)
{
	check(f == &face && c == 65);
	return 7;
}


/* Return the bounded slot after checking production load flags. */
FT_Error
FT_Load_Glyph(FT_Face f, FT_UInt c, FT_Int32 flags)
{
	check(f == &face && c == 7 && (flags&FT_LOAD_RENDER));
	return 0;
}


/* Supply only the native error lookup boundary required by the extracted loader. */
static char*
fterrstr(int code)
{
	return code == 0 ? nil : "renderer failure";
}

NATIVE_ENTRIES


/* Check actual target widths, null semantics, signed coefficients and publication refusal. */
int
main(void)
{
	FTface f;
	FTmatrix m[2]; // Adjacent bounded record keeps the restored-cast control memory-safe.
	FTvector v[2]; // Two records cover even the old native-long cast extent.
	FTglyph glyph, poison;
	uchar bytes[4] = {1,2,3,4};
	char *err;
	int i;
	int high, pattern, expected_short;
	vlong expected_long;
	uchar input[4], reverse[4], *cursor;

	check(FT_SIZEOF_INT == sizeof(int) && FT_SIZEOF_LONG == sizeof(long));
	for(high=0; high<256; high++)
	for(pattern=0; pattern<4; pattern++){
		input[0] = high;
		input[1] = pattern == 0 ? 0 : pattern == 1 ? 255 : pattern == 2 ? 85 : 170;
		input[2] = input[1]; input[3] = input[1];
		for(i=0; i<4; i++)
			reverse[i] = input[3-i];
		expected_short = (high < 128 ? high : high-256)*256+input[1];
		expected_long = (vlong)(high < 128 ? high : high-256)*16777216+
			(vlong)input[1]*65536+(vlong)input[2]*256+input[3];
		check(FT_PEEK_SHORT(input) == expected_short);
		check(FT_PEEK_LONG(input) == expected_long && FT_PEEK_LONG_LE(reverse) == expected_long);
		cursor = input;
		check(FT_NEXT_LONG(cursor) == expected_long && cursor == input+4);
	}
	f.ft_face = &face;
	f.ft_lib = nil;
	memset(m, 0, sizeof(m));
	memset(v, 0, sizeof(v));
	m[0].a = 65536; m[0].b = -32768; m[0].c = 12345; m[0].d = -65536;
	v[0].dx = -129; v[0].dy = 321;
	for(i=0; i<4; i++){
		ftsettransform(f, i&1 ? m : nil, i&2 ? v : nil);
		check(saw_matrix == (i&1) && saw_vector == ((i>>1)&1));
		if(i&1)
			check(observed_matrix.xx == m[0].a && observed_matrix.xy == m[0].b && observed_matrix.yx == m[0].c && observed_matrix.yy == m[0].d);
		if(i&2)
			check(observed_vector.x == v[0].dx && observed_vector.y == v[0].dy);
	}
	face.glyph = &slot;
	slot.bitmap.rows = 2; slot.bitmap.width = 2; slot.bitmap.pitch = 2;
	slot.bitmap.buffer = bytes; slot.bitmap.pixel_mode = FT_PIXEL_MODE_GRAY;
	slot.advance.x = -129; slot.advance.y = 321;
	check(ftloadglyph(f, 65, &glyph) == nil);
	check(glyph.bitmap == bytes && glyph.width == 2 && glyph.height == 2 && glyph.advx == -129 && glyph.advy == 321);
	memset(&poison, 0x5a, sizeof(poison));
	for(i=0; i<5; i++){
		slot.bitmap.rows = 2; slot.bitmap.width = 2; slot.bitmap.pixel_mode = FT_PIXEL_MODE_GRAY;
		slot.advance.x = -129; slot.advance.y = 321;
		switch(i){
		case 0: slot.bitmap.rows = -1; break;
		case 1: slot.bitmap.width = -1; break;
		case 2: slot.bitmap.pixel_mode = FT_PIXEL_MODE_MONO; break;
		case 3: if(sizeof(long) == 4) continue; slot.advance.x = (FT_Pos)((uvlong)1<<32); break;
		case 4: if(sizeof(long) == 4) continue; slot.advance.y = -(FT_Pos)((uvlong)1<<32); break;
		}
		glyph = poison;
		err = ftloadglyph(f, 65, &glyph);
		check(err != nil && memcmp(&glyph, &poison, sizeof(glyph)) == 0);
	}
	printf("PASS Freetype native bridge: int%u/long%u; target limits, transforms and glyph refusal\n", (unsigned)(8*sizeof(int)), (unsigned)(8*sizeof(long)));
	return 0;
}
