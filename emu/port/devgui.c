#include	"dat.h"
#include	"fns.h"
#include	"error.h"
//#include	"audio.h"

#include "interp.h"

#include "draw.h"
#include "tk.h"

#include "../libtk/canvs.h"
#include "isa.h"
#include "../libinterp/runt.h"

//#include <stdbool.h>
//#define TRUE true
//#define FALSE false


#ifdef ANDROID_NO_DEF
#include <android/log.h>
	
#define  LOG_TAG    "Inferno GUI"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGW(...)  __android_log_print(ANDROID_LOG_WARN,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)
#else
#define  LOGI(...)  print(__VA_ARGS__)
#define  LOGW(...)  print(__VA_ARGS__)
#define  LOGE(...)  print(__VA_ARGS__)
#endif


enum
{
	Qdir = 0,		/* must start at 0 representing a directory */
	Qgui,
	Qguictl
};



#define	QSHIFT		16	// 4		/* location in qid of pid */
#define	QID(q)		(((ulong)(q).path&0x00007ffF)>>0)
#define QPID(pid)	(((pid)<<QSHIFT))
#define	PID(q)		((q).vers)
#define PATH(q)		( (ulong)(q).path & ~((1<<QSHIFT)-1) )
#define QCTL_NAME	((ulong)0x00008000)
//#define QIDgetRV(q)	( (ulong)( (uvlong)(q).path >> (sizeof(ulong)*8) ) )
//#define QIDsetRV(q, v)	( (uvlong)(q).path | (uvlong)v << (sizeof(ulong)*8) )




/* Protected by acquire/release in read, write and close. */
typedef struct _rslt_string rslt_string;
struct _rslt_string {
	rslt_string *next;
	Qid qid;
	long len;
	long pos;
	char str[1];
};

static rslt_string *root_rslt_strings;

static int
guisameresult(rslt_string *r, Qid *qid)
{
	return r->qid.path == qid->path && r->qid.vers == qid->vers;
}

/* -1 means no reply is pending; 0 may be an empty completed reply. */
static long
guiresultread(Qid *qid, void *va, long count)
{
	rslt_string **link, *r;
	long n;

	for(link=&root_rslt_strings; (r=*link) != nil; link=&r->next) {
		if(!guisameresult(r, qid))
			continue;
		n=r->len-r->pos;
		if(n > count)
			n=count;
		memmove(va, r->str+r->pos, n);
		r->pos+=n;
		if(r->pos == r->len) {
			*link=r->next;
			free(r);
		}
		return n;
	}
	return -1;
}

static void
guiresultqueue(rslt_string *r)
{
	rslt_string **link;
	for(link=&root_rslt_strings; *link != nil; link=&(*link)->next)
		;
	r->next=nil;
	*link=r;
}

static void
guiresultclear(Qid *qid)
{
	rslt_string **link, *r;
	for(link=&root_rslt_strings; (r=*link) != nil; ) {
		if(guisameresult(r, qid)) {
			*link=r->next;
			free(r);
		}else
			link=&r->next;
	}
}

typedef struct GuiRead GuiRead;
struct GuiRead {
	char *data;
	long len;
};

extern char rootdir[];


#if 0
struct DDisplay
{
	Draw_Display	drawdisplay;
	Display*	display;
	/*DRef*/ void*		dref;
};
#endif


void guictl_wr_pars(char* in, int count);


extern TkMethod *tkmethod[];
extern TkCimeth tkcimethod[];

extern char* tksinglecmd(TkTop *t, char *arg, char **val);
//extern char* tkwidgetcmd(TkTop *t, Tk *tk, char *arg, char **val);



static Tk* 
qid2tk(Qid *qid, TkTop *t){
	ulong path = QID(*qid); //PATH(c->qid) >> QSHIFT;
	int i;
	Tk *tkr, *tk;
	
	tkr = t->root;
	
	for( i = 1, tk = tkr; i < path; i++, tk = tk->siblings ){
		if(tk == nil){
			return nil;
		}
	}
	return tk;
}

static int 
tk2idx(TkTop *t, Tk *tk){
	int i;
	Tk *tkr, *tk2;
	
	tkr = t->root;
	
	for( i = 1, tk2 = tkr; tk2 != tk; tk2 = tk2->siblings, i++){
		if(tk2 == nil){
			return -1;
		}
	}
	return i;
}


// VV dinamic names-tree helper VV

