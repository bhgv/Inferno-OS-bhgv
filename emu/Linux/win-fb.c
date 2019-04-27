/*
 * This implementation of the screen functions for X11 uses the
 * portable implementation of the Inferno drawing operations (libmemdraw)
 * to do the work, then has flushmemscreen copy the result to the X11 display.
 * Thus it potentially supports all colour depths but with a possible
 * performance penalty (although it tries to use the X11 shared memory extension
 * to copy the result to the screen, which might reduce the latter).
 *
 *       CraigN
 */

#define _GNU_SOURCE 1
//#define XTHREADS
#include "dat.h"
#include "fns.h"
#undef log2
#include <draw.h>
#include "cursor.h"
#include "keyboard.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <linux/fb.h>
#include <sys/mman.h>
#include <stdio.h>

#include <linux/input.h>

//#include "keysym2ucs.h"

#include <sys/ipc.h>
//#include <sys/shm.h>

#define  LOG_TAG    "Inferno WIN"
#define  LOGI(...)  //printf(__VA_ARGS__)
#define  LOGW(...)  //printf(__VA_ARGS__)
#define  LOGE(...)  //printf(__VA_ARGS__)



static int displaydepth = 32;
static ulong displaychan = CHAN4(CAlpha, 8, CRed, 8, CGreen, 8, CBlue, 8);

enum
{
    DblTime = 300       /* double click time in msec */
};

/* screen data .... */
static uchar*   gscreendata = NULL;
static uchar*   xscreendata = NULL;

//typedef unsigned int XColor;

uchar   map7to8[128][2];

/* for copy/paste, lifted from plan9ports via drawterm */
//static Atom clipboard;
//static Atom utf8string;
//static Atom targets;
//static Atom text;
//static Atom compoundtext;

//static Atom cursorchange;

static int      triedscreen = 0;

void        xexpose(); //XEvent*);
static void     xmouse(); //XEvent*);
static void     xkeyboard(); ///*XEvent*/void*);
//static void       xsetcursor(/*XEvent*/void*);
static void     xkbdproc(void*);
//static void       xdestroy(XEvent*);
//static void       xselect(XEvent*, XDisplay*);
static void     xproc_expose(void*);
static void     xproc_mouse(void*);

static int      xscreendepth;

static int putsnarf, assertsnarf;
//char *gkscanid = "emu_x11";


extern int Xsize;
extern int Ysize;

int fd_mou = -1;

/*
 * The documentation for the XSHM extension implies that if the server
 * supports XSHM but is not the local machine, the XShm calls will
 * return False; but this turns out not to be the case.  Instead, the
 * server throws a BadAccess error.  So, we need to catch X errors
 * around all of our XSHM calls, sigh.
 */
//static int shm_got_x_error = 0;
//static XErrorHandler old_handler = 0;
//static XErrorHandler old_io_handler = 0;



struct {
    int fd;
    uchar* base;
    uchar* data;

    int width;
    int height;
    int bpp;
    int stride;

    int alloc;
} fb;



static uchar*
attach_fb()
{
  struct fb_var_screeninfo fb_var;
  struct fb_fix_screeninfo fb_fix;
  int                      off;

  fb.alloc = 0;
  fb.fd = -1;

  if ((fb.fd = open ("/dev/fb0", O_RDWR)) < 0)
    {
      LOGE ("Error opening /dev/fb0");
      return NULL;
    }

  if (ioctl (fb.fd, FBIOGET_VSCREENINFO, &fb_var) == -1)
    {
      LOGE ("Error getting variable framebuffer info");
      return NULL;
    }

  if (fb_var.bits_per_pixel < 16)
    {
        fprintf(stderr,
              "Error, no support currently for %i bpp frame buffers\n"
              "Trying to change pixel format...\n",
              fb_var.bits_per_pixel);
        return NULL;
    }

  if (ioctl (fb.fd, FBIOGET_VSCREENINFO, &fb_var) == -1)
    {
      LOGE ("Error getting variable framebuffer info (2)");
      return NULL;
    }

  /* NB: It looks like the fbdev concept of fixed vs variable screen info is
   * broken. The line_length is part of the fixed info but it can be changed
   * if you set a new pixel format. */
  if (ioctl (fb.fd, FBIOGET_FSCREENINFO, &fb_fix) == -1)
    {
      LOGE ("Error getting fixed framebuffer info");
      return NULL;
    }
  /*fb.real_width  =*/ Xsize = fb.width  = fb_var.xres;
  /*fb.real_height =*/ Ysize = fb.height = fb_var.yres;

  fb.bpp    = fb_var.bits_per_pixel;
  fb.stride = fb_fix.line_length;

/*
  fb.type   = fb_fix.type;
  fb.visual = fb_fix.visual;

  fb.red_offset = fb_var.red.offset;
  fb.red_length = fb_var.red.length;
  fb.green_offset = fb_var.green.offset;
  fb.green_length = fb_var.green.length;
  fb.blue_offset = fb_var.blue.offset;
  fb.blue_length = fb_var.blue.length;

  if (fb.red_offset == 11 && fb.red_length == 5 &&
      fb.green_offset == 5 && fb.green_length == 6 &&
      fb.blue_offset == 0 && fb.blue_length == 5) {
         fb.rgbmode = RGB565;
  } else if (fb.red_offset == 0 && fb.red_length == 5 &&
      fb.green_offset == 5 && fb.green_length == 6 &&
      fb.blue_offset == 11 && fb.blue_length == 5) {
         fb.rgbmode = BGR565;
  } else if (fb.red_offset == 16 && fb.red_length == 8 &&
      fb.green_offset == 8 && fb.green_length == 8 &&
      fb.blue_offset == 0 && fb.blue_length == 8) {
         fb.rgbmode = fb.bpp == 32 ? ARGB888 : RGB888;
  } else if (fb.red_offset == 0 && fb.red_length == 8 &&
      fb.green_offset == 8 && fb.green_length == 8 &&
      fb.blue_offset == 8 && fb.blue_length == 8) {
         fb.rgbmode = fb.bpp == 32 ? ABGR888 : BGR888;
  } else {
         fb.rgbmode = GENERIC;
  }
*/
  LOGI("width: %i, height: %i, bpp: %i, stride: %i\n",
      fb.width, fb.height, fb.bpp, fb.stride);

  size_t size = fb.stride * fb.height;

  fb.base = (uchar*) mmap ((caddr_t) NULL,
                /*fb_fix.smem_len */
                size,
                PROT_READ|PROT_WRITE,
                MAP_SHARED,
                fb.fd, 0);

  if (fb.base == (uchar*)-1)
    {
      LOGE("Error cannot mmap framebuffer. Using malloc instead.\n");
      fb.base = (char*)malloc(size);
      if (!fb.base)
        {
          LOGE("Error cannot allocate memory.");
          return NULL;
        }
      fb.alloc = 1;
    }

  off = (unsigned long) fb_fix.smem_start % (unsigned long) getpagesize();

  fb.data = fb.base + off;

  return fb.data;
}


