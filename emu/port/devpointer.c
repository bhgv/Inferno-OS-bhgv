/*
 * mouse or stylus
 */
#include <stdio.h>

#include    "dat.h"
#include    "fns.h"
#include    "../port/error.h"

#include <draw.h>
#include <memdraw.h>
#include <cursor.h>

#define cursorenable()
#define cursordisable()

enum{
    Qdir,
    Qpointer,
    Qcursor
};

typedef struct Pointer Pointer;

struct Pointer {
    int x;
    int y;
    int b;
    ulong   msec;
};

static struct
{
    Pointer v;
    int modify;
    int lastb;
    Rendez  r;
    Ref ref;
    QLock   q;
} mouse;

static
Dirtab pointertab[]={
    ".",            {Qdir, 0, QTDIR},   0,  0555,
    "pointer",      {Qpointer}, 0,  0666,
    "cursor",       {Qcursor},      0,  0222,
};

enum {
    Nevent = 16 /* enough for some */
};

static struct {
    int rd;
    int wr;
    Pointer clicks[Nevent];
    Rendez r;
    int full;
    int put;
    int get;
} ptrq;



#define Nx 10
#define Ny 10


typedef struct {
    int x;
    int y;
    int b;
} in_touch_evt;

typedef struct {
    int x;
    int y;
    int id;
    
    double a;
    double a45;
    double l;
} touch_evt;

typedef struct _evt_node evt_node;
struct _evt_node {
    touch_evt *pt;
    evt_node *next;
};

static void
bresenham_line (int* map, int i, int x0, int y0, int x1, int y1)
{
  int dx =  abs (x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -abs (y1 - y0), sy = y0 < y1 ? 1 : -1; 
  int err = dx + dy, e2; /* error value e_xy */
 
  for (;;){  /* loop */
    int* ppxl = &map[y0 * Nx + x0];
    //setPixel (x0,y0);
    if(*ppxl == 0)
        *ppxl |= (2 << (i * 2));
    if (x0 == x1 && y0 == y1) break;
    e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; } /* e_xy+e_x > 0 */
    if (e2 <= dx) { err += dx; y0 += sy; } /* e_xy+e_y < 0 */
  }
}

#include <math.h>

#define PI 3.14159265                                       // число ПИ

#define ANGLE_EMPTY 10000.

