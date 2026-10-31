#include "storage.h"
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>


/* Resolve only the user-owned shared tree; never create or relocate any directory. */
int
inferno_android_root(char *out, size_t length)
{
	const char *path = "/sdcard/Inferno";
	struct stat state;

	if(out == NULL || length == 0){
		errno = EINVAL;
		return -1;
	}
	if(strlen(path) >= length){
		errno = ENAMETOOLONG;
		return -1;
	}
	if(stat(path, &state) != 0)
		return -1;
	if(!S_ISDIR(state.st_mode)){
		errno = ENOTDIR;
		return -1;
	}
	if(access(path, R_OK|W_OK|X_OK) != 0)
		return -1;
	memcpy(out, path, strlen(path)+1);
	return 0;
}