/* go to lower dir (..) */
static int
guigen_folder_back(Chan *c, Prog *p, Dir *dp)
{
	Qid qid;

	// Tk pointers, counters
	TkTop *t;
	Tk *tk=nil, *tkr=nil;
	int n;
	
	Osenv *o;

	t = p->tktop;
	
	if(!t){
		mkqid(&qid, Qdir, 0, QTDIR);
		devdir(c, qid, "#G", 0, eve, DMDIR|0555, dp);
		return 1;
	}

	tkr = t->root;

	tk = qid2tk(&c->qid, t);
	if(tk == nil){
		error(Enonexist);
	}
	if(tk)
		tk = tk->master;

	if( !tk /*|| tk == tkr*/){
		mkqid(&qid, Qdir, 0, QTDIR);
		devdir(c, qid, "#G", 0, eve, DMDIR|0555, dp);
		return 1;
	}

	n = tk2idx(t, tk);
	if(n == -1){
		error(Enonexist);
	}

	snprint(up->genbuf, 127, "%s", 
			tk->name_tail
			? tk->name_tail
			: tkname(tk)
	);
	mkqid(&qid, (ulong)n | PATH(c->qid), c->qid.vers, QTDIR);
	o = p->osenv;
	devdir(c, qid, up->genbuf, 0,  o->user, DMDIR|0777, dp);
	return 1;
}


/* ignore s and use name to find pid */
static int
guigen_by_name(Chan *c, Prog *p, char *name, Dir *dp)
{
	Qid qid;
	ulong pid, path;

	TkTop *t;
	Tk *tk=nil, *tk2=nil, *tkr=nil;
	int n;

	Osenv *o;
	
	char *e;


	/* if it is the Top dir of the device */
	if((ulong)c->qid.path == Qdir) {
		pid = strtoul(name, &e, 0);  // Top dir of devgui is a list of numbers of gui-procs. test this
		if(pid == 0 || *e != '\0')
			return -1;
		p = progpid(pid);
		if(p == nil){
			return -1;
		}
		o = p->osenv;
	
		// common part
		snprint(up->genbuf, 127, "%lud", pid);  // up->genbuf is a small (127 bytes) per-process character buffer
		if(name != nil && strcmp(name, up->genbuf) != 0){
			return -1;
		}
		mkqid(&qid, pid << QSHIFT, pid, QTDIR);  // in the Top dir of devgui all the entryes are folders
		devdir(c, qid, up->genbuf, 0, o->user, DMDIR|0777, dp);
		return 1;
	}

	path = QID(c->qid); //PATH(c->qid) >> QSHIFT;

	if(p->tktop == nil) {
		return -1;
	}
	else{
		t = p->tktop;
		
		tkr = t->root;
		n = path;

		tk = qid2tk(&c->qid, t);
		if(tk == nil){
			return -1;
		}

		/* .self file controls the folder (widget-container) properties. it has n==0 */
		if( strcmp(name, ".self") ){
			char *p, *pr = nil;
			
			for(p = name; *p != '\0'; p++){
				if(*p == '.'){
					pr = p;
					*p = '\0';
					break;
				}
			}

			for( tk2 = tk->slave; tk2; tk2 = tk2->next){
				if( !strcmp(name, 
						tk2->name_tail
						? tk2->name_tail
						: tkname(tk2) ) 
				){
					break;
				}
			}
			if(pr != nil)
				*pr = '.';
			if(tk2 == nil){
				return -1;
			}
			
			n = tk2idx(t, tk2);
			if(n == -1){
				return -1;
			}
		}else{
			n = 0;
		}
	}

	if(n == 0){
		strcpy(up->genbuf, ".self");
		if(name != nil && strcmp(name, up->genbuf) != 0){
			return -1;
		}
		mkqid(&qid, (ulong)path | PATH(c->qid) | QCTL_NAME, c->qid.vers, QTFILE);
		o = p->osenv;
		devdir(c, qid, up->genbuf, 0, o->user, 0664, dp);
		return 1;
		
	}else if(tk2){
		snprint(up->genbuf, 127, "%s.%s", 
				tk2->name_tail
				? tk2->name_tail
				: tkname(tk2),
				tkmethod[tk2->type]->name
		);
		if(name != nil && strcmp(name, up->genbuf) != 0){
			return -1;
		}
		if(tk2->slave) 
			tk2->is_container = 1;
		mkqid(&qid, (ulong)n | PATH(c->qid), c->qid.vers, (tk2->is_container) ? QTDIR : QTFILE);
		o = p->osenv;
		devdir(c, qid, up->genbuf, 0, o->user, (tk2->is_container) ? DMDIR|0777 : 0664, dp);
		return 1;
	}else{
		return -1;
	}

}