uchar*
attachscreen(Rectangle *r, ulong *chan, int *d, int *width, int *softscreen)
{
    int depth;
LOGE("attachscreen\n");
    
    if(triedscreen)
        return gscreendata;

    // stop cursor blinking
    int f = open("/dev/tty1", O_WRONLY);
    if(f >= 0){
        char s[]="\033[?17;0;0c";
        write(f, s, strlen(s));
        close(f);
    }

    // prepare mouse
    if(fd_mou == -1){
        fd_mou = open("/dev/input/event1", O_RDWR);
    }

    triedscreen = 1;

    //if(gscreendata != xscreendata)
    kproc("xproc_expose", xproc_expose, NULL/*xmcon*/, 0);
    kproc("xproc_mouse", xproc_mouse, NULL/*xmcon*/, 0);
//    kproc("xkbdproc", xkbdproc, NULL/*xkbdcon*/, 0/*KPX11*/);   /* silly stack size for bloated X11 */

    xscreendata = attach_fb(); //malloc(Xsize * Ysize * 4);
    xscreendepth = fb.bpp; //32;

    Xsize &= ~0x3;  /* ensure multiple of 4 */

    r->min.x = 0;
    r->min.y = 0;
    r->max.x = Xsize;
    r->max.y = Ysize;

        /*
         * moved xproc from here to end since it could cause an expose event and
         * hence a flushmemscreen before xscreendata is initialized
         */

    *chan = displaychan;        /* not every channel description will work */
    *d = chantodepth(displaychan);
    displaydepth = *d;

    printf("%s:%d displaychan=%X, displaydepth=%d\n", __func__, __LINE__, displaychan, displaydepth);

//      gscreendata = xscreendata;
    gscreendata = malloc(Xsize * (Ysize+1) * (displaydepth >> 3));

    LOGE("attachscreen: gscreendata=%x, (displaydepth>>3)=%d\n", gscreendata, (displaydepth>>3));

    *width = (Xsize/4)*(*d/8);
    *softscreen = 1;

    return gscreendata;
}


static void
copy32to32(Rectangle r)
{
    int dx, width;
    uchar *p, *ep, *cp;
    u32int v, w, *dp, *wp, *edp, *lp;

LOGE("copy32to32");
    width = Dx(r);
    dx = Xsize - width;
    dp = (u32int*)(gscreendata + (r.min.y * Xsize + r.min.x) * 4);
    wp = (u32int*)(xscreendata + (r.min.y * Xsize + r.min.x) * 4);
    edp = (u32int*)(gscreendata + (r.max.y * Xsize + r.max.x) * 4);
    while(dp < edp) {
        lp = dp + width;
        while(dp < lp){
            v = *dp++;
            //w = v
//                  infernortox11[ (v >> 16) & 0xff ] << 16
//                  | infernogtox11[ (v >> 8) & 0xff ] << 8
//                  | infernobtox11[ (v >> 0) & 0xff ] << 0
            //;
            *wp++ = v; //w;
        }
        dp += dx;
        wp += dx;
    }
}

static void
copy8to32(Rectangle r)
{
    int dx, width;
    uchar *p, *ep, *lp;
    u32int *wp;

LOGE("copy8to32");
    width = Dx(r);
    dx = Xsize - width;
    p = gscreendata + r.min.y * Xsize + r.min.x;
    wp = (u32int *)(xscreendata + (r.min.y * Xsize + r.min.x) * 4);
    ep = gscreendata + r.max.y * Xsize + r.max.x;
    while(p < ep) {
        lp = p + width;
        while(p < lp)
            *wp++ = *p++; //infernotox11[*p++];
        p += dx;
        wp += dx;
    }
}

static void
copy8to24(Rectangle r)
{
    int dx, width, v;
    uchar *p, *cp, *ep, *lp;

LOGE("copy8to24");
    width = Dx(r);
    dx = Xsize - width;
    p = gscreendata + r.min.y * Xsize + r.min.x;
    cp = xscreendata + (r.min.y * Xsize + r.min.x) * 3;
    ep = gscreendata + r.max.y * Xsize + r.max.x;
    while(p < ep) {
        lp = p + width;
        while(p < lp){
            v = *p++; //infernotox11[*p++];
            cp[0] = (v>>16)&0xff;
            cp[1] = (v>>8)&0xff;
            cp[2] = (v>>0)&0xff;
            cp += 3;
        }
        p += dx;
        cp += 3*dx;
    }
}

static void
copy8to16(Rectangle r)
{
    int dx, width;
    uchar *p, *ep, *lp;
    u16int *sp;

LOGE("copy8to16");
    width = Dx(r);
    dx = Xsize - width;
    p = gscreendata + r.min.y * Xsize + r.min.x;
    sp = (unsigned short *)(xscreendata + (r.min.y * Xsize + r.min.x) * 2);
    ep = gscreendata + r.max.y * Xsize + r.max.x;
    while(p < ep) {
        lp = p + width;
        while(p < lp)
            *sp++ = *p++; //infernotox11[*p++];
        p += dx;
        sp += dx;
    }
}

