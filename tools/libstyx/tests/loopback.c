/* Copyright (c) 2026 bhgv
 * Real original POSIX Styx server with per-client live fixture views, no host files.
 * Настоящий POSIX Styx-сервер с живыми представлениями клиентов, без файлов хоста. */
#include <lib9.h>
#include <styx.h>
#include <styxserver.h>
#include <sys/socket.h>
#include <netinet/in.h>

typedef struct View View;
struct View { int bob; int changed; };
static Styxserver server;
static int stopping, closed, released;
#define Dataqid 0xfedcba9876543210ULL
#define Ctlqid 0xfedcba9876543211ULL

/* Borrow only the current client's native fixture state.
 * Заимствовать native-состояние только текущего клиента. */
static View*
view(void)
{
	return styxclient(&server)->u;
}

/* Allocate isolated backend state; freeclient also handles a failed allocation.
 * Выделить отдельный backend; freeclient обрабатывает и отказ выделения. */
static char*
newview(Client *c)
{
	c->u = calloc(1, sizeof(View));
	return c->u == nil ? Enomem : nil;
}

/* Release the private view after all open fids have closed.
 * Освободить представление после закрытия открытых fid. */
static char*
freeview(Client *c)
{
	free(c->u);
	c->u = nil;
	released++;
	return nil;
}

/* Fixture routing by uname does not authenticate any real identity.
 * Маршрутизация теста по uname не аутентифицирует реального пользователя. */
static char*
attach(char *user, char *name)
{
	USED(name);
	if(strcmp(user, "alice") != 0 && strcmp(user, "bob") != 0)
		return Eperm;
	view()->bob = strcmp(user, "bob") == 0;
	return nil;
}

/* The exposed child changes live; parent traversal stays at the connection root.
 * Выдаваемый ребёнок меняется на лету; родитель ограничен корнем соединения. */
static char*
walk(Qid *q, char *name)
{
	char *current;
	if(q->path != Qroot)
		return "not a directory";
	current = view()->bob ? "bob" : view()->changed ? "later" : "first";
	if(strcmp(name, "..") == 0)
		*q = styxqid(Qroot, 1);
	else if(strcmp(name, current) == 0)
		*q = styxqid(Dataqid, 0);
	else if(strcmp(name, "ctl") == 0 && !view()->bob)
		*q = styxqid(Ctlqid, 0);
	else
		return Enonexist;
	return nil;
}

/* Only the fixture control endpoint can be opened for writing.
 * Только тестовый управляющий endpoint разрешено открыть для записи. */
static char*
openfile(Qid *q, int mode)
{
	if(q->path == Ctlqid)
		return !view()->bob && mode == OWRITE ? nil : Eperm;
	return mode == OREAD ? nil : Eperm;
}

/* Supply current one-node stat attributes without constructing a tree snapshot.
 * Выдать текущие атрибуты одной ноды без снимка дерева. */
static char*
statfile(Qid q, Dir *d)
{
	memset(d, 0, sizeof(*d));
	d->qid = q;
	d->name = view()->bob ? "bob" : view()->changed ? "later" : "first";
	d->mode = view()->changed ? 0640 : 0444;
	d->uid = styxclient(&server)->uname;
	d->gid = "fixture";
	d->muid = "";
	return nil;
}

/* Read exactly one live directory level or the current retained file contents.
 * Прочитать один уровень живого каталога либо текущее содержимое открытого файла. */
static char*
readfile(Qid q, char *buf, ulong *n, vlong offset)
{
	Dir d;
	uint size;
	if(q.path == Qroot){
		statfile(styxqid(Dataqid, 0), &d);
		size = sizeD2M(&d);
		if(offset == size){
			*n = 0;
			return nil;
		}
		if(offset != 0 || *n < size)
			return Ebadarg;
		*n = convD2M(&d, (uchar*)buf, *n);
		return nil;
	}
	if(q.path != Dataqid || offset < 0)
		return Ebadarg;
	*n = styxreadstr(offset, buf, *n, view()->changed ? "changed" : "initial");
	return nil;
}

/* Test-only commands update this client's view or finish the isolated server.
 * Тестовые команды меняют представление клиента либо завершают изолированный сервер. */
static char*
writefile(Qid q, char *buf, ulong *n, vlong offset)
{
	if(q.path != Ctlqid || view()->bob || offset != 0)
		return Eperm;
	if(*n == 6 && memcmp(buf, "change", 6) == 0)
		view()->changed = 1;
	else if(*n == 4 && memcmp(buf, "stop", 4) == 0)
		stopping = 1;
	else
		return Ebadarg;
	return nil;
}

/* Record native open-handle cleanup at disconnect and shutdown.
 * Учесть закрытие открытых handles при разрыве и завершении. */
static char*
closefile(Qid q, int mode)
{
	USED(q.path); USED(mode);
	closed++;
	return nil;
}

/* Serve only synthetic test data using the complete original C transport/library.
 * Обслужить только синтетические данные исходными C-транспортом и библиотекой. */
int
main(void)
{
	Styxops ops;
	struct sockaddr_in address;
	socklen_t length;
	char *error;
	memset(&ops, 0, sizeof(ops));
	ops.newclient = newview;
	ops.freeclient = freeview;
	ops.attach = attach;
	ops.walk = walk;
	ops.open = openfile;
	ops.read = readfile;
	ops.write = writefile;
	ops.stat = statfile;
	ops.close = closefile;
	error = styxinit(&server, &ops, "0", 0555, 0);
	if(error != nil){
		fprint(2, "%s\n", error);
		return 1;
	}
	length = sizeof(address);
	if(getsockname(server.connfd, (struct sockaddr*)&address, &length) < 0)
		return 2;
	print("%d\n", ntohs(address.sin_port));
	while(!stopping){
		if(styxwait(&server) != nil || styxprocess(&server) != nil)
			return 3;
	}
	if(styxend(&server) != nil || closed != 4 || released != 2)
		return 4;
	print("POSIX loopback: closed %d handles, released %d clients\n", closed, released);
	return 0;
}
