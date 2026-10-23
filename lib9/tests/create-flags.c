#include "lib9.h"
#include <sys/stat.h>
#include <assert.h>
#include <stdio.h>

static int failget, failset, opened, closed, checks;

/* Retain memory instrumentation without unsupported process enumeration. */
int __lsan_is_turned_off(void) { return 1; }

/* Record only descriptors owned by the production creation path. */
static int
createopen(char *name, int flags, int perm)
{
	opened = open(name, flags, perm);
	return opened;
}

/* Force close to alter errno so the caller must preserve the original failure. */
static int
createclose(int fd)
{
	int result;
	assert(fd == opened);
	closed++;
	result = close(fd);
	errno = EIO;
	return result;
}

/* Inject either fallback fcntl failure while using actual descriptor flags. */
static int
createfcntl(int fd, int operation, ...)
{
	int flags;
	va_list ap;
	if((operation == F_GETFD && failget) || (operation == F_SETFD && failset)){
		errno = EPERM;
		return -1;
	}
	if(operation == F_GETFD)return fcntl(fd, operation);
	assert(operation == F_SETFD);
	va_start(ap, operation); flags = va_arg(ap, int); va_end(ap);
	return fcntl(fd, operation, flags);
}

#ifdef FALLBACK_CEXEC
#undef O_CLOEXEC
#undef _O_NOINHERIT
#undef O_NOINHERIT
#endif
#define open createopen
#define close createclose
#define fcntl createfcntl
IMPLEMENTATION
#undef open
#undef close
#undef fcntl

/* Read the whole sentinel after an exclusive-create attempt to detect data loss. */
static void
verify(char *name)
{
	char data[8];
	int fd = open(name, O_RDONLY);
	assert(fd >= 0 && read(fd, data, sizeof data) == 4);
	assert(memcmp(data, "KEEP", 4) == 0);
	assert(close(fd) == 0);
}

/* Verify native and fcntl-based inheritance plus every access-mode/exclusive pair. */
int
main(int argc, char **argv)
{
	char name[4096], dirname[4096];
	int mode, flags, exclusive, fd;
	struct stat st;
	assert(argc == 2);
	snprintf(name, sizeof name, "%s/file", argv[1]);
	snprintf(dirname, sizeof dirname, "%s/directory", argv[1]);
	unlink(name); rmdir(dirname);
	for(mode = 0; mode < 4; mode++)
		for(flags = 0; flags <= 1; flags++)
			for(exclusive = 0; exclusive <= 1; exclusive++){
				fd = open(name, O_WRONLY|O_CREAT|O_TRUNC, 0600);
				assert(fd >= 0 && write(fd, "KEEP", 4) == 4 && close(fd) == 0);
				fd = create(name, mode|(flags ? OCEXEC : 0)|(exclusive ? OEXCL : 0), 0600);
				if(exclusive){ assert(fd < 0 && errno == EEXIST); verify(name); }
				else{
					assert(fd >= 0 && fstat(fd, &st) == 0 && st.st_size == 0);
					assert(!!(fcntl(fd, F_GETFD)&FD_CLOEXEC) == flags);
					assert(close(fd) == 0);
				}
				assert(unlink(name) == 0);
				fd = create(name, mode|(flags ? OCEXEC : 0)|OEXCL, 0600);
				assert(fd >= 0 && !!(fcntl(fd, F_GETFD)&FD_CLOEXEC) == flags);
				assert(close(fd) == 0); checks++;
			}
	assert(unlink(name) == 0);
	for(flags = 0; flags <= 1; flags++){
		fd = create(dirname, OREAD|(flags ? OCEXEC : 0)|OEXCL, (int)DMDIR|0700);
		assert(fd >= 0 && fstat(fd, &st) == 0 && S_ISDIR(st.st_mode));
		assert(!!(fcntl(fd, F_GETFD)&FD_CLOEXEC) == flags);
		assert(close(fd) == 0);
		assert(create(dirname, OREAD|OEXCL, (int)DMDIR|0700) < 0);
		assert(rmdir(dirname) == 0); checks++;
	}
#ifdef FALLBACK_CEXEC
	for(flags = 0; flags <= 1; flags++){
		failget = flags == 0; failset = flags == 1; closed = 0;
		assert(create(name, OWRITE|OCEXEC, 0600) == -1 && errno == EPERM);
		assert(closed == 1 && fcntl(opened, F_GETFD) == -1 && errno == EBADF);
		assert(unlink(name) == 0); checks++;
	}
#endif
	printf("create flags: %d checks passed\n", checks);
	return 0;
}