static void
copy8to8(Rectangle r)
{
    int dx, width;
    uchar *p, *cp, *ep, *lp;

LOGE("copy8to8");
    width = Dx(r);
    dx = Xsize - width;
    p = gscreendata + r.min.y * Xsize + r.min.x;
    cp = xscreendata + r.min.y * Xsize + r.min.x;
    ep = gscreendata + r.max.y * Xsize + r.max.x;
    while(p < ep) {
        lp = p + width;
        while(p < lp)
            *cp++ = *p++; //infernotox11[*p++];
        p += dx;
        cp += dx;
    }
}

static void
copy8topixel(Rectangle r)
{
    int x, y;
    uchar *p;

LOGE("copy8topixel");
    if(xscreendata == NULL)
        return;

    /* mainly for 4-bit greyscale */
    for (y = r.min.y; y < r.max.y; y++) {
        x = r.min.x;
        p = gscreendata + y * Xsize + x;
        while (x < r.max.x){
            xscreendata[y * Xsize * 4 + x] = *p++;
            x++;
//          XPutPixel(img, x++, y, infernotox11[*p++]);
        }
    }
}


//extern void int_refresh(int xb_, int yb_, int xe_, int ye_);

void
flushmemscreen(Rectangle r)
{
    char chanbuf[16];

    // Clip to screen
    if(r.min.x < 0)
        r.min.x = 0;
    if(r.min.y < 0)
        r.min.y = 0;
    if(r.max.x >= Xsize)
        r.max.x = Xsize - 1;
    if(r.max.y >= Ysize)
        r.max.y = Ysize - 1;

    if(r.max.x <= r.min.x || r.max.y <= r.min.y)
        return;

    if(xscreendata == NULL)
        return;

    LOGE("flushmemscreen: rect=(%d, %d)-(%d, %d), xscreendata=%x, displaydepth=%d, xscreendepth=%d\n",
            r.min.x, r.min.y, r.max.x, r.max.y, xscreendata, displaydepth, xscreendepth
            );

#if 1
    if(gscreendata != xscreendata)
    switch(displaydepth){
    case 32:
        copy32to32(r);
        break;
    case 8:
        switch(xscreendepth){
        case 24:
            /* copy8to24(r); */ /* doesn't happen? */
            /* break */
        case 32:
            copy8to32(r);
            break;
        case 16:
            copy8to16(r);
            break;
        case 8:
            copy8to8(r);
            break;
        default:
            copy8topixel(r);
            break;
        }
        break;
    default:
        fprint(2, "emu: bad display depth %d chan %s xscreendepth %d\n", displaydepth,
            chantostr(chanbuf, displaychan), xscreendepth);
        cleanexit(0);
    }
#endif

//    int_refresh(r.min.x, r.min.y, r.max.x, r.max.y);
}

static int
revbyte(int b)
{
    int r;

    r = 0;
    r |= (b&0x01) << 7;
    r |= (b&0x02) << 5;
    r |= (b&0x04) << 3;
    r |= (b&0x08) << 1;
    r |= (b&0x10) >> 1;
    r |= (b&0x20) >> 3;
    r |= (b&0x40) >> 5;
    r |= (b&0x80) >> 7;
    return r;
}

void
setpointer(int x, int y)
{
/*
    drawqlock();
    XLockDisplay(xdisplay);
    XWarpPointer(xdisplay, None, xdrawable, 0, 0, 0, 0, x, y);
    XFlush(xdisplay);
    XUnlockDisplay(xdisplay);
    drawqunlock();
*/
}

static void
xkbdproc(void *arg)
{
//  XEvent event;
//  XDisplay *xd;

//  xd = arg;

    /* BEWARE: the value of up is not defined for this proc on some systems */

//  XLockDisplay(xd);   /* should be ours alone */
//  XSelectInput(xd, xdrawable, KeyPressMask | KeyReleaseMask);
    for(;;){
//      XNextEvent(xd, &event);
        xkeyboard(); //&event);
//      xsetcursor(); //&event);
    }
}

static void
xproc_expose(void *arg)
{
//  ulong mask;
//  XEvent event;
//  XDisplay *xd;

    closepgrp(up->env->pgrp);
    closefgrp(up->env->fgrp);
    closeegrp(up->env->egrp);
    closesigs(up->env->sigs);

#if 0
    xd = arg;
    mask = ButtonPressMask|
        ButtonReleaseMask|
        PointerMotionMask|
        Button1MotionMask|
        Button2MotionMask|
        Button3MotionMask|
        Button4MotionMask|
        Button5MotionMask|
        ExposureMask|
        StructureNotifyMask;

    XLockDisplay(xd);   /* should be ours alone */
    XSelectInput(xd, xdrawable, mask);
#endif
    for(;;){
        osmillisleep(10);
//      XNextEvent(xd, &event);
//      xselect(&event, xd);
        //xmouse(); //&event);
        xexpose(); //&event);
//      xdestroy(&event);
    }
}

static void
xproc_mouse(void *arg)
{
//  ulong mask;
//  XEvent event;
//  XDisplay *xd;

    closepgrp(up->env->pgrp);
    closefgrp(up->env->fgrp);
    closeegrp(up->env->egrp);
    closesigs(up->env->sigs);

#if 0
    xd = arg;
    mask = ButtonPressMask|
        ButtonReleaseMask|
        PointerMotionMask|
        Button1MotionMask|
        Button2MotionMask|
        Button3MotionMask|
        Button4MotionMask|
        Button5MotionMask|
        ExposureMask|
        StructureNotifyMask;

    XLockDisplay(xd);   /* should be ours alone */
    XSelectInput(xd, xdrawable, mask);
#endif
    for(;;){
        osmillisleep(100);
//      XNextEvent(xd, &event);
//      xselect(&event, xd);
        xmouse(); //&event);
        //xexpose(); //&event);
//      xdestroy(&event);
    }
}

