/* Production image loading; Draw transport is the boundary stub. */
#include "lib9.h"
#include "draw.h"
#include "kernel.h"
#include <assert.h>
#include <stdio.h>
#ifdef REAL_DECODERS
#define NANOSVG_IMPLEMENTATION
#define NANOSVGRAST_IMPLEMENTATION
#endif
#include "svg/nanosvg.h"
#include "svg/nanosvgrast.h"
#include "png/lodepng.h"

static int live,fail_after=-1,locked,ownlock=1,alloc_fail,load_fail;
static int opened,closed,image_live;
static Image output;
static uchar loaded[65536];
static int loaded_bytes;
static char seen_path[4096];
char rootdir[4096]="root";
extern void hostfree(void*) __asm__("free");
extern void *hostmalloc(size_t) __asm__("malloc");
extern void *hostrealloc(void*,size_t) __asm__("realloc");
void *testmalloc(size_t n) {
    void *p;
    if(fail_after==0)return nil;
    if(fail_after>0)fail_after--;
    p=hostmalloc(n?n:1);if(p)live++;return p;
}
void *testrealloc(void *old,size_t n) {
    void *p;
    if(fail_after==0)return nil;
    if(fail_after>0)fail_after--;
    p=hostrealloc(old,n?n:1);if(p && !old)live++;return p;
}
void testfree(void *p) {if(p){assert(live>0);live--;hostfree(p);}}
int __lsan_is_turned_off(void) {return 1;}
int lockdisplay(Display *d) {(void)d;if(ownlock){assert(!locked);locked=1;}return ownlock;}
void unlockdisplay(Display *d) {(void)d;assert(locked);locked=0;}
Image *allocimage(Display *d,Rectangle r,ulong channel,int repl,ulong color) {
    (void)d;(void)repl;assert(locked && channel==ARGB32 && color==DTransparent);
    assert(!image_live);if(alloc_fail)return nil;
    image_live=1;output.r=r;return &output;
}
int loadimage(Image *i,Rectangle r,uchar *data,int bytes) {
    (void)i;(void)r;assert(locked && bytes>0 && bytes<=(int)sizeof(loaded));
    loaded_bytes=bytes;memcpy(loaded,data,bytes);return load_fail ? bytes-1 : bytes;
}
int freeimage(Image *i) {assert(i==&output && image_live);image_live=0;return 0;}
int libopen(char *name,int mode) {(void)mode;assert(name);opened++;return 3;}
int libclose(int fd) {assert(fd==3);closed++;return 0;}
Image *readimage(Display *d,int fd,int dolock) {
    (void)d;assert(fd==3 && dolock==1);image_live=1;return &output;
}
#ifndef REAL_DECODERS
static unsigned pngw=2,pngh=1,png_error;
static int png_nil,svg_nil,rast_nil,raster_calls;
static float svgw=2,svgh=1;
unsigned lodepng_decode32_file(unsigned char **out,unsigned *w,unsigned *h,const char *path) {
    unsigned n,j;
    assert(strlen(path)<sizeof(seen_path));strcpy(seen_path,path);
    *w=pngw;*h=pngh;
    n=pngw<=1000 && pngh<=1000 ? pngw*pngh*4 : 4;
    *out=png_nil ? nil : malloc(n?n:4);
    if(!*out)return 83;
    for(j=0;j+3<n;j+=4){(*out)[j]=200;(*out)[j+1]=100;(*out)[j+2]=50;(*out)[j+3]=128;}
    return png_error;
}
NSVGimage *nsvgParseFromFile(const char *path,const char *units,float dpi) {
    NSVGimage *s;
    assert(strcmp(units,"px")==0 && dpi==72.0f);
    assert(strlen(path)<sizeof(seen_path));strcpy(seen_path,path);
    s=svg_nil ? nil : malloc(sizeof(*s));
    if(s){s->width=svgw;s->height=svgh;}return s;
}
void nsvgDelete(NSVGimage *s) {free(s);}
NSVGrasterizer *nsvgCreateRasterizer(void) {return rast_nil ? nil : (NSVGrasterizer*)malloc(1);}
void nsvgDeleteRasterizer(NSVGrasterizer *r) {free(r);}
void nsvgRasterize(NSVGrasterizer *r,NSVGimage *s,float tx,float ty,float scale,
                  unsigned char *dst,int w,int h,int stride) {
    int j;
    (void)r;(void)s;assert(tx==0 && ty==0 && scale>0 && stride==w*4);raster_calls++;
    for(j=0;j<w*h*4;j+=4){dst[j]=200;dst[j+1]=100;dst[j+2]=50;dst[j+3]=128;}
}
#endif
DRAWIMAGE_IMPLEMENTATION
static void clean(Image *i) {if(i)freeimage(i);assert(live==0 && !locked && !image_live);}
int main(int argc,char **argv) {
    Display display={0};
    Image *i;
    int n;
    assert(display_open(&display,nil)==nil && display_open(&display,"")==nil);
    clean(display_open(&display,"x"));clean(display_open(&display,"ab"));clean(display_open(&display,"abc"));
    assert(opened==3 && closed==3);
    assert(!display_open(&display,":0:x.png"));assert(!display_open(&display,":-1:x.png"));
    assert(!display_open(&display,":2x:x.png"));assert(!display_open(&display,":2:"));
    assert(!display_open(&display,":2147483648:x.png"));assert(!display_open(&display,":1:x"));
    assert(!display_open(&display,":1x.png"));
    assert(!drawimagesize(0,1,&n) && !drawimagesize(1,0,&n));
    assert(!drawimagesize(0x40000000UL,2,&n) && !drawimagesize(2,0xffffffffUL,&n));
#ifdef REAL_DECODERS
    assert(argc==2);strcpy(rootdir,argv[1]);
    i=display_open(&display,"sample.PnG");assert(i && loaded_bytes==8);
    assert(loaded[0]==25 && loaded[1]==50 && loaded[2]==100 && loaded[3]==128);
    clean(i);
    i=display_open(&display,":1:sample.PnG");assert(i && loaded_bytes==4);clean(i);
    i=display_open(&display,"sample.SvG");assert(i && loaded_bytes==8);
    assert(loaded[0]==0 && loaded[1]==0 && loaded[2]==255 && loaded[3]==255);clean(i);
    i=display_open(&display,"alpha.svg");assert(i && loaded[0]==0 && loaded[1]==0);
    assert(loaded[3]>=126 && loaded[3]<=128 && loaded[2]==loaded[3]);clean(i);
    i=display_open(&display,"gradient.svg");assert(i && loaded_bytes==16);
    assert(loaded[2]>loaded[0] && loaded[12]>loaded[14] && loaded[3]==255 && loaded[15]==255);clean(i);
    clean(display_open(&display,"broken.png"));clean(display_open(&display,"broken.svg"));
    puts("PASS Draw images: actual PNG decoder and SVG parser/rasterizer, scaling and pixel channels");
#else
    (void)argc;(void)argv;
    i=display_open(&display,"sample.PNG");assert(i && loaded_bytes==8);
    assert(loaded[0]==25 && loaded[1]==50 && loaded[2]==100 && loaded[3]==128);clean(i);
    i=display_open(&display,"sample.SVG");assert(i && loaded_bytes==8 && loaded[0]==25 && loaded[2]==100);clean(i);
    pngw=1;pngh=1000;
    i=display_open(&display,":1:thin.png");assert(i && output.r.max.x==1 && output.r.max.y==1);clean(i);
    pngw=2;pngh=1;
    svgw=3;svgh=2;i=display_open(&display,":2:x.svg");assert(i && output.r.max.x==2 && output.r.max.y==1);clean(i);
    svgw=2;svgh=1;
    for(n=0;n<20;n++) {
        fail_after=n;i=display_open(&display,":3:x.png");fail_after=-1;clean(i);
        fail_after=n;i=display_open(&display,":3:x.svg");fail_after=-1;clean(i);
    }
    png_error=1;clean(display_open(&display,"x.png"));png_error=0;
    png_nil=1;clean(display_open(&display,"x.png"));png_nil=0;
    pngw=0;clean(display_open(&display,"x.png"));pngw=0x80000000UL;clean(display_open(&display,"x.png"));pngw=2;
    assert(!display_open(&display,":2147483647:x.png"));
    svgw=0;clean(display_open(&display,"x.svg"));svgw=-1;clean(display_open(&display,"x.svg"));
    svgw=0.0f/0.0f;clean(display_open(&display,"x.svg"));svgw=1.0f/0.0f;clean(display_open(&display,"x.svg"));
    svgw=2;svgh=1.0f/0.0f;clean(display_open(&display,"x.svg"));svgh=1;
    assert(!display_open(&display,":2147483647:x.svg"));
    svg_nil=1;clean(display_open(&display,"x.svg"));svg_nil=0;
    rast_nil=1;clean(display_open(&display,"x.svg"));rast_nil=0;
    alloc_fail=1;clean(display_open(&display,"x.png"));clean(display_open(&display,"x.svg"));alloc_fail=0;
    load_fail=1;clean(display_open(&display,"x.png"));clean(display_open(&display,"x.svg"));load_fail=0;
    ownlock=0;locked=1;i=display_open(&display,"x.png");assert(i && locked);locked=0;clean(i);ownlock=1;
    {
        char path[512];memset(path,'n',sizeof(path));strcpy(path+sizeof(path)-5,".png");
        i=display_open(&display,path);assert(i && strlen(seen_path)>500);clean(i);
    }
    assert(raster_calls>0);
    puts("PASS Draw images: short/invalid names, dimensions, resize bounds, long paths, allocation failures, transport failures and lock balance");
#endif
    return 0;
}
