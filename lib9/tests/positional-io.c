#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
typedef int64_t vlong;
static vlong position, target;
static int calls, failseek, readerror, reads, dispatch;
static long result;
/* Model descriptor position and independently inject each seek failure. */
static vlong
mockseek(int fd, vlong offset, int origin)
{
	assert(fd == 7);
	calls++;
	if(calls == failseek){ errno = ESPIPE; return -1; }
	if(origin == 1) return position;
	assert(origin == 0);
	position = offset;
	return position;
}
/* Check that reading occurs at the requested offset, including short reads. */
static long
mockread(int fd, void *buffer, long count)
{
	assert(fd == 7 && buffer != 0 && count == 8);
	assert(position == target);
	reads++;
	position += result > 0 ? result : 0;
	errno = readerror;
	return result;
}
#define seek mockseek
#define read mockread
#define pread testedpread
PREAD_IMPLEMENTATION
#undef pread
#undef read
#undef seek
/* Record which platform entry point receives the unmodified signed offset. */
static vlong
_lseeki64(int fd, vlong offset, int origin)
{
	assert(fd == 7 && origin == 2);
	dispatch = 1;
	return offset;
}
/* Give the POSIX boundary the same explicit wide result type in this fixture. */
static vlong
lseek(int fd, vlong offset, int origin)
{
	assert(fd == 7 && origin == 2);
	dispatch = 2;
	return offset;
}
SEEK_IMPLEMENTATION
/* Exercise successful, short, EOF and failed reads plus every seek failure. */
int
main(void)
{
	char buffer[8];
	int i, failure, checks = 0;
	vlong offsets[] = {0, 2147483648LL, INT64_MAX, INT64_MIN};
	for(i = 0; i < 4; i++){
		assert(seek(7, offsets[i], 2) == offsets[i]);
#ifdef _WIN32
		assert(dispatch == 1);
#else
		assert(dispatch == 2);
#endif
		checks++;
	}
	for(i = -1; i <= 8; i++){
		for(failure = 0; failure <= 3; failure++){
			position = 9000000000LL; target = 5000000000LL;
			result = i; readerror = EIO; reads = calls = 0;
			failseek = failure; errno = 0;
			if(failure){
				assert(testedpread(7, buffer, 8, target) == -1);
				assert(errno == ESPIPE);
				assert(reads == (failure == 3));
			}else{
				assert(testedpread(7, buffer, 8, target) == i);
				assert(position == 9000000000LL && errno == EIO);
				assert(reads == 1);
			}
			checks++;
		}
	}
	calls = reads = 0;
	assert(testedpread(7, buffer, -1, 0) == -1 && errno == EINVAL);
	assert(testedpread(7, buffer, 8, -1) == -1 && errno == EINVAL);
	assert(calls == 0 && reads == 0);
	printf("positional I/O: %d checks passed\n", checks + 2);
	return 0;
}
