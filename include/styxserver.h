
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
typedef struct StyxRead StyxRead; /* Owned owner-thread deferred byte read. */
typedef struct StyxRead StyxWrite; /* Owned owner-thread deferred byte write. */

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
	StyxRead *reads; /* Owned pending byte I/O, at most128 per client. */
	StyxRead *reading; /* Borrowed stack stage during a byte I/O callback. */
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

/* EN: Copy the current wire fid number during a dispatcher callback.
 * RU: Скопировать номер текущего fid в callback диспетчера. */
int styxcurrentfid(Styxserver *, u32int *);


/* Borrow current fid context; set transfers non-nil ownership only into an empty slot.
 * No automatic cloning: unopened walk clones start empty. Rebuild all consumers. */
void *styxgetaux(Styxserver *server);
int styxsetaux(Styxserver *server, void *aux);
/* Attach an owned context and its individual destructor to the current fid. */
int styxsetauxowned(Styxserver *server, void *aux, void (*release)(void *));

/* Native owner-thread extension, no wire changes. Defer only inside attach callback.
 * Returned request is caller-owned; release with styxattachfree after retirement.
 * release(aux) runs once on success/error/flush/reset/disconnect, before freeclient.
 * Cleanup must not reenter dispatch/teardown or wait for external work.
 * These calls are serialized on the socket owner; worker threads signal a native
 * mailbox/fd instead of touching Client or request pointers. Rebuild consumers.
 * Native owner-thread API; defer только внутри attach callback, свой request.
 * release(aux) один раз при завершении/отмене; worker сообщает через mailbox/fd. */
/* EN/RU: Socket-owner root selection, pending NULL uses current attach;0/-1, no authority grant.
 * Выбор root на socket-owner; pending NULL — текущий attach;0/-1, без выдачи authority. */
int styxattachroot(Styxserver *server, StyxAttach *pending, Qid qid);

StyxAttach *styxdeferattach(Styxserver*, void *aux, void (*release)(void*));
Client *styxattachclient(StyxAttach*); /* Borrow live pending client, nil after retirement. */
int styxattachreply(StyxAttach*, char *error); /* 1 consumed/attempted,0 retired/staged; nil Rattach. */
void styxattachfree(StyxAttach*); /* Drop caller ref; reply error before abandoning live work. */

/* POSIX borrowed readiness fd: owner registers fd or -1 to remove;0/-1.
 * Caller drains and closes it after unregister/end. Other ports return -1.
 * POSIX borrowed fd готовности; owner регистрирует, сам читает и закрывает. */
int styxnotifyfd(Styxserver*, int fd);

/* Native owner-thread byte-read extension, no new wire messages. Defer only during
 * read callback, once per non-directory/nonzero read and at most one per fid.
 * Set *n=0 and return nil on success. Returned request is caller-owned; NULL
 * leaves aux caller-owned. At most128 pending reads per client. No buffers retained.
 * Worker threads pass copied data through native mailboxes/fds, never request pointers.
 * Native byte-read extension; только read callback, *n=0/nil при успехе, свой request.
 * Worker передаёт copied data через mailbox/fd; request доступен только owner-thread. */
StyxRead *styxdeferread(Styxserver*, void *aux, void (*release)(void*));

/* Borrow pending client/fid aux and exact capacity/offset only on native socket owner.
 * Client/aux become nil on retirement; zero limit then distinguishes inactivity.
 * Заимствовать client/fid aux, capacity/offset на owner; после retirement nil/zero. */
Client *styxreadclient(StyxRead*);
void *styxreadaux(StyxRead*);
ulong styxreadlimit(StyxRead*);
vlong styxreadoffset(StyxRead*);

/* Complete once; borrowed bytes copied during send, nil error means Rread.
 * Invalid count/buffer leaves request pending and returns0; inactive/staged returns0.
 * Return1 consumes live transaction/attempts response, not peer acknowledgement.
 * release(aux) runs once after detachment, before fid/client cleanup; never wait or
 * reenter dispatch/teardown. It may free the caller request ref. Flush/reset/disconnect
 * cancel without late replies; fid retirement sends Rerror before its clunk reply.
 * Завершить один раз; bytes borrowed до возврата.1 attempt,0 inactive/invalid/staged.
 * release(aux) один раз после detach до fid/client cleanup; без ожидания/reentry. */
int styxreadreply(StyxRead*, void *bytes, ulong count, char *error);

/* Consume caller ref on owner; freeing a live handle alone does not answer/cancel it.
 * Поглотить caller ref на owner; само освобождение не отвечает/не отменяет запрос. */
void styxreadfree(StyxRead*);


/* EN: Stage one deferred Twrite from the current write callback. On success
 * set its count to zero and return nil. Copy input before returning. The
 * request owns no payload. The shared byte I/O quota is 128 per client and
 * one per fid. Release aux once on retirement, on the server owner. On failure
 * return nil and leave aux with the caller.
 * RU: Отложить текущий Twrite из колбэка записи. При успехе обнулить count и
 * вернуть nil. Скопировать вход до возврата. Запрос не владеет данными.
 * Общий предел I/O равен 128 на клиента и одному на fid. Освободить aux
 * один раз у владельца при снятии. При отказе вернуть nil и сохранить aux. */
StyxWrite *styxdeferwrite(Styxserver *, void *aux, void (*release)(void *));


/* EN: Complete a waiting Twrite once on the dispatcher. With nil error,
 * count is the acknowledged prefix. A non-nil error requires count zero.
 * Return 1 when retired, 0 on invalid, staged or already retired requests.
 * RU: Однократно завершить ожидающий Twrite у диспетчера. При nil error
 * count задаёт подтверждённый префикс. При ошибке count равен нулю.
 * Вернуть 1 после снятия, 0 для неверного, ещё не принятого или снятого запроса. */
int styxwritereply(StyxWrite *, ulong count, char *error);


/* EN: Consume the caller's owned reference on the dispatcher. NULL is harmless.
 * RU: Освободить ссылку вызывающего у диспетчера. NULL допустим. */
void styxwritefree(StyxWrite *);