/*
 * this crud is here because X11 can put huge amount of data
 * on the stack during keyboard translation and cursor changing(!).
 * we do both in a dedicated process with lots of stack, perhaps even enough.
 */

enum {
    CursorSize= 32  /* biggest cursor size */
};

typedef struct ICursor ICursor;
struct ICursor {
    int inuse;
    int modify;
    int hotx;
    int hoty;
    int w;
    int h;
    uchar   src[(CursorSize/8)*CursorSize]; /* image and mask bitmaps */
    uchar   mask[(CursorSize/8)*CursorSize];
};
static ICursor icursor;

static void
xcurslock(void)
{
    while(_tas(&icursor.inuse) != 0)
        osyield();
}

static void
xcursunlock(void)
{
    coherence();
    icursor.inuse = 0;
}

#if 0
static void
xcursnotify(void)
{
    XClientMessageEvent e;

    memset(&e, 0, sizeof e);
    e.type = ClientMessage;
    e.window = xdrawable;
    e.message_type = cursorchange;
    e.format = 8;
    XLockDisplay(xdisplay);
    XSendEvent(xdisplay, xdrawable, True, KeyPressMask, (XEvent*)&e);
    XFlush(xdisplay);
    XUnlockDisplay(xdisplay);
}
#endif

void
drawcursor(Drawcursor* c)
{
#if 0
    uchar *bs, *bc, *ps, *pm;
    int i, j, w, h, bpl;

    if(c->data == nil){
        drawqlock();
        if(icursor.h != 0){
            xcurslock();
            icursor.h = 0;
            icursor.modify = 1;
            xcursunlock();
        }
        xcursnotify();
        drawqunlock();
        return;
    }

    drawqlock();
    xcurslock();
    icursor.modify = 0; /* xsetcursor will now ignore it */
    xcursunlock();

    h = (c->maxy-c->miny)/2;    /* image, then mask */
    if(h > CursorSize)
        h = CursorSize;
    bpl = bytesperline(Rect(c->minx, c->miny, c->maxx, c->maxy), 1);
    w = bpl;
    if(w > CursorSize/8)
        w = CursorSize/8;

    ps = icursor.src;
    pm = icursor.mask;
    bc = c->data;
    bs = c->data + h*bpl;
    for(i = 0; i < h; i++){
        for(j = 0; j < bpl && j < w; j++) {
            *ps++ = revbyte(bs[j]);
            *pm++ = revbyte(bs[j] | bc[j]);
        }
        bs += bpl;
        bc += bpl;
    }
    icursor.h = h;
    icursor.w = w*8;
    icursor.hotx = c->hotx;
    icursor.hoty = c->hoty;
    icursor.modify = 1;
    xcursnotify();
    drawqunlock();
#endif
}

#if 0
static void
xsetcursor(XEvent *e)
{
    ICursor ic;
    XCursor xc;
    XColor fg, bg;
    Pixmap xsrc, xmask;
    static XCursor xcursor;

    if(e->type != ClientMessage || !e->xclient.send_event || e->xclient.message_type != cursorchange)
        return;

    xcurslock();
    if(icursor.modify == 0){
        xcursunlock();
        return;
    }
    icursor.modify = 0;
    if(icursor.h == 0){
        xcursunlock();
        /* set the default system cursor */
        if(xcursor != 0) {
            XFreeCursor(xkbdcon, xcursor);
            xcursor = 0;
        }
        XUndefineCursor(xkbdcon, xdrawable);
        XFlush(xkbdcon);
        return;
    }
    ic = icursor;
    xcursunlock();

    xsrc = XCreateBitmapFromData(xkbdcon, xdrawable, (char*)ic.src, ic.w, ic.h);
    xmask = XCreateBitmapFromData(xkbdcon, xdrawable, (char*)ic.mask, ic.w, ic.h);

    fg = map[0];
    bg = map[255];
    fg.pixel = infernotox11[0];
    bg.pixel = infernotox11[255];
    xc = XCreatePixmapCursor(xkbdcon, xsrc, xmask, &fg, &bg, -ic.hotx, -ic.hoty);
    if(xc != 0) {
        XDefineCursor(xkbdcon, xdrawable, xc);
        if(xcursor != 0)
            XFreeCursor(xkbdcon, xcursor);
        xcursor = xc;
    }
    XFreePixmap(xkbdcon, xsrc);
    XFreePixmap(xkbdcon, xmask);
    XFlush(xkbdcon);
}
#endif

typedef struct Mg Mg;
struct Mg
{
    int code;
    int bit;
    int len;
    ulong   mask;
};

static int
maskx(Mg* g, int code, ulong mask)
{
    int i;

    for(i=0; i<32; i++)
        if(mask & (1<<i))
            break;
    if(i == 32)
        return 0;
    g->code = code;
    g->bit = i;
    g->mask = mask;
    for(g->len = 0; i<32 && (mask & (1<<i))!=0; i++)
        g->len++;
    return 1;
}

/*
 * for a given depth, we need to check the available formats
 * to find how many actual bits are used per pixel.
 */
#if 0
static int
xactualdepth(int screenno, int depth)
{
    XPixmapFormatValues *pfmt;
    int i, n;

    pfmt = XListPixmapFormats(xdisplay, &n);
    for(i=0; i<n; i++)
        if(pfmt[i].depth == depth)
            return pfmt[i].bits_per_pixel;
    return -1;
}

