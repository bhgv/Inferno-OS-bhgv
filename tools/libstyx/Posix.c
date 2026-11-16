#define	__EXTENSIONS__
#define	_BSD_COMPAT
#include <lib9.h>
#include 	<sys/types.h>
#include	<sys/time.h>
#include	<sys/socket.h>
#include	<netinet/in.h>

#include "styxserver.h"
#include "styxaux.h"

typedef struct Fdset Fdset;

struct Fdset
{
	fd_set infds, outfds, excfds, r_infds, r_outfds, r_excfds;
	int notifyfd; /* Borrowed native mailbox readiness fd, never closed here. */
};

int
styxinitsocket(void)
{
	return 0;
}

void
styxendsocket(void)
{
}

void
styxclosesocket(int fd)
{
	close(fd);
}

int
styxannounce(Styxserver *server, char *port)
{
	struct sockaddr_in sin;
	int s, one;

	USED(server);
	s = socket(AF_INET, SOCK_STREAM, 0);
	if(s < 0)
		return s;
	if(s >= FD_SETSIZE){
		close(s);
		return -1;
	}
	one = 1;
	if(setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*)&one, sizeof(one)) < 0)
		fprint(2, "setsockopt failed\n");
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = 0;
	sin.sin_port = htons(atoi(port));
	if(bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0){
		close(s);
		return -1;
	}
	if(listen(s, 20) < 0){
		close(s);
		return -1;
	}
	return s;
}

int
styxaccept(Styxserver *server)
{
	struct sockaddr_in sin;
	int s;
	socklen_t len;

	len = sizeof(sin);
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	s = accept(server->connfd, (struct sockaddr *)&sin, &len);
	if(s >= FD_SETSIZE){
		close(s);
		return -1;
	}
	if(s < 0){
		if(errno != EINTR)
			fprint(2, "error in accept: %s\n", strerror(errno));
	}
	return s;
}

void
styxinitwait(Styxserver *server)
{
	Fdset *fs;

	server->priv = fs = malloc(sizeof(Fdset));
	FD_ZERO(&fs->infds);
	FD_ZERO(&fs->outfds);
	FD_ZERO(&fs->excfds);
	FD_SET(server->connfd, &fs->infds);
	fs->notifyfd = -1;
}

int
styxnewcall(Styxserver *server)
{
	Fdset *fs;

	fs = server->priv;
	return FD_ISSET(server->connfd, &fs->r_infds);
}

void
styxnewclient(Styxserver *server, int s)
{
	Fdset *fs;

	fs = server->priv;
	FD_SET(s, &fs->infds);
}

void
styxfreeclient(Styxserver *server, int s)
{
	Fdset *fs;

	fs = server->priv;
	FD_CLR(s, &fs->infds);
}

int
styxnewmsg(Styxserver *server, int s)
{
	Fdset *fs;

	fs = server->priv;
	return FD_ISSET(s, &fs->r_infds) || FD_ISSET(s, &fs->r_excfds);
}

char*
styxwaitmsg(Styxserver *server)
{
	struct timeval seltime;
	int nfds;
	Fdset *fs;

	fs = server->priv;
	fs->r_infds = fs->infds;
	fs->r_outfds = fs->outfds;
	fs->r_excfds = fs->excfds;
	seltime.tv_sec = 10;
	seltime.tv_usec = 0L;
	nfds = select(sizeof(fd_set)*8, &fs->r_infds, &fs->r_outfds, &fs->r_excfds, &seltime);
	if(nfds < 0){
		FD_ZERO(&fs->r_infds);
		FD_ZERO(&fs->r_outfds);
		FD_ZERO(&fs->r_excfds);
		if(errno != EINTR)
			return "error in select";
	}
	return nil;
}

int
styxrecv(Styxserver *server, int fd, char *buf, int n, int m)
{
	return recv(fd, buf, n, m);
}

int
styxsend(Styxserver *server, int fd, char *buf, int n, int m)
{
#ifdef MSG_NOSIGNAL
	m |= MSG_NOSIGNAL;
#endif
	return send(fd, buf, n, m);
}

void
styxexit(int n)
{
	exit(n);
}

/* End the native socket owner once, after clunking clients and their backend state. */
char*
styxend(Styxserver *server)
{
	if(server->ftab == nil)
		return nil;
	styxendserver(server);
	free(server->priv);
	server->priv = nil;
	styxendsocket();
	return nil;
}

/* EN/RU: Register borrowed native readiness input on the socket owner;0/-1.
 * Зарегистрировать borrowed native readiness fd на socket-owner;0/-1. */
int
styxnotifyfd(Styxserver *server, int fd)
{
    Fdset *fs;
    if(server == nil || server->priv == nil || fd < -1 || fd >= FD_SETSIZE)
        return -1;
    fs = server->priv;
    if(fd >= 0 && fd != fs->notifyfd && FD_ISSET(fd, &fs->infds))
        return -1;
    if(fs->notifyfd >= 0)
        FD_CLR(fs->notifyfd, &fs->infds);
    fs->notifyfd = fd;
    if(fd >= 0)
        FD_SET(fd, &fs->infds);
    return 0;
}
