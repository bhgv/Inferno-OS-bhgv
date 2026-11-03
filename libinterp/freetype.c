#include <lib9.h>
#include <kernel.h>
#include "interp.h"
#include "isa.h"
#include "runt.h"
#include "raise.h"
#include "nativeptrmap.h"
#include "freetypemod.h"
#include "freetype.h"


typedef struct Face Face;
struct Face {
	Freetype_Face	freetypeface; // Public Limbo fields and managed names.
	FTface		ftface; // Private native library and face handles.
};

Type*	TMatrix;
Type*	TVector;
Type*	TFace;
Type*	TGlyph;

static uchar	Matrixmap[] = Freetype_Matrix_map;
static uchar	Vectormap[] = Freetype_Vector_map;
static uchar	Facemap[] = Freetype_Face_map;
static uchar	Glyphmap[] = Freetype_Glyph_map;

static void		freeface(Heap*, int);
static Face*	ckface(Freetype_Face*);


/* Build pointer maps from native public fields, excluding private FT handles.
 * Caller holds VM initialization exclusion. Stage every descriptor and retain
 * registration diagnostics until all creator owners can be published together. */
void
freetypemodinit(void)
{
	Type *types[4] = {nil};
	uchar matrixbits[sizeof(Matrixmap)], vectorbits[sizeof(Vectormap)];
	uchar facebits[sizeof(Facemap)], glyphbits[sizeof(Glyphmap)];
	char reason[ERRMAX], *failure;
	int i;

	if(TMatrix != nil || TVector != nil || TFace != nil || TGlyph != nil){
		if(TMatrix != nil && TVector != nil && TFace != nil && TGlyph != nil)
			return;
		error("partial Freetype type initialization");
		return;
	}
	memset(matrixbits, 0, sizeof(matrixbits));
	memset(vectorbits, 0, sizeof(vectorbits));
	memset(facebits, 0, sizeof(facebits));
	nativefieldmap(facebits, Face, freetypeface.familyname);
	nativefieldmap(facebits, Face, freetypeface.stylename);
	memset(glyphbits, 0, sizeof(glyphbits));
	nativefieldmap(glyphbits, Freetype_Glyph, bitmap);

	failure = exNomem;
	types[0] = dtype(freeheap, sizeof(Freetype_Matrix), matrixbits, sizeof(matrixbits));
	if(types[0] == nil)
		goto failed;
	types[1] = dtype(freeheap, sizeof(Freetype_Vector), vectorbits, sizeof(vectorbits));
	if(types[1] == nil)
		goto failed;
	types[2] = dtype(freeface, sizeof(Face), facebits, sizeof(facebits));
	if(types[2] == nil)
		goto failed;
	types[3] = dtype(freeheap, sizeof(Freetype_Glyph), glyphbits, sizeof(glyphbits));
	if(types[3] == nil)
		goto failed;
	if(builtinmod("$Freetype", Freetypemodtab, Freetypemodlen) == nil){
		reason[0] = 0;
		kgerrstr(reason, sizeof(reason));
		reason[sizeof(reason)-1] = 0;
		failure = reason[0] != 0 ? reason : "Freetype module registration failed";
		goto failed;
	}
	memmove(Matrixmap, matrixbits, sizeof(Matrixmap));
	memmove(Vectormap, vectorbits, sizeof(Vectormap));
	memmove(Facemap, facebits, sizeof(Facemap));
	memmove(Glyphmap, glyphbits, sizeof(Glyphmap));
	TMatrix = types[0];
	TVector = types[1];
	TFace = types[2];
	TGlyph = types[3];
	return;
failed:
	for(i=0; i<4; i++)
		freetype(types[i]);
	error(failure);
}


/* Refuse narrowing from an experimental WORD64 value into the native FT int ABI. */
static int
ftargument(WORD value)
{
	if(value < -0x7fffffff-1 || value > 0x7fffffff)
		error(exRange);
	return (int)value;
}


/* Query native character coverage while releasing VM exclusion for library work. */
void
Face_haschar(void *fp)
{
	F_Face_haschar *f = fp;
	Face *face;
	int c, present;

	c = ftargument(f->c);
	*f->ret = 0;
	face = ckface(f->face);
	release();
	present = fthaschar(face->ftface, c);
	acquire();
	*f->ret = present;
}