/* ignore name and use s to find pid */
static int
guigen_by_number(Chan *c, Prog *p, int s, Dir *dp)
{
	Qid qid;
	ulong pid, path;

	TkTop *t;
	Tk *tk=nil, *tk2=nil, *tkr=nil;
	int i, n;
	
	Osenv *o;
	
	
	/* if it is the Top dir of the device */
	if((ulong)c->qid.path == Qdir) {
		p = progn(s);
		if(p == nil) {
			return -1;
		}
		pid = p->pid;
		o = p->osenv;
	
		// common part
		snprint(up->genbuf, 127, "%lud", pid);  // up->genbuf is a small (127 bytes) per-process character buffer
//printf("%s: %d > %s\n", __func__, __LINE__, up->genbuf);
		mkqid(&qid, pid << QSHIFT, pid, QTDIR);  // in the Top dir of devgui all the entryes are folders
		devdir(c, qid, up->genbuf, 0, o->user, DMDIR|0777, dp);
		return 1;
	}

	path = QID(c->qid); //PATH(c->qid) >> QSHIFT;

	if(p->tktop == nil) {
		return -1;
	}
	else{
		t = p->tktop;
		
		tkr = t->root;
		n = path;
		
		tk = qid2tk(&c->qid, t);
		if(tk == nil){
			return -1;
		}

		if(s > 0){
			for( i = 1, tk2 = tk->slave; tk2 && i < s; i++, tk2 = tk2->next);
			if(tk2 == nil){
				return -1;
			}

			n = tk2idx(t, tk2);
			if(n == -1){
				return -1;
			}
		}else{
			n = 0;
		}
	}
	
	/* .self file controls the folder (widget-container) properties. n==0 */
	if(n == 0){
		strcpy(up->genbuf, ".self");
		mkqid(&qid, (ulong)path | PATH(c->qid) | QCTL_NAME, c->qid.vers, QTFILE);
		o = p->osenv;
		devdir(c, qid, up->genbuf, 0, o->user, 0664, dp);
		return 1;
		
	}else if(tk2){
		snprint(up->genbuf, 127, "%s.%s", 
				tk2->name_tail
				? tk2->name_tail
				: tkname(tk2),
				tkmethod[tk2->type]->name
		);
//printf("%s: %d > %s\n", __func__, __LINE__, up->genbuf);
		if(tk2->slave) 
			tk2->is_container = 1;
		mkqid(&qid, (ulong)n | PATH(c->qid), c->qid.vers, (tk2->is_container) ? QTDIR : QTFILE);
		o = p->osenv;
		devdir(c, qid, up->genbuf, 0, o->user, (tk2->is_container) ? DMDIR|0777 : 0664, dp);
		return 1;
	}else{
		return -1;
	}

}


static int
guigen(Chan *c, char *name, Dirtab *tab, int ntab, int s, Dir *dp)
{
	Prog *p;
	char *e;
	ulong pid, path;

	int n;
	

	USED(ntab);

	if(s == DEVDOTDOT){
		p = progpid(PID(c->qid));
		if(p == nil){
			error(Enonexist);
		}

		return guigen_folder_back(/*Chan **/c, /*Prog **/p, /*Dir **/dp);
	}

	path = QID(c->qid); //PATH(c->qid) >> QSHIFT;
	
	acquire();
	
	if((ulong)c->qid.path == Qdir){
		if(name == nil){
			/* ignore s and use name to find pid */
			p = progn(s);
		}else{
			pid = strtoul(name, &e, 0);
			if(pid == 0 || *e != '\0'){
				release();
				return -1;
			}
			p = progpid(pid);
		}

	}else{
		p = progpid(PID(c->qid));
	}
	if(p == nil) {
		release();
		return -1;
	}

	if(name != nil)
		n = guigen_by_name(/*Chan **/c, /*Prog **/p, /*char **/name, /*Dir **/dp);
	else
		n = guigen_by_number(/*Chan **/c, /*Prog **/p, /*int*/ s, /*Dir **/dp);

	release();
	
	return n;

#if 0
		perm = tab->perm;
		if((perm & 7) == 0)
			perm = (perm|(perm>>3)|(perm>>6)) & o->pgrp->progmode;
		
		len = tab->length;
		mkqid(&qid, /*path|tab->qid.path*/ n, c->qid.vers, (tk2->slave) ? QTDIR : QTFILE);
		devdir(c, qid, tab->name, len, o->user, perm, dp);
		release();
		return 1;
#endif

}
// AA dinamic names-tree helper AA





