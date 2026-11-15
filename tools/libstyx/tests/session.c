/* Copyright (c) 2026 bhgv
 * Original session dispatcher: negotiation, attach authority and fid lifetime.
 * Исходный dispatcher: согласование, полномочия attach и время жизни fid. */
#define styxsend capture
#define styxnewcall newcall
#define styxnewmsg newmsg
#define styxfreeclient forgetclient
#include "../styxserver.c"
#undef styxsend
#undef styxnewcall
#undef styxnewmsg
#undef styxfreeclient

static uchar reply[MSGMAX];
static int nreply, closed, freed, attached, capacity;
static Styxserver server;

/* No listener readiness is needed for a buffered malformed request.
 * Для ошибочного буферизованного запроса listener не нужен. */
int
newcall(Styxserver *s)
{
	USED(s);
	return 0;
}

/* This test supplies complete buffered frames, without socket readiness.
 * Тест подаёт готовые кадры без событий сокета. */
int
newmsg(Styxserver *s, int fd)
{
	USED(s); USED(fd);
	return 0;
}

/* The deterministic test has no platform readiness registration to release.
 * В тесте нет платформенной регистрации readiness для освобождения. */
void
forgetclient(Styxserver *s, int fd)
{
	USED(s); USED(fd);
}

/* Copy a whole native reply without retaining caller storage.
 * Копирует native-ответ, не сохраняя буфер вызывающей стороны. */
int
capture(Styxserver *s, int fd, char *buf, int n, int flags)
{
	USED(s); USED(fd); USED(flags);
	if(n <= 0 || n > sizeof(reply))
		exit(2);
	memmove(reply, buf, n);
	nreply = n;
	return n;
}

/* Keep every check active in release mode.
 * Сохраняет все проверки в release-режиме. */
static void
check(int ok, char *why)
{
	if(!ok){
		fprint(2, "session: %s\n", why);
		exit(1);
	}
}

/* Reject a fixture identity; packet names are not authentication.
 * Отказ тестовому имени; имя в пакете не является аутентификацией. */
static char*
attach(char *user, char *name)
{
	USED(name);
	attached++;
	return strcmp(user, "denied") == 0 ? user : nil;
}

/* Fill exactly the admitted response capacity.
 * Заполняет ровно разрешённый объём ответа. */
static char*
readfile(Qid q, char *buf, ulong *n, vlong offset)
{
	USED(q.path); USED(offset);
	capacity = *n;
	memset(buf, 'x', *n);
	return nil;
}

/* A legal record larger than negotiated msize must fail as a complete reply.
 * Допустимая запись больше msize должна вернуть целый ответ с ошибкой. */
static char*
statfile(Qid q, Dir *d)
{
	static char name[301];
	memset(name, 'n', 300);
	memset(d, 0, sizeof(*d));
	d->qid = q;
	d->name = name;
	d->uid = d->gid = d->muid = "";
	return nil;
}

/* Close must run while the owning client backend is alive.
 * Close выполняется, пока backend клиента ещё существует. */
static char*
closefile(Qid q, int mode)
{
	USED(q.path); USED(mode);
	check(styxclient(&server) != nil && styxclient(&server)->u != nil, "backend already released");
	closed++;
	return nil;
}

/* Release the backend only after all fids have been clunked.
 * Освободить backend только после закрытия всех fid. */
static char*
release(Client *c)
{
	check(c->fids == nil, "client released before fids");
	c->u = nil;
	freed++;
	return nil;
}

/* Dispatch a real encoded request and decode its copied native reply.
 * Выполнить настоящий кодированный запрос и разобрать копию native-ответа. */
static Fcall
request(Client *c, Fcall f)
{
	Fcall out;
	c->nc = 0;
	c->nread = convS2M(&f, (uchar*)c->msg, sizeof(c->msg));
	check(c->nread > 0, "request encode");
	c->state = CNREAD;
	nreply = 0;
	server.curc = c;
	run(c);
	check(nreply > 0 && convM2S(reply, nreply, &out) == nreply, "reply decode");
	check(out.tag == f.tag, "tag changed");
	return out;
}

/* Negotiate independent connections and exercise failures without leaking handles.
 * Согласовать независимые соединения и проверить отказы без утечек дескрипторов. */