/* Validate bitmap extents before allocation or signed stride arithmetic. */
void
Face_loadglyph(void *fp)
{
	F_Face_loadglyph *f = fp;
	Heap *h;
	Face *face;
	Freetype_Glyph *g;
	FTglyph ftg;
	int c, n, i;
	vlong pitch, span;
	uchar *source;
	char *err;

	c = ftargument(f->c);
	face = ckface(f->face);

	destroy(*f->ret);
	*f->ret = H;

	release();
	err = ftloadglyph(face->ftface, c, &ftg);
	acquire();
	if (err != nil) {
		kwerrstr(err);
		return;
	}

	pitch = ftg.bpr;
	if(pitch < 0)
		pitch = -pitch;
	if(ftg.width < 0 || ftg.height < 0 ||
	   (ftg.height != 0 && ftg.width > 0x7fffffff/ftg.height)){
		kwerrstr("invalid Freetype glyph dimensions");
		return;
	}
	n = ftg.width*ftg.height;
	if(n != 0){
		span = (vlong)(ftg.height-1)*pitch+ftg.width;
		if(ftg.bitmap == nil || pitch < ftg.width || span > 0x7fffffff){
			kwerrstr("invalid Freetype glyph bitmap");
			return;
		}
	}

	h = heap(TGlyph);
	if (h == H) {
		kwerrstr(exNomem);
		return;
	}
	g = H2D(Freetype_Glyph*, h);
	h = heaparray(&Tbyte, n);
	if (h == H) {
		destroy(g);
		kwerrstr(exNomem);
		return;
	}
	g->bitmap = H2D(Array*, h);
	g->top = ftg.top;
	g->left = ftg.left;
	g->height = ftg.height;
	g->width = ftg.width;
	g->advance.x = ftg.advx;
	g->advance.y = ftg.advy;

	source = ftg.bitmap;
	if(n != 0){
		for(i=0; i<ftg.height; i++){
			memcpy(g->bitmap->data+i*ftg.width, source, ftg.width);
			if(i+1 < ftg.height)
				source += ftg.bpr;
		}
	}
	*f->ret = g;
}


/* Publish the managed face owner before allocation can raise a VM exception. */
void
Freetype_newface(void *fp)
{
	F_Freetype_newface *f = fp;
	Heap *h;
	Face *face;
	Freetype_Face *limboface;
	FTfaceinfo finfo;
	char *path, *name;
	char *err;
	int index;

	index = ftargument(f->index);
	destroy(*f->ret);
	*f->ret = H;

	h = heapz(TFace);
	if (h == H) {
		kwerrstr(exNomem);
		return;
	}

	face = H2D(Face*, h);
	limboface = (Freetype_Face*)face;
	*f->ret = limboface;
	path = strdup(string2c(f->path)); /* string2c() can call error(). */
	if(path == nil){
		*f->ret = H;
		destroy(face);
		kwerrstr(exNomem);
		return;
	}
	release();
	err = ftnewface(path, index, &face->ftface, &finfo);
	acquire();
	free(path);
	if (err != nil) {
		*f->ret = H;
		destroy(face);
		kwerrstr(err);
		return;
	}
	limboface->nfaces = finfo.nfaces;
	limboface->index = finfo.index;
	limboface->style = finfo.style;
	limboface->height = finfo.height;
	limboface->ascent = finfo.ascent;
	name = finfo.familyname != nil ? finfo.familyname : "";
	limboface->familyname = c2string(name, strlen(name));
	name = finfo.stylename != nil ? finfo.stylename : "";
	limboface->stylename = c2string(name, strlen(name));
	*f->ret = limboface;
}


/* Keep the unimplemented memory-face entry explicit and clear its result owner. */
void
Freetype_newmemface(void *fp)
{
	F_Freetype_newmemface *f = fp;

	destroy(*f->ret);
	*f->ret = H;

	kwerrstr("not implemented");
}


/* Convert VM scalar arguments before releasing exclusion for native library work. */
void
Face_setcharsize(void *fp)
{
	F_Face_setcharsize *f = fp;
	Face *face;
	Freetype_Face *limboface;
	FTfaceinfo finfo;
	char *err;
	int pts, hdpi, vdpi;

	pts = ftargument(f->pts);
	hdpi = ftargument(f->hdpi);
	vdpi = ftargument(f->vdpi);
	face = ckface(f->face);
	limboface = (Freetype_Face*)face;
	release();
	err = ftsetcharsize(face->ftface, pts, hdpi, vdpi, &finfo);
	acquire();
	if (err == nil) {
		limboface->height = finfo.height;
		limboface->ascent = finfo.ascent;
	}
	retstr(err, f->ret);
}


/* Copy VM-word records into the fixed native-int ABI; never alias their layouts. */
void
Face_settransform(void *fp)
{
	F_Face_settransform *f = fp;
	FTmatrix *m = nil;
	FTvector *v = nil;
	FTmatrix matrix;
	FTvector vector;
	Face *face;

	face = ckface(f->face);

	if(f->m != H){
		matrix.a = ftargument(f->m->a);
		matrix.b = ftargument(f->m->b);
		matrix.c = ftargument(f->m->c);
		matrix.d = ftargument(f->m->d);
		m = &matrix;
	}
	if(f->v != H){
		vector.dx = ftargument(f->v->dx);
		vector.dy = ftargument(f->v->dy);
		v = &vector;
	}
	destroy(*f->ret);
	*f->ret = H;

	release();
	ftsettransform(face->ftface, m, v);
	acquire();
}


/* Release managed names only outside a sweep, then close private native handles. */
static void
freeface(Heap *h, int swept)
{
	Face *face = H2D(Face*, h);

	if (!swept) {
		destroy(face->freetypeface.familyname);
		destroy(face->freetypeface.stylename);
	}
	release();
	ftdoneface(face->ftface);
	acquire();
	memset(&face->ftface, 0, sizeof(face->ftface));
}


/* Accept only a live Face payload constructed by this module. */
static Face*
ckface(Freetype_Face *face)
{
	if (face == nil || face == H)
		error("nil Face");
	if (D2H(face)->t != TFace)
		error(exType);
	return (Face*)face;
}

