#include <lib9.h>
#include <kernel.h>
#include "pool.h"
#include "interp.h"
#include "isa.h"
/* Use one explicit int32/ref64 interface for every owning Draw consumer. */
#ifdef DIS_NATIVE64_DRAW_HEADER
#if !defined(DIS_WORD32) || DIS_WORD32 != 1
#error native64 Draw declarations require DIS_WORD32=1
#endif
#define Rect_Xrect Draw_Rect_Xrect // Preserve the generated qualified callable identity.
#define F_Rect_Xrect F_Draw_Rect_Xrect // Use the corresponding generated Frame64 declaration.
#define Point_add Draw_Point_add // Preserve the generated qualified callable identity.
#define F_Point_add F_Draw_Point_add // Use the corresponding generated Frame64 declaration.
#define Rect_addpt Draw_Rect_addpt // Preserve the generated qualified callable identity.
#define F_Rect_addpt F_Draw_Rect_addpt // Use the corresponding generated Frame64 declaration.
#define Display_allocate Draw_Display_allocate // Preserve the generated qualified callable identity.
#define F_Display_allocate F_Draw_Display_allocate // Use the corresponding generated Frame64 declaration.
#define Screen_allocate Draw_Screen_allocate // Preserve the generated qualified callable identity.
#define F_Screen_allocate F_Draw_Screen_allocate // Use the corresponding generated Frame64 declaration.
#define Image_arc Draw_Image_arc // Preserve the generated qualified callable identity.
#define F_Image_arc F_Draw_Image_arc // Use the corresponding generated Frame64 declaration.
#define Image_arcop Draw_Image_arcop // Preserve the generated qualified callable identity.
#define F_Image_arcop F_Draw_Image_arcop // Use the corresponding generated Frame64 declaration.
#define Image_arrow Draw_Image_arrow // Preserve the generated qualified callable identity.
#define F_Image_arrow F_Draw_Image_arrow // Use the corresponding generated Frame64 declaration.
#define Font_bbox Draw_Font_bbox // Preserve the generated qualified callable identity.
#define F_Font_bbox F_Draw_Font_bbox // Use the corresponding generated Frame64 declaration.
#define Image_bezier Draw_Image_bezier // Preserve the generated qualified callable identity.
#define F_Image_bezier F_Draw_Image_bezier // Use the corresponding generated Frame64 declaration.
#define Image_bezierop Draw_Image_bezierop // Preserve the generated qualified callable identity.
#define F_Image_bezierop F_Draw_Image_bezierop // Use the corresponding generated Frame64 declaration.
#define Image_bezspline Draw_Image_bezspline // Preserve the generated qualified callable identity.
#define F_Image_bezspline F_Draw_Image_bezspline // Use the corresponding generated Frame64 declaration.
#define Image_bezsplineop Draw_Image_bezsplineop // Preserve the generated qualified callable identity.
#define F_Image_bezsplineop F_Draw_Image_bezsplineop // Use the corresponding generated Frame64 declaration.
#define Image_border Draw_Image_border // Preserve the generated qualified callable identity.
#define F_Image_border F_Draw_Image_border // Use the corresponding generated Frame64 declaration.
#define Image_bottom Draw_Image_bottom // Preserve the generated qualified callable identity.
#define F_Image_bottom F_Draw_Image_bottom // Use the corresponding generated Frame64 declaration.
#define Screen_bottom Draw_Screen_bottom // Preserve the generated qualified callable identity.
#define F_Screen_bottom F_Draw_Screen_bottom // Use the corresponding generated Frame64 declaration.
#define Font_build Draw_Font_build // Preserve the generated qualified callable identity.
#define F_Font_build F_Draw_Font_build // Use the corresponding generated Frame64 declaration.
#define Rect_canon Draw_Rect_canon // Preserve the generated qualified callable identity.
#define F_Rect_canon F_Draw_Rect_canon // Use the corresponding generated Frame64 declaration.
#define Rect_clip Draw_Rect_clip // Preserve the generated qualified callable identity.
#define F_Rect_clip F_Draw_Rect_clip // Use the corresponding generated Frame64 declaration.
#define Display_cmap2rgb Draw_Display_cmap2rgb // Preserve the generated qualified callable identity.
#define F_Display_cmap2rgb F_Draw_Display_cmap2rgb // Use the corresponding generated Frame64 declaration.
#define Display_cmap2rgba Draw_Display_cmap2rgba // Preserve the generated qualified callable identity.
#define F_Display_cmap2rgba F_Draw_Display_cmap2rgba // Use the corresponding generated Frame64 declaration.
#define Display_color Draw_Display_color // Preserve the generated qualified callable identity.
#define F_Display_color F_Draw_Display_color // Use the corresponding generated Frame64 declaration.
#define Display_colormix Draw_Display_colormix // Preserve the generated qualified callable identity.
#define F_Display_colormix F_Draw_Display_colormix // Use the corresponding generated Frame64 declaration.
#define Rect_combine Draw_Rect_combine // Preserve the generated qualified callable identity.
#define F_Rect_combine F_Draw_Rect_combine // Use the corresponding generated Frame64 declaration.
#define Rect_contains Draw_Rect_contains // Preserve the generated qualified callable identity.
#define F_Rect_contains F_Draw_Rect_contains // Use the corresponding generated Frame64 declaration.
#define Chans_depth Draw_Chans_depth // Preserve the generated qualified callable identity.
#define F_Chans_depth F_Draw_Chans_depth // Use the corresponding generated Frame64 declaration.
#define Point_div Draw_Point_div // Preserve the generated qualified callable identity.
#define F_Point_div F_Draw_Point_div // Use the corresponding generated Frame64 declaration.
#define Image_draw Draw_Image_draw // Preserve the generated qualified callable identity.
#define F_Image_draw F_Draw_Image_draw // Use the corresponding generated Frame64 declaration.
#define Image_drawop Draw_Image_drawop // Preserve the generated qualified callable identity.
#define F_Image_drawop F_Draw_Image_drawop // Use the corresponding generated Frame64 declaration.
#define Rect_dx Draw_Rect_dx // Preserve the generated qualified callable identity.
#define F_Rect_dx F_Draw_Rect_dx // Use the corresponding generated Frame64 declaration.
#define Rect_dy Draw_Rect_dy // Preserve the generated qualified callable identity.
#define F_Rect_dy F_Draw_Rect_dy // Use the corresponding generated Frame64 declaration.
#define Image_ellipse Draw_Image_ellipse // Preserve the generated qualified callable identity.
#define F_Image_ellipse F_Draw_Image_ellipse // Use the corresponding generated Frame64 declaration.
#define Image_ellipseop Draw_Image_ellipseop // Preserve the generated qualified callable identity.
#define F_Image_ellipseop F_Draw_Image_ellipseop // Use the corresponding generated Frame64 declaration.
#define Chans_eq Draw_Chans_eq // Preserve the generated qualified callable identity.
#define F_Chans_eq F_Draw_Chans_eq // Use the corresponding generated Frame64 declaration.
#define Point_eq Draw_Point_eq // Preserve the generated qualified callable identity.
#define F_Point_eq F_Draw_Point_eq // Use the corresponding generated Frame64 declaration.
#define Rect_eq Draw_Rect_eq // Preserve the generated qualified callable identity.
#define F_Rect_eq F_Draw_Rect_eq // Use the corresponding generated Frame64 declaration.
#define Image_fillarc Draw_Image_fillarc // Preserve the generated qualified callable identity.
#define F_Image_fillarc F_Draw_Image_fillarc // Use the corresponding generated Frame64 declaration.
#define Image_fillarcop Draw_Image_fillarcop // Preserve the generated qualified callable identity.
#define F_Image_fillarcop F_Draw_Image_fillarcop // Use the corresponding generated Frame64 declaration.
#define Image_fillbezier Draw_Image_fillbezier // Preserve the generated qualified callable identity.
#define F_Image_fillbezier F_Draw_Image_fillbezier // Use the corresponding generated Frame64 declaration.
#define Image_fillbezierop Draw_Image_fillbezierop // Preserve the generated qualified callable identity.
#define F_Image_fillbezierop F_Draw_Image_fillbezierop // Use the corresponding generated Frame64 declaration.
#define Image_fillbezspline Draw_Image_fillbezspline // Preserve the generated qualified callable identity.
#define F_Image_fillbezspline F_Draw_Image_fillbezspline // Use the corresponding generated Frame64 declaration.
#define Image_fillbezsplineop Draw_Image_fillbezsplineop // Preserve the generated qualified callable identity.
#define F_Image_fillbezsplineop F_Draw_Image_fillbezsplineop // Use the corresponding generated Frame64 declaration.
#define Image_fillellipse Draw_Image_fillellipse // Preserve the generated qualified callable identity.
#define F_Image_fillellipse F_Draw_Image_fillellipse // Use the corresponding generated Frame64 declaration.
#define Image_fillellipseop Draw_Image_fillellipseop // Preserve the generated qualified callable identity.
#define F_Image_fillellipseop F_Draw_Image_fillellipseop // Use the corresponding generated Frame64 declaration.
#define Image_fillpoly Draw_Image_fillpoly // Preserve the generated qualified callable identity.
#define F_Image_fillpoly F_Draw_Image_fillpoly // Use the corresponding generated Frame64 declaration.
#define Image_fillpolyop Draw_Image_fillpolyop // Preserve the generated qualified callable identity.
#define F_Image_fillpolyop F_Draw_Image_fillpolyop // Use the corresponding generated Frame64 declaration.
#define Image_flush Draw_Image_flush // Preserve the generated qualified callable identity.
#define F_Image_flush F_Draw_Image_flush // Use the corresponding generated Frame64 declaration.
#define Image_gendraw Draw_Image_gendraw // Preserve the generated qualified callable identity.
#define F_Image_gendraw F_Draw_Image_gendraw // Use the corresponding generated Frame64 declaration.
#define Image_gendrawop Draw_Image_gendrawop // Preserve the generated qualified callable identity.
#define F_Image_gendrawop F_Draw_Image_gendrawop // Use the corresponding generated Frame64 declaration.
#define Display_getwindow Draw_Display_getwindow // Preserve the generated qualified callable identity.
#define F_Display_getwindow F_Draw_Display_getwindow // Use the corresponding generated Frame64 declaration.
#define Point_in Draw_Point_in // Preserve the generated qualified callable identity.
#define F_Point_in F_Draw_Point_in // Use the corresponding generated Frame64 declaration.
#define Rect_inrect Draw_Rect_inrect // Preserve the generated qualified callable identity.
#define F_Rect_inrect F_Draw_Rect_inrect // Use the corresponding generated Frame64 declaration.
#define Rect_inset Draw_Rect_inset // Preserve the generated qualified callable identity.
#define F_Rect_inset F_Draw_Rect_inset // Use the corresponding generated Frame64 declaration.
#define Image_line Draw_Image_line // Preserve the generated qualified callable identity.
#define F_Image_line F_Draw_Image_line // Use the corresponding generated Frame64 declaration.
#define Image_lineop Draw_Image_lineop // Preserve the generated qualified callable identity.
#define F_Image_lineop F_Draw_Image_lineop // Use the corresponding generated Frame64 declaration.
#define Chans_mk Draw_Chans_mk // Preserve the generated qualified callable identity.
#define F_Chans_mk F_Draw_Chans_mk // Use the corresponding generated Frame64 declaration.
#define Point_mul Draw_Point_mul // Preserve the generated qualified callable identity.
#define F_Point_mul F_Draw_Point_mul // Use the corresponding generated Frame64 declaration.
#define Image_name Draw_Image_name // Preserve the generated qualified callable identity.
#define F_Image_name F_Draw_Image_name // Use the corresponding generated Frame64 declaration.
#define Display_namedimage Draw_Display_namedimage // Preserve the generated qualified callable identity.
#define F_Display_namedimage F_Draw_Display_namedimage // Use the corresponding generated Frame64 declaration.
#define Display_newimage Draw_Display_newimage // Preserve the generated qualified callable identity.
#define F_Display_newimage F_Draw_Display_newimage // Use the corresponding generated Frame64 declaration.
#define Screen_newwindow Draw_Screen_newwindow // Preserve the generated qualified callable identity.
#define F_Screen_newwindow F_Draw_Screen_newwindow // Use the corresponding generated Frame64 declaration.
#define Display_open Draw_Display_open // Preserve the generated qualified callable identity.
#define F_Display_open F_Draw_Display_open // Use the corresponding generated Frame64 declaration.
#define Font_open Draw_Font_open // Preserve the generated qualified callable identity.
#define F_Font_open F_Draw_Font_open // Use the corresponding generated Frame64 declaration.
#define Image_origin Draw_Image_origin // Preserve the generated qualified callable identity.
#define F_Image_origin F_Draw_Image_origin // Use the corresponding generated Frame64 declaration.
#define Image_poly Draw_Image_poly // Preserve the generated qualified callable identity.
#define F_Image_poly F_Draw_Image_poly // Use the corresponding generated Frame64 declaration.
#define Image_polyop Draw_Image_polyop // Preserve the generated qualified callable identity.
#define F_Image_polyop F_Draw_Image_polyop // Use the corresponding generated Frame64 declaration.
#define Display_publicscreen Draw_Display_publicscreen // Preserve the generated qualified callable identity.
#define F_Display_publicscreen F_Draw_Display_publicscreen // Use the corresponding generated Frame64 declaration.
#define Display_readimage Draw_Display_readimage // Preserve the generated qualified callable identity.
#define F_Display_readimage F_Draw_Display_readimage // Use the corresponding generated Frame64 declaration.
#define Image_readpixels Draw_Image_readpixels // Preserve the generated qualified callable identity.
#define F_Image_readpixels F_Draw_Image_readpixels // Use the corresponding generated Frame64 declaration.
#define Display_rgb Draw_Display_rgb // Preserve the generated qualified callable identity.
#define F_Display_rgb F_Draw_Display_rgb // Use the corresponding generated Frame64 declaration.
#define Display_rgb2cmap Draw_Display_rgb2cmap // Preserve the generated qualified callable identity.
#define F_Display_rgb2cmap F_Draw_Display_rgb2cmap // Use the corresponding generated Frame64 declaration.
#define Rect_size Draw_Rect_size // Preserve the generated qualified callable identity.
#define F_Rect_size F_Draw_Rect_size // Use the corresponding generated Frame64 declaration.
#define Display_startrefresh Draw_Display_startrefresh // Preserve the generated qualified callable identity.
#define F_Display_startrefresh F_Draw_Display_startrefresh // Use the corresponding generated Frame64 declaration.
#define Point_sub Draw_Point_sub // Preserve the generated qualified callable identity.
#define F_Point_sub F_Draw_Point_sub // Use the corresponding generated Frame64 declaration.
#define Rect_subpt Draw_Rect_subpt // Preserve the generated qualified callable identity.
#define F_Rect_subpt F_Draw_Rect_subpt // Use the corresponding generated Frame64 declaration.
#define Chans_text Draw_Chans_text // Preserve the generated qualified callable identity.
#define F_Chans_text F_Draw_Chans_text // Use the corresponding generated Frame64 declaration.
#define Image_text Draw_Image_text // Preserve the generated qualified callable identity.
#define F_Image_text F_Draw_Image_text // Use the corresponding generated Frame64 declaration.
#define Image_textbg Draw_Image_textbg // Preserve the generated qualified callable identity.
#define F_Image_textbg F_Draw_Image_textbg // Use the corresponding generated Frame64 declaration.
#define Image_textbgop Draw_Image_textbgop // Preserve the generated qualified callable identity.
#define F_Image_textbgop F_Draw_Image_textbgop // Use the corresponding generated Frame64 declaration.
#define Image_textop Draw_Image_textop // Preserve the generated qualified callable identity.
#define F_Image_textop F_Draw_Image_textop // Use the corresponding generated Frame64 declaration.
#define Image_top Draw_Image_top // Preserve the generated qualified callable identity.
#define F_Image_top F_Draw_Image_top // Use the corresponding generated Frame64 declaration.
#define Screen_top Draw_Screen_top // Preserve the generated qualified callable identity.
#define F_Screen_top F_Draw_Screen_top // Use the corresponding generated Frame64 declaration.
#define Font_width Draw_Font_width // Preserve the generated qualified callable identity.
#define F_Font_width F_Draw_Font_width // Use the corresponding generated Frame64 declaration.
#define Display_writeimage Draw_Display_writeimage // Preserve the generated qualified callable identity.
#define F_Display_writeimage F_Draw_Display_writeimage // Use the corresponding generated Frame64 declaration.
#define Image_writepixels Draw_Image_writepixels // Preserve the generated qualified callable identity.
#define F_Image_writepixels F_Draw_Image_writepixels // Use the corresponding generated Frame64 declaration.
#include DIS_NATIVE64_DRAW_HEADER
#else
#include "runt.h"
#endif
#include "raise.h"
#include "nativeptrmap.h"
#ifndef DIS_NATIVE64_DRAW_HEADER
#include "drawmod.h"
#endif
#include "draw.h"
#include "drawif.h"
#include "memdraw.h"
#include "memlayer.h"