void
process_gestures(in_touch_evt *cur_evts, int max_evts)
{
    int i, j;
    int id;
    int cnt = 0;
    static evt_node* evt_root[10];
    static int first_time = 1;
    
    static double oa[10];
    
    static int minX = 100000;
    static int maxX = -100000;
    static int minY = 100000;
    static int maxY = -100000;
    
    static double an[10];
    
    if(cur_evts == NULL)
        return;
        
    if(first_time != 0){
printf("first_time (%d)\n", sizeof(evt_root));
        memset(evt_root, 0, sizeof(evt_root));
        for(i = 0; i < 10; i++){
            an[i] = ANGLE_EMPTY;
            oa[i] = ANGLE_EMPTY;
        }
        first_time = 0;
    }
    
    for(i = 0; i < max_evts; i++){
        id = cur_evts[i].b;
        
        if(id > 0){
            int x = cur_evts[i].x;
            int y = cur_evts[i].y;
//      printf("%s:%d i=%d, id=%d, xy=(%d, %d)\n", __func__, __LINE__, i, id, cur_evts[i].x, cur_evts[i].y);
            
            if(id > 0)
                cnt++;

//          printf("%s:%d !!! i=%d, id=%d\n", __func__, __LINE__, i, id);
            for(j = 0; j < 10; j++){
                //j = i;
                if(
                    (id > 0 && evt_root[j] == NULL) || 
                    evt_root[j]->pt->id == id
                ){
                    if(
                        evt_root[j] != NULL &&
                        evt_root[j]->pt->x == x &&
                        evt_root[j]->pt->y == y
                    )
                        break;
                    
                    double         a = ANGLE_EMPTY;
                    //static double  oa = ANGLE_EMPTY;
                    int            l2 = 0;
                    
                    evt_node*  tmp_node; // = malloc(sizeof(evt_node));
                    touch_evt* tmp_evt ; // = malloc(sizeof(touch_evt));
                    
                    double dx = 0., dy = 0.;
                    
                    if(evt_root[j] != NULL){
                        dx = (double)(x - evt_root[j]->pt->x);
                        dy = (double)(y - evt_root[j]->pt->y);
                        
                        l2 = dx*dx + dy*dy;
                        if(l2 < 100*100)
                            break;

                        a = atan2(dy, dx);
    //                    evt_root[j]->pt->a = a;
    printf("%s: i=%d, id=%d, a=%f\n", (evt_root[j] == NULL ? "OOO" : "___"), i, id, a * 180. / PI);

                        //if(an[j] == ANGLE_EMPTY)
                        //    an[j] = a;
                    }
                    
                    if(x > maxX) maxX = x;
                    if(x < minX) minX = x;
                    
                    if(y > maxY) maxY = y;
                    if(y < minY) minY = y;
                        
    //                  printf("%s:%d j=%d, node=%x\n", __func__, __LINE__, j, evt_root[j]);

                //if(l2 > 100){
                    if(evt_root[j] != NULL && oa[j] != ANGLE_EMPTY 
                        && a < oa[j] + (PI/8.) && a > oa[j] - (PI/8.)
                    ){
                        if(evt_root[j]->next){
                            dx = (double)(x - evt_root[j]->next->pt->x);
                            dy = (double)(y - evt_root[j]->next->pt->y);
                            l2 = dx*dx + dy*dy;
                            evt_root[j]->next->pt->l = sqrt((double)l2);
                            
                            a = atan2(dy, dx);
                            evt_root[j]->next->pt->a = a;
                            oa[j] = a;
                        }else{
                            evt_root[j]->pt->l += sqrt((double)l2);
                        }
                        evt_root[j]->pt->x = x;
                        evt_root[j]->pt->y = y;
                    }else{
                        if(evt_root[j] != NULL){
                            evt_root[j]->pt->a = a /*- an[j]*/;
                            
                            oa[j] = a;
                            evt_root[j]->pt->l = sqrt((double)l2);
                        }
                        tmp_node = malloc(sizeof(evt_node));
                        tmp_evt  = malloc(sizeof(touch_evt));
                        
                        tmp_evt->x = x;
                        tmp_evt->y = y;
                        tmp_evt->id = id;
//                        tmp_evt->l = sqrt((double)l2);

                        tmp_node->pt = tmp_evt;
                        tmp_node->next = evt_root[j];
                        evt_root[j] = tmp_node;
                    }
                    
                    break;
                //}
                }
            }
        }
    }
    
//  printf("%s:%d cnt=%d\n", __func__, __LINE__, cnt);
    if(cnt == 0){
        for(i = 0; i < 10; i++){
            evt_node* tmp_node = evt_root[i];
            static int ox, oy;
            evt_node* sh_evt = NULL;
            
            int stage = 0;
            
            double oa2 = ANGLE_EMPTY;
            double an = 0.;
            static touch_evt** popt = NULL;
            
            if(tmp_node != NULL)
            printf("----------\n");
            
            while(tmp_node != NULL){
                touch_evt* pt  = tmp_node->pt;
                evt_node*  nxt_nd = tmp_node->next;
                
                double a, a2, da;
                int a2i;
                
                if(pt->l == 0.){
                    free(pt);
                    free(tmp_node);
                    tmp_node = nxt_nd;
                    continue;
                }
                
                a = pt->a /*- an/*- an[i]*/;
                da = modf(a / (PI/8.), &a2);
                a2i = (int)a2;
                if(a2i >= 0){
                    a2i = (a2i + 1) / 2;
                }else{
                    a2i = (a2i - 1) / 2;
                }
                
                if(a2i <= -4)
                    a2i = 8 + a2i;
                else if(a2i > 4)
                    a2i = -8 + a2i;
                
                a = (double)a2i * (PI/4.);
                pt->a45 = a;
                
                if(stage == 0 || stage == 1 || a != oa2){
                    tmp_node->next = sh_evt;
                    sh_evt = tmp_node;
                    
                    popt = &tmp_node->pt;
                    
                    printf(">>> i=%d, pt->a=%f, pt->a45=%f, pt->l=%f\n", i, pt->a * 180. / PI, pt->a45 * 180. / PI, pt->l);
                }else{
                    if(popt != NULL){
                        pt->l += (*popt)->l;
                        
                        free(*popt);
                        *popt = pt;
                        
                        printf(">>> ...+...  pt->l=%f\n", pt->l);
                    }else{
                        free(pt);
                    }
                    free(tmp_node);
                }
                tmp_node = nxt_nd;
                
                oa2 = a;
                
                stage++;
            }
            
            oa2 = ANGLE_EMPTY;
            
            stage = 0;
            
            evt_root[i] = sh_evt;
            
            /**/
if(evt_root[i])
printf("\n%d) -V--- normalisation --\n", i);
            // angle (a45) normalisation
            an = ANGLE_EMPTY;
            tmp_node = evt_root[i];
            if(tmp_node && an == ANGLE_EMPTY)
                an = tmp_node->pt->a45;
            while(tmp_node != NULL){
                touch_evt* pt  = tmp_node->pt;
                double a = pt->a45 - an;
                
                if(a <= -PI)
                    a = 2.*PI + a;
                else if(a > PI)
                    a = -2.*PI + a;

                pt->a45 = a;
printf("> a45=%f, L=%f, an=%f\n", pt->a45 * 180. / PI, pt->l, an * 180. / PI);
                tmp_node = tmp_node->next;
            }
if(evt_root[i])
printf("%d) -A--- normalisation --\n\n", i);
            /**/
        }        
        
        // cln
        
        for(i == 0; i < 10; i++){
            evt_node* tmp_node = evt_root[i];
            while(tmp_node != NULL){
                touch_evt* tmp_evt  = tmp_node->pt;
                
                evt_node* tmp_node2 = tmp_node->next;
                
                free(tmp_evt);
                free(tmp_node);
                
                tmp_node = tmp_node2;
            }
            evt_root[i] = NULL;
        }
        first_time = 1;
        
        minX = 100000;
        maxX = -100000;
        minY = 100000;
        maxY = -100000;
    }
}


