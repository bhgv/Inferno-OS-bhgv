#include "lib9.h"
#include <sys/types.h>
#include <fcntl.h>

/* Keep the full signed offset when calling the Windows CRT seek entry point. */
vlong
seek(int fd, vlong where, int from)
{
#ifdef _WIN32
	return _lseeki64(fd, where, from);
#else
	return lseek(fd, where, from);
#endif
}