#include "tk.h"

#include <stdio.h>
#include <string.h>
#include <float.h>
#include <math.h>

#define NANOSVG_IMPLEMENTATION
#define NANOSVG_ALL_COLOR_KEYWORDS	// Include full list of color keywords.
#include "svg/nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#define NSVG_QSORT_ARGUMENT void* // Inferno's infqsort callback uses mutable argument types.
#include "svg/nanosvgrast.h"
#undef NSVG_QSORT_ARGUMENT


#include "png/lodepng.h"


#ifdef ANDROID
#include <android/log.h>
	
#define  LOG_TAG    "Inferno DRAW"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGW(...)  __android_log_print(ANDROID_LOG_WARN,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)
#else
#define  LOGI(...)  print(__VA_ARGS__)
#define  LOGW(...)  print(__VA_ARGS__)
#define  LOGE(...)  print(__VA_ARGS__)
#endif



/*
 * When a Display is remote, it must be locked to synchronize the
 * outgoing message buffer with the refresh demon, which runs as a
 * different process.  When it is local, the refresh demon does nothing
 * and it is sufficient to use the interpreter's own acquire/release protection
 * to lock the buffer.
 *
 * Most action to the buffer is caused by calls from Limbo, so locking at
 * the top before going into the library is good enough.  However, the
 * garbage collector can call the free routines at other times, so they
 * need to protect themselves whether called through the Draw module
 * or not; hence the need for check against recursive locking in lockdisplay().
 * This also means that we needn't lock around calls to destroy if it's
 * extra work to do so.
 */

typedef struct Cache Cache;
typedef struct DRef DRef;
typedef struct DDisplay DDisplay;
typedef struct DImage DImage;
typedef struct DScreen DScreen;
typedef struct DFont DFont;

/*
struct Cache
{
	int	ref;
	char*	name;
	Display*display;
	union{
		Subfont*	sf;
		Font*		f;
		void*		ptr;
	}u;
	Cache*	next;
};
*/

/* not visible to Limbo; used only for internal reference counting */
struct DRef
{
	int ref; // Native display leases, independent of managed Heap references.
	Display *display; // Native display owned until the final native lease retires.
};

struct DDisplay
{
	Draw_Display drawdisplay; // Managed public fields; map excludes private native pointers.
	Display *display; // Native display, detached before managed child destruction.
	DRef *dref; // One independently owned native display lease.
};

struct DImage
{
	Draw_Image drawimage; // Managed public image fields and references.
	Image *image; // Native image consumed only by its owning wrapper's retirement.
	void *refreshptr; // Borrowed refresh context, not a managed reference.
	DRef *dref; // Native display lease held through all managed reference cleanup.
	int flush; // Existing automatic flush policy.
};

struct DScreen
{
	Draw_Screen drawscreen; // Managed image/fill/display owners.
	Screen *screen; // Native screen retired before its borrowed native images disappear.
	DRef *dref; // Native display lease held through mapped owner retirement.
};

struct DFont
{
	Draw_Font drawfont; // Managed font name/display owners.
	Font *font; // Native cached font, or nil for a partial/retired wrapper.
	DRef *dref; // Native display lease independent of managed references.
};

Cache*	sfcache[BIHASH];
Cache*	fcache[BIHASH];
void*	cacheqlock;

static	Cache	*cachelookup(Cache**, Display*, char*);

uchar fontmap[] = Draw_Font_map;
uchar imagemap[] = Draw_Image_map;
uchar screenmap[] = Draw_Screen_map;
uchar displaymap[] = Draw_Display_map;

Type*	TFont;
Type*	TImage;
Type*	TScreen;
Type*	TDisplay;

Draw_Image*	allocdrawimage(DDisplay*, Draw_Rect, ulong, Image*, int, int);
Draw_Image*	color(DDisplay*, ulong);
Draw_Screen	*mkdrawscreen(Screen*, Draw_Display*);

char		deffontname[] = "*default*";
void		refreshslave(Display*);
void		subfont_close(Subfont*);
void		freeallsubfonts(Display*);

extern char rootdir[];


/* Build private native wrapper descriptors; the caller owns every partial descriptor on refusal. */
static int
drawmaketypes(Type **types, uchar *fontbits, uchar *imagebits, uchar *screenbits, uchar *displaybits)
{
	memset(fontbits, 0, sizeof(fontmap));
	nativeptrmap(fontbits, sizeof(fontmap), sizeof(DFont), O(DFont, drawfont.name));
	nativeptrmap(fontbits, sizeof(fontmap), sizeof(DFont), O(DFont, drawfont.display));
	memset(imagebits, 0, sizeof(imagemap));
	nativeptrmap(imagebits, sizeof(imagemap), sizeof(DImage), O(DImage, drawimage.display));
	nativeptrmap(imagebits, sizeof(imagemap), sizeof(DImage), O(DImage, drawimage.screen));
	nativeptrmap(imagebits, sizeof(imagemap), sizeof(DImage), O(DImage, drawimage.iname));
	memset(screenbits, 0, sizeof(screenmap));
	nativeptrmap(screenbits, sizeof(screenmap), sizeof(DScreen), O(DScreen, drawscreen.image));
	nativeptrmap(screenbits, sizeof(screenmap), sizeof(DScreen), O(DScreen, drawscreen.fill));
	nativeptrmap(screenbits, sizeof(screenmap), sizeof(DScreen), O(DScreen, drawscreen.display));
	memset(displaybits, 0, sizeof(displaymap));
	nativeptrmap(displaybits, sizeof(displaymap), sizeof(DDisplay), O(DDisplay, drawdisplay.image));
	nativeptrmap(displaybits, sizeof(displaymap), sizeof(DDisplay), O(DDisplay, drawdisplay.white));
	nativeptrmap(displaybits, sizeof(displaymap), sizeof(DDisplay), O(DDisplay, drawdisplay.black));
	nativeptrmap(displaybits, sizeof(displaymap), sizeof(DDisplay), O(DDisplay, drawdisplay.opaque));
	nativeptrmap(displaybits, sizeof(displaymap), sizeof(DDisplay), O(DDisplay, drawdisplay.transparent));

	types[0] = dtype(freedrawfont, sizeof(DFont), fontbits, sizeof(fontmap));
	if(types[0] == nil)
		return 0;
	types[1] = dtype(freedrawimage, sizeof(DImage), imagebits, sizeof(imagemap));
	if(types[1] == nil)
		return 0;
	types[2] = dtype(freedrawscreen, sizeof(DScreen), screenbits, sizeof(screenmap));
	if(types[2] == nil)
		return 0;
	types[3] = dtype(freedrawdisplay, sizeof(DDisplay), displaybits, sizeof(displaymap));
	if(types[3] == nil)
		return 0;
	return 1;
}


/* Stage every wrapper descriptor before publishing a complete Draw module.
 * Caller holds VM initialization exclusion. dtype returns nil; builtinmod owns
 * its protected failure cleanup. No wrapper type escapes on either refusal. */
void
drawmodinit(void)
{
	Type *types[4] = {nil};
	uchar fontbits[sizeof(fontmap)], imagebits[sizeof(imagemap)];
	uchar screenbits[sizeof(screenmap)], displaybits[sizeof(displaymap)];
	char reason[ERRMAX], *failure;
	int i;

#ifdef DIS_NATIVE64_DRAW_HEADER
	error("experimental native64 Draw registration is not supported");
	return;
#endif
	if(TFont != nil || TImage != nil || TScreen != nil || TDisplay != nil){
		if(TFont != nil && TImage != nil && TScreen != nil && TDisplay != nil)
			return;
		error("partial Draw type initialization");
		return;
	}
	failure = exNomem;
	if(!drawmaketypes(types, fontbits, imagebits, screenbits, displaybits))
		goto failed;
#ifndef DIS_NATIVE64_DRAW_HEADER
	if(builtinmod("$Draw", Drawmodtab, Drawmodlen) == nil){
		reason[0] = 0;
		kgerrstr(reason, sizeof(reason));
		reason[sizeof(reason)-1] = 0;
		failure = reason[0] != 0 ? reason : "Draw module registration failed";
		goto failed;
	}
#endif

	/* No allocation or callback separates module registration from this publication. */
	memmove(fontmap, fontbits, sizeof(fontmap));
	memmove(imagemap, imagebits, sizeof(imagemap));
	memmove(screenmap, screenbits, sizeof(screenmap));
	memmove(displaymap, displaybits, sizeof(displaymap));
	TFont = types[0];
	TImage = types[1];
	TScreen = types[2];
	TDisplay = types[3];
	return;
failed:
	for(i=0; i<4; i++)
		freetype(types[i]);
	error(failure);
}


typedef struct DrawHeapBuild DrawHeapBuild;

struct DrawHeapBuild {
	Type *type; // Borrowed complete descriptor under VM admission.
	Heap *owner; // New unpublished managed allocation, or H on refusal.
};


/* Keep a failed heap allocation inside the private constructor, before any native resource escapes. */
static int
drawheapbuild(void *argument)
{
	DrawHeapBuild *build;

	build = argument;
	build->owner = heap(build->type);
	return build->owner != H;
}


/* Preserve the existing H-return allocation API while retaining the actual VM diagnostic. */
static Heap*
drawheap(Type *type)
{
	DrawHeapBuild build;

	build.type = type;
	build.owner = H;
	if(!kprotect(drawheapbuild, &build))
		return H;
	return build.owner;
}


/* Roll back a newly allocated native image without losing the managed allocation diagnostic. */
static void
drawdiscardimage(Image *image)
{
	char reason[ERRMAX];

	kgerrstr(reason, sizeof(reason));
	freeimage(image);
	kwerrstr("%s", reason);
}