static void
guiinit(void)
{
	//gui_file_init();
}


static void
guishutdown(void)
{
}


static Chan*
guiattach(char *spec)
{
	return devattach('G', spec);
}


static Walkqid*
guiwalk(Chan *c, Chan *nc, char **name, int nname)
{
	return devwalk(c, nc, name, nname, 0, 0, guigen);
}


static int
guistat(Chan *c, uchar *db, int n)
{
	return devstat(c, db, n, 0, 0, guigen);
}


static Chan*
guiopen(Chan *c, int omode)
{
	Prog *p;
	Osenv *o;
//	Progctl *ctl;
	int perm;

	if(c->qid.type & QTDIR)
		return devopen(c, omode, 0, 0, guigen);

	acquire();
	if (waserror()) {
		release();
		nexterror();
	}
	p = progpid(PID(c->qid));
	if(p == nil)
		error(Ethread);
	o = p->osenv;
	perm = 0777; //progdir[QID(c->qid)-1].perm;
	if((perm & 7) == 0)
		perm = (perm|(perm>>3)|(perm>>6)) & o->pgrp->progmode;
	devpermcheck(o->user, perm, omode);
	omode = openmode(omode);

	poperror();
	release();
	c->aux = nil;
	c->offset = 0;
	c->mode = omode;
	c->flag |= COPEN;
	return c;
}


static void
guiclose(Chan *c)
{
	GuiRead *state;
	if((c->flag & COPEN) == 0)
		return;
	acquire();
	guiresultclear(&c->qid);
	state=c->aux;
	if(state != nil) {
		free(state->data);
		free(state);
		c->aux=nil;
	}
	release();
}


