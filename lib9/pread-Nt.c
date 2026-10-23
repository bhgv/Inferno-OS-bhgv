#include "lib9.h"

/* Restore the shared descriptor offset and the read error after positional I/O. */
long
pread(int fd, void *buf, long n, vlong offset)
{
	vlong previous;
	long count;
	int saved;

	if(n < 0 || offset < 0){
		errno = EINVAL;
		return -1;
	}
	previous = seek(fd, 0, 1);
	if(previous < 0 || seek(fd, offset, 0) < 0)
		return -1;
	count = read(fd, buf, n);
	saved = errno;
	if(seek(fd, previous, 0) < 0)
		return -1;
	errno = saved;
	return count;
}