/* Publish an owned result before retirement, preserving the requested operation's diagnostic. */
static void
drawcommitresult(void *slot, void *value, void *old)
{
	char reason[ERRMAX];

	kgerrstr(reason, sizeof(reason));
	memmove(slot, &value, sizeof(value));
	destroy(old);
	kwerrstr("%s", reason);
}


/* Hash bytes with defined unsigned wraparound on every host word size. */
static int
drawhash(char *s)
{
	uint h;

	h = 0;
	while(*s){
		h += (uchar)*s++;
		h <<= 1;
		if(h & (1<<8))
			h |= 1;
	}
	return (h&0xFFFF)%BIHASH;
}

static Cache*
cachelookup(Cache *cache[], Display *d, char *name)
{
	Cache *c;

	libqlock(cacheqlock);
	c = cache[drawhash(name)];
	while(c!=nil && (d!=c->display || strcmp(name, c->name)!=0))
		c = c->next;
	libqunlock(cacheqlock);
	return c;
}

/* Copy all owned state before publishing a cache record. */
Cache*
cacheinstall(Cache **cache, Display *d, char *name, void *ptr, char *type)
{
	Cache *c;
	int hash;

	USED(type);
	c = cachelookup(cache, d, name);
	if(c){
/*		print("%s %s already in cache\n", type, name); /**/
		return nil;
	}
	c = malloc(sizeof(Cache));
	if(c == nil)
		return nil;
	hash = drawhash(name);
	c->ref = 0;	/* will be incremented by caller */
	c->display = d;
	c->name = strdup(name);
	if(c->name == nil){
		free(c);
		return nil;
	}
	c->u.ptr = ptr;
	libqlock(cacheqlock);
	c->next = cache[hash];
	cache[hash] = c;
	libqunlock(cacheqlock);
	return c;
}

void
cacheuninstall(Cache **cache, Display *d, char *name, char *type)
{
	Cache *c, *prev;
	int hash;

	hash = drawhash(name);
	libqlock(cacheqlock);
	c = cache[hash];
	if(c == nil){
   Notfound:
		libqunlock(cacheqlock);
		print("%s not in %s cache\n", name, type);
		return;
	}
	prev = nil;
	while(c!=nil && (d!=c->display || strcmp(name, c->name)!=0)){
		prev = c;
		c = c->next;
	}
	if(c == nil)
		goto Notfound;
	if(prev == 0)
		cache[hash] = c->next;
	else
		prev->next = c->next;
	libqunlock(cacheqlock);
	free(c->name);
	free(c);
}

Image*
lookupimage(Draw_Image *di)
{
	Display *disp;
	Image *i;
	int locked;

	if(di == H || D2H(di)->t != TImage)
		return nil;
	i = ((DImage*)di)->image;
	if(i == nil)
		return nil;
	if(!eqrect(IRECT(di->clipr), i->clipr) || di->repl!=i->repl){
		disp = i->display;
		locked = lockdisplay(disp);
		replclipr(i, di->repl, IRECT(di->clipr));
		if(locked)
			unlockdisplay(disp);
	}
	return i;
}

Screen*
lookupscreen(Draw_Screen *ds)
{
	if(ds == H || D2H(ds)->t != TScreen)
		return nil;
	return ((DScreen*)ds)->screen;
}

Font*
lookupfont(Draw_Font *df)
{
	if(df == H || D2H(df)->t != TFont)
		return nil;
	return ((DFont*)df)->font;
}

Display*
lookupdisplay(Draw_Display *dd)
{
	if(dd == H || D2H(dd)->t != TDisplay)
		return nil;
	return ((DDisplay*)dd)->display;
}

Image*
checkimage(Draw_Image *di)
{
	Image *i;

	if(di == H)
		error("nil Image");
	i = lookupimage(di);
	if(i == nil)
		error(exType);
	return i;
}

Screen*
checkscreen(Draw_Screen *ds)
{
	Screen *s;

	if(ds == H)
		error("nil Screen");
	s = lookupscreen(ds);
	if(s == nil)
		error(exType);
	return s;
}

Font*
checkfont(Draw_Font *df)
{
	Font *f;

	if(df == H)
		error("nil Font");
	f = lookupfont(df);
	if(f == nil)
		error(exType);
	return f;
}

Display*
checkdisplay(Draw_Display *dd)
{
	Display *d;

	if(dd == H)
		error("nil Display");
	d = lookupdisplay(dd);
	if(d == nil)
		error(exType);
	return d;
}


/* Retain a cached display before replacing an aliased result; each returned value owns one reference. */
static int
drawcachedisplay(Draw_Display **result, String *device)
{
	TkTop *top;
	Draw_Display *display;
	void *old;
	Heap *heap;

	if(string2c(device)[0] != 0)
		return 0;
	top = currun()->tktop;
	if(top == nil || top->dd == H || top->dd == nil)
		return 0;
	display = top->dd;
	heap = D2H(display);
	heap->ref++;
	poolmutable(heap);
	old = *result;
	*result = display;
	destroy(old);
	return 1;
}


/* Allocate an explicit display or return an owned default-display reference. */
void
Display_allocate(void *fp)
{
	F_Display_allocate *f;
	char buf[128], *dev;
	Subfont *df;
	Display *display;
	DDisplay *dd;
	Heap *h;
	Draw_Rect r;
	DRef *dr;
	Cache *c;

	f = fp;
	if(drawcachedisplay(f->ret, f->dev))
		return;
	destroy(*f->ret);
	*f->ret = H;

	if(cacheqlock == nil){
		cacheqlock = libqlalloc();
		if(cacheqlock == nil)
			return;
	}
	dev = string2c(f->dev);
	if(dev[0] == 0)
		dev = 0;
	display = initdisplay(dev, dev, nil);	/* TO DO: win, error */
	if(display == 0)
		return;

	dr = malloc(sizeof(DRef));
	if(dr == nil)
		return;
	h = heap(TDisplay);
	if(h == H){
		closedisplay(display);
		return;
	}
	dd = H2D(DDisplay*, h);
	dd->display = display;
	*f->ret = &dd->drawdisplay;
	dd->dref = dr;
	display->limbo = dr;
	dr->display = display;
	dr->ref = 1;
	df = getdefont(display);
	if(df){
		display->defaultsubfont = df;
		sprint(buf, "%d %d\n0 %d\t%s\n", df->height, df->ascent,
			df->n-1, deffontname);
		display->defaultfont = buildfont(display, buf, deffontname);
		if(display->defaultfont){
			c = cacheinstall(fcache, display, deffontname, display->defaultfont, "font");
			if(c)
				c->ref++;
			/* else BUG? */
		}
	}

	R2R(r, display->image->r);
	dd->drawdisplay.image = allocdrawimage(dd, r, display->image->chan, display->image, 0, 0);
	R2R(r, display->white->r);
	dd->drawdisplay.black = allocdrawimage(dd, r, display->black->chan, display->black, 1, 0);
	dd->drawdisplay.white = allocdrawimage(dd, r, display->white->chan, display->white, 1, 0);
	dd->drawdisplay.opaque = allocdrawimage(dd, r, display->opaque->chan, display->opaque, 1, 0);
	dd->drawdisplay.transparent = allocdrawimage(dd, r, display->transparent->chan, display->transparent, 1, 0);

	/* don't call unlockdisplay because the qlock was left up by initdisplay */
	libqunlock(display->qlock);
}

void
Display_getwindow(void *fp)
{
	F_Display_getwindow *f;
	Display *disp;
	int locked;
	Image *image;
	Screen *screen;
	char *wn;
	void *r;

	f = fp;
	r = f->ret->t0;
	f->ret->t0 = H;
	destroy(r);
	r = f->ret->t1;
	f->ret->t1 = H;
	destroy(r);
	disp = checkdisplay(f->d);
	if(f->winname == H)
		wn = "/dev/winname";
	else
		wn = string2c(f->winname);
	if(f->image == H)
		image = nil;
	else
		image = checkimage(f->image);
	if(f->screen == H)
		screen = nil;
	else
		screen = checkscreen(f->screen);
	locked = lockdisplay(disp);
	if(gengetwindow(disp, wn, &image, &screen, f->backup) < 0){
		/* TO DO: eliminate f->image and f->screen's references to Image and Screen */
		goto Return;
	}
	if(screen != nil){
		if(f->screen != H){
			f->ret->t0 = f->screen;
			D2H(f->screen)->ref++;
		}else
			f->ret->t0 = mkdrawscreen(screen, f->d);
	}
	if(image != nil){
		if(f->image != H){
			f->ret->t1 = f->image;
			D2H(f->image)->ref++;
		}else
			f->ret->t1 = mkdrawimage(image, f->ret->t0, f->d, nil);
	}

Return:
	if(locked)
		unlockdisplay(disp);
}

void
Display_startrefresh(void *fp)
{
	F_Display_startrefresh *f;
	Display *disp;

	f = fp;
	disp = checkdisplay(f->d);
	refreshslave(disp);
}

void
display_dec(void *v)
{
	DRef *dr;
	Display *d;
	int locked;

	dr = v;
	if(dr->ref-- != 1)
		return;

	d = dr->display;
	locked = lockdisplay(d);
	if(d->defaultfont != nil)
		font_close(d->defaultfont);
	if(d->defaultsubfont != nil)
		subfont_close(d->defaultsubfont);
	if(locked)
		unlockdisplay(d);
	freeallsubfonts(d);
	closedisplay(d);
	free(dr);
}

/* Detach native default aliases before managed child cleanup can retire their Image allocations. */
void
freedrawdisplay(Heap *h, int swept)
{
	DDisplay *dd;
	Display *display;
	DRef *reference;

	dd = H2D(DDisplay*, h);
	display = dd->display;
	reference = dd->dref;
	dd->display = nil;
	dd->dref = nil;
	if(display != nil){
		display->image = nil;
		display->white = nil;
		display->black = nil;
		display->opaque = nil;
		display->transparent = nil;
	}
	if(!swept)
		freeptrs(dd, h->t);
	if(reference != nil)
		display_dec(reference);
}


/* Retain the source display until its new solid-colour image replaces any aliased result owner. */
void
Display_color(void *fp)
{
	F_Display_color *f;
	Draw_Display *source;
	Draw_Image *result;
	Display *display;
	void *old, *slot;
	ulong bits;
	int locked;

	f = fp;
	slot = f->ret;
	if(slot == nil || slot == H || (uintptr)slot > (uintptr)-1-sizeof(old))
		error(exHeap);
	memmove(&old, slot, sizeof(old));
	if(old == nil)
		error(exNilref);
	source = f->d;
	bits = f->color;
	display = checkdisplay(source);
	locked = lockdisplay(display);
	result = color((DDisplay*)source, bits);
	if(locked)
		unlockdisplay(display);
	drawcommitresult(slot, result, old);
}


/* Capture the flush policy before resolving an image that may need a clip update. */
void
Image_flush(void *fp)
{
	F_Image_flush *f;
	Image *d;
	DImage *di;
	int locked, func;
	Draw_Image *source;

	f = fp;
	source = f->win;
	func = f->func;
	d = checkimage(source);
	di = (DImage*)source;
	switch(func){
	case 0:	/* Draw->Flushoff */
		di->flush = 0;
		break;
	case 1:	/* Draw->Flushon */
		di->flush = 1;
		/* fall through */
	case 2:	/* Draw->Flushnow */
		locked = lockdisplay(d->display);
		if(d->id==0 || d->screen!=0)
			flushimage(d->display, 1);
		if(locked)
			unlockdisplay(d->display);
		break;
	default:
		error(exInval);
	}
}

void
checkflush(Draw_Image *dst)
{
	DImage  *di;

	di = (DImage*)dst;
	if(di->flush && (di->image->id==0 || di->image->screen!=nil))
		flushimage(di->image->display, 1);
}

static void
imagedraw(void *fp, int op)
{
	F_Image_draw *f;
	Image *d, *s, *m;
	int locked;

	f = fp;
	d = checkimage(f->dst);
	if(f->src == H)
		s = d->display->black;
	else
		s = checkimage(f->src);
	if(f->matte == H)
		m = d->display->white;	/* ones */
	else
		m = checkimage(f->matte);
	if(d->display!=s->display || d->display!=m->display)
		return;
	locked = lockdisplay(d->display);
	drawop(d, IRECT(f->r), s, m, IPOINT(f->p), op);
	checkflush(f->dst);
	if(locked)
		unlockdisplay(d->display);
}

void
Image_draw(void *fp)
{
	imagedraw(fp, SoverD);
}

void
Image_drawop(void *fp)
{
	F_Image_drawop *f;

	f = fp;
	imagedraw(fp, f->op);
}

static void
imagegendraw(void *fp, int op)
{
	F_Image_gendraw *f;
	Image *d, *s, *m;
	int locked;

	f = fp;
	d = checkimage(f->dst);
	if(f->src == H)
		s = d->display->black;
	else
		s = checkimage(f->src);
	if(f->matte == H)
		m = d->display->white;	/* ones */
	else
		m = checkimage(f->matte);
	if(d->display!=s->display || d->display!=m->display)
		return;
	locked = lockdisplay(d->display);
	gendrawop(d, IRECT(f->r), s, IPOINT(f->p0), m, IPOINT(f->p1), op);
	checkflush(f->dst);
	if(locked)
		unlockdisplay(d->display);
}

void
Image_gendraw(void *fp)
{
	imagegendraw(fp, SoverD);
}