static int
xtruevisual(int screenno, int reqdepth, XVisualInfo *vi, ulong *chan)
{
    XVisual *xv;
    Mg r, g, b;
    int pad, d;
    ulong c;
    char buf[30];

    if(XMatchVisualInfo(xdisplay, screenno, reqdepth, TrueColor, vi) ||
       XMatchVisualInfo(xdisplay, screenno, reqdepth, DirectColor, vi)){
        xv = vi->visual;
        if(maskx(&r, CRed, xv->red_mask) &&
           maskx(&g, CGreen, xv->green_mask) &&
           maskx(&b, CBlue, xv->blue_mask)){
            d = xactualdepth(screenno, reqdepth);
            if(d < 0)
                return 0;
            pad = d - (r.len + g.len + b.len);
            if(0){
                fprint(2, "r: %8.8lux %d %d\ng: %8.8lux %d %d\nb: %8.8lux %d %d\n",
                 xv->red_mask, r.bit, r.len, xv->green_mask, g.bit, g.len, xv->blue_mask, b.bit, b.len);
            }
            if(r.bit > b.bit)
                c = CHAN3(CRed, r.len, CGreen, g.len, CBlue, b.len);
            else
                c = CHAN3(CBlue, b.len, CGreen, g.len, CRed, r.len);
            if(pad > 0)
                c |= CHAN1(CIgnore, pad) << 24;
            *chan = c;
            xscreendepth = reqdepth;
            if(0)
                fprint(2, "chan=%s reqdepth=%d bits=%d\n", chantostr(buf, c), reqdepth, d);
            return 1;
        }
    }
    return 0;
}

static int
xmapvisual(int screenno, XVisualInfo *vi, ulong *chan)
{
    if(XMatchVisualInfo(xdisplay, screenno, 8, PseudoColor, vi) ||
       XMatchVisualInfo(xdisplay, screenno, 8, StaticColor, vi)){
        *chan = CMAP8;
        xscreendepth = 8;
        return 1;
    }
    return 0;
}
#endif


/*
 * Initialize and install the Inferno colormap as a private colormap for this
 * application.  Inferno gets the best colors here when it has the cursor focus.
 */
#if 0
static void
xdestroy(XEvent *e)
{
    XDestroyWindowEvent *xe;
    if(e->type != DestroyNotify)
        return;
    xe = (XDestroyWindowEvent*)e;
    if(xe->window == xdrawable)
        cleanexit(0);
}
#endif

void
xexpose() //XEvent *e)
{
    static int cntr = 0;
    Rectangle r;
//  XExposeEvent *xe;

//  cntr++;
//  if(cntr == 100000)
//      cntr = 0;

//  if(cntr != 1) // e->type != Expose)
//      return;
//  xe = (XExposeEvent*)e;
    r.min.x = 0; //xe->x;
    r.min.y = 0; //xe->y;
    r.max.x = Xsize; //xe->x + xe->width;
    r.max.y = Ysize; //xe->y + xe->height;

LOGE("xexpose");
    drawqlock();
    flushmemscreen(r);
    drawqunlock();
}

static void
xkeyboard() ///*XEvent*/void *e)
{
    int ind, md;
//  KeySym k;
    int k;

//  if(gkscanq != nil && (e->type == KeyPress || e->type == KeyRelease)){
//      uchar ch = e->xkey.keycode;
//      if(e->xany.type == KeyRelease)
//          ch |= 0x80;
//      qproduce(gkscanq, &ch, 1);
//      return;
//  }

#if 0
    /*
     * I tried using XtGetActionKeysym, but it didn't seem to
     * do case conversion properly
     * (at least, with Xterminal servers and R4 intrinsics)
     */
    if(e->xany.type != KeyPress)
        return;

    md = e->xkey.state;
    ind = 0;
    if(md & ShiftMask)
        ind = 1;
    if(0){
        k = XKeycodeToKeysym(e->xany.display, (KeyCode)e->xkey.keycode, ind);

        /* May have to try unshifted version */
        if(k == NoSymbol && ind == 1)
            k = XKeycodeToKeysym(e->xany.display, (KeyCode)e->xkey.keycode, 0);
    }else
        XLookupString((XKeyEvent*)e, NULL, 0, &k, NULL);

    if(k == XK_Multi_key || k == NoSymbol)
        return;
    if(k&0xFF00){
        switch(k){
        case XK_BackSpace:
        case XK_Tab:
        case XK_Escape:
        case XK_Delete:
        case XK_KP_0:
        case XK_KP_1:
        case XK_KP_2:
        case XK_KP_3:
        case XK_KP_4:
        case XK_KP_5:
        case XK_KP_6:
        case XK_KP_7:
        case XK_KP_8:
        case XK_KP_9:
        case XK_KP_Divide:
        case XK_KP_Multiply:
        case XK_KP_Subtract:
        case XK_KP_Add:
        case XK_KP_Decimal:
            k &= 0x7F;
            break;
        case XK_Linefeed:
            k = '\r';
            break;
        case XK_KP_Space:
            k = ' ';
            break;
        case XK_Home:
        case XK_KP_Home:
            k = Home;
            break;
        case XK_Left:
        case XK_KP_Left:
            k = Left;
            break;
        case XK_Up:
        case XK_KP_Up:
            k = Up;
            break;
        case XK_Down:
        case XK_KP_Down:
            k = Down;
            break;
        case XK_Right:
        case XK_KP_Right:
            k = Right;
            break;
        case XK_Page_Down:
        case XK_KP_Page_Down:
            k = Pgdown;
            break;
        case XK_End:
        case XK_KP_End:
            k = End;
            break;
        case XK_Page_Up:
        case XK_KP_Page_Up:
            k = Pgup;
            break;
        case XK_Insert:
        case XK_KP_Insert:
            k = Ins;
            break;
        case XK_KP_Enter:
        case XK_Return:
            k = '\n';
            break;
        case XK_Alt_L:
        case XK_Alt_R:
            k = Latin;
            break;
        case XK_Shift_L:
        case XK_Shift_R:
        case XK_Control_L:
        case XK_Control_R:
        case XK_Caps_Lock:
        case XK_Shift_Lock:

        case XK_Meta_L:
        case XK_Meta_R:
        case XK_Super_L:
        case XK_Super_R:
        case XK_Hyper_L:
        case XK_Hyper_R:
            return;
        default:                /* not ISO-1 or tty control */
            if(k>0xff){
                k = keysym2ucs(k); /* supplied by X */
                if(k == -1)
                    return;
            }
            break;
        }
    }

    /* Compensate for servers that call a minus a hyphen */
    if(k == XK_hyphen)
        k = XK_minus;
    /* Do control mapping ourselves if translator doesn't */
    if(md & ControlMask)
        k &= 0x9f;
#endif
/*
    if(0){
        if(k == '\t' && ind)
            k = BackTab;

        if(md & Mod1Mask)
            k = APP|(k&0xff);
    }
    if(k == NoSymbol)
        return;

*/
        gkbdputc(gkbdq, k);
}


