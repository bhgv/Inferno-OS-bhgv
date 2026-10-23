#include "lib9.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>

static uchar output[2048];
static int used, fragment, stopat, failkind, writes, logs, priority, expectedfd, checks;

/* Retain bounded memory checks without unsupported process inspection. */
int __lsan_is_turned_off(void) { return 1; }

/* Produce short native writes or a terminal error/zero-progress result. */
static long
outputwrite(int fd, void *data, long n)
{
	int count;
	assert(fd == expectedfd && n > 0 && n <= sizeof output);
	writes++;
	if(stopat >= 0 && used >= stopat)return failkind ? -1 : 0;
	count = n < fragment ? n : fragment;
	if(stopat >= 0 && count > stopat-used)count = stopat-used;
	memmove(output+used, data, count);
	used += count;
	return count;
}

/* A non-NUL-terminated full buffer must remain bounded by the supplied precision. */
static int
outputlog(int level, char *fmt, ...)
{
	va_list ap;
	int n;
	va_start(ap, fmt);
	n = vsnprintf((char*)output, sizeof output, fmt, ap);
	va_end(ap);
	assert(n >= 0 && n < sizeof output);
	used = n; logs++; priority = level;
	return n;
}

#define LOGI(...) outputlog(1, __VA_ARGS__)
#define LOGE(...) outputlog(2, __VA_ARGS__)
#define write outputwrite
IMPLEMENTATION
#undef write

/* Compare byte-exact output and retained suffix before retrying failed flushes. */
static void
check(int n, int chunk, int cut, int failure, int fd)
{
	Fmt f;
	char *buffer, original[512];
	int success, first, logged = 0, result;
	buffer = malloc(n ? n : 1); assert(buffer != nil);
	for(int i = 0; i < n; i++)original[i] = 1+(i*17)%126;
	memmove(buffer, original, n);
	fmtfdinit(&f, fd, buffer, n);
	f.to = buffer+n; f.nfmt = n;
	fragment = chunk; stopat = cut; failkind = failure; expectedfd = fd;
	used = writes = logs = 0;
#ifdef ANDROID
	logged = fd == 1 || fd == 2;
#endif
	success = logged || cut < 0 || cut >= n;
	result = fmtfdflush(&f);
	assert(result == (success ? n : -1));
	first = success ? n : cut;
	assert(used == first && memcmp(output, original, first) == 0);
	assert(f.to == buffer+n-first);
	assert(f.nfmt == n);
	if(logged){
		assert(writes == 0 && logs == (n > 0));
		if(n > 0)assert(priority == fd);
	}else{
		assert(logs == 0);
		if(!success){
			assert(memcmp(buffer, original+first, n-first) == 0);
			stopat = -1;
			assert(fmtfdflush(&f) == n && f.to == buffer);
			assert(used == n && memcmp(output, original, n) == 0);
		}
	}
	free(buffer); checks++;
}

/* Cover full buffers, all short-write sizes, failures, retry and Android descriptors. */
int
main(void)
{
	for(int n = 0; n <= 512; n += n < 64 ? 1 : 17)
		for(int chunk = 1; chunk <= 17; chunk++)
			for(int fd = 1; fd <= 3; fd++){
				check(n, chunk, -1, 0, fd);
				check(n, chunk, 0, 0, fd);
				check(n, chunk, n/2, 1, fd);
			}
	printf("Fmt file I/O: %d checks passed\n", checks);
	return 0;
}
