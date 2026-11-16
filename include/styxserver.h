
#define Qroot	0

#define MSGMAX	((((8192+128)*2)+3) & ~3)

extern char Enomem[];	/* out of memory */
extern char Eperm[];		/* permission denied */
extern char Enodev[];	/* no free devices */
extern char Ehungup[];	/* i/o on hungup channel */
extern char Eexist[];		/* file exists */
extern char Enonexist[];	/* file does not exist */
extern char Ebadcmd[];	/* bad command */
extern char Ebadarg[];	/* bad arguments */

typedef uvlong	Path;
typedef struct Styxserver	Styxserver;
typedef struct Styxops Styxops;
typedef struct Styxfile Styxfile;
typedef struct Client Client;
typedef struct Fid Fid;
typedef struct StyxAttach StyxAttach;

struct Styxserver
{
	Styxops *ops;
	Path qidgen;
	int connfd;
	int needfile;
	Client *clients;
	Client *curc;
	Styxfile *root;
	Styxfile **ftab;
	void	*priv;	/* private */
};

struct Client
{
	Styxserver *server;
	Client *next;
	int		fd;
	char	msg[MSGMAX];
	uint		nread;		/* valid bytes in msg (including nc)*/
	int		nc;			/* bytes consumed from front of msg by convM2S */
	char	data[MSGMAX];	/* Tread/Rread data */
	int		state;
	uint		msize;	/* negotiated full-message bound */
	int		versioned;	/* negative rejects unsupported sessions */
	Fid		*fids;
	StyxAttach *attach;	/* staged/pending attach, owned by dispatcher */
	Fid		*current;	/* borrowed only during one dispatch */
	char		*uname;	/* uid */
	char		*aname;	/* attach name */
	void		*u;
};

struct Styxops
{
	char *(*newclient)(Client *c);
	char *(*freeclient)(Client *c);

	char *(*attach)(char *uname, char *aname);
	char *(*walk)(Qid *qid, char *name);
	char *(*open)(Qid *qid, int mode);
	char *(*create)(Qid *qid, char *name, int perm, int mode);
	char *(*read)(Qid qid, char *buf, ulong *n, vlong offset);
	char *(*write)(Qid qid, char *buf, ulong *n, vlong offset);
	char *(*close)(Qid qid, int mode);
	char *(*remove)(Qid qid);
	char *(*stat)(Qid qid, Dir *d);
	char *(*wstat)(Qid qid, Dir *d);
	void (*freefid)(void *aux);	/* consumes fid-owned native context, before freeclient */
};

struct Styxfile
{
	Dir	d;
	Styxfile *parent;
	Styxfile *child;
	Styxfile *sibling;
	Styxfile *next;
	int ref;
	int open;
	void	*u;
};

char *styxinit(Styxserver *server, Styxops *ops, char *port, int perm, int needfile);
char *styxwait(Styxserver *server);
char *styxprocess(Styxserver *server);
char *styxend(Styxserver *server);

Client *styxclient(Styxserver *server);

Styxfile *styxaddfile(Styxserver *server, Path pqid, Path qid, char *name, int mode, char *owner);
Styxfile *styxadddir(Styxserver *server, Path pqid, Path qid, char *name, int mode, char *owner);
int styxrmfile(Styxserver *server, Path qid);
Styxfile *styxfindfile(Styxserver *server, Path qid);

int	styxperm(Styxfile *file, char *uid, int mode);
long styxreadstr(ulong off, char *buf, ulong n, char *str);
Qid styxqid(Path path, int isdir);
void *styxmalloc(int n);
void styxfree(void *p);
void styxdebug(void);
void styxsetowner(char*);

/* Borrow current fid context; set transfers non-nil ownership only into an empty slot.
 * No automatic cloning: unopened walk clones start empty. Rebuild all consumers. */
void *styxgetaux(Styxserver *server);
int styxsetaux(Styxserver *server, void *aux);

/* Native owner-thread extension, no wire changes. Defer only inside attach callback.
 * Returned request is caller-owned; release with styxattachfree after retirement.
 * release(aux) runs once on success/error/flush/reset/disconnect, before freeclient.
 * Cleanup must not reenter dispatch/teardown or wait for external work.
 * These calls are serialized on the socket owner; worker threads signal a native
 * mailbox/fd instead of touching Client or request pointers. Rebuild consumers.
 * Native owner-thread API; defer только внутри attach callback, свой request.
 * release(aux) один раз при завершении/отмене; worker сообщает через mailbox/fd. */
StyxAttach *styxdeferattach(Styxserver*, void *aux, void (*release)(void*));
Client *styxattachclient(StyxAttach*); /* Borrow live pending client, nil after retirement. */
int styxattachreply(StyxAttach*, char *error); /* 1 consumed/attempted,0 retired/staged; nil Rattach. */
void styxattachfree(StyxAttach*); /* Drop caller ref; reply error before abandoning live work. */

/* POSIX borrowed readiness fd: owner registers fd or -1 to remove;0/-1.
 * Caller drains and closes it after unregister/end. Other ports return -1.
 * POSIX borrowed fd готовности; owner регистрирует, сам читает и закрывает. */
int styxnotifyfd(Styxserver*, int fd);