#if TOUCHSCREEN_CAPACITIVE

typedef struct {
    int x;
    int y;
    int b;
} touch_evt;

static touch_evt touch_events[10] = {0};
static int touch_btns = 0;

static int capacitive_events(int fd, int* b, int* x, int* y)
{
    struct input_event ev[64];
    int i, j, rd;
    fd_set rdfs;
    struct timeval to;
    
    int is_changed = 0;
    
//    static touch_evt touch_event[10] = {0};
    static int cur_event_n = 0;

    FD_ZERO(&rdfs);
    FD_SET(fd, &rdfs);

    *b = touch_btns;
    
    *x = touch_events[cur_event_n].x;
    *y = touch_events[cur_event_n].y;

    int rv = select(fd + 1, &rdfs, NULL, NULL, &to);
    if(rv == -1){
        /* an error accured */
        return 1;
    }
    else if(rv == 0){
        //printf("timeout\n");
        /* a timeout occured */
        //break;
        return 1;
    }

    rd = read(fd, ev, sizeof(ev));
    if (rd < (int) sizeof(struct input_event)) {
        //printf("expected %d bytes, got %d\n", (int) sizeof(struct input_event), rd);
        //perror("\nevtest: error reading");
        return 1;
    }
    
    for (i = 0; i < rd / sizeof(struct input_event); i++) {
        unsigned int type, code;
        type = ev[i].type;
        code = ev[i].code;

        if(type == EV_ABS){
            int v = ev[i].value;
            
            switch(code){
                case ABS_MT_SLOT:
                    cur_event_n = v;
                    //if(cur_event_n >= 0 && cur_event_n < 10)
                    //    touch_events[cur_event_n].b = 1;
                    break;
                
                case ABS_MT_TRACKING_ID:
                    if(cur_event_n >= 0 && cur_event_n < 10){
                        touch_events[cur_event_n].b = v;
                        
                        is_changed = 1;
                        
                        if(v > 0)
                            touch_btns |= 1 << cur_event_n;
                        else
                            touch_btns &= ~(1 << cur_event_n);
                    }
                    //printf("ABS_MT_TRACKING_ID id=%d, btns=%d\n", v, touch_btns);
                    break;
                
                case ABS_MT_POSITION_X:
                    is_changed = 1;
                    if(cur_event_n >= 0 && cur_event_n < 10)
                        touch_events[cur_event_n].x = v;
                    break;
                
                case ABS_MT_POSITION_Y:
                    is_changed = 1;
                    if(cur_event_n >= 0 && cur_event_n < 10)
                        touch_events[cur_event_n].y = v;
                    break;
                
            }
        }
    }
    
    *x = touch_events[0].x;
    *y = touch_events[0].y;
    
    *b = touch_btns;
    
    return !is_changed;
}

#else

/*
** calibration (perfect = 0 0 4096 4096)
*/
//#define NOMINAL_CALIBRATION
//#ifdef NOMINAL_CALIBRATION
//#define MIN_X   0
//#define MIN_Y   0
//#define MAX_X   4096
//#define MAX_Y   4096
//#else
#define MIN_X   200
#define MIN_Y   260
#define MAX_X   3900
#define MAX_Y   3800
//#endif

//#define NOT_MUCH 200    /* small dx or dy ; about 3% of full width */
#define SHORT_CLICK (150) /* 200 ms */

static int tsc2003_events(int fd, int* b, int* x, int* y)
{
    struct input_event ev[64];
    int i, rd;
    fd_set rdfs;
    struct timeval to;
    static int ox=0, oy = 0, ob = 0;
    int jx, jy, nx = 0, ny = 0;
    int st = 0;
    
    to.tv_sec = 0;
    to.tv_usec = SHORT_CLICK * 1000;

    FD_ZERO(&rdfs);
    FD_SET(fd, &rdfs);

    *b = 0;
    
    if(ob == 0){
        ox = 0;
        oy = 0;
    }
    *x = ox;
    *y = oy;

    for(jx = jy = 0; jx < 5 || jy < 5; )
    {
        int rv = select(fd + 1, &rdfs, NULL, NULL, &to);
        if(rv == -1){
            /* an error accured */
            return 1;
        }
        else if(rv == 0){
            //printf("timeout\n");
            /* a timeout occured */
            break;
        }

        rd = read(fd, ev, sizeof(ev));
        if (rd < (int) sizeof(struct input_event)) {
            //printf("expected %d bytes, got %d\n", (int) sizeof(struct input_event), rd);
            //perror("\nevtest: error reading");
            return 1;
        }

        int xt = -1, yt = -1;
        for (i = 0; i < rd / sizeof(struct input_event); i++) {
            unsigned int type, code;
            type = ev[i].type;
            code = ev[i].code;

            if(type == EV_ABS){
                int v = ev[i].value;
                
                if(code == ABS_PRESSURE)
                    *b |= v > 10 ? 1 : 0;
                else if(code == ABS_X){
                    int t = Xsize * (v - MIN_X) / (MAX_X-MIN_X);
#if 0
                    if(/* *b && j > 0 &&*/ ox > 0){
                        //int t = nx/j;
                        if(t < ox - 100)
                            t = ox - 100;
                        else 
                        if(t > ox + 100)
                            t = ox + 100;
                    }
#endif
                    if(t >= 0 && t < Xsize){
                        xt = t;
                        jx++;
                        nx += t;
                    //    st |= 1;
                    //}else{
                    //    j--;
                    //    break;
                    }
                }else if(code == ABS_Y){
                    int t = Ysize * (v - MIN_Y) / (MAX_Y-MIN_Y);
#if 0
                    if(/* *b && j > 0 &&*/ oy > 0){
                        //int t = ny/j;
                        if(t < oy - 100)
                            t = oy - 100;
                        else 
                        if(t > oy + 100)
                            t = oy + 100;
                    }
#endif
                    if(t >= 0 && t < Ysize){
                        yt = t;
                        jy++;
                        ny += t;
                    //    st |=2;
                    //}else{
                    //    j--;
                    //    break;
                    }
                }
            }
        }
        //printf("xyt=(%d, %d)\n", xt, yt);
        
        /*
        if(xt >= 0 && yt >= 0){
            nx += xt;
            ny += yt
        }
        */
//        if(st | 3 != 3)
//            j--;
    }
    
    //printf("nxy=(%d, %d) jxy=(%d, %d)\n", nx, ny, jx, jy);

    if(jx < 1 || jy < 1){
        *b = ob = 0;
        return 0;
    }

    *x = nx/jx;
    *y = ny/jy;

    if(*x < 0)
        *x = 0;
    if(*y < 0)
        *y = 0;

    if(*x > Xsize)
        *x = Xsize;
    if(*y > Ysize)
        *y = Ysize;

    ox = *x;
    oy = *y;
    
    ob = *b;

    //ioctl(fd, EVIOCGRAB, (void*)0);
    return 0;
}

