/* Copyright (c) 2026 bhgv
 * Real POSIX teardown and native fd ownership with a deterministic announce boundary.
 * Настоящее POSIX-завершение и владение fd с детерминированной границей announce. */
#define styxannounce announcefixture
#define styxinitsocket initfixture
#include "../styxserver.c"
#undef styxannounce
#undef styxinitsocket

static int listener, denied, initdenied, closed, released;
static Styxserver server;

/* Simulate socket initialization failure without changing platform globals.
 * Смоделировать отказ инициализации сокетов без изменения глобального состояния. */
int
initfixture(void)
{
	return initdenied ? -1 : 0;
}

/* A real owned native fd replaces only listener creation; no TCP claim is made.
 * Настоящий fd заменяет только создание listener; это не проверка TCP. */
int
announcefixture(Styxserver *s, char *port)
{
	USED(s); USED(port);
	listener = denied ? -1 : dup(0);
	return listener;
}

/* Explicit checks remain active in release builds.
 * Явные проверки работают в release-сборке. */
static void
check(int ok, char *why)
{
	if(!ok){
		fprint(2, "lifecycle: %s\n", why);
		exit(1);
	}
}

/* Clunk must precede backend release and see the proper current client.
 * Clunk предшествует освобождению backend и видит правильного клиента. */
static char*
closehandle(Qid q, int mode)
{
	USED(q.path); USED(mode);
	check(styxclient(&server) != nil && styxclient(&server)->u != nil, "backend released before close");
	closed++;
	return nil;
}

/* The backend owns its private state until all fids have gone.
 * Backend владеет своим состоянием до освобождения всех fid. */
static char*
release(Client *c)
{
	check(c->fids == nil && c->u != nil, "invalid release order");
	c->u = nil;
	released++;
	return nil;
}

/* Cover nested roots, clients, native fds, repeat shutdown and failed initialization.
 * Проверить вложенное дерево, клиентов, fd, повторное закрытие и отказы init. */
int
main(void)
{
	Styxops ops;
	Client *client;
	Fid *fid;
	int a, b, owner;

	memset(&server, 0, sizeof(server));
	memset(&ops, 0, sizeof(ops));
	ops.close = closehandle;
	ops.freeclient = release;
	check(styxinit(&server, &ops, "0", 0555, 1) == nil, "init");
	owner = listener;
	check(owner >= 0 && server.priv != nil, "native owner");
	check(styxadddir(&server, Qroot, 1, "nested", 0555, "alice") != nil, "nested dir");
	check(styxaddfile(&server, 1, 2, "data", 0444, "alice") != nil, "nested file");
	a = dup(0);
	b = dup(0);
	check(a >= 0 && b >= 0, "client fds");
	client = newclient(&server, a);
	client->u = &server;
	styxnewclient(&server, a);
	fid = newfid(client, 7, styxqid(2, 0));
	fid->open = 1;
	client = newclient(&server, b);
	client->u = &server;
	styxnewclient(&server, b);
	fid = newfid(client, 8, styxqid(2, 0));
	fid->open = 1;
	check(styxend(&server) == nil, "shutdown");
	check(server.clients == nil && server.root == nil && server.ftab == nil && server.priv == nil && server.curc == nil && server.connfd == -1, "retained server resources");
	check(closed == 2 && released == 2, "backend lifetime");
	check(fcntl(owner, F_GETFD) == -1 && errno == EBADF, "listener fd leaked");
	check(fcntl(a, F_GETFD) == -1 && errno == EBADF, "client fd leaked");
	check(fcntl(b, F_GETFD) == -1 && errno == EBADF, "second client fd leaked");
	check(styxend(&server) == nil && closed == 2 && released == 2, "double shutdown");
	denied = 1;
	check(styxinit(&server, &ops, "0", 0555, 1) != nil && server.ftab == nil && server.priv == nil, "announce failure leak");
	initdenied = 1;
	check(styxinit(&server, &ops, "0", 0555, 1) != nil && server.ftab == nil, "socket init failure leak");
	initdenied = denied = 0;
	check(styxinit(&server, &ops, "0", 0555, 1) == nil && styxend(&server) == nil, "reinitialization");
	print("Styx POSIX lifecycle: passed\n");
	return 0;
}
