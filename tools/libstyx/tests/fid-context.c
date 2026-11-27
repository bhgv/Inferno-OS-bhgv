/* Copyright (c) 2026 bhgv
 * Per-open native context ownership across clunk, reset and native shutdown.
 * Владение native-контекстом открытия при clunk, сбросе и завершении. */
#define styxsend capture
#include "../styxserver.c"
#undef styxsend

static Styxserver server;
static uchar reply[MSGMAX];
static int nreply, nextid, released;

/* Capture one real-codec reply without retaining caller storage.
 * Скопировать ответ настоящего кодека, не сохраняя чужой буфер. */
int
capture(Styxserver *s, int fd, char *buf, int n, int flags)
{
	USED(s); USED(fd); USED(flags);
	memmove(reply, buf, n);
	nreply = n;
	return n;
}

/* Fail explicitly in every build profile.
 * Явный отказ в каждом профиле сборки. */
static void
check(int ok, char *why)
{
	if(!ok){
		fprint(2, "fid context: %s\n", why);
		exit(1);
	}
}

/* Each opening allocates a separate owned context, even for the same Qid.
 * Каждое открытие создаёт отдельный контекст, даже для одинакового Qid. */
static char*
openfile(Qid *q, int mode)
{
	int *id;
	USED(q); USED(mode);
	id = malloc(sizeof(*id));
	check(id != nil, "allocate context");
	*id = ++nextid;
	check(styxsetaux(&server, id) == 0, "context adoption");
	check(styxsetaux(&server, id) < 0, "context replacement accepted");
	return nil;
}

/* Observe only the current fid's own native handle.
 * Получить native-handle только текущего fid. */
static char*
readfile(Qid q, char *buf, ulong *n, vlong offset)
{
	int *id = styxgetaux(&server);
	USED(q.path); USED(offset);
	check(id != nil, "missing current context");
	if(*n > 0){
		buf[0] = *id;
		*n = 1;
	}
	return nil;
}

/* Consume the context exactly once, before its client backend is released.
 * Поглотить контекст ровно один раз, до освобождения backend клиента. */
static void
freecontext(void *aux)
{
	check(aux != nil, "empty context cleanup");
	released++;
	free(aux);
}

/* Execute one encoded request, then ensure the borrowed scope ended.
 * Выполнить кодированный запрос и проверить завершение заимствования. */
static Fcall
request(Client *c, Fcall f)
{
	Fcall out;
	c->nc = 0;
	c->nread = convS2M(&f, (uchar*)c->msg, sizeof(c->msg));
	check(c->nread > 0, "encode");
	c->state = CNREAD;
	nreply = 0;
	server.curc = c;
	run(c);
	check(styxgetaux(&server) == nil, "context escaped dispatch");
	check(nreply > 0 && convM2S(reply, nreply, &out) == nreply, "decode");
	return out;
}

/* Two equal Qids must never alias context or double-close at reset/disconnect.
 * Два одинаковых Qid не разделяют контекст и не закрываются дважды. */
int
main(void)
{
	Styxops ops;
	Styxfile *table[TABSZ];
	Client c;
	Fcall f, out;
	int id;
	memset(&ops, 0, sizeof(ops));
	memset(&c, 0, sizeof(c));
	memset(table, 0, sizeof(table));
	server.ops = &ops;
	server.ftab = table;
	ops.open = openfile;
	ops.read = readfile;
	ops.freefid = freecontext;
	c.server = &server;
	newfid(&c, 1, styxqid(99, 0));
	newfid(&c, 2, styxqid(99, 0));
	memset(&f, 0, sizeof(f));
	f.type = Topen;
	f.mode = OREAD;
	f.fid = 1;
	out = request(&c, f);
	check(out.type == Ropen, "first open");
	f.fid = 2;
	out = request(&c, f);
	check(out.type == Ropen, "second open");
	f.type = Tread;
	f.count = 1;
	out = request(&c, f);
	id = (uchar)out.data[0];
	f.fid = 1;
	out = request(&c, f);
	check(out.type == Rread && (uchar)out.data[0] != id, "shared context between equal Qids");
	f.type = Tclunk;
	out = request(&c, f);
	check(out.type == Rclunk && released == 1, "clunk cleanup");
	f.type = Tversion;
	f.tag = NOTAG;
	f.msize = 1024;
	f.version = VERSION9P;
	out = request(&c, f);
	check(out.type == Rversion && released == 2, "reset cleanup");
	deletefids(&c);
	check(released == 2, "double cleanup");
	print("Styx per-fid native context: passed\n");
	return 0;
}