void
Image_gendrawop(void *fp)
{
	F_Image_gendrawop *f;

	f = fp;
	imagegendraw(fp, f->op);
}

static void
drawline(void *fp, int op)
{
	F_Image_line *f;
	Image *d, *s;
	int locked;

	f = fp;
	d = checkimage(f->dst);
	s = checkimage(f->src);
	if(d->display != s->display || f->radius < 0)
		return;
	locked = lockdisplay(d->display);
	lineop(d, IPOINT(f->p0), IPOINT(f->p1), f->end0, f->end1, f->radius, s, IPOINT(f->sp), op);
	checkflush(f->dst);
	if(locked)
		unlockdisplay(d->display);
}

void
Image_line(void *fp)
{
	drawline(fp, SoverD);
}

void
Image_lineop(void *fp)
{
	F_Image_lineop *f;

	f = fp;
	drawline(fp, f->op);
}

/* Capture a complete native-compatible Point array before image lookup or display effects.
 * VM roots protect actual storage; bounds checks are not arbitrary-pointer provenance. */
static int
drawpointdata(Array *a, Point **points)
{
	Array view;
	Heap owner;
	int size, pointers;
	uintptr address, extent;

	if(a == H || a == nil)
		error(exNilref);
	address = (uintptr)a;
	if(sizeof(WORD) != 4 || sizeof(Draw_Point) != sizeof(Point) ||
	   address <= sizeof(Heap) || address > (uintptr)-1-sizeof(view))
		error(exBounds);
	memmove(&owner, (uchar*)a-sizeof(Heap), sizeof(owner));
	if(owner.t != &Tarray)
		error(exType);
	memmove(&view, a, sizeof(view));
	if(view.t == nil || view.t == H)
		error(exType);
	extent = offsetof(Type, size) > offsetof(Type, np) ? offsetof(Type, size) : offsetof(Type, np);
	if((uintptr)view.t > (uintptr)-1-extent-sizeof(int))
		error(exType);
	memmove(&size, (uchar*)view.t+offsetof(Type, size), sizeof(size));
	memmove(&pointers, (uchar*)view.t+offsetof(Type, np), sizeof(pointers));
	if(size != sizeof(Point) || pointers != 0)
		error(exType);
	if(view.len < 0 || (uvlong)view.len > 65536 ||
	   (view.len != 0 && (view.data == nil || view.data == H ||
	    (uintptr)view.data%sizeof(int) != 0 ||
	    (uintptr)view.data > (uintptr)-1-(uintptr)view.len*sizeof(Point))))
		error(exBounds);
	if(view.len == 0){
		*points = nil;
		return 0;
	}
	*points = (Point*)view.data;
	return (int)view.len;
}


/* Validate Point storage once, then preserve the outlined polygon/spline call and lock order. */
static void
drawsplinepoly(void *fp, int smooth, int op)
{
	F_Image_poly *f;
	Image *d, *s;
	int locked, count;
	Point *points;

	f = fp;
	count = drawpointdata(f->p, &points);
	d = checkimage(f->dst);
	s = checkimage(f->src);
	if(d->display != s->display|| f->radius < 0)
		return;
	locked = lockdisplay(d->display);
	if(smooth)
		bezsplineop(d, points, count,
			f->end0, f->end1, f->radius, s, IPOINT(f->sp), op);
	else
		polyop(d, points, count, f->end0,
			f->end1, f->radius, s, IPOINT(f->sp), op);
	checkflush(f->dst);
	if(locked)
		unlockdisplay(d->display);
}


/* Dispatch the checked outlined polygon with default compositing. */
void
Image_poly(void *fp)
{
	drawsplinepoly(fp, 0, SoverD);
}


/* Dispatch the checked outlined polygon with explicit compositing. */
void
Image_polyop(void *fp)
{
	F_Image_polyop *f;

	f = fp;
	drawsplinepoly(fp, 0, f->op);
}


/* Dispatch the checked outlined spline with default compositing. */
void
Image_bezspline(void *fp)
{
	drawsplinepoly(fp, 1, SoverD);
}


/* Dispatch the checked outlined spline with explicit compositing. */
void
Image_bezsplineop(void *fp)
{
	F_Image_bezsplineop *f;

	f = fp;
	drawsplinepoly(fp, 1, f->op);
}

static void
drawbezier(void *fp, int op)
{
	F_Image_bezier *f;
	Image *d, *s;
	int locked;

	f = fp;
	d = checkimage(f->dst);
	s = checkimage(f->src);
	if(d->display != s->display || f->radius < 0)
		return;
	locked = lockdisplay(d->display);
	bezierop(d, IPOINT(f->a), IPOINT(f->b), IPOINT(f->c),
		  IPOINT(f->d), f->end0, f->end1, f->radius, s, IPOINT(f->sp), op);
	checkflush(f->dst);
	if(locked)
		unlockdisplay(d->display);
}

void
Image_bezier(void *fp)
{
	drawbezier(fp, SoverD);
}

void
Image_bezierop(void *fp)
{
	F_Image_bezierop *f;

	f = fp;
	drawbezier(fp, f->op);
}

static void
drawfillbezier(void *fp, int op)
{
	F_Image_fillbezier *f;
	Image *d, *s;
	int locked;

	f = fp;
	d = checkimage(f->dst);
	s = checkimage(f->src);
	if(d->display != s->display)
		return;
	locked = lockdisplay(d->display);
	fillbezierop(d, IPOINT(f->a), IPOINT(f->b), IPOINT(f->c),
			IPOINT(f->d), f->wind, s, IPOINT(f->sp), op);
	checkflush(f->dst);
	if(locked)
		unlockdisplay(d->display);
}

void
Image_fillbezier(void *fp)
{
	drawfillbezier(fp, SoverD);
}

void
Image_fillbezierop(void *fp)
{
	F_Image_fillbezierop *f;

	f = fp;
	drawfillbezier(fp, f->op);
}


/* Validate Point storage before image effects in filled polygon/spline calls. */
static void
drawfillsplinepoly(void *fp, int smooth, int op)
{
	F_Image_fillpoly *f;
	Image *d, *s;
	int locked, count;
	Point *points;

	f = fp;
	count = drawpointdata(f->p, &points);
	d = checkimage(f->dst);
	s = checkimage(f->src);
	if(d->display != s->display)
		return;
	locked = lockdisplay(d->display);
	if(smooth)
		fillbezsplineop(d, points, count,
			f->wind, s, IPOINT(f->sp), op);
	else
		fillpolyop(d, points, count,
			f->wind, s, IPOINT(f->sp), op);
	checkflush(f->dst);
	if(locked)
		unlockdisplay(d->display);
}


/* Dispatch the checked filled polygon with default compositing. */
void
Image_fillpoly(void *fp)
{
	drawfillsplinepoly(fp, 0, SoverD);
}


/* Dispatch the checked filled polygon with explicit compositing. */
void
Image_fillpolyop(void *fp)
{
	F_Image_fillpolyop *f;

	f = fp;
	drawfillsplinepoly(fp, 0, f->op);
}


/* Dispatch the checked filled spline with default compositing. */
void
Image_fillbezspline(void *fp)
{
	drawfillsplinepoly(fp, 1, SoverD);
}


/* Dispatch the checked filled spline with explicit compositing. */
void
Image_fillbezsplineop(void *fp)
{
	F_Image_fillbezsplineop *f;

	f = fp;
	drawfillsplinepoly(fp, 1, f->op);
}

static void
drawarcellipse(void *fp, int isarc, int alpha, int phi, int op)
{
	F_Image_arc *f;
	Image *d, *s;
	int locked;

	f = fp;
	d = checkimage(f->dst);
	s = checkimage(f->src);
	if(d->display != s->display || f->thick < 0 || f->a<0 || f->b<0)
		return;

	locked = lockdisplay(d->display);
	if(isarc)
		arcop(d, IPOINT(f->c), f->a, f->b, f->thick, s,
			IPOINT(f->sp), alpha, phi, op);
	else
		ellipseop(d, IPOINT(f->c), f->a, f->b, f->thick, s,
			IPOINT(f->sp), op);
	checkflush(f->dst);
	if(locked)
		unlockdisplay(d->display);
}

void
Image_ellipse(void *fp)
{
	drawarcellipse(fp, 0, 0, 0, SoverD);
}

void
Image_ellipseop(void *fp)
{
	F_Image_ellipseop *f;

	f = fp;
	drawarcellipse(fp, 0, 0, 0, f->op);
}

void
Image_arc(void *fp)
{
	F_Image_arc *f;

	f = fp;
	drawarcellipse(fp, 1, f->alpha, f->phi, SoverD);
}

void
Image_arcop(void *fp)
{
	F_Image_arcop *f;

	f = fp;
	drawarcellipse(fp, 1, f->alpha, f->phi, f->op);
}

static void
drawfillarcellipse(void *fp, int isarc, int alpha, int phi, int op)
{
	F_Image_fillarc *f;
	Image *d, *s;
	int locked;

	f = fp;
	d = checkimage(f->dst);
	s = checkimage(f->src);
	if(d->display != s->display || f->a<0 || f->b<0)
		return;

	locked = lockdisplay(d->display);
	if(isarc)
		fillarcop(d, IPOINT(f->c), f->a, f->b, s, IPOINT(f->sp), alpha, phi, op);
	else
		fillellipseop(d, IPOINT(f->c), f->a, f->b, s, IPOINT(f->sp), op);
	checkflush(f->dst);
	if(locked)
		unlockdisplay(d->display);
}

void
Image_fillellipse(void *fp)
{
	drawfillarcellipse(fp, 0, 0, 0, SoverD);
}

void
Image_fillellipseop(void *fp)
{
	F_Image_fillellipseop *f;

	f = fp;
	drawfillarcellipse(fp, 0, 0, 0, f->op);
}

void
Image_fillarc(void *fp)
{
	F_Image_fillarc *f;

	f = fp;
	drawfillarcellipse(fp, 1, f->alpha, f->phi, SoverD);
}

void
Image_fillarcop(void *fp)
{
	F_Image_fillarcop *f;

	f = fp;
	drawfillarcellipse(fp, 1, f->alpha, f->phi, f->op);
}

typedef struct DrawTextView DrawTextView;

struct DrawTextView {
	char *ascii; // Borrowed terminated byte representation, or nil for Rune input.
	Rune *runes; // Borrowed aligned native Rune representation, or nil for ASCII.
	int count; // Captured nonnegative logical length, never negated before validation.
};


/* Validate and terminate the borrowed representation before native font/cache side effects.
 * Caller retains the String and VM admission; native font work later acquires display exclusion.
 * This is not a GC pin or arbitrary-pointer provenance. */
static void
drawtextview(String *string, DrawTextView *view)
{
	int length, capacity;
	uintptr address;
	uvlong bytes;
	Rune zero;

	memset(view, 0, sizeof(*view));
	if(string == H)
		return;
	if(string == nil)
		error(exNilref);
	address = (uintptr)string;
	if(address > (uintptr)-1-O(String, data))
		error(exHeap);
	memmove(&length, (uchar*)string+O(String, len), sizeof(length));
	memmove(&capacity, (uchar*)string+O(String, max), sizeof(capacity));
	if(length == (-2147483647-1))
		error(exHeap);
	view->count = length < 0 ? -length : length;
	if(capacity < view->count)
		error(exHeap);
	address += O(String, data);
	bytes = ((uvlong)view->count+1)*(length < 0 ? sizeof(Rune) : 1);
	if(bytes > (uintptr)-1-address || (length < 0 && address%sizeof(Rune) != 0))
		error(exHeap);
	/* Slices can shorten len without changing old trailing payload; bound native lookahead. */
	if(length < 0){
		zero = 0;
		memmove((void*)(address+(uintptr)view->count*sizeof(Rune)), &zero, sizeof(zero));
		view->runes = (Rune*)address;
	}else{
		*(char*)(address+view->count) = 0;
		view->ascii = (char*)address;
	}
}


/* Return the requested point deterministically for skipped text and capture the caller result slot. */
static void
drawtext(void *fp, int op)
{
	F_Image_text *f;
	Font *font;
	Point pt;
	Image *s, *d;
	DrawTextView text;
	Draw_Point result, *ret;
	int locked;

	f = fp;
	ret = f->ret;
	if(ret == nil || ret == H || (uintptr)ret > (uintptr)-1-sizeof(result))
		error(exHeap);
	pt = IPOINT(f->p);
	if(f->dst == H || f->src == H)
		goto Return;
	if(f->font == H || f->str == H)
		goto Return;
	if((unsigned int)op >= Ncomp)
		goto Return;
	drawtextview(f->str, &text);
	if(text.count == 0)
		goto Return;
	d = checkimage(f->dst);
	s = checkimage(f->src);
	font = checkfont(f->font);
	if(d->display!=s->display || d->display!=font->display)
		goto Return;
	locked = lockdisplay(d->display);
	if(text.ascii != nil)
		pt = stringnop(d, pt, s, IPOINT(f->sp), font, text.ascii, text.count, op);
	else
		pt = runestringnop(d, pt, s, IPOINT(f->sp), font, text.runes, text.count, op);
	checkflush(f->dst);
	if(locked)
		unlockdisplay(d->display);
Return:
	result.x = pt.x;
	result.y = pt.y;
	memmove(ret, &result, sizeof(result));
}


/* Dispatch Image_text through the checked borrowed text and captured result path. */
void
Image_text(void *fp)
{
	drawtext(fp, SoverD);
}