/*
 * called by any source of pointer data
 */
void
mousetrack(int b, int x, int y, int isdelta)
{
    int lastb;
    ulong msec;
    Pointer e;

    if(isdelta){
        x += mouse.v.x;
        y += mouse.v.y;
    }
    msec = osmillisec();
    lastb = mouse.v.b;
    mouse.v.x = x;
    mouse.v.y = y;
    mouse.v.b = b;
    mouse.v.msec = msec;
    if(!ptrq.full && lastb != b){
        e = mouse.v;
        ptrq.clicks[ptrq.wr] = e;
        if(++ptrq.wr >= Nevent)
            ptrq.wr = 0;
        if(ptrq.wr == ptrq.rd)
            ptrq.full = 1;
    }
    mouse.modify = 1;
    ptrq.put++;
    Wakeup(&ptrq.r);
/*  drawactive(1);  */
/*  setpointer(x, y); */
}

static int
ptrqnotempty(void *x)
{
    USED(x);
    return ptrq.full || ptrq.put != ptrq.get;
}

static Pointer
mouseconsume(void)
{
    Pointer e;

    Sleep(&ptrq.r, ptrqnotempty, 0);
    ptrq.full = 0;
    ptrq.get = ptrq.put;
    if(ptrq.rd != ptrq.wr){
        e = ptrq.clicks[ptrq.rd];
        if(++ptrq.rd >= Nevent)
            ptrq.rd = 0;
    }else
        e = mouse.v;
    return e;
}

Point
mousexy(void)
{
    return Pt(mouse.v.x, mouse.v.y);
}


static Chan*
pointerattach(char* spec)
{
    return devattach('m', spec);
}

static Walkqid*
pointerwalk(Chan *c, Chan *nc, char **name, int nname)
{
    Walkqid *wq;

    wq = devwalk(c, nc, name, nname, pointertab, nelem(pointertab), devgen);
    if(wq != nil && wq->clone != c && wq->clone != nil && (ulong)c->qid.path == Qpointer)
        incref(&mouse.ref); /* can this happen? */
    return wq;
}