void
guicreate(Chan *c, char *name, int mode, ulong perm)
{
	Prog *p;
	
	TkTop *t;
	Tk    *tk;

	char *rt, *type;

	Tk *tk2 = nil;	
	

	if(strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
		error(Efilename);
	
	if((ulong)c->qid.path != Qdir) {
		TkMethod *tkm;
		int i;
	
		type = nil;
		
		for(rt = name; *rt != '\0' && *rt != '.'; rt++);
		if(*rt){
			*rt = '\0';
			type = rt+1;
		}else{
			rt = nil;
		}
		if(type == nil){
			error(Eperm);
			return;
		}

		for(i=0, tkm = nil; i < TKwidgets; i++){
			tkm = tkmethod[i];
			if( !strcmp(tkm->name, type) )
				break;
		}
		if(tkm == nil){
			if(rt)
				*rt = '.';
			error(Eperm);
			return;
		}
	
		acquire();
		p = progpid(PID(c->qid));
		if(p == nil) {
			if(rt)
				*rt = '.';
			release();
			return;
		}
		t = p->tktop;

		if(!t){
			Draw_Display *dd = H;
			
			F_Tk_toplevel tf;
			F_Display_allocate df;
			F_Tk_putimage pif;

			void *tf_ret = H;
			void *pi_ret = H;

			char *ts;

			df.ret = &dd;
			df.dev = H;
			Display_allocate( &df );

			tf.ret = &tf_ret;
			tf.d = *df.ret;
			tf.arg = H;
			Tk_toplevel( &tf );

			t = *tf.ret;
			poolimmutable(D2H(t));

#if 0   // ?? need to configure wmctxt before 
			{
				Draw_Wmcontext *wmctxt = t->wmctxt;
				Sys_FD *connfd = nil;
				
				Array *a;
				void *w;
				
				if(wmctxt) 
					connfd = wmctxt->connfd;
				
				if(connfd != nil){
					Channel *c;
					
					if(fprint(connfd->fd, "!reshape . -1 0 0 50 40 place") == -1)
						return "err!"; //sprint("%r");
					
					//recvimage(top, name, reqid);
					/*
					i := <-top.ctxt.images;
					if(i == nil){
					        cmd(top, name + " suspend");
					        i = <-top.ctxt.images;
					}
					tk->putimage(top, name+" "+reqid, i, nil);
					*/

					c = wmctxt->images;

					// VVV read from Channel
					if((a = c->buf) != H)
						//return 0;
					//else
					if(c->size > 0){
						w = a->data+c->front*a->t->size;
						c->front++;
						if(c->front == c->buf->len)
							c->front = 0;
						c->size--;
						//R.s = w;
						//R.m = &c->mid;
						//R.d = v;
						c->mover();
						if(a->t->np){
							freeptrs(w, a->t);
							initmem(a->t, w);
						}
						if(w)
							t->di = *(void**)w;
					}
					// AAA write to Channel
					//return nil;
				}
			}
#endif
			/*
			if(top.image == nil){
					di := top.display.image;
					screen := Screen.allocate(di, top.display.color(Background), 0);
					di.draw(di.r, screen.fill, nil, screen.fill.r.min);
					i = screen.newwindow(di.r, Draw->Refbackup, Draw->Nofill);
			}else{
					if(name == ".")
							i = top.image;
					else
							i = top.image.screen.newwindow(s2r(req, e).t0, Draw->Refbackup, Draw->Red);
			}
			tk->putimage(top, name+" "+reqid, i, nil);
			*/
			if(t->di == H){
				Draw_Image *di = dd->image;
				Draw_Screen *ds = H;

				F_Screen_allocate	saf;
				F_Image_draw		idf;
				F_Screen_newwindow	snwf;

				saf.ret = &ds;
				saf.public = 0;
				saf.image = di;
				saf.fill = dd->opaque;
				Screen_allocate( &saf );

				idf.dst = di;
				idf.r = di->r;
				idf.src = ds->fill;
				idf.matte = H;
				idf.p = ds->fill->r.min;
				Image_draw( &idf );

				snwf.ret = &t->di;
				snwf.screen = ds;
				snwf.r = di->r;
				snwf.backing = 1;
				snwf.color = 0x0000ff;
				Screen_newwindow( &snwf );
			}else{
			}

			pif.ret = &pi_ret;
			pif.i = t->di;
			pif.t = t;
			pif.m = H;
			ts = ". -1";
			pif.name = c2string(ts, strlen(ts));
			Tk_putimage( &pif );
			
			p->tktop = t;
			t->prog = p;

//			libqunlock(
//				((struct DDisplay*)dd)->display->qlock
//			);
			//destroy(df_ret);
		}

		tk = qid2tk(&c->qid, t);
		if(tk == nil){
			if(rt)
				*rt = '.';
			error(Eperm);
			release();
			return;
		}

		{
			char buf[100];
			char *e, *rv;
			char *par_tkname = tkname(tk);
			
			if(par_tkname){
				snprint(buf, sizeof(buf), "%s %s%s%s", 
							type, 
							par_tkname, 
							(*par_tkname == '.' && *(char*)(par_tkname+1) == '\0') 
											? "" 
											: ".", 
							name
				);
				rv = nil;
				e = tksinglecmd(t, buf, &rv);
				if(!e){
					if(rv){
						LOGI("AFTER CR %s", rv);
						free(rv);
					}
					
					snprint(buf, sizeof(buf), "pack %s%s%s -side left",
								par_tkname, 
								(*par_tkname == '.' && *(char*)(par_tkname+1) == '\0') 
												? "" 
												: ".", 
								name
					);
					e = tksinglecmd(t, buf, nil);
					if(!e){
						tksinglecmd(t, "update", nil);

						for( tk2 = tk->slave; tk2; tk2 = tk2->next){
							if( !strcmp(name, 
									tk2->name_tail
									? tk2->name_tail
									: tkname(tk2) ) 
							){
								break;
							}
						}
						if(rt != nil)
							*rt = '.';
						if(tk2 == nil){
							release();
							return;
						}
						if(perm & DMDIR)
							tk2->is_container = 1;
					}			
				}
			}
		}

		if(tk2){
			int n = tk2idx(t, tk2);
			if(n == -1){
				release();
				return;
			}
			mkqid(&c->qid, (ulong)n | PATH(c->qid), c->qid.vers, (tk2->is_container) ? QTDIR : QTFILE);
			c->mode = openmode(mode);
			c->aux = nil;
			c->offset = 0;
			c->flag |= COPEN;
		}

		if(rt)
			*rt = '.';
		release();
		return;

		
//	o = p->osenv;
//	sprint(up->genbuf, "%lud", pid);
//	if(name != nil && strcmp(name, up->genbuf) != 0){
//		release();
//		return -1;
//	}
	}
}


void
guiremove(Chan *c)
{
	Prog *p;
	
	TkTop *t;
	Tk    *tk;
//	Tk *tk2 = nil;	

//	char *rt;
	char *name = c->name->s;

	if(strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
		error(Efilename);
	
	if((ulong)c->qid.path != Qdir) {
		acquire();
		p = progpid(PID(c->qid));
		if(p == nil) {
			release();
			return;
		}
		t = p->tktop;

		tk = qid2tk(&c->qid, t);
		if(tk == nil){
			release();
			error(Eperm);
			return;
		}
		else{
			char buf[100];
			char *e, *rv;
			char *par_tkname = tkname(tk);
			
			if(par_tkname){
				snprint(buf, sizeof(buf), "destroy %s", par_tkname );
				rv = nil;
				e = tksinglecmd(t, buf, &rv);
				if(!e){
					if(rv){
						free(rv);
					}
					tksinglecmd(t, "update", nil);
				}
			}
		}
	}

	release();
}


typedef struct GuiSnapshot GuiSnapshot;
struct GuiSnapshot {
	Fmt fmt;
	char *command;
	char *value;
};

static TkMethod*
guimethod(Tk *tk)
{
	if(tk == nil || (uint)tk->type >= TKwidgets || tkmethod[tk->type] == nil)
		error(Ebadarg);
	return tkmethod[tk->type];
}

static void
guiprint(GuiSnapshot *s, char *format, ...)
{
	va_list args;
	int n;
	va_start(args, format);
	n=fmtvprint(&s->fmt, format, args);
	va_end(args);
	if(n < 0)
		error(Enomem);
}

/* Build a complete snapshot independently of the size of a read request.
 * Mutable cleanup state lives on the heap across waserror/longjmp. */
static char*
guisnapshot(TkTop *top, Tk *tk)
{
	GuiSnapshot *s;
	TkMethod *method;
	TkOption **opts, *opt;
	TkCmdtab *cmd;
	Tk *slave;
	char *e, *result;
	int i, j;

	s=mallocz(sizeof(*s), 1);
	if(s == nil)
		error(Enomem);
	if(waserror()) {
		free(s->value);
		free(s->command);
		free(s->fmt.start);
		free(s);
		nexterror();
	}
	if(fmtstrinit(&s->fmt) < 0)
		error(Enomem);
	method=guimethod(tk);
	guiprint(s, "# type=%s\n", method->name);
	if(tk->name == nil)
		guiprint(s, "# name=%p\n", tk->name);
	else
		guiprint(s, "# name='%s'\n", tkname(tk));
	guiprint(s, "\n# Options:\n# --------\n\n");
	opts=method->opts;
	for(i=0; opts != nil && opts[i] != nil; i++) {
		for(j=0; opts[i][j].o != nil; j++) {
			opt=&opts[i][j];
			s->command=smprint("%s cget %s", tkname(tk), opt->o);
			if(s->command == nil)
				error(Enomem);
			e=tksinglecmd(top, s->command, &s->value);
			if(e == nil && s->value != nil)
				guiprint(s, "\"configure -%s \t%s\"\n", opt->o, s->value);
			free(s->command); s->command=nil;
			free(s->value); s->value=nil;
		}
		guiprint(s, "\n");
	}
	cmd=method->cmd;
	if(cmd != nil) {
		guiprint(s, "\n# Other Data:\n# -----------\n\n");
		for(i=0; cmd[i].name != nil; i++) {
			if(cmd[i].flag != TK_CMD_OUT || cmd[i].fn == nil)
				continue;
			e=cmd[i].fn(tk, "", &s->value);
			if(e == nil && s->value != nil)
				guiprint(s, "\"%s %s\"\n", cmd[i].name, s->value);
			free(s->value); s->value=nil;
		}
	}
	if(tk->slave != nil) {
		guiprint(s, "\n# Slaves:\n# -------\n\n");
		for(slave=tk->slave; slave != nil; slave=slave->next)
			guiprint(s, "# [%q %s]\n", tkname(slave), guimethod(slave)->name);
	}
	guiprint(s, "\n");
	if(tk->type == TKtext) {
		s->command=smprint("%s get 1.0 end", tkname(tk));
		if(s->command == nil)
			error(Enomem);
		e=tksinglecmd(top, s->command, &s->value);
		if(e == nil && s->value != nil)
			guiprint(s, "\"insert end '%s\"\n", s->value);
		free(s->command); s->command=nil;
		free(s->value); s->value=nil;
	}else if(tk->type == TKcanvas) {
		TkCanvas *canvas;
		TkCitem *item;
		TkCwind *window;
		char x[Tkminitem], y[Tkminitem];

		canvas=TKobj(TkCanvas, tk);
		guiprint(s, "\n# Canvas cmds:\n# ------------\n\n");
		tkfprint(x, canvas->width); tkfprint(y, canvas->height);
		guiprint(s, "configure -width %s -height %s\n", x, y);
		guiprint(s, "# focus %#p mouse %#p grab %#p\n\n", canvas->focus, canvas->mouse, canvas->grab);
		for(item=canvas->head; item != nil; item=item->next) {
			if((uint)item->type > TkCVarc || tkcimethod[item->type].name == nil)
				error(Ebadarg);
			guiprint(s, "create %q", tkcimethod[item->type].name);
			for(i=0; i<item->p.npoint; i++) {
				tkfprint(x, item->p.parampt[i].x); tkfprint(y, item->p.parampt[i].y);
				guiprint(s, " %s %s", x, y);
			}
			guiprint(s, "\n");
			if(item->type == TkCVwindow) {
				window=TKobj(TkCwind, item);
				if(window->sub != nil)
					guiprint(s, " -window %q", tkname(window->sub));
				guiprint(s, " # item %#p id %d sub %#p focus [%#p %q]\n",
					item, item->id, window->sub, window->focus, tkname(window->focus));
			}else
				guiprint(s, "# item %#p id %d\n", item, item->id);
		}
	}
	result=fmtstrflush(&s->fmt);
	if(result == nil)
		error(Enomem);
	s->fmt.start=nil;
	poperror();
	free(s);
	return result;
}

static long
guiread(Chan *c, void *va, long count, vlong offset)
{
	Prog *p;
	TkTop *top;
	Tk *tk;
	GuiRead *state;
	char *snapshot;
	long n;

	if(count < 0 || offset < 0)
		error(Ebadarg);
	if(count == 0)
		return 0;
	if(c->qid.type & QTDIR)
		return devdirread(c, va, count, 0, 0, guigen);
	acquire();
	if(waserror()) {
		release();
		nexterror();
	}
	n=guiresultread(&c->qid, va, count);
	if(n >= 0)
		goto Done;
	state=c->aux;
	if(state == nil || state->data == nil || offset == 0) {
		p=progpid(PID(c->qid));
		if(p == nil)
			error(Ethread);
		top=p->tktop;
		tk=top != nil ? qid2tk(&c->qid, top) : nil;
		if(tk == nil) {
			if(state != nil) {
				free(state->data); state->data=nil; state->len=0;
			}
			n=0;
			goto Done;
		}
		if(state == nil) {
			state=mallocz(sizeof(*state), 1);
			if(state == nil)
				error(Enomem);
			c->aux=state;
		}
		snapshot=guisnapshot(top, tk);
		free(state->data);
		state->data=snapshot;
		state->len=strlen(snapshot);
	}
	n=0;
	if(offset < state->len) {
		n=state->len-offset;
		if(n > count)
			n=count;
		memmove(va, state->data+offset, n);
	}
Done:
	poperror();
	release();
	return n;
}


static long
guiwrite(Chan *c, void *va, long count, vlong offset)
{
	Prog *p;
	int totl = 0;
	char *commands = nil;

	if(c->qid.type & QTDIR)
		error(Eisdir);

	/* A device write is length-delimited, not a NUL-terminated C string.
	 * Parse a private copy: callers may pass read-only or exactly-sized data.
	 */
	if(offset == 0 && count > 0){
		if(count >= 0x7fffffffL)
			error(Ebadarg);
		commands = malloc(count + 1);
		if(commands == nil)
			error(Enomem);
		memmove(commands, va, count);
		commands[count] = '\0';
	}
	acquire();
	if(waserror()) {
		free(commands);
		release();
		nexterror();
	}
	p = progpid(PID(c->qid));
	if(p == nil){
		error(Ethread);
	}
	
	if(offset == 0 && count > 0 && p->tktop){
		TkTop *t;
		Tk *tkr, *tk;
		int i, j, n;
		char *args[100], *rvs[100];
		char *va2 = commands;

		int pars = 0;
		int cmnt = 0;

		j = 0; rvs[0] = nil; args[0] = nil;
		
		for(i = 0, j = 0; i < count && va2[i] != '\0' && j < 100-1; i++){
			char c = va2[i];
			
			if(c == '"'){
				pars ^= 1;
				va2[i] = '\0';
			}else
			if(
				(!pars) && 
				(c == '\n' || c == '\r')
			){
				cmnt = 0;
				va2[i] = '\0';
				if(args[j] != nil){
					j++;
					args[j] = nil;
				}
			}else{
				if(args[j] == nil){
					if(c == '#' || cmnt){
						cmnt = 1;
					}else{
						args[j] = &va2[i];
						args[j+1] = nil;
					}
				}
			}
		}
		/* Reserve args[99] for the sentinel even when all slots are used. */
		if(j < 99 && args[j] != nil)
			j++;
		args[j] = nil;
	
		t = p->tktop;
		
		tkr = t->root;
		n = QID(c->qid);
		
		tk = qid2tk(&c->qid, t);
		if(tk == nil)
			goto Done;
		
		{
//			TkOption **opts = tkmethod[tk->type]->opts;
//			TkCmdtab *cmd = tkmethod[tk->type]->cmd;	
			rslt_string *r = nil;
			int rl = 0;

			for(i = 0, j = 0; args[i] != nil; i++){
				char *buf;
				int blen;
				char *rv = nil;
				char *arg = args[i];
				char *tknm = tkname(tk);
				
				blen = strlen( tknm ) + strlen( arg ) + 2 + 2;
				buf = malloc( blen );
				
				snprint(buf, blen, "%s %s", tknm, arg);

//				if( !
					tksinglecmd(t, buf, &rv);
//				)
//					tksinglecmd(t, "update", nil);
				free(buf);

				if(rv != nil){
					rl += strlen(rv) + 1;
					
					rvs[j] = rv;
					j++;
					rvs[j] = nil;
				}
			}
			tksinglecmd(t, "update -onscreen", nil);

			r = malloc(sizeof(rslt_string) + rl + 2);

			rl = 0;
			for(i = 0; rvs[i] != 0 && i < j; i++){
				int l = strlen( rvs[i] );
				
				memcpy(&(r->str[rl]), rvs[i], l);
				rl += l;
				r->str[rl] = '\n';
				rl++;

				free(rvs[i]);
			}
			r->str[rl] = '\0';
			
			//QIDsetRV(c->qid, r);
			r->qid = c->qid;
			r->len = rl;
			r->pos = 0;
			guiresultqueue(r);

			totl = count;
		}
	}
Done:
	poperror();
	free(commands);
	release();

	return totl;
}


static int
guiwstat(Chan *c, uchar *db, int n)
{
	Dir d;
	Prog *p;
	char *u;
	Osenv *o;

	if(c->qid.type & QTDIR)
		error(Eperm);
	
	acquire();
	p = progpid(PID(c->qid));
	if(p == nil) {
		release();
		error(Ethread);
	}

	u = up->env->user;
	o = p->osenv;
	if(strcmp(u, o->user) != 0 && strcmp(u, eve) != 0) {
		release();
		error(Eperm);
	}

	n = convM2D(db, n, &d, nil);
	if(n == 0){
		release();
		error(Eshortstat);
	}
	if(d.mode != ~0UL)
		o->pgrp->progmode = d.mode&0777;
	release();
	return n;
}


#if 0
static int
str2val(svp_t* t, char* s, ulong *v)
{
	if(t == nil || s == nil)
		return 0;
	for(; t->s != nil; t++) {
		if(strncmp(t->s, s, strlen(t->s)) == 0) {
			*v = t->v;
			return 1;
		}
	}
	return 0;
}

static char*
val2str(svp_t* t, ulong v)
{
	if(t == nil)
		return nil;
	for(; t->s != nil; t++)
		if(t->v == v)
			return t->s;
	return nil;
}

static int 
sval(char* buf, ulong* v, ulong max, ulong min)
{
	unsigned long val = strtoul(buf, 0, 10);

	if(val > max || val < min)
		return 0;
	*v = val;
	return 1;
}
#endif

Dev guidevtab = {
        'G',
        "gui",

        guiinit,
        guiattach,
        guiwalk,
        guistat,
        guiopen,
        guicreate,
        guiclose,
        guiread,
        devbread,
        guiwrite,
        devbwrite,
        guiremove,
        guiwstat
};