/* Dispatch Image_textop through the checked borrowed text and captured result path. */
void
Image_textop(void *fp)
{
	F_Image_textop *f;

	f = fp;
	drawtext(fp, f->op);
}


/* Check background display identity and preserve a deterministic point on every skipped draw. */
static void
drawtextbg(void *fp, int op)
{
	F_Image_textbg *f;
	Font *font;
	Point pt;
	Image *s, *d, *bg;
	DrawTextView text;
	Draw_Point result, *ret;
	int locked;

	f = fp;
	ret = f->ret;
	if(ret == nil || ret == H || (uintptr)ret > (uintptr)-1-sizeof(result))
		error(exHeap);
	pt = IPOINT(f->p);
	if(f->dst == H || f->src == H)
		goto Return;
	if(f->font == H || f->str == H || f->bg == H)
		goto Return;
	if((unsigned int)op >= Ncomp)
		goto Return;
	drawtextview(f->str, &text);
	if(text.count == 0)
		goto Return;
	d = checkimage(f->dst);
	s = checkimage(f->src);
	bg = checkimage(f->bg);
	font = checkfont(f->font);
	if(d->display!=s->display || d->display!=font->display || d->display!=bg->display)
		goto Return;
	locked = lockdisplay(d->display);
	if(text.ascii != nil)
		pt = stringnbgop(d, pt, s, IPOINT(f->sp), font, text.ascii, text.count, bg, IPOINT(f->bgp), op);
	else
		pt = runestringnbgop(d, pt, s, IPOINT(f->sp), font, text.runes, text.count, bg, IPOINT(f->bgp), op);
	checkflush(f->dst);
	if(locked)
		unlockdisplay(d->display);
Return:
	result.x = pt.x;
	result.y = pt.y;
	memmove(ret, &result, sizeof(result));
}


/* Dispatch Image_textbg through the checked borrowed text and captured result path. */
void
Image_textbg(void *fp)
{
	drawtextbg(fp, SoverD);
}


/* Dispatch Image_textbgop through the checked borrowed text and captured result path. */
void
Image_textbgop(void *fp)
{
	F_Image_textbgop *f;

	f = fp;
	drawtextbg(fp, f->op);
}

static void
drawborder(void *fp, int op)
{
	F_Image_border *f;
	Image *d, *s;
	int locked;

	f = fp;
	d = checkimage(f->dst);
	s = checkimage(f->src);
	if(d->display != s->display)
		return;
	locked = lockdisplay(d->display);
	borderop(d, IRECT(f->r), f->i, s, IPOINT(f->sp), op);
	checkflush(f->dst);
	if(locked)
		unlockdisplay(d->display);
}

void
Image_border(void *fp)
{
	drawborder(fp, SoverD);
}

/* Stage the new image while aliased source/result owners remain alive, then publish before retirement. */
void
Display_newimage(void *fp)
{
	F_Display_newimage *f;
	Draw_Display *source;
	Draw_Image *result;
	void *old, *slot;
	Display *display;
	int locked;

	f = fp;
	slot = f->ret;
	if(slot == nil || slot == H || (uintptr)slot > (uintptr)-1-sizeof(old))
		error(exHeap);
	memmove(&old, slot, sizeof(old));
	if(old == nil)
		error(exNilref);
	source = f->d;
	display = checkdisplay(source);
	locked = lockdisplay(display);
	result = allocdrawimage((DDisplay*)source, f->r, f->chans.desc, nil, f->repl, f->color);
	if(locked)
		unlockdisplay(display);
	drawcommitresult(slot, result, old);
}

/* Keep an aliased display owner until native colour mixing and managed wrapping have finished. */
void
Display_colormix(void *fp)
{
	F_Display_colormix *f;
	Draw_Display *source;
	Draw_Image *result;
	Display *display;
	Image *image;
	void *old, *slot;
	int locked;

	f = fp;
	slot = f->ret;
	if(slot == nil || slot == H || (uintptr)slot > (uintptr)-1-sizeof(old))
		error(exHeap);
	memmove(&old, slot, sizeof(old));
	if(old == nil)
		error(exNilref);
	source = f->d;
	display = checkdisplay(source);
	locked = lockdisplay(display);
	image = allocimagemix(display, f->c1, f->c2);
	result = image == nil ? H : mkdrawimage(image, H, source, nil);
	if(image != nil && result == H)
		drawdiscardimage(image);
	if(locked)
		unlockdisplay(display);
	drawcommitresult(slot, result, old);
}


/* Validate a stable byte view before image lookup can synchronize display state.
 * The VM owns the input object/root; address bounds do not establish provenance. */
static int
drawpixeldata(Array *a, uchar **data)
{
	Array view;
	Heap owner;
	int size, pointers;
	uintptr address, extent;

	if(a == H || a == nil)
		error(exNilref);
	address = (uintptr)a;
	if(address <= sizeof(Heap) || address > (uintptr)-1-sizeof(view))
		error(exBounds);
	/* Copy potentially packed array and element metadata through byte views. */
	memmove(&owner, (uchar*)a-sizeof(Heap), sizeof(owner));
	if(owner.t != &Tarray)
		error(exType);
	memmove(&view, a, sizeof(view));
	if(view.t != &Tbyte) {
		extent = offsetof(Type, size) > offsetof(Type, np) ? offsetof(Type, size) : offsetof(Type, np);
		if(view.t == nil || view.t == H || (uintptr)view.t > (uintptr)-1-extent-sizeof(int))
			error(exType);
		memmove(&size, (uchar*)view.t+offsetof(Type, size), sizeof(size));
		memmove(&pointers, (uchar*)view.t+offsetof(Type, np), sizeof(pointers));
		if(size != 1 || pointers != 0)
			error(exType);
	}
	if(view.len < 0 || (uvlong)view.len > 0x7fffffff ||
	   (view.len != 0 && (view.data == nil || view.data == H ||
	    (uintptr)view.data > (uintptr)-1-(uintptr)view.len)))
		error(exBounds);
	*data = view.data;
	return (int)view.len;
}


/* Complete the native read and unlock before copying a possibly unaligned result. */
void
Image_readpixels(void *fp)
{
	uchar *data;
	int length;
	WORD result;
	F_Image_readpixels *f;
	Rectangle r;
	Image *i;
	int locked;

	f = fp;
	length = drawpixeldata(f->data, &data);
	R2R(r, f->r);
	i = checkimage(f->src);
	locked = lockdisplay(i->display);
	result = unloadimage(i, r, data, length);
	if(locked)
		unlockdisplay(i->display);
	memmove(f->ret, &result, sizeof(result));
}


/* Validate the byte view before image effects and publish the result after unlocking. */
void
Image_writepixels(void *fp)
{
	uchar *data;
	int length;
	WORD result;
	Rectangle r;
	F_Image_writepixels *f;
	Image *i;
	int locked;

	f = fp;
	length = drawpixeldata(f->data, &data);
	R2R(r, f->r);
	i = checkimage(f->dst);
	locked = lockdisplay(i->display);
	result = loadimage(i, r, data, length);
	checkflush(f->dst);
	if(locked)
		unlockdisplay(i->display);
	memmove(f->ret, &result, sizeof(result));
}


/* Pack line-end bits with defined unsigned shifts before copying the VM result. */
void
Image_arrow(void *fp)
{
	F_Image_arrow *f;
	WORD result;

	f = fp;
	result = (UWORD)Endarrow|((UWORD)f->a<<5)|
		((UWORD)f->b<<14)|((UWORD)f->c<<23);
	memmove(f->ret, &result, sizeof(result));
}


/* Acquire the new name before releasing a displaced name, including a self-aliasing String owner. */
void
Image_name(void *fp)
{
	F_Image_name *f;
	Draw_Image *source;
	Image *image;
	String *name, *replacement, *old;
	WORD *slot, result;
	int locked, in;
	char *text;

	f = fp;
	slot = f->ret;
	if(slot == nil || slot == H || (uintptr)slot > (uintptr)-1-sizeof(result))
		error(exHeap);
	source = f->src;
	name = f->name;
	in = f->in;
	image = checkimage(source);
	text = string2c(name);
	locked = lockdisplay(image->display);
	result = nameimage(image, text, in);
	if(locked)
		unlockdisplay(image->display);
	if(result){
		replacement = in ? name : H;
		if(replacement != H)
			D2H(replacement)->ref++;
		old = source->iname;
		drawcommitresult(&source->iname, replacement, old);
	}
	memmove(slot, &result, sizeof(result));
}

/* loadimage/nsvg strides use signed int; validate before multiplying. */
static int
drawimagesize(unsigned w, unsigned h, int *bytes)
{
	if(w == 0 || h == 0 || h > 0x7fffffffUL/4 || w > 0x7fffffffUL/4/h)
		return 0;
	*bytes=w*h*4;
	return 1;
}

/* Optional :positive-size:path prefix, shared by PNG and SVG. */
static int
drawimagespec(char **name, unsigned *size)
{
	char *p;
	unsigned n, digit;
	p=*name;
	*size=0;
	if(p == nil || *p == '\0')
		return 0;
	if(*p == ':') {
		p++;
		n=0;
		if(*p < '0' || *p > '9')
			return 0;
		while(*p >= '0' && *p <= '9') {
			digit=*p++-'0';
			if(n > (0x7fffffffUL-digit)/10)
				return 0;
			n=n*10+digit;
		}
		if(n == 0 || *p++ != ':' || *p == '\0')
			return 0;
		*size=n;
	}
	*name=p;
	return 1;
}

static char*
drawimagepath(char *name)
{
	ulong a, b;
	char *path;
	a=strlen(rootdir); b=strlen(name);
	if(a > 0x7ffffffdUL || b > 0x7ffffffdUL-a)
		return nil;
	path=malloc(a+b+2);
	if(path != nil) {
		memmove(path, rootdir, a);
		path[a]='/';
		memmove(path+a+1, name, b+1);
	}
	return path;
}

/* Both decoders return straight RGBA. Draw ARGB32 is byte-addressed BGRA
 * with premultiplied components, as required by libmemdraw's compositing. */
static Image*
drawimageload(Display *disp, unsigned char *pixels, unsigned w, unsigned h)
{
	Rectangle r;
	Image *image;
	int bytes, j, locked;
	unsigned red, alpha;
	if(!drawimagesize(w,h,&bytes))
		return nil;
	for(j=0; j<bytes; j+=4) {
		red=pixels[j]; alpha=pixels[j+3];
		pixels[j]=(pixels[j+2]*alpha+127)/255;
		pixels[j+1]=(pixels[j+1]*alpha+127)/255;
		pixels[j+2]=(red*alpha+127)/255;
	}
	r.min.x=r.min.y=0; r.max.x=w; r.max.y=h;
	locked=lockdisplay(disp);
	image=allocimage(disp, r, ARGB32, 0, DTransparent);
	if(image != nil && loadimage(image,r,pixels,bytes) != bytes) {
		freeimage(image);
		image=nil;
	}
	if(locked)
		unlockdisplay(disp);
	return image;
}

static Image*
drawimagepng(Display *disp, char *path, unsigned size)
{
	unsigned char *pixels, *scaled;
	unsigned w, h, nw, nh, x, y, sx, sy, longest, code;
	int bytes;
	Image *image;
	pixels=nil;
	code=lodepng_decode32_file(&pixels,&w,&h,path);
	if(code != 0 || pixels == nil || !drawimagesize(w,h,&bytes)) {
		free(pixels);
		return nil;
	}
	if(size != 0) {
		longest=w > h ? w : h;
		nw=(uvlong)w*size/longest; nh=(uvlong)h*size/longest;
		if(nw == 0) nw=1;
		if(nh == 0) nh=1;
		if(!drawimagesize(nw,nh,&bytes)) {
			free(pixels);
			return nil;
		}
		scaled=malloc(bytes);
		if(scaled == nil) {
			free(pixels);
			return nil;
		}
		for(y=0; y<nh; y++) {
			sy=(uvlong)y*h/nh;
			for(x=0; x<nw; x++) {
				sx=(uvlong)x*w/nw;
				memmove(scaled+((ulong)y*nw+x)*4, pixels+((ulong)sy*w+sx)*4, 4);
			}
		}
		free(pixels); pixels=scaled; w=nw; h=nh;
	}
	image=drawimageload(disp,pixels,w,h);
	free(pixels);
	return image;
}

static Image*
drawimagesvg(Display *disp, char *path, unsigned size)
{
	NSVGimage *svg;
	NSVGrasterizer *rast;
	unsigned char *pixels;
	unsigned w, h;
	double width, height, ratio;
	float scale;
	int bytes;
	Image *image;
	svg=nsvgParseFromFile(path,"px",72.0f);
	if(svg == nil)
		return nil;
	width=svg->width; height=svg->height;
	if(!(width > 0) || !(height > 0) || width > 0x7fffffffUL || height > 0x7fffffffUL) {
		nsvgDelete(svg);
		return nil;
	}
	ratio=size == 0 ? 1.0 : size/(width > height ? width : height);
	if(!(ratio > 0) || ratio > FLT_MAX) {
		nsvgDelete(svg);
		return nil;
	}
	scale=ratio;
	width*=scale; height*=scale;
	if(!(width > 0) || !(height > 0) || width > 0x7fffffffUL || height > 0x7fffffffUL) {
		nsvgDelete(svg);
		return nil;
	}
	w=width < 1 ? 1 : (unsigned)width;
	h=height < 1 ? 1 : (unsigned)height;
	/* Float rounding must not shorten the requested longest side. */
	if(size != 0) {
		if(svg->width >= svg->height) w=size;
		else h=size;
	}
	if(!drawimagesize(w,h,&bytes)) {
		nsvgDelete(svg);
		return nil;
	}
	rast=nsvgCreateRasterizer();
	if(rast == nil) {
		nsvgDelete(svg);
		return nil;
	}
	pixels=malloc(bytes);
	if(pixels == nil) {
		nsvgDeleteRasterizer(rast); nsvgDelete(svg);
		return nil;
	}
	nsvgRasterize(rast,svg,0,0,scale,pixels,w,h,w*4);
	nsvgDeleteRasterizer(rast); nsvgDelete(svg);
	image=drawimageload(disp,pixels,w,h);
	free(pixels);
	return image;
}

