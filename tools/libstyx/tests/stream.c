/* Copyright (c) 2026 bhgv
 * Original stream reader/writer with deterministic transport fragmentation.
 * Исходные чтение/запись с детерминированной фрагментацией транспорта. */
#define styxsend capture
#define styxrecv receive
#include "../styxserver.c"
#undef styxsend
#undef styxrecv

static uchar input[MSGMAX], output[MSGMAX];
static int available, consumed, produced, interruptsend;
static char longname[1201];
static int changed;

/* Supply only the bytes currently delivered by the native transport.
 * Выдать только уже поступившие байты транспорта. */
int
receive(Styxserver *s, int fd, char *buf, int n, int flags)
{
	USED(s); USED(fd); USED(flags);
	if(n > available-consumed)
		n = available-consumed;
	memmove(buf, input+consumed, n);
	consumed += n;
	return n;
}

/* Short writes and EINTR must not lose bytes or duplicate the prefix.
 * Короткая запись и EINTR не должны терять байты или повторять префикс. */
int
capture(Styxserver *s, int fd, char *buf, int n, int flags)
{
	USED(s); USED(fd); USED(flags);
	if(interruptsend){
		interruptsend = 0;
		errno = EINTR;
		return -1;
	}
	if(n > 3)
		n = 3;
	if(produced+n > sizeof(output))
		exit(2);
	memmove(output+produced, buf, n);
	produced += n;
	return n;
}

/* Explicit release-build check; failure terminates this isolated process.
 * Явная release-проверка; ошибка завершает изолированный процесс. */
static void
check(int ok, char *text)
{
	if(!ok){
		fprint(2, "stream: %s\n", text);
		exit(1);
	}
}

/* Return a long legal record without depending on a cached directory tree.
 * Вернуть длинную допустимую запись без кэшированного дерева каталогов. */
static char*
statrecord(Qid q, Dir *d)
{
	memset(d, 0, sizeof(*d));
	d->qid = q;
	d->name = longname;
	d->uid = d->gid = d->muid = "";
	return nil;
}

/* Confirm the decoded long name survived the bounded wstat string storage.
 * Проверить длинное имя после разбора в ограниченный буфер wstat. */
static char*
changerecord(Qid q, Dir *d)
{
	USED(q.path);
	changed = d->name != nil && strlen(d->name) == 1200;
	return nil;
}

/* Send a request directly through the original dispatcher and captured writer.
 * Передать запрос исходному dispatcher с перехватом отправки ответа. */
static Fcall
request(Client *c, Fcall *f)
{
	Fcall out;
	c->nc = 0;
	c->nread = convS2M(f, (uchar*)c->msg, sizeof(c->msg));
	check(c->nread > 0, "dispatch encode");
	c->state = CNREAD;
	produced = 0;
	run(c);
	check(convM2S(output, produced, &out) == produced, "dispatch decode");
	return out;
}

/* Exercise every split point, queued frames, malformed lengths and short replies.
 * Проверить все разрезы, очередь кадров, неверные размеры и короткую отправку. */
int
main(void)
{
	Client c;
	Fcall f, result;
	int n, split;
	Styxserver server;
	Styxops ops;
	Styxfile *table[TABSZ];
	Dir d;

	memset(&f, 0, sizeof(f));
	f.type = Tread;
	f.tag = 9;
	f.fid = 0xf1234567;
	f.offset = 0x123456789abcdefLL;
	f.count = 20;
	n = convS2M(&f, input, sizeof(input));
	check(n > 0, "encode");
	for(split = 1; split < n; split++){
		memset(&c, 0, sizeof(c));
		consumed = 0;
		available = split;
		c.state = CRECV;
		check(rd(&c, &result) == 0 && c.nread == split, "fragment discarded");
		available = n;
		c.state |= CRECV;
		check(rd(&c, &result) == 1 && result.fid == f.fid && result.offset == f.offset, "split decode");
	}
	memmove(input+n, input, n);
	memset(&c, 0, sizeof(c));
	consumed = 0;
	available = 2*n;
	c.state = CRECV;
	check(rd(&c, &result) == 1 && (c.state&CNREAD), "queued frame flag");
	check(rd(&c, &result) == 1 && !(c.state&CNREAD), "queued frame decode");
	check(rd(&c, &result) == 0, "empty buffered stream");
	memset(&c, 0, sizeof(c));
	PBIT32(input, MSGMAX+1);
	available = 4;
	consumed = 0;
	c.state = CRECV;
	check(rd(&c, &result) < 0 && (c.state&CDISC), "oversized frame");
	f.type = Rwrite;
	produced = 0;
	interruptsend = 1;
	memset(&c, 0, sizeof(c));
	n = wr(&c, &f);
	check(n == produced && produced > 0, "short write completion");
	check(convM2S(output, produced, &result) == produced && result.count == f.count, "short reply decode");
	memset(&server, 0, sizeof(server));
	memset(&ops, 0, sizeof(ops));
	memset(table, 0, sizeof(table));
	memset(&c, 0, sizeof(c));
	server.ftab = table;
	server.ops = &ops;
	c.server = &server;
	ops.stat = statrecord;
	ops.wstat = changerecord;
	memset(longname, 'n', 1200);
	newfid(&c, 42, styxqid(1, 0));
	memset(&f, 0, sizeof(f));
	f.type = Tstat;
	f.fid = 42;
	result = request(&c, &f);
	check(result.type == Rstat && result.nstat > 1200, "long stat truncated");
	statrecord(styxqid(1, 0), &d);
	f.type = Twstat;
	f.nstat = convD2M(&d, input, sizeof(input));
	f.stat = input;
	result = request(&c, &f);
	check(result.type == Rwstat && changed, "long wstat decode");
	deletefids(&c);
	print("Styx stream boundaries: passed\n");
	return 0;
}
