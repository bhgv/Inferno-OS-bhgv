#!/usr/bin/env python3
"""Exercise the production layer allocator with independent image geometry and failure cleanup."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
TEXT = (ROOT/'libmemlayer/lalloc.c').read_text()
BODY = TEXT[TEXT.index('Memimage*\nmemlalloc('):]
SOURCE = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef unsigned int LayerWord;
#define ulong LayerWord // Model the Android 32-bit pixel word.
typedef struct Point {int x,y;} Point;
typedef struct Rectangle {Point min,max;} Rectangle;
typedef struct Memimage Memimage;
typedef struct Memscreen Memscreen;
typedef struct Memlayer Memlayer;
typedef void (*Refreshfn)(void);
typedef struct Memdata {int ref; unsigned char *bdata; size_t bytes;} Memdata;
struct Memimage {Memdata *data; long zero; int width,chan,flags; Rectangle r,clipr; Memlayer *layer; void *ext_win;};
struct Memlayer {Memscreen *screen; Memimage *save,*front,*rear; Refreshfn refreshfn; void *refreshptr; Rectangle screenr; Point delta; int clear;};
struct Memscreen {Memimage *image,*rearmost,*frontmost;};
#define nil NULL // Model Inferno nil.
#define RGBA32 32 // Fixture channel depth.
#define Frepl 1 // Replicated solid image flag.
#define DNofill 0xffffffffU // Skip painting; exercise storage only.
static int fail_layer, fail_save, fail_actor, freed, actors;


/* Inject a layer-structure allocation failure without changing image allocation. */
static void *layer_malloc(size_t n) {if(fail_layer){fail_layer=0;return NULL;}return malloc(n);}


/* Construct a fixture point. */
static Point Pt(int x,int y) {return (Point){x,y};}


/* Construct the independent image extent. */
static Rectangle Rect(int x,int y,int X,int Y) {return (Rectangle){Pt(x,y),Pt(X,Y)};}


/* Model the existing image-view addressing convention. */
static Memimage *allocmemimaged(Rectangle r,int chan,Memdata *data)
{
    Memimage *im=calloc(1,sizeof(*im));
    im->r=im->clipr=r;im->chan=chan;im->data=data;
    im->width=r.max.x-r.min.x;
    im->zero=-(long)4*(r.min.y*im->width+r.min.x);
    return im;
}


/* Allocate exactly the independently computed pixel span. */
static Memimage *allocmemimage(Rectangle r,int chan)
{
    Memdata *d;
    if(fail_save){fail_save=0;return NULL;}
    d=calloc(1,sizeof(*d));d->ref=1;
    d->bytes=(size_t)(r.max.x-r.min.x)*(r.max.y-r.min.y)*4;
    d->bdata=calloc(1,d->bytes);
    return allocmemimaged(r,chan,d);
}


/* Count released data and actor ownership. */
static void freememimage(Memimage *im)
{
    if(!im)return;
    if(im->ext_win){actors--;im->ext_win=NULL;}
    if(--im->data->ref==0){free(im->data->bdata);free(im->data);freed++;}
    free(im);
}


/* Model successful or refused native actor creation. */
static void *attach_clutter_actor(char *p,int x,int y,int X,int Y)
{
    (void)x;(void)y;(void)X;(void)Y;
    if(fail_actor)return NULL;
    actors++;return p;
}


/* Avoid display side effects in this storage-only fixture. */
static void _memltofrontfill(Memimage *i,int fill) {(void)i;(void)fill;}


/* Retain the requested channel in the fixture. */
static void memsetchan(Memimage *i,int c) {i->chan=c;}


/* Leave painting outside the ownership fixture. */
static void memfillcolor(Memimage *i,ulong c) {(void)i;(void)c;}


/* Leave actual display compositing to its owning tests. */
static void memdraw(Memimage *i,Rectangle r,Memimage *p,Point q,void *m,Point n,int op) {(void)i;(void)r;(void)p;(void)q;(void)m;(void)n;(void)op;}
#define S 0 // Source compositing operation stub.
#define malloc layer_malloc // Inject only the production layer-structure failure.
BODY
#undef malloc


/* Check both geometries and unwind every acquired resource on refusal. */
int main(void)
{
    Memimage *screen=allocmemimage(Rect(0,0,1080,2400),32),*window;
    Memscreen s={.image=screen};
    int x,y,before;
    /* A 127-pixel nonzero-origin window must address every row inside its own allocation. */
    window=memlalloc(&s,Rect(317,201,444,601),(Refreshfn)1,NULL,DNofill);
    if(!window)return 1;
#ifdef EXT_WIN
    if(window->data==screen->data || window->data->ref!=1)return 2;
    for(y=201;y<601;y++)for(x=317;x<444;x++){
        long at=window->zero+4L*(y*window->width+x);
        if(at<0 || (size_t)at+4>window->data->bytes)return 3;
        memset(window->data->bdata+at,0xa5,4);
    }
#else
    if(window->data!=screen->data || screen->data->ref!=2 || window->width!=1080)return 4;
#endif
    free(window->layer);freememimage(window);
    if(screen->data->ref!=1)return 5;
    before=freed;fail_layer=1;
    if(memlalloc(&s,Rect(0,0,127,400),(Refreshfn)1,NULL,DNofill)!=NULL)return 6;
#ifdef EXT_WIN
    if(actors!=0 || freed!=before+1)return 7;
    before=freed;fail_actor=1;
    if(memlalloc(&s,Rect(0,0,127,400),(Refreshfn)1,NULL,DNofill)!=NULL || freed!=before+1)return 8;
#endif
    puts("PASS layer storage: nonzero origin, narrow stride, last row, ownership and allocation refusal");
    return 0;
}
'''.replace('BODY',BODY)


# Compile both existing software layers and GLES independent images from the same production function.
def main():
    with tempfile.TemporaryDirectory(prefix='layer-storage-') as directory:
        work=Path(directory)
        (work/'test.c').write_text(SOURCE)
        for defines in ([],['-DEXT_WIN']):
            command=shlex.split(os.environ.get('CC','cc'))
            command+=shlex.split(os.environ.get('CFLAGS','-O2 -fsanitize=undefined -fno-sanitize-recover=all'))
            subprocess.run(command+defines+[str(work/'test.c'),'-o',str(work/'test')],check=True)
            subprocess.run([str(work/'test')],check=True)


if __name__=='__main__':
    main()