Image*
display_open(Display *disp, char *name)
{
	char *extension, *path;
	Image *image;
	unsigned size;
	ulong length;
	int fd, png, svg;
	if(!drawimagespec(&name,&size))
		return nil;
	length=strlen(name);
	extension=length >= 4 ? name+length-4 : "";
	png=cistrcmp(extension,".png")==0;
	svg=cistrcmp(extension,".svg")==0;
	if(png || svg) {
		path=drawimagepath(name);
		if(path == nil)
			return nil;
		image=png ? drawimagepng(disp,path,size) : drawimagesvg(disp,path,size);
		free(path);
		return image;
	}
	if(size != 0)
		return nil;
	fd=libopen(name,OREAD);
	if(fd < 0)
		return nil;
	image=readimage(disp,fd,1);
	libclose(fd);
	return image;
}


/* Consume a newly loaded native image, rolling it back if its managed wrapper cannot be built. */
static Draw_Image*
drawownedimage(Draw_Display *source, Image *image, int repl)
{
	Draw_Image *result;
	Display *display;
	int locked;

	if(image == nil)
		return H;
	display = image->display;
	locked = lockdisplay(display);
	result = allocdrawimage((DDisplay*)source, DRECT(image->r), image->chan, image, repl, 0);
	if(result == H)
		drawdiscardimage(image);
	if(locked)
		unlockdisplay(display);
	return result;
}


/* Keep the display and filename alive until loading and managed ownership transfer finish. */
void
Display_open(void *fp)
{
	Image *image;
	Display *display;
	Draw_Display *source;
	Draw_Image *result;
	String *name;
	F_Display_open *f;
	void *old, *slot;

	f = fp;
	slot = f->ret;
	if(slot == nil || slot == H || (uintptr)slot > (uintptr)-1-sizeof(old))
		error(exHeap);
	memmove(&old, slot, sizeof(old));
	if(old == nil)
		error(exNilref);
	source = f->d;
	name = f->name;
	display = lookupdisplay(source);
	result = H;
	if(display != nil){
		image = display_open(display, string2c(name));
		result = drawownedimage(source, image, 0);
	}
	drawcommitresult(slot, result, old);
}


/* Retain a successful named-image name before retiring a result that may own that same String. */
void
Display_namedimage(void *fp)
{
	F_Display_namedimage *f;
	Draw_Display *source;
	Display *display;
	Image *image;
	Draw_Image *result;
	String *name;
	char *text;
	void *old, *slot;
	int locked;

	f = fp;
	slot = f->ret;
	if(slot == nil || slot == H || (uintptr)slot > (uintptr)-1-sizeof(old))
		error(exHeap);
	memmove(&old, slot, sizeof(old));
	if(old == nil)
		error(exNilref);
	source = f->d;
	name = f->name;
	display = checkdisplay(source);
	text = string2c(name);
	locked = lockdisplay(display);
	image = namedimage(display, text);
	if(locked)
		unlockdisplay(display);
	result = drawownedimage(source, image, image == nil ? 0 : image->repl);
	if(result != H){
		result->iname = name;
		if(name != H)
			D2H(name)->ref++;
	}
	drawcommitresult(slot, result, old);
}


/* Capture the file descriptor and display before loading; failed wrapping consumes the new image. */
void
Display_readimage(void *fp)
{
	Image *image;
	Display *display;
	Draw_Display *source;
	Draw_Image *result;
	F_Display_readimage *f;
	Sys_FD *fd;
	void *old, *slot;
	int descriptor;

	f = fp;
	slot = f->ret;
	if(slot == nil || slot == H || (uintptr)slot > (uintptr)-1-sizeof(old))
		error(exHeap);
	memmove(&old, slot, sizeof(old));
	if(old == nil)
		error(exNilref);
	source = f->d;
	fd = f->fd;
	result = H;
	if(fd == nil)
		error(exNilref);
	if(fd != H){
		descriptor = fd->fd;
		display = checkdisplay(source);
		image = readimage(display, descriptor, 1);
		result = drawownedimage(source, image, 0);
	}
	drawcommitresult(slot, result, old);
}


/* Complete source validation and native writing before publishing an unaligned or overlapping scalar. */
void
Display_writeimage(void *fp)
{
	Image *image;
	F_Display_writeimage *f;
	Sys_FD *fd;
	Draw_Display *display;
	Draw_Image *source;
	WORD *slot, result;
	int descriptor;

	f = fp;
	slot = f->ret;
	if(slot == nil || slot == H || (uintptr)slot > (uintptr)-1-sizeof(result))
		error(exHeap);
	fd = f->fd;
	display = f->d;
	source = f->i;
	result = -1;
	if(fd == nil)
		error(exNilref);
	if(fd != H){
		descriptor = fd->fd;
		image = checkimage(source);
		if(checkdisplay(display) == image->display)
			result = writeimage(descriptor, image, 1);
	}
	memmove(slot, &result, sizeof(result));
}

Draw_Screen*
mkdrawscreen(Screen *s, Draw_Display *display)
{
	Heap *h;
	DScreen *ds;
	Draw_Image *dimage, *dfill;

	dimage = mkdrawimage(s->image, H, display, nil);
	dfill = mkdrawimage(s->fill, H, display, nil);
	h = heap(TScreen);
	if(h == H)
		return nil;
	ds = H2D(DScreen*, h);
	ds->screen = s;
	ds->drawscreen.fill = dfill;
	D2H(dfill)->ref++;
	ds->drawscreen.image = dimage;
	D2H(dimage)->ref++;
	ds->drawscreen.display = dimage->display;
	D2H(dimage->display)->ref++;
	ds->drawscreen.id = s->id;
	ds->dref = s->display->limbo;
	ds->dref->ref++;
	return &ds->drawscreen;
}

/* Construct allocdrawscreen privately; native input ownership transfers only after managed allocation succeeds. */
static DScreen*
allocdrawscreen(Draw_Image *dimage, Draw_Image *dfill, int public)
{
	Heap *h;
	Screen *s;
	DScreen *ds;
	Image *image, *fill;
	char reason[ERRMAX];

	image = ((DImage*)dimage)->image;
	fill = ((DImage*)dfill)->image;
	s = allocscreen(image, fill, public);
	if(s == 0)
		return nil;
	h = drawheap(TScreen);
	if(h == H){
		kgerrstr(reason, sizeof(reason));
		freescreen(s);
		kwerrstr("%s", reason);
		return nil;
	}
	ds = H2D(DScreen*, h);
	ds->screen = s;
	ds->drawscreen.fill = dfill;
	D2H(dfill)->ref++;
	ds->drawscreen.image = dimage;
	D2H(dimage)->ref++;
	ds->drawscreen.display = dimage->display;
	D2H(dimage->display)->ref++;
	ds->drawscreen.id = s->id;
	ds->dref = image->display->limbo;
	ds->dref->ref++;
	return ds;
}

/* Keep source images rooted until the new Screen owns them; replace aliased results only afterward. */
void
Screen_allocate(void *fp)
{
	F_Screen_allocate *f;
	Draw_Image *image, *fill;
	Draw_Screen *result;
	DScreen *created;
	Image *native;
	void *old, *slot;
	int locked;

	f = fp;
	slot = f->ret;
	if(slot == nil || slot == H || (uintptr)slot > (uintptr)-1-sizeof(old))
		error(exHeap);
	memmove(&old, slot, sizeof(old));
	if(old == nil)
		error(exNilref);
	image = f->image;
	fill = f->fill;
	native = checkimage(image);
	checkimage(fill);
	locked = lockdisplay(native->display);
	created = allocdrawscreen(image, fill, f->public);
	result = created == nil ? H : &created->drawscreen;
	if(locked)
		unlockdisplay(native->display);
	drawcommitresult(slot, result, old);
}

/* Roll back native screens on managed allocation refusal and retain an aliased display through publication. */
void
Display_publicscreen(void *fp)
{
	F_Display_publicscreen *f;
	Draw_Display *source;
	Draw_Screen *result;
	Heap *h;
	Screen *screen;
	DScreen *ds;
	Display *display;
	void *old, *slot;
	char reason[ERRMAX];
	int locked;

	f = fp;
	slot = f->ret;
	if(slot == nil || slot == H || (uintptr)slot > (uintptr)-1-sizeof(old))
		error(exHeap);
	memmove(&old, slot, sizeof(old));
	if(old == nil)
		error(exNilref);
	source = f->d;
	display = checkdisplay(source);
	result = H;
	if(display->image != nil){
		locked = lockdisplay(display);
		screen = publicscreen(display, f->id, display->image->chan);
		if(screen != nil){
			h = drawheap(TScreen);
			if(h == H){
				kgerrstr(reason, sizeof(reason));
				freescreen(screen);
				kwerrstr("%s", reason);
			}else{
				ds = H2D(DScreen*, h);
				ds->screen = screen;
				ds->drawscreen.fill = H;
				ds->drawscreen.image = H;
				ds->drawscreen.id = screen->id;
				ds->drawscreen.display = source;
				D2H(source)->ref++;
				ds->dref = display->limbo;
				ds->dref->ref++;
				result = &ds->drawscreen;
			}
		}
		if(locked)
			unlockdisplay(display);
	}
	drawcommitresult(slot, result, old);
}

/* Retire the native screen while its images/display remain owned, then release each mapped edge once. */
void
freedrawscreen(Heap *h, int swept)
{
	DScreen *ds;
	Screen *screen;
	DRef *reference;
	Display *display;
	int locked;

	ds = H2D(DScreen*, h);
	screen = ds->screen;
	reference = ds->dref;
	ds->screen = nil;
	ds->dref = nil;
	if(screen != nil){
		display = screen->display;
		locked = lockdisplay(display);
		freescreen(screen);
		if(locked)
			unlockdisplay(display);
	}
	if(!swept)
		freeptrs(ds, h->t);
	if(reference != nil)
		display_dec(reference);
}


/* Consume an opened native font only after retaining its managed name and display owners. */
static Draw_Font*
drawownedfont(Font *font, Draw_Display *display, String *name)
{
	DFont *df;
	Heap *h;
	DRef *reference;
	char reason[ERRMAX];

	if(font == nil)
		return H;
	reference = font->display->limbo;
	h = reference == nil ? H : drawheap(TFont);
	if(h == H){
		kgerrstr(reason, sizeof(reason));
		font_close(font);
		kwerrstr("%s", reason);
		return H;
	}
	df = H2D(DFont*, h);
	df->font = font;
	df->drawfont.name = name;
	if(name != H)
		D2H(name)->ref++;
	df->drawfont.height = font->height;
	df->drawfont.ascent = font->ascent;
	df->drawfont.display = display;
	D2H(display)->ref++;
	df->dref = reference;
	reference->ref++;
	return &df->drawfont;
}


/* Build or reuse a font before replacing a result that may also contain an input owner. */
void
Font_build(void *fp)
{
	F_Font_build *f;
	Font *font;
	Draw_Display *display;
	Draw_Font *result;
	String *nameowner, *description;
	char buf[128];
	char *name, *data;
	Subfont *df;
	Display *disp;
	Cache *c;
	void *old, *slot;
	int locked;

	f = fp;
	slot = f->ret;
	if(slot == nil || slot == H || (uintptr)slot > (uintptr)-1-sizeof(old))
		error(exHeap);
	memmove(&old, slot, sizeof(old));
	if(old == nil)
		error(exNilref);
	display = f->d;
	nameowner = f->name;
	description = f->desc;
	disp = checkdisplay(display);
	name = string2c(nameowner);
	font = font_open(disp, name);
	if(font == nil){
		if(strcmp(name, deffontname) == 0){
			df = disp->defaultsubfont;
			if(df == nil){
				drawcommitresult(slot, H, old);
				return;
			}
			snprint(buf, sizeof(buf), "%d %d\n0 %d\t%s\n",
				df->height, df->ascent, df->n-1, name);
			data = buf;
		}else{
			if(description == H){
				drawcommitresult(slot, H, old);
				return;
			}
			data = string2c(description);
		}
		locked = lockdisplay(disp);
		font = buildfont(disp, data, name);
		if(locked)
			unlockdisplay(disp);
		if(font != nil){
			c = cacheinstall(fcache, disp, name, font, "font");
			if(c != nil)
				c->ref++;
		}
	}
	result = drawownedfont(font, display, nameowner);
	drawcommitresult(slot, result, old);
}