#endif


static void
xmouse() //XEvent *e)
{
    int s, dbl;
//  XButtonEvent *be;
//  XMotionEvent *me;
//  XEvent motion;
    int x=0, y=0, b=0;
    //char buf[64];
    static ob = 0;
//  static ulong lastb, lastt;

//  if(putsnarf != assertsnarf){
//      assertsnarf = putsnarf;
//      XSetSelectionOwner(xmcon, XA_PRIMARY, xdrawable, CurrentTime);
//      if(clipboard != None)
//          XSetSelectionOwner(xmcon, clipboard, xdrawable, CurrentTime);
//      XFlush(xmcon);
//  }

    if(fd_mou >= 0){
#if TOUCHSCREEN_CAPACITIVE
        if(!capacitive_events(fd_mou, &b, &x, &y)){
            //printf("capt x=%d, y=%d, b=%x\n", x, y, b);
            mousetrack(b, x, y, 0);
        }
#else
        tsc2003_events(fd_mou, &b, &x, &y);
        if(b > 0 || (b == 0 && ob > 0)){
            ob = b;
            //printf("rest x=%d, y=%d, b=%x\n", x, y, b);
            mousetrack(b, x, y, 0);
            //printf("ob = %d, b=%d, xy=(%d, %d)\n", ob, b, x, y);
        }
#endif
    }
#if 0
    dbl = 0;
    switch(e->type){
    case ButtonPress:
        be = (XButtonEvent *)e;
        /*
         * Fake message, just sent to make us announce snarf.
         * Apparently state and button are 16 and 8 bits on
         * the wire, since they are truncated by the time they
         * get to us.
         */
        if(be->send_event
        && (~be->state&0xFFFF)==0
        && (~be->button&0xFF)==0)
            return;
        x = be->x;
        y = be->y;
        s = be->state;
        if(be->button == lastb && be->time - lastt < DblTime)
            dbl = 1;
        lastb = be->button;
        lastt = be->time;
        switch(be->button){
        case 1:
            s |= Button1Mask;
            break;
        case 2:
            s |= Button2Mask;
            break;
        case 3:
            s |= Button3Mask;
            break;
        case 4:
            s |= Button4Mask;
            break;
        case 5:
            s |= Button5Mask;
            break;
        }
        break;
    case ButtonRelease:
        be = (XButtonEvent *)e;
        x = be->x;
        y = be->y;
        s = be->state;
        switch(be->button){
        case 1:
            s &= ~Button1Mask;
            break;
        case 2:
            s &= ~Button2Mask;
            break;
        case 3:
            s &= ~Button3Mask;
            break;
        case 4:
            s &= ~Button4Mask;
            break;
        case 5:
            s &= ~Button5Mask;
            break;
        }
        break;
    case MotionNotify:
        me = (XMotionEvent *) e;

        /* remove excess MotionNotify events from queue and keep last one */
        while(XCheckTypedWindowEvent(xmcon, xdrawable, MotionNotify, &motion) == True)
            me = (XMotionEvent *) &motion;

        s = me->state;
        x = me->x;
        y = me->y;
        break;
    default:
        return;
    }

    b = 0;
    if(s & Button1Mask)
        b |= 1;
    if(s & Button2Mask)
        b |= 2;
    if(s & Button3Mask)
        b |= 4;
    if(s & Button4Mask)
        b |= 8;
    if(s & Button5Mask)
        b |= 16;
    if(dbl)
        b |= 1<<8;
#endif

    //mousetrack(b, x, y, 0);
}


void xmouse_btn(int x, int y, int btn){
    mousetrack(btn, x, y, 0);
}


//#include "x11-keysym2ucs.c"

/*
 * Cut and paste.  Just couldn't stand to make this simple...
 */

enum{
    SnarfSize=  100*1024
};

typedef struct Clip Clip;
struct Clip
{
    char buf[SnarfSize];
    QLock lk;
};
Clip clip;

#undef long /* sic */
#undef ulong

