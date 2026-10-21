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



/* Qid.path: PID in bits 32..63, control flag in bit 31, stable widget
 * identity in bits 0..30. Identity zero denotes the process directory. */
#define QSHIFT 32
#define QID(q) ((uint)((q).path & 0x7fffffffUL))
#define QPID(pid) ((uvlong)(pid)<<QSHIFT)
#define PID(q) ((q).vers)
#define PATH(q) ((q).path & ~(((uvlong)1<<QSHIFT)-1))
#define QCTL_NAME ((uint)0x80000000UL)

/* Protected by acquire/release in read, write and close. */
typedef struct _rslt_string rslt_string;
struct _rslt_string {
	rslt_string *next;
	Chan *owner;	/* replies belong to one open channel */
	Qid qid;
	long len;
	long pos;
	char str[1];
};

static rslt_string *root_rslt_strings;

static int
guisameresult(rslt_string *r, Chan *c)
{
	return r->owner == c && r->qid.path == c->qid.path && r->qid.vers == c->qid.vers;
}

/* -1 means no reply is pending; 0 may be an empty completed reply. */
static long
guiresultread(Chan *c, void *va, long count)
{
	rslt_string **link, *r;
	long n;

	for(link=&root_rslt_strings; (r=*link) != nil; link=&r->next) {
		if(!guisameresult(r, c))
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
guiresultqueue(Chan *c, rslt_string *r)
{
	rslt_string **link;
	for(link=&root_rslt_strings; *link != nil; link=&(*link)->next)
		;
	r->owner=c;
	r->qid=c->qid;
	r->next=nil;
	*link=r;
}

static void
guiresultclear(Chan *c)
{
	rslt_string **link, *r;
	for(link=&root_rslt_strings; (r=*link) != nil; ) {
		if(guisameresult(r, c)) {
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

static TkMethod *guimethod(Tk*);

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



/* Protected by the VM lock. Exhaustion fails instead of reusing an old Qid. */
static uint guinextid;

static Tk*
qid2tk(Qid *qid, TkTop *t)
{
	Tk *tk;
	uint id;
	if(t == nil)
		return nil;
	id=QID(*qid);
	if(id == 0)
		return t->root; /* live process directory, not a particular widget */
	for(tk=t->root; tk != nil; tk=tk->siblings)
		if(tk->guiqid == id)
			return tk;
	return nil;
}

static int
tk2id(TkTop *t, Tk *tk)
{
	Tk *p;
	if(t == nil || tk == nil)
		return -1;
	for(p=t->root; p != nil; p=p->siblings)
		if(p == tk) {
			if(tk->guiqid == 0) {
				if(guinextid == 0x7fffffffUL)
					error(Etoobig);
				tk->guiqid=++guinextid;
			}
			return tk->guiqid;
		}
	return -1;
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

	n = tk2id(t, tk);
	if(n == -1){
		error(Enonexist);
	}

	if(strlen(tk->name_tail ? tk->name_tail : tkname(tk)) >= sizeof(up->genbuf))
		error(Etoobig);
	snprint(up->genbuf, sizeof(up->genbuf), "%s",
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
	if(c->qid.path == Qdir) {
		pid = strtoul(name, &e, 0);  // Top dir of devgui is a list of numbers of gui-procs. test this
		if(pid == 0 || pid > 0x7fffffffUL || *e != '\0')
			return -1;
		p = progpid(pid);
		if(p == nil){
			return -1;
		}
		o = p->osenv;
	
		// common part
		snprint(up->genbuf, sizeof(up->genbuf), "%lud", pid);  // up->genbuf is a small (127 bytes) per-process character buffer
		if(name != nil && strcmp(name, up->genbuf) != 0){
			return -1;
		}
		mkqid(&qid, (uvlong)pid << QSHIFT, pid, QTDIR);  // in the Top dir of devgui all the entryes are folders
		devdir(c, qid, up->genbuf, 0, o->user, DMDIR|0777, dp);
		return 1;
	}

	path = QID(c->qid);

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
			char *dot, *childname;
			int length;
			dot=strchr(name, '.');
			length=dot != nil ? dot-name : strlen(name);
			for(tk2=tk->slave; tk2 != nil; tk2=tk2->next) {
				childname=tk2->name_tail != nil ? tk2->name_tail : tkname(tk2);
				if(strlen(childname) == length && strncmp(name,childname,length) == 0)
					break;
			}
			if(tk2 == nil)
				return -1;

			n = tk2id(t, tk2);
			if(n == -1){
				return -1;
			}
		}else{
			n = 0;
		}
	}

	if(n == 0){
		path=tk2id(t, tk);
		strcpy(up->genbuf, ".self");
		if(name != nil && strcmp(name, up->genbuf) != 0){
			return -1;
		}
		mkqid(&qid, (ulong)path | PATH(c->qid) | QCTL_NAME, c->qid.vers, QTFILE);
		o = p->osenv;
		devdir(c, qid, up->genbuf, 0, o->user, 0664, dp);
		return 1;
		
	}else if(tk2){
		if(strlen(tk2->name_tail ? tk2->name_tail : tkname(tk2)) +
		   strlen(guimethod(tk2)->name) + 1 >= sizeof(up->genbuf))
			error(Etoobig);
		snprint(up->genbuf, sizeof(up->genbuf), "%s.%s",
				tk2->name_tail
				? tk2->name_tail
				: tkname(tk2),
				guimethod(tk2)->name
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
	if(c->qid.path == Qdir) {
		p = progn(s);
		if(p == nil) {
			return -1;
		}
		pid = p->pid;
		o = p->osenv;
	
		// common part
		snprint(up->genbuf, sizeof(up->genbuf), "%lud", pid);  // up->genbuf is a small (127 bytes) per-process character buffer
//printf("%s: %d > %s\n", __func__, __LINE__, up->genbuf);
		mkqid(&qid, (uvlong)pid << QSHIFT, pid, QTDIR);  // in the Top dir of devgui all the entryes are folders
		devdir(c, qid, up->genbuf, 0, o->user, DMDIR|0777, dp);
		return 1;
	}

	path = QID(c->qid);

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

			n = tk2id(t, tk2);
			if(n == -1){
				return -1;
			}
		}else{
			n = 0;
		}
	}
	
	/* .self file controls the folder (widget-container) properties. n==0 */
	if(n == 0){
		path=tk2id(t, tk);
		strcpy(up->genbuf, ".self");
		mkqid(&qid, (ulong)path | PATH(c->qid) | QCTL_NAME, c->qid.vers, QTFILE);
		o = p->osenv;
		devdir(c, qid, up->genbuf, 0, o->user, 0664, dp);
		return 1;
		
	}else if(tk2){
		if(strlen(tk2->name_tail ? tk2->name_tail : tkname(tk2)) +
		   strlen(guimethod(tk2)->name) + 1 >= sizeof(up->genbuf))
			error(Etoobig);
		snprint(up->genbuf, sizeof(up->genbuf), "%s.%s",
				tk2->name_tail
				? tk2->name_tail
				: tkname(tk2),
				guimethod(tk2)->name
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
	char *end;
	ulong pid;
	int n;
	Qid qid;

	USED(tab); USED(ntab);
	acquire();
	if(waserror()) {
		release();
		nexterror();
	}
	n=-1;
	if(s == DEVDOTDOT) {
		if(c->qid.path == Qdir) {
			mkqid(&qid, Qdir, 0, QTDIR);
			devdir(c, qid, "#G", 0, eve, DMDIR|0555, dp);
			n=1;
		}else{
			p=progpid(PID(c->qid));
			if(p == nil)
				error(Enonexist);
			n=guigen_folder_back(c, p, dp);
		}
		goto Done;
	}
	if(s < 0)
		goto Done;
	if(c->qid.path == Qdir) {
		if(name == nil)
			p=progn(s);
		else{
			pid=strtoul(name, &end, 0);
			if(pid == 0 || pid > 0x7fffffffUL || *end != '\0')
				goto Done;
			p=progpid(pid);
		}
	}else
		p=progpid(PID(c->qid));
	if(p != nil)
		n=name != nil ? guigen_by_name(c,p,name,dp) : guigen_by_number(c,p,s,dp);
Done:
	poperror();
	release();
	return n;
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
	guiresultclear(c);
	state=c->aux;
	if(state != nil) {
		free(state->data);
		free(state);
		c->aux=nil;
	}
	release();
}


typedef struct GuiChange GuiChange;
struct GuiChange {
	int locked;
	char *name, *path, *command, *value;
	Draw_Display *display;
	Draw_Screen *screen;
	Draw_Image *image;
	String *label, *message;
};

static GuiChange*
guichangenew(void)
{
	GuiChange *s;
	s=mallocz(sizeof(*s), 1);
	if(s == nil)
		error(Enomem);
	s->display=H; s->screen=H; s->image=H;
	s->label=H; s->message=H;
	return s;
}

static void
guichangefree(GuiChange *s)
{
	int locked;
	free(s->name); free(s->path); free(s->command); free(s->value);
	destroy(s->message); destroy(s->label);
	destroy(s->image); destroy(s->screen); destroy(s->display);
	locked=s->locked;
	free(s);
	if(locked)
		release();
}

/* The module APIs below create/cache the current Prog's GUI. Never attach
 * that GUI to a different target Prog; an existing remote top is usable. */
static TkTop*
guimaketop(Prog *p, GuiChange *s)
{
	F_Display_allocate display={0};
	F_Tk_toplevel toplevel={0};
	F_Screen_allocate screen={0};
	F_Screen_newwindow window={0};
	F_Image_draw draw={0};
	F_Tk_putimage put={0};
	Tk_Toplevel *result=H;
	TkTop *top;
	Draw_Image *image;

	if(p != currun())
		error(Eperm);
	display.ret=&s->display; display.dev=H;
	Display_allocate(&display);
	if(s->display == H || s->display == nil)
		error(Eio);
	toplevel.ret=&result; toplevel.d=s->display; toplevel.arg=H;
	Tk_toplevel(&toplevel);
	if(result == H || result == nil)
		error(Enomem);
	top=(TkTop*)result;
	if(p->tktop != top)
		error(Eio);
	poolimmutable(D2H(top));
	if(top->di == H || top->di == nil) {
		image=s->display->image;
		if(image == H || image == nil || s->display->opaque == H || s->display->opaque == nil)
			error(Eio);
		screen.ret=&s->screen; screen.image=image;
		screen.fill=s->display->opaque; screen.public=0;
		Screen_allocate(&screen);
		if(s->screen == H || s->screen == nil)
			error(Enomem);
		draw.dst=image; draw.r=image->r; draw.src=s->screen->fill;
		draw.matte=H; draw.p=s->screen->fill->r.min;
		Image_draw(&draw);
		window.ret=&s->image; window.screen=s->screen;
		window.r=image->r; window.backing=1; window.color=0x0000ff;
		Screen_newwindow(&window);
		if(s->image == H || s->image == nil)
			error(Enomem);
		top->di=s->image; s->image=H; /* owned by the published top */
	}
	s->label=c2string(". -1", 4);
	if(s->label == H || s->label == nil)
		error(Enomem);
	put.ret=&s->message; put.i=top->di; put.t=(Tk_Toplevel*)top;
	put.m=H; put.name=s->label;
	Tk_putimage(&put);
	if(s->message != H && s->message != nil)
		error(string2c(s->message));
	return top;
}

void
guicreate(Chan *c, char *name, int mode, ulong perm)
{
	GuiChange *s;
	Prog *p;
	TkTop *top;
	Tk *parent, *created;
	char *type, *e, *parentname;
	int i, index;

	if(name == nil || strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
		error(Efilename);
	if(c->qid.path == Qdir)
		error(Eperm);
	mode=openmode(mode);
	if(strlen(name) >= sizeof(up->genbuf))
		error(Etoobig);
	s=guichangenew();
	if(waserror()) {
		guichangefree(s);
		nexterror();
	}
	s->name=smprint("%s", name);
	if(s->name == nil)
		error(Enomem);
	type=strchr(s->name, '.');
	if(type == nil || type == s->name || type[1] == '\0')
		error(Eperm);
	*type++='\0';
	for(i=0; i<TKwidgets; i++)
		if(tkmethod[i] != nil && strcmp(tkmethod[i]->name, type) == 0)
			break;
	if(i == TKwidgets)
		error(Eperm);
	acquire(); s->locked=1;
	p=progpid(PID(c->qid));
	if(p == nil)
		error(Ethread);
	top=p->tktop;
	if(top == nil)
		top=guimaketop(p, s);
	parent=qid2tk(&c->qid, top);
	if(parent == nil)
		error(Enonexist);
	parentname=tkname(parent);
	s->path=smprint("%s%s%s", parentname, strcmp(parentname,".")==0 ? "" : ".", s->name);
	if(s->path == nil)
		error(Enomem);
	s->command=smprint("%s %s", type, s->path);
	if(s->command == nil)
		error(Enomem);
	e=tksinglecmd(top, s->command, &s->value);
	if(e != nil)
		error(e);
	free(s->command); s->command=nil;
	free(s->value); s->value=nil;
	if(p->tktop != top)
		error(Enonexist);
	s->command=smprint("pack %s -side left", s->path);
	if(s->command == nil)
		error(Enomem);
	e=tksinglecmd(top, s->command, nil);
	if(e != nil)
		error(e);
	if(p->tktop != top)
		error(Enonexist);
	e=tksinglecmd(top, "update", nil);
	if(e != nil)
		error(e);
	if(p->tktop != top || (created=tklook(top, s->path, 0)) == nil)
		error(Enonexist);
	index=tk2id(top, created);
	if(index < 0)
		error(Etoobig);
	if(perm & DMDIR)
		created->is_container=1;
	mkqid(&c->qid, (uvlong)index|PATH(c->qid), c->qid.vers, created->is_container ? QTDIR : QTFILE);
	c->mode=mode; c->aux=nil; c->offset=0; c->flag|=COPEN;
	poperror();
	guichangefree(s);
}

void
guiremove(Chan *c)
{
	GuiChange *s;
	GuiRead *cached;
	Prog *p;
	TkTop *top;
	Tk *tk;
	char *e;

	if(c->name == nil || strcmp(c->name->s,".")==0 || strcmp(c->name->s,"..")==0)
		error(Efilename);
	if(c->qid.path == Qdir)
		error(Eperm);
	s=guichangenew();
	if(waserror()) {
		guichangefree(s);
		nexterror();
	}
	acquire(); s->locked=1;
	p=progpid(PID(c->qid));
	if(p == nil)
		error(Ethread);
	top=p->tktop;
	tk=top != nil ? qid2tk(&c->qid, top) : nil;
	if(tk == nil)
		error(Enonexist);
	s->command=smprint("destroy %s", tkname(tk));
	if(s->command == nil)
		error(Enomem);
	e=tksinglecmd(top, s->command, &s->value);
	if(e != nil)
		error(e);
	if(p->tktop == top) {
		e=tksinglecmd(top, "update", nil);
		if(e != nil)
			error(e);
	}
	guiresultclear(c);
	cached=c->aux;
	if(cached != nil) {
		free(cached->data); cached->data=nil; cached->len=0;
	}
	poperror();
	guichangefree(s);
}

typedef struct GuiSnapshot GuiSnapshot;
struct GuiSnapshot {
	Fmt fmt;
	char *command;
	char *value;
	char *wire;
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

/* Encode the command envelope understood by guiparse. Preserve literal
 * quotes, backslashes and embedded newlines from Tk getter results. */
static void
guicommand(GuiSnapshot *s, char *format, ...)
{
	va_list args;
	char *p, *start;
	int c;
	va_start(args, format);
	s->wire=vsmprint(format, args);
	va_end(args);
	if(s->wire == nil)
		error(Enomem);
	guiprint(s, "\"");
	start=s->wire;
	for(p=start; *p != '\0'; p++) {
		if(*p != '\\' && *p != '"')
			continue;
		c=*p; *p='\0';
		guiprint(s, "%s\\%c", start, c);
		*p=c; start=p+1;
	}
	guiprint(s, "%s\"\n", start);
	free(s->wire); s->wire=nil;
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
		free(s->wire);
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
				guicommand(s, "configure -%s \t%s", opt->o, s->value);
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
				guicommand(s, "%s %s", cmd[i].name, s->value);
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
			guicommand(s, "insert end '%s", s->value);
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
	n=guiresultread(c, va, count);
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


typedef struct GuiWrite GuiWrite;
struct GuiWrite {
	char *commands;
	char *name;
	char *buffer;
	char *values[100];
	rslt_string *reply;
};

static void
guifreewrite(GuiWrite *s)
{
	int i;
	for(i=0; i<nelem(s->values); i++)
		free(s->values[i]);
	free(s->commands);
	free(s->name);
	free(s->buffer);
	free(s->reply);
	free(s);
}

/* Raw lines are forwarded verbatim. A leading double quote wraps a whole
 * command, including newlines; only escaped quote/backslash are decoded. */
static int
guiparse(char *text, char **args, int capacity)
{
	char *p, *start, *out;
	int n;

	p=text;
	n=0;
	while(*p != '\0') {
		while(*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
			p++;
		if(*p == '\0')
			break;
		if(*p == '#') {
			while(*p != '\0' && *p != '\r' && *p != '\n')
				p++;
			continue;
		}
		if(*p == '"') {
			start=out=++p;
			while(*p != '\0' && *p != '"') {
				if(*p == '\\' && (p[1] == '"' || p[1] == '\\'))
					p++;
				*out++=*p++;
			}
			if(*p != '"')
				error(Ebadarg);
			p++;
			while(*p == ' ' || *p == '\t')
				p++;
			if(*p != '\0' && *p != '\r' && *p != '\n')
				error(Ebadarg);
			/* Advance before terminating a non-escaped quoted command. */
			if(*p != '\0')
				p++;
			*out='\0';
		}else{
			start=p;
			while(*p != '\0' && *p != '\r' && *p != '\n')
				p++;
			if(*p != '\0')
				*p++='\0';
		}
		if(*start != '\0') {
			if(n >= capacity-1)
				error(Etoobig);
			args[n++]=start;
		}
	}
	args[n]=nil;
	return n;
}

static long
guiwrite(Chan *c, void *va, long count, vlong offset)
{
	GuiWrite *s;
	GuiRead *cached;
	Prog *p;
	TkTop *top;
	Tk *tk;
	char *args[100], *e;
	ulong length, total, limit;
	long result;
	int i, j, n;

	if(c->qid.type & QTDIR)
		error(Eisdir);
	if(count < 0 || count >= 0x7fffffffL || offset < 0)
		error(Ebadarg);
	if(count == 0)
		return 0;
	s=mallocz(sizeof(*s), 1);
	if(s == nil)
		error(Enomem);
	acquire();
	if(waserror()) {
		guifreewrite(s);
		release();
		nexterror();
	}
	if(memchr(va, '\0', count) != nil)
		error(Ebadarg);
	s->commands=malloc(count+1);
	if(s->commands == nil)
		error(Enomem);
	memmove(s->commands, va, count);
	s->commands[count]='\0';
	n=guiparse(s->commands, args, nelem(args));
	p=progpid(PID(c->qid));
	if(p == nil)
		error(Ethread);
	result=0;
	top=p->tktop;
	tk=top != nil ? qid2tk(&c->qid, top) : nil;
	if(tk == nil)
		goto Done;
	if(n == 0) {
		result=count;
		goto Done;
	}
	s->name=smprint("%s", tkname(tk));
	if(s->name == nil)
		error(Enomem);
	total=0;
	j=0;
	limit=0x7fffffffUL-sizeof(rslt_string)-1;
	for(i=0; i<n; i++) {
		if(p->tktop != top || qid2tk(&c->qid, top) != tk)
			error(Enonexist);
		s->buffer=smprint("%s %s", s->name, args[i]);
		if(s->buffer == nil)
			error(Enomem);
		e=tksinglecmd(top, s->buffer, &s->values[j]);
		if(e != nil)
			error(e);
		free(s->buffer); s->buffer=nil;
		if(s->values[j] != nil) {
			length=strlen(s->values[j]);
			if(total >= limit || length > limit-total-1)
				error(Etoobig);
			total+=length+1;
			j++;
		}
	}
	if(p->tktop != top || qid2tk(&c->qid, top) != tk)
		error(Enonexist);
	e=tksinglecmd(top, "update -onscreen", nil);
	if(e != nil)
		error(e);
	if(p->tktop != top || qid2tk(&c->qid, top) != tk)
		error(Enonexist);
	s->reply=malloc(sizeof(rslt_string)+total);
	if(s->reply == nil)
		error(Enomem);
	total=0;
	for(i=0; i<j; i++) {
		length=strlen(s->values[i]);
		memmove(s->reply->str+total, s->values[i], length);
		total+=length;
		s->reply->str[total++]='\n';
	}
	s->reply->str[total]='\0';
	s->reply->qid=c->qid;
	s->reply->len=total;
	s->reply->pos=0;
	guiresultqueue(c, s->reply);
	s->reply=nil;
	cached=c->aux;
	if(cached != nil) {
		free(cached->data); cached->data=nil; cached->len=0;
	}
	result=count;
Done:
	poperror();
	guifreewrite(s);
	release();
	return result;
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