Font*
font_open(Display *display, char *name)
{
	Cache *c;
	Font *font;
	int locked;

	c = cachelookup(fcache, display, name);
	if(c)
		font = c->u.f;
	else {
		locked = lockdisplay(display);
		font = openfont(display, name);
		if(locked)
			unlockdisplay(display);
		if(font == nil)
			return nil;
		c = cacheinstall(fcache, display, name, font, "font");
	}
	if(c)
		c->ref++;

	return font;
}

void
font_close(Font *f)
{
	Cache *c;
	Display *disp;
	int locked;
	disp = f->display;
	if(f->name == nil)
		return;

	/* fonts from Font_build() aren't always in fcache, but we still need to free them */
	c = cachelookup(fcache, disp, f->name);
	if(c != nil && f == c->u.f) {
		if(c->ref <= 0)
			return;
		if(c->ref-- != 1)
			return;
		cacheuninstall(fcache, disp, f->name, "font");
	}

	locked = lockdisplay(disp);
	freefont(f);
	if(locked)
		unlockdisplay(disp);
}

void
freecachedsubfont(Subfont *sf)
{
	Cache *c;
	Display *disp;

	disp = sf->bits->display;
	c = cachelookup(sfcache, disp, sf->name);
	if(c == nil){
		fprint(2, "subfont %s not cached\n", sf->name);
		return;
	}
	if(c->ref > 0)
		c->ref--;
	/* if ref is zero, we leave it around for later harvesting by freeallsubfonts */
}

void
freeallsubfonts(Display *d)
{
	int i;
	Cache *c, *prev, *o;
	Subfont *sf;
	int locked;
	if(cacheqlock == nil)	/* may not have allocated anything yet */
		return;
	libqlock(cacheqlock);
	for(i=0; i<BIHASH; i++){
		c = sfcache[i];
		prev = 0;
		while(c != nil){
			if(c->ref==0 && (d==nil || c->display==d)){
				if(prev == 0)
					sfcache[i] = c->next;
				else
					prev->next = c->next;
				free(c->name);
				sf = c->u.sf;
				if(--sf->ref==0){
					free(sf->info);
					locked = lockdisplay(c->display);
					freeimage(sf->bits);
					if(locked)
						unlockdisplay(c->display);
					free(sf);
				}
				o = c;
				c = c->next;
				free(o);
			}else{
				prev = c;
				c = c->next;
			}
		}
	}
	libqunlock(cacheqlock);
}

void
subfont_close(Subfont *sf)
{
	freecachedsubfont(sf);
}

void
freesubfont(Subfont *sf)
{
	freecachedsubfont(sf);
}


/* Keep all source owners alive until the native font and its managed wrapper are ready. */
void
Font_open(void *fp)
{
	Font *font;
	Display *disp;
	Draw_Display *display;
	Draw_Font *result;
	String *name;
	F_Font_open *f;
	void *old, *slot;

	f = fp;
	slot = f->ret;
	if(slot == nil || slot == H || (uintptr)slot > (uintptr)-1-sizeof(old))
		error(exHeap);
	memmove(&old, slot, sizeof(old));
	if(old == nil)
		error(exNilref);
	display = f->d;
	name = f->name;
	disp = checkdisplay(display);
	font = font_open(disp, string2c(name));
	result = drawownedfont(font, display, name);
	drawcommitresult(slot, result, old);
}

/* Measure the native representation directly without allocating a UTF-8 cache for Rune strings. */
void
Font_width(void *fp)
{
	F_Font_width *f;
	Font *font;
	DrawTextView text;
	WORD result, *ret;
	int locked;

	f = fp;
	ret = f->ret;
	if(ret == nil || ret == H || (uintptr)ret > (uintptr)-1-sizeof(result))
		error(exHeap);
	result = 0;
	drawtextview(f->str, &text);
	if(f->f != H && text.count != 0){
		font = checkfont(f->f);
		locked = lockdisplay(font->display);
		result = text.ascii != nil ? stringnwidth(font, text.ascii, text.count) :
			runestringnwidth(font, text.runes, text.count);
		if(locked)
			unlockdisplay(font->display);
	}
	memmove(ret, &result, sizeof(result));
}


/* Preserve the placeholder rectangle while supporting every caller byte alignment. */
void
Font_bbox(void *fp)
{
	F_Font_bbox *f;
	Draw_Rect result;

	/* place holder for the real thing */
	f = fp;
	memset(&result, 0, sizeof(result));
	memmove(f->ret, &result, sizeof(result));
}

/*
 * BUG: would be nice if this cached the whole font.
 * Instead only the subfonts are cached and the fonts are
 * freed when released.
 */
/* A partial font owner may have no native font or display lease; mapped fields still retire once. */
void
freedrawfont(Heap *h, int swept)
{
	DFont *df;
	Font *font;
	DRef *reference;

	df = H2D(DFont*, h);
	font = df->font;
	reference = df->dref;
	df->font = nil;
	df->dref = nil;
	if(font != nil)
		font_close(font);
	if(!swept)
		freeptrs(df, h->t);
	if(reference != nil)
		display_dec(reference);
}


/* Finish channel conversion and String allocation before replacing an aliased old result owner. */
void
Chans_text(void *fp)
{
	F_Chans_text *f;
	char buf[16];
	String *created;
	void *old, *slot;
	ulong descriptor;

	f = fp;
	slot = f->ret;
	if(slot == nil || slot == H || (uintptr)slot > (uintptr)-1-sizeof(old))
		error(exHeap);
	memmove(&old, slot, sizeof(old));
	if(old == nil)
		error(exNilref);
	descriptor = f->c.desc;
	created = H;
	if(chantostr(buf, descriptor) != nil)
		retstr(buf, &created);
	drawcommitresult(slot, created, old);
}


/* Return channel depth without assuming result alignment. */
void
Chans_depth(void *fp)
{
	WORD result;
	F_Chans_depth *f;

	f = fp;
	result = chantodepth(f->c.desc);
	memmove(f->ret, &result, sizeof(result));
}


/* Compare both channel inputs before writing a possibly aliased result. */
void
Chans_eq(void *fp)
{
	WORD result;
	F_Chans_eq *f;

	f = fp;
	result = f->c.desc == f->d.desc;
	memmove(f->ret, &result, sizeof(result));
}


/* Capture the source String and publish the complete channel value without requiring result alignment. */
void
Chans_mk(void *fp)
{
	F_Chans_mk *f;
	Draw_Chans result;
	String *source;
	void *slot;

	f = fp;
	slot = f->ret;
	if(slot == nil || slot == H || (uintptr)slot > (uintptr)-1-sizeof(result))
		error(exHeap);
	source = f->s;
	result.desc = strtochan(string2c(source));
	memmove(slot, &result, sizeof(result));
}


/* Capture colour bits and keep the display alive until publishing the newly owned image. */
void
Display_rgb(void *fp)
{
	ulong colorbits;
	Display *display;
	Draw_Display *source;
	Draw_Image *result;
	F_Display_rgb *f;
	void *old, *slot;
	int locked;

	f = fp;
	slot = f->ret;
	if(slot == nil || slot == H || (uintptr)slot > (uintptr)-1-sizeof(old))
		error(exHeap);
	memmove(&old, slot, sizeof(old));
	if(old == nil)
		error(exNilref);
	source = f->d;
	colorbits = ((ulong)(f->r&255)<<24)|((ulong)(f->g&255)<<16)|
		((ulong)(f->b&255)<<8)|0xFFUL;
	display = checkdisplay(source);
	locked = lockdisplay(display);
	result = color((DDisplay*)source, colorbits);
	if(locked)
		unlockdisplay(display);
	drawcommitresult(slot, result, old);
}


/* Map RGB before copying the complete VM scalar result. */
void
Display_rgb2cmap(void *fp)
{
	WORD result;
	F_Display_rgb2cmap *f;

	f = fp;
	/* f->display is unused, but someday may have color map */
	result = rgb2cmap(f->r, f->g, f->b);
	memmove(f->ret, &result, sizeof(result));
}


/* Finish the complete tuple before copying to an unaligned or overlapping result. */
void
Display_cmap2rgb(void *fp)
{
	WORD result[3];
	F_Display_cmap2rgb *f;
	ulong c;

	f = fp;
	/* f->display is unused, but someday may have color map */
	c = cmap2rgb(f->c);
	result[0] = (c>>16)&0xFF;
	result[1] = (c>>8)&0xFF;
	result[2] = (c>>0)&0xFF;
	memmove(f->ret, result, sizeof(result));
}


/* Copy packed colour bits without an aligned caller-address requirement. */
void
Display_cmap2rgba(void *fp)
{
	WORD result;
	F_Display_cmap2rgba *f;

	f = fp;
	/* f->display is unused, but someday may have color map */
	result = cmap2rgba(f->c);
	memmove(f->ret, &result, sizeof(result));
}


/* Complete alpha conversion before publishing the VM scalar. */
void
Draw_setalpha(void *fp)
{
	WORD result;
	F_Draw_setalpha *f;

	f = fp;
	result = setalpha(f->c, f->a);
	memmove(f->ret, &result, sizeof(result));
}


/* Finish the complete tuple before copying to an unaligned or overlapping result. */
void
Draw_icossin(void *fp)
{
	WORD result[2];
	F_Draw_icossin *f;
	int s, c;

	f = fp;
	icossin(f->deg, &s, &c);
	result[0] = s;
	result[1] = c;
	memmove(f->ret, result, sizeof(result));
}


/* Finish the complete tuple before copying to an unaligned or overlapping result. */
void
Draw_icossin2(void *fp)
{
	WORD result[2];
	F_Draw_icossin2 *f;
	int s, c;

	f = fp;
	icossin2(f->p.x, f->p.y, &s, &c);
	result[0] = s;
	result[1] = c;
	memmove(f->ret, result, sizeof(result));
}


/* Read the full rectangle before writing a possibly overlapping scalar. */
void
Draw_bytesperline(void *fp)
{
	WORD result;
	F_Draw_bytesperline *f;

	f = fp;
	result = bytesperline(IRECT(f->r), f->d);
	memmove(f->ret, &result, sizeof(result));
}

Draw_Image*
color(DDisplay *dd, ulong color)
{
	int c;
	Draw_Rect r;

	r.min.x = 0;
	r.min.y = 0;
	r.max.x = 1;
	r.max.y = 1;
	c = (color&0xff) == 0xff ? RGB24: RGBA32;
	return allocdrawimage(dd, r, c, nil, 1, color);
}

/* Construct mkdrawimage privately; native input ownership transfers only after managed allocation succeeds. */
Draw_Image*
mkdrawimage(Image *i, Draw_Screen *screen, Draw_Display *display, void *ref)
{
	Heap *h;
	DImage *di;

	if(i == nil || i->display == nil || i->display->limbo == nil)
		return H;
	h = drawheap(TImage);
	if(h == H)
		return H;

	di = H2D(DImage*, h);
	di->image = i;
	di->drawimage.screen = screen;
	if(screen != H)
		D2H(screen)->ref++;
	di->drawimage.display = display;
	if(display != H)
		D2H(display)->ref++;
	di->refreshptr = ref;

	R2R(di->drawimage.r, i->r);
	R2R(di->drawimage.clipr, i->clipr);
	di->drawimage.chans.desc = i->chan;
	di->drawimage.depth = i->depth;
	di->drawimage.repl = i->repl;
	di->flush = 1;
	di->dref = i->display->limbo;
	di->dref->ref++;
	return &di->drawimage;
}

/* Retain the source screen through window allocation/wrapping before replacing a possibly aliased result. */
void
Screen_newwindow(void *fp)
{
	F_Screen_newwindow *f;
	Draw_Screen *source;
	Draw_Image *result;
	Image *image;
	Screen *screen;
	Rectangle rectangle;
	void *old, *slot;
	int locked, backing;

	f = fp;
	slot = f->ret;
	if(slot == nil || slot == H || (uintptr)slot > (uintptr)-1-sizeof(old))
		error(exHeap);
	memmove(&old, slot, sizeof(old));
	if(old == nil)
		error(exNilref);
	source = f->screen;
	screen = checkscreen(source);
	R2R(rectangle, f->r);
	backing = f->backing;
	if(backing != Refnone && backing != Refbackup)
		backing = Refbackup;
	locked = lockdisplay(screen->display);
	image = allocwindow(screen, rectangle, backing, f->color);
	result = image == nil ? H : mkdrawimage(image, source, source->display, nil);
	if(image != nil && result == H)
		drawdiscardimage(image);
	if(locked)
		unlockdisplay(screen->display);
	drawcommitresult(slot, result, old);
}

/* Treat nil window arrays as empty and bound native pointer-vector allocation. */
static void
screentopbot(Draw_Screen *screen, Array *array, void (*topbot)(Image **, int))
{
	Screen *s;
	Draw_Image *entry;
	uchar *data;
	Image **ip;
	int i, n, locked;
	WORD length;

	s = checkscreen(screen);
	if(array == H || array == nil ||
	   (uintptr)array > (uintptr)-1 - (offsetof(Array, data)+sizeof(data)))
		return;
	memmove(&length, (uchar*)array+offsetof(Array, len), sizeof(length));
	if(length <= 0 || (uvlong)length > 0x7fffffff/sizeof(Image*))
		return;
	memmove(&data, (uchar*)array+offsetof(Array, data), sizeof(data));
	if(data == nil || data == H || (uintptr)data > (uintptr)-1-(uintptr)length*sizeof(entry))
		return;
	ip = malloc(length * sizeof(Image*));
	if(ip == nil)
		return;
	n = 0;
	for(i=0; i<length; i++){
		memmove(&entry, data+i*sizeof(entry), sizeof(entry));
		if(entry != H){
			ip[n] = entry == nil ? nil : lookupimage(entry);
			if(ip[n]==nil || ip[n]->screen != s){
				free(ip);
				return;
			}
			n++;
		}
	}
	if(n == 0){
		free(ip);
		return;
	}
	locked = lockdisplay(s->display);
	(*topbot)(ip, n);
	free(ip);
	flushimage(s->display, 1);
	if(locked)
		unlockdisplay(s->display);
}


