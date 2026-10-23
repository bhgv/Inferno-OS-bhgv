#include "lib9.h"
#include <sys/types.h>
#include <fcntl.h>

/* Preserve exclusive creation and descriptor inheritance flags in native opens. */
int
create(char *f, int mode, int perm)
{
	int m, fd;
#if !defined(O_CLOEXEC) && !defined(_O_NOINHERIT) && !defined(O_NOINHERIT) \
 && defined(F_GETFD) && defined(F_SETFD) && defined(FD_CLOEXEC)
	int flags, saved;
#endif

	m = 0;
	switch(mode & 3){
	case OREAD:
	case OEXEC:
		m = O_RDONLY;
		break;
	case OWRITE:
		m = O_WRONLY;
		break;
	case ORDWR:
		m = O_RDWR;
		break;
	}
	m |= O_CREAT|O_TRUNC;
	if(mode & OEXCL)
		m |= O_EXCL;

	if(perm & DMDIR){
		if(mkdir(f, perm&0777) < 0)
			return -1;
		perm &= ~DMDIR;
		m &= 3;
	}
	if(mode & OCEXEC){
#ifdef O_CLOEXEC
		m |= O_CLOEXEC;
#elif defined(_O_NOINHERIT)
		m |= _O_NOINHERIT;
#elif defined(O_NOINHERIT)
		m |= O_NOINHERIT;
#endif
	}
	fd = open(f, m, perm);
#if !defined(O_CLOEXEC) && !defined(_O_NOINHERIT) && !defined(O_NOINHERIT) \
 && defined(F_GETFD) && defined(F_SETFD) && defined(FD_CLOEXEC)
	if(fd >= 0 && (mode & OCEXEC)){
		flags = fcntl(fd, F_GETFD);
		if(flags < 0 || fcntl(fd, F_SETFD, flags|FD_CLOEXEC) < 0){
			saved = errno;
			close(fd);
			errno = saved;
			return -1;
		}
	}
#endif
	return fd;
}
