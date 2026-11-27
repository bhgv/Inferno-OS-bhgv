

#include "freetype/freetype.h"
#include "freetype.h"

static char* fterrstr(int);

#ifdef EXT_WIN
extern char rootdir[];
#endif

/* Open a native library/face pair and publish handles only after success. */
char*
ftnewface(char *path, int index, FTface *f, FTfaceinfo *finfo)
{
	FT_Library ft_lib;
	FT_Face ft_face;
	char *err;
#ifdef EXT_WIN
	char *path2;
#endif

	err = fterrstr(FT_Init_FreeType(&ft_lib));
	if (err != nil)
		return err;

#ifdef EXT_WIN
	path2 = malloc(strlen(rootdir) + 1 + strlen(path) + 1);
	sprintf(path2, "%s%s", rootdir, path);
	path = path2;
#endif
	
	err = fterrstr(FT_New_Face(ft_lib, path, index, &ft_face));

#ifdef EXT_WIN
	free(path2);
#endif

	if (err != nil) {
		FT_Done_FreeType(ft_lib);
		return err;
	}

	f->ft_lib = ft_lib;
	f->ft_face = ft_face;
	finfo->nfaces = ft_face->num_faces;
	finfo->index = ft_face->face_index;
	finfo->style = ft_face->style_flags;
	finfo->height = (FT_MulFix(ft_face->height, ft_face->size->metrics.y_scale)+32)/64;
	finfo->ascent = (FT_MulFix(ft_face->ascender, ft_face->size->metrics.y_scale)+32)/64;
	finfo->familyname = ft_face->family_name;
	finfo->stylename = ft_face->style_name;

	finfo->num_glyphs = ft_face->num_glyphs;
	
	return nil;
}


/* Keep memory-face loading explicitly unsupported. */
char*
ftloadmemface(void *buf, int nbytes, int index, FTface *f, FTfaceinfo *finfo)
{
	USED(buf);
	USED(f);
	USED(finfo);
	return "not implemented";
}


/* Set native character metrics through the fixed integer bridge. */
char*
ftsetcharsize(FTface f, int pt, int hdpi, int vdpi, FTfaceinfo *finfo)
{
	FT_Face ft_face = f.ft_face;
	char *err;

	err = fterrstr(FT_Set_Char_Size(ft_face, 0, pt, hdpi, vdpi));
	if (err != nil)
		return err;
	finfo->height = (FT_MulFix(ft_face->height, ft_face->size->metrics.y_scale)+32)/64;
	finfo->ascent = (FT_MulFix(ft_face->ascender, ft_face->size->metrics.y_scale)+32)/64;
	return nil;
}


/* Translate fixed-width bridge fields into native FreeType long fields. */
void
ftsettransform(FTface f, FTmatrix *m, FTvector *v)
{
	FT_Matrix matrix, *native_matrix = nil;
	FT_Vector vector, *native_vector = nil;

	if(m != nil){
		matrix.xx = m->a;
		matrix.xy = m->b;
		matrix.yx = m->c;
		matrix.yy = m->d;
		native_matrix = &matrix;
	}
	if(v != nil){
		vector.x = v->dx;
		vector.y = v->dy;
		native_vector = &vector;
	}
	FT_Set_Transform(f.ft_face, native_matrix, native_vector);
}


/* Query the native character map. */
int
fthaschar(FTface f, int c)
{
	return FT_Get_Char_Index(f.ft_face, c) != 0;
}


/* Publish only a byte-per-pixel glyph whose advances fit the public int ABI. */
char*
ftloadglyph(FTface f, int ix, FTglyph *g)
{
	FT_Face ft_face = f.ft_face;
	FT_GlyphSlot ft_glyph;
	char *err;

	ix = FT_Get_Char_Index(ft_face, ix);
	err = fterrstr(FT_Load_Glyph(ft_face, ix, FT_LOAD_NO_BITMAP|FT_LOAD_RENDER|FT_LOAD_CROP_BITMAP));
	if (err != nil)
		return err;

	
	ft_glyph = ft_face->glyph;
	if(ft_glyph->advance.x < -0x7fffffffL-1 || ft_glyph->advance.x > 0x7fffffffL ||
	   ft_glyph->advance.y < -0x7fffffffL-1 || ft_glyph->advance.y > 0x7fffffffL)
		return "Freetype glyph advance out of range";
	if(ft_glyph->bitmap.width < 0 || ft_glyph->bitmap.rows < 0)
		return "invalid Freetype glyph dimensions";
	if(ft_glyph->bitmap.width != 0 && ft_glyph->bitmap.rows != 0 &&
	   ft_glyph->bitmap.pixel_mode != FT_PIXEL_MODE_GRAY)
		return "unsupported Freetype glyph pixel mode";
	g->top = ft_glyph->bitmap_top;
	g->left = ft_glyph->bitmap_left;
	g->height = ft_glyph->bitmap.rows;
	g->width = ft_glyph->bitmap.width;
	g->advx = ft_glyph->advance.x;
	g->advy = ft_glyph->advance.y;
	g->bpr = ft_glyph->bitmap.pitch;
	g->bitmap = ft_glyph->bitmap.buffer;
	return nil;
}


/* Retire the native face before its library. */
void
ftdoneface(FTface f)
{
	if (f.ft_face != nil)
		FT_Done_Face(f.ft_face);
	if (f.ft_lib != nil)
		FT_Done_FreeType(f.ft_lib);
}


/*
 * get the freetype error strings
 */

typedef struct FTerr FTerr;
struct FTerr {
	int		code; // Native error number.
	char*	text; // Static diagnostic text.
};

#define FT_NOERRORDEF_(l,c,t) // Omit the success sentinel.
#define FT_ERRORDEF_(l,c,t)	c,t, // Expand one static diagnostic pair.

static FTerr fterrs[] = {
#include "freetype/fterrdef.h"
	-1, "",
};

/* Translate a native FreeType error without allocating. */
static char*
fterrstr(int code)
{
	int i;
	if (code == 0)
		return nil;
	for (i = 0; fterrs[i].code > 0; i++) {
		if (fterrs[i].code == code)
			return fterrs[i].text;
	}
	return "unknown FreeType error";
}