static int
pointerstat(Chan* c, uchar *db, int n)
{
    return devstat(c, db, n, pointertab, nelem(pointertab), devgen);
}

static Chan*
pointeropen(Chan* c, int omode)
{
    c = devopen(c, omode, pointertab, nelem(pointertab), devgen);
    if((ulong)c->qid.path == Qpointer){
        if(waserror()){
            c->flag &= ~COPEN;
            nexterror();
        }
        if(!canqlock(&mouse.q))
            error(Einuse);
        if(incref(&mouse.ref) != 1){
            qunlock(&mouse.q);
            error(Einuse);
        }
        cursorenable();
        qunlock(&mouse.q);
        poperror();
    }
    return c;
}

static void
pointerclose(Chan* c)
{
    if((c->flag & COPEN) == 0)
        return;
    switch((ulong)c->qid.path){
    case Qpointer:
        qlock(&mouse.q);
        if(decref(&mouse.ref) == 0){
            cursordisable();
        }
        qunlock(&mouse.q);
        break;
    }
}

static long
pointerread(Chan* c, void* a, long n, vlong off)
{
    Pointer mt;
    char buf[1+4*12+1];
    int l;

    USED(&off);
    switch((ulong)c->qid.path){
    case Qdir:
        return devdirread(c, a, n, pointertab, nelem(pointertab), devgen);
    case Qpointer:
        qlock(&mouse.q);
        if(waserror()) {
            qunlock(&mouse.q);
            nexterror();
        }
        mt = mouseconsume();
        poperror();
        qunlock(&mouse.q);
        l = snprint(buf, sizeof(buf), "m%11d %11d %11d %11lud ", mt.x, mt.y, mt.b, mt.msec);
        if(l < n)
            n = l;
        memmove(a, buf, n);
        break;
    default:
        n=0;
        break;
    }
    return n;
}

static long
pointerwrite(Chan* c, void* va, long n, vlong off)
{
    char *a = va;
    char buf[128];
    int b, x, y;
    Drawcursor cur;

    USED(&off);
    switch((ulong)c->qid.path){
    case Qpointer:
        if(n > sizeof buf-1)
            n = sizeof buf -1;
        memmove(buf, va, n);
        buf[n] = 0;
        x = strtoul(buf+1, &a, 0);
        if(*a == 0)
            error(Eshort);
        y = strtoul(a, &a, 0);
        if(*a != 0)
            b = strtoul(a, 0, 0);
        else
            b = mouse.v.b;
        /*mousetrack(b, x, y, msec);*/
        setpointer(x, y);
        USED(b);
        break;
    case Qcursor:
        /* TO DO: perhaps interpret data as an Image */
        /*
         *  hotx[4] hoty[4] dx[4] dy[4] clr[dx/8 * dy/2] set[dx/8 * dy/2]
         *  dx must be a multiple of 8; dy must be a multiple of 2.
         */
        if(n == 0){
            cur.data = nil;
            drawcursor(&cur);
            break;
        }
        if(n < 8)
            error(Eshort);
        cur.hotx = BGLONG((uchar*)va+0*4);
        cur.hoty = BGLONG((uchar*)va+1*4);
        cur.minx = 0;
        cur.miny = 0;
        cur.maxx = BGLONG((uchar*)va+2*4);
        cur.maxy = BGLONG((uchar*)va+3*4);
        if(cur.maxx%8 != 0 || cur.maxy%2 != 0 || n-4*4 != (cur.maxx/8 * cur.maxy))
            error(Ebadarg);
        cur.data = (uchar*)va + 4*4;
        drawcursor(&cur);
        break;
    default:
        error(Ebadusefd);
    }
    return n;
}

Dev pointerdevtab = {
    'm',
    "pointer",

    devinit,
    pointerattach,
    pointerwalk,
    pointerstat,
    pointeropen,
    devcreate,
    pointerclose,
    pointerread,
    devbread,
    pointerwrite,
    devbwrite,
    devremove,
    devwstat,
};