#if 0
static char*
_xgetsnarf(XDisplay *xd)
{
    uchar *data, *xdata;
    Atom clipboard, type, prop;
    unsigned long len, lastlen, dummy;
    int fmt, i;
    XWindow w;

    qlock(&clip.lk);
    /*
     * Have we snarfed recently and the X server hasn't caught up?
     */
    if(putsnarf != assertsnarf)
        goto mine;

    /*
     * Is there a primary selection (highlighted text in an xterm)?
     */
    clipboard = XA_PRIMARY;
    w = XGetSelectionOwner(xd, XA_PRIMARY);
    if(w == xdrawable){
    mine:
        data = (uchar*)strdup(clip.buf);
        goto out;
    }

    /*
     * If not, is there a clipboard selection?
     */
    if(w == None && clipboard != None){
        clipboard = clipboard;
        w = XGetSelectionOwner(xd, clipboard);
        if(w == xdrawable)
            goto mine;
    }

    /*
     * If not, give up.
     */
    if(w == None){
        data = nil;
        goto out;
    }

    /*
     * We should be waiting for SelectionNotify here, but it might never
     * come, and we have no way to time out.  Instead, we will clear
     * local property #1, request our buddy to fill it in for us, and poll
     * until he's done or we get tired of waiting.
     *
     * We should try to go for utf8string instead of XA_STRING,
     * but that would add to the polling.
     */
    prop = 1;
    XChangeProperty(xd, xdrawable, prop, XA_STRING, 8, PropModeReplace, (uchar*)"", 0);
    XConvertSelection(xd, clipboard, XA_STRING, prop, xdrawable, CurrentTime);
    XFlush(xd);
    lastlen = 0;
    for(i=0; i<10 || (lastlen!=0 && i<30); i++){
        osmillisleep(100);
        XGetWindowProperty(xd, xdrawable, prop, 0, 0, 0, AnyPropertyType,
            &type, &fmt, &dummy, &len, &data);
        if(lastlen == len && len > 0)
            break;
        lastlen = len;
    }
    if(i == 10){
        data = nil;
        goto out;
    }
    /* get the property */
    data = nil;
    XGetWindowProperty(xd, xdrawable, prop, 0, SnarfSize/sizeof(unsigned long), 0,
        AnyPropertyType, &type, &fmt, &len, &dummy, &xdata);
    if((type != XA_STRING && type != utf8string) || len == 0){
        if(xdata)
            XFree(xdata);
        data = nil;
    }else{
        if(xdata){
            data = (uchar*)strdup((char*)xdata);
            XFree(xdata);
        }else
            data = nil;
    }
out:
    qunlock(&clip.lk);
    return (char*)data;
}

static void
_xputsnarf(XDisplay *xd, char *data)
{
    XButtonEvent e;

    if(strlen(data) >= SnarfSize)
        return;
    qlock(&clip.lk);
    strcpy(clip.buf, data);

    /* leave note for mouse proc to assert selection ownership */
    putsnarf++;

    /* send mouse a fake event so snarf is announced */
    memset(&e, 0, sizeof e);
    e.type = ButtonPress;
    e.window = xdrawable;
    e.state = ~0;
    e.button = ~0;
    XSendEvent(xd, xdrawable, True, ButtonPressMask, (XEvent*)&e);
    XFlush(xd);
    qunlock(&clip.lk);
}

static void
xselect(XEvent *e, XDisplay *xd)
{
    char *name;
    XEvent r;
    XSelectionRequestEvent *xe;
    Atom a[4];

    if(e->xany.type != SelectionRequest)
        return;

    memset(&r, 0, sizeof r);
    xe = (XSelectionRequestEvent*)e;
if(0) iprint("xselect target=%d requestor=%d property=%d selection=%d\n",
    xe->target, xe->requestor, xe->property, xe->selection);
    r.xselection.property = xe->property;
    if(xe->target == targets){
        a[0] = XA_STRING;
        a[1] = utf8string;
        a[2] = text;
        a[3] = compoundtext;

        XChangeProperty(xd, xe->requestor, xe->property, xe->target,
            8, PropModeReplace, (uchar*)a, sizeof a);
    }else if(xe->target == XA_STRING || xe->target == utf8string || xe->target == text || xe->target == compoundtext){
        /* if the target is STRING we're supposed to reply with Latin1 XXX */
        qlock(&clip.lk);
        XChangeProperty(xd, xe->requestor, xe->property, xe->target,
            8, PropModeReplace, (uchar*)clip.buf, strlen(clip.buf));
        qunlock(&clip.lk);
    }else{
        iprint("get %d\n", xe->target);
        name = XGetAtomName(xd, xe->target);
        if(name == nil)
            iprint("XGetAtomName failed\n");
        else if(strcmp(name, "TIMESTAMP") != 0)
            iprint("%s: cannot handle selection request for '%s' (%d)\n", argv0, name, (int)xe->target);
        r.xselection.property = None;
    }

    r.xselection.display = xe->display;
    /* r.xselection.property filled above */
    r.xselection.target = xe->target;
    r.xselection.type = SelectionNotify;
    r.xselection.requestor = xe->requestor;
    r.xselection.time = xe->time;
    r.xselection.send_event = True;
    r.xselection.selection = xe->selection;
    XSendEvent(xd, xe->requestor, False, 0, &r);
    XFlush(xd);
}
#endif


char *snarf_buf = nil;

char*
clipread(void)
{
    char *p = NULL;

//  if(xsnarfcon == nil)
//      return nil;
//  XLockDisplay(xsnarfcon);
//  p = _xgetsnarf(xsnarfcon);
//  XUnlockDisplay(xsnarfcon);

    p = strdup(snarf_buf);

    return p;
}

int
clipwrite(char *buf)
{
//  buf = NULL;

//  if(xsnarfcon == nil)
//      return 0;
//  XLockDisplay(xsnarfcon);
//  _xputsnarf(xsnarfcon, buf);
//  XUnlockDisplay(xsnarfcon);
    int l = strlen(buf);

    if(snarf_buf)
        free(snarf_buf);
    snarf_buf = nil;

    if(l >= SnarfSize)
        return 0;

    snarf_buf = strdup(buf);

    /* leave note for mouse proc to assert selection ownership */
    putsnarf++;

    return 0;
}