int
main(void)
{
	Styxops ops;
	Styxfile *table[TABSZ];
	Client *a, *b;
	Client bad;
	Fcall f, out;
	Fid *fid;
	int before;

	memset(&ops, 0, sizeof(ops));
	memset(table, 0, sizeof(table));
	server.ops = &ops;
	server.ftab = table;
	ops.attach = attach;
	ops.read = readfile;
	ops.stat = statfile;
	ops.close = closefile;
	ops.freeclient = release;
	a = newclient(&server, -1);
	b = newclient(&server, -1);
	a->u = b->u = &server;
	memset(&f, 0, sizeof(f));
	f.type = Tattach;
	f.tag = 1;
	f.fid = 3;
	f.afid = NOFID;
	f.uname = "alice";
	f.aname = "";
	out = request(a, f);
	check(out.type == Rerror && a->fids == nil && attached == 0, "attach before version");
	f.type = Tversion;
	f.tag = NOTAG;
	f.msize = 256;
	f.version = "9P2000.u";
	out = request(a, f);
	check(out.type == Rversion && out.msize == 256 && strcmp(out.version, VERSION9P) == 0, "negotiation");
	f.msize = 32;
	out = request(a, f);
	check(out.type == Rerror && a->msize == 256, "invalid msize accepted");
	f.msize = MSGMAX+100;
	out = request(b, f);
	check(out.msize == MSGMAX && a->msize == 256, "independent limits");
	f.type = Tattach;
	f.tag = 1;
	f.fid = 7;
	f.afid = NOFID;
	f.uname = "denied";
	f.aname = "private";
	out = request(a, f);
	check(out.type == Rerror && a->fids == nil && strcmp(a->uname, eve) == 0 && a->aname[0] == 0, "failed attach leaked state");
	f.uname = "alice";
	out = request(a, f);
	check(out.type == Rattach && findfid(a, 7) != nil, "attach");
	f.fid = 8;
	f.uname = "bob";
	before = attached;
	out = request(a, f);
	check(out.type == Rerror && attached == before && strcmp(a->uname, "alice") == 0, "existing authority replaced");
	f.type = Tauth;
	out = request(a, f);
	check(out.type == Rerror, "fabricated authentication qid");
	fid = newfid(a, 42, styxqid(42, 0));
	fid->open = 1;
	fid->mode = OREAD;
	f.type = Tread;
	f.fid = 42;
	f.count = 4096;
	f.offset = 0;
	out = request(a, f);
	check(out.type == Rread && nreply <= 256 && capacity == 256-IOHDRSZ, "read exceeds msize");
	f.type = Tstat;
	out = request(a, f);
	check(out.type == Rerror && nreply <= 256, "stat exceeds msize");
	f.type = Tversion;
	f.msize = 256;
	f.version = VERSION9P;
	out = request(a, f);
	check(out.type == Rerror && findfid(a, 42) == fid, "bad tag reset session");
	f.tag = NOTAG;
	f.version = "9P2099";
	out = request(a, f);
	check(out.type == Rversion && strcmp(out.version, "unknown") == 0 && a->fids == nil && closed == 1, "unknown session reset");
	f.type = Tattach;
	f.fid = 9;
	f.uname = "alice";
	out = request(a, f);
	check(out.type == Rerror && a->fids == nil, "unsupported version accepted attach");
	f.type = Tversion;
	f.version = VERSION9P;
	out = request(a, f);
	check(out.type == Rversion && a->versioned == 1, "renegotiation");
	memset(&bad, 0, sizeof(bad));
	bad.msize = 256;
	bad.nread = 5;
	PBIT32((uchar*)bad.msg, 257);
	bad.msg[4] = Twrite;
	check(rd(&bad, &out) < 0 && (bad.state&CDISC), "oversized incoming frame");
	fid = newfid(b, 99, styxqid(99, 0));
	fid->open = 1;
	freeclient(a);
	b->nread = 5;
	b->nc = 0;
	PBIT32((uchar*)b->msg, MSGMAX+1);
	b->msg[4] = Twrite;
	b->state = CNREAD;
	styxprocess(&server);
	check(server.curc == nil, "dangling current client");
	check(closed == 2 && freed == 2 && server.clients == nil, "disconnect lifetime");
	print("Styx session boundaries: passed\n");
	return 0;
}