/* Keep filtered caller order when placing windows at the top. */
void
Screen_top(void *fp)
{
	F_Screen_top *f;
	f = fp;
	screentopbot(f->screen, f->wins, topnwindows);
}


/* Keep filtered caller order using the generated bottom-operation frame. */
void
Screen_bottom(void *fp)
{
	F_Screen_bottom *f;
	f = fp;
	screentopbot(f->screen, f->wins, bottomnwindows);
}

/* Detach the native image before cleanup; release mapped owners once, unless the collector owns them. */
void
freedrawimage(Heap *h, int swept)
{
	DImage *di;
	Image *image;
	DRef *reference;
	Display *display;
	int locked;

	di = H2D(DImage*, h);
	image = di->image;
	reference = di->dref;
	di->image = nil;
	di->dref = nil;
	if(image != nil){
		display = image->display;
		locked = lockdisplay(display);
		freeimage(image);
		if(locked)
			unlockdisplay(display);
	}
	if(!swept)
		freeptrs(di, h->t);
	if(reference != nil)
		display_dec(reference);
}


/* Send the resolved image to the top while holding its display exclusion. */
void
Image_top(void *fp)
{
	F_Image_top *f;
	Image *i;
	int locked;

	f = fp;
	i = checkimage(f->win);
	locked = lockdisplay(i->display);
	topwindow(i);
	flushimage(i->display, 1);
	if(locked)
		unlockdisplay(i->display);
}


/* Capture points before native I/O and copy the final status after publishing successful geometry. */
void
Image_origin(void *fp)
{
	F_Image_origin *f;
	Draw_Image *source;
	Image *image;
	Point log, scr;
	WORD *slot, result;
	int locked;

	f = fp;
	slot = f->ret;
	if(slot == nil || slot == H || (uintptr)slot > (uintptr)-1-sizeof(result))
		error(exHeap);
	source = f->win;
	log = IPOINT(f->log);
	scr = IPOINT(f->scr);
	image = checkimage(source);
	locked = lockdisplay(image->display);
	result = -1;
	if(originwindow(image, log, scr) > 0){
		source->r = DRECT(image->r);
		source->clipr = DRECT(image->clipr);
		result = 1;
	}
	if(locked)
		unlockdisplay(image->display);
	memmove(slot, &result, sizeof(result));
}


/* Send the resolved image to the bottom using its own generated frame type. */
void
Image_bottom(void *fp)
{
	F_Image_bottom *f;
	Image *i;
	int locked;

	f = fp;
	i = checkimage(f->win);
	locked = lockdisplay(i->display);
	bottomwindow(i);
	flushimage(i->display, 1);
	if(locked)
		unlockdisplay(i->display);
}

/* Construct allocdrawimage privately; native input ownership transfers only after managed allocation succeeds. */
Draw_Image*
allocdrawimage(DDisplay *ddisplay, Draw_Rect r, ulong chan, Image *iimage, int repl, int color)
{
	Heap *h;
	DImage *di;
	Rectangle rr;
	Image *image;
	char reason[ERRMAX];

	if(ddisplay == nil || (void*)ddisplay == H || ddisplay->display == nil ||
	   ddisplay->display->limbo == nil || (iimage != nil && iimage->display != ddisplay->display))
		return H;
	image = iimage;
	if(iimage == nil){
		R2R(rr, r);
		image = allocimage(ddisplay->display, rr, chan, repl, color);
		if(image == nil)
			return H;
	}

	h = drawheap(TImage);
	if(h == H){
		if(iimage == nil){
			kgerrstr(reason, sizeof(reason));
			freeimage(image);
			kwerrstr("%s", reason);
		}
		return H;
	}

	di = H2D(DImage*, h);
	di->drawimage.r = r;
	R2R(di->drawimage.clipr, image->clipr);
	di->drawimage.chans.desc = chan;
	di->drawimage.depth = chantodepth(chan);
	di->drawimage.repl = repl;
	di->drawimage.display = (Draw_Display*)ddisplay;
	D2H(di->drawimage.display)->ref++;
	di->drawimage.screen = H;
	di->dref = ddisplay->display->limbo;
	di->dref->ref++;
	di->image = image;
	di->refreshptr = 0;
	di->flush = 1;

	return &di->drawimage;
}

/*
 * Entry points called from the draw library
 */
Subfont*
lookupsubfont(Display *d, char *name)
{
	Cache *c;

	c = cachelookup(sfcache, d, name);
	if(c == nil)
		return nil;
	/*c->u.sf->ref++;*/	/* TO DO: need to revisit the reference counting */
	return c->u.sf;
}

void
installsubfont(char *name, Subfont *subfont)
{
	Cache *c;

	c = cacheinstall(sfcache, subfont->bits->display, name, subfont, "subfont");
	if(c)
		c->ref++;
}


/* Build an owned full path and probe complete suffixes without fixed buffers or truncation. */
char*
subfontname(char *cfname, char *fname, int maxdepth)
{
	char *path, *slash;
	uvlong namesize, prefix, length;
	int i, fd;

	if(cfname == nil)
		return nil;
	if(strcmp(cfname, deffontname) == 0)
		return strdup(cfname);
	namesize = strlen(cfname);
	prefix = 0;
	slash = nil;
	length = namesize;
	if(cfname[0] != '/'){
		if(fname == nil)
			return nil;
		slash = strrchr(fname, '/');
		prefix = slash == nil ? 1 : (uvlong)(slash-fname);
		if(namesize >= 0x7ffffffcUL || prefix > 0x7ffffffbUL-namesize){
			kwerrstr("subfont path too long");
			return nil;
		}
		length += prefix+1;
	}
	if(length > 0x7ffffffcUL){
		kwerrstr("subfont path too long");
		return nil;
	}
	path = malloc((ulong)length+3);
	if(path == nil)
		return nil;
	if(cfname[0] == '/')
		memmove(path, cfname, namesize);
	else{
		if(slash == nil)
			path[0] = '.';
		else
			memmove(path, fname, prefix);
		path[prefix] = '/';
		memmove(path+prefix+1, cfname, namesize);
	}
	path[length] = '\0';
	if(maxdepth > 8)
		maxdepth = 8;
	for(i = 3; i >= 0; i--){
		if((1<<i) > maxdepth)
			continue;
		path[length] = '.';
		path[length+1] = '0'+i;
		path[length+2] = '\0';
		fd = libopen(path, OREAD);
		if(fd >= 0){
			libclose(fd);
			return path;
		}
	}
	path[length] = '\0';
	return path;
}


/* Accumulate complete wire records; the caller keeps Display and callback payloads alive. */
void
refreshslave(Display *d)
{
	int i, n, id, used, complete;
	uchar buf[5*(5*4)], *p;
	Rectangle r;
	Image *im;
	int locked;

	if(d->closing)
		return;
	used = 0;
	for(;;){
		release();
		n = kchanio(d->refchan, buf+used, sizeof(buf)-used, OREAD);
		acquire();
		if(n <= 0){
			if(n == 0 && used != 0)
				kwerrstr("truncated refresh record");
			return;
		}
		if(n > sizeof(buf)-used){
			kwerrstr("refresh: oversized response");
			return;
		}
		if(d->closing)
			return;
		used += n;
		complete = used-used%(5*4);
		if(complete == 0)
			continue;
		locked = lockdisplay(d);
		for(i=0; i<complete; i+=5*4){
			if(d->closing)
				break;
			p = buf+i;
			id = (int)BGLONG(p);
			r.min.x = (int)BGLONG(p+4);
			r.min.y = (int)BGLONG(p+8);
			r.max.x = (int)BGLONG(p+12);
			r.max.y = (int)BGLONG(p+16);
			for(im=d->windows; im; im=im->next)
				if(im->id == id)
					break;
			if(im && im->screen && im->reffn)
				(*im->reffn)(im, r, im->refptr);
		}
		n = flushimage(d, 1);
		if(locked)
			unlockdisplay(d);
		if(n < 0 || d->closing)
			return;
		used -= complete;
		memmove(buf, buf+complete, used);
	}
}

void
startrefresh(Display *disp)
{
	USED(disp);
}


/* Bound the embedded buffer numerically before pointer subtraction or append. */
static int
drawbuffered(Display *d, int limit)
{
	uintptr base, current;

	base = (uintptr)d->buf;
	current = (uintptr)d->bufp;
	if(current < base || current-base > (uintptr)limit){
		kwerrstr("invalid display buffer cursor");
		return -1;
	}
	return current-base;
}


/* Keep existing error filtering while reporting the exact accepted byte prefix. */
static void
drawflushfailed(int written, int total)
{
	char reason[ERRMAX];

	reason[0] = 0;
	kgerrstr(reason, sizeof(reason));
	reason[sizeof(reason)-1] = 0;
	if(_drawdebug || (strcmp(reason, "screen id in use") != 0 && strcmp(reason, exImage) != 0))
		print("flushimage fail: (%d of %d bytes sent): %s\n", written, total, reason);
}


/* Complete short writes without retrying bytes already accepted by the channel. */
static int
doflush(Display *d)
{
	int n, written, amount;

	n = drawbuffered(d, Displaybufsize+1);
	if(n < 0)
		return -1;
	if(n == 0)
		return 1;
	written = 0;
	while(written < n){
		if(d->local == 0)
			release();
		amount = kchanio(d->datachan, d->buf+written, n-written, OWRITE);
		if(d->local == 0)
			acquire();
		if(amount < 0){
			drawflushfailed(written, n);
			d->bufp = d->buf;
			return -1;
		}
		if(amount == 0 || amount > n-written){
			kwerrstr("incomplete display write");
			drawflushfailed(written, n);
			d->bufp = d->buf;
			return -1;
		}
		written += amount;
	}
	d->bufp = d->buf;
	return 1;
}


/* Detach and retire private entries before callbacks; failed writes retain the queue. */
int
flushimage(Display *d, int visible)
{
	int ret;
	Refreshq *r;
	Reffn callback;
	Rectangle rectangle;
	void *context;

	for(;;){
		if(visible){
			if(drawbuffered(d, Displaybufsize) < 0)
				return -1;
			*d->bufp++ = 'v';
		}
		ret = doflush(d);
		if(ret < 0 || d->refhead == nil || d->closing)
			return ret;
		while((r = d->refhead) != nil){
			d->refhead = r->next;
			if(d->refhead == nil)
				d->reftail = nil;
			callback = r->reffn;
			rectangle = r->r;
			context = r->refptr;
			free(r);
			callback(nil, rectangle, context);
		}
	}
}

/*
 * Turn off refresh for this window and remove any pending refresh events for it.
 */
void
delrefresh(Image *i)
{
	Refreshq *r, *prev, *next;
	int locked;
	Display *d;
	void *refptr;

	d = i->display;
	locked = lockdisplay(d);
	/*
	 * Any refresh function will do, because the data pointer is nil.
	 * Can't use nil, though, because that turns backing store back on.
	 */
	if(d->local)
		drawlsetrefresh(d->dataqid, i->id, memlnorefresh, nil);
	refptr = i->refptr;
	i->refptr = nil;
	i->reffn = nil;
	if(d->refhead == nil || refptr == nil){
		if(locked)
			unlockdisplay(d);
		return;
	}
	prev = nil;
	for(r=d->refhead; r; r=next){
		next = r->next;
		if(r->refptr == refptr){
			if(prev)
				prev->next = next;
			else
				d->refhead = next;
			if(d->reftail == r)
				d->reftail = prev;
			free(r);
		}else
			prev = r;
	}
	if(locked)
		unlockdisplay(d);
}


/* Caller holds display exclusion; initialize every private field before publication. */
void
queuerefresh(Image *i, Rectangle r, Reffn reffn, void *refptr)
{
	Display *d;
	Refreshq *rq;

	if(reffn == nil)
		return;
	d = i->display;
	if(d->closing)
		return;
	rq = malloc(sizeof(Refreshq));
	if(rq == nil)
		return;
	rq->next = nil;
	rq->reffn = reffn;
	rq->refptr = refptr;
	rq->r = r;
	if(d->reftail)
		d->reftail->next = rq;
	else
		d->refhead = rq;
	d->reftail = rq;
}


/* Reserve payload bytes without constructing an out-of-array comparison pointer. */
uchar*
bufimage(Display *d, int n)
{
	uchar *p;
	int used;

	if(n < 0 || n > Displaybufsize){
		kwerrstr("bad count in bufimage");
		return nil;
	}
	used = drawbuffered(d, Displaybufsize);
	if(used < 0)
		return nil;
	if(n > Displaybufsize-used){
		if(d->local == 0 && currun() != libqlowner(d->qlock)){
			kwerrstr("display buffer flush requires its owner");
			return nil;
		}
		if(doflush(d) < 0)
			return nil;
	}
	p = d->bufp;
	d->bufp += n;
	return p;
}

void
drawerror(Display *d, char *s)
{
	USED(d);
	fprint(2, "draw: %s: %r\n", s);
}
