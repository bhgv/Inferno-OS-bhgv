#include "storage.h"
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define StoragePath 4096 // Reject overlong directory names instead of truncating them.
static char internalpath[StoragePath]; // NativeActivity internalDataPath, copied before emulator startup.
static char externalpath[StoragePath]; // Optional NativeActivity externalDataPath, copied likewise.


/* Validate both borrowed launcher strings before replacing either stored directory. */
int
inferno_android_set_storage(const char *internal, const char *external)
{
	if((internal != NULL && (internal[0] != '/' || strlen(internal) >= StoragePath)) ||
	   (external != NULL && (external[0] != '/' || strlen(external) >= StoragePath))){
		errno = EINVAL;
		return -1;
	}
	strcpy(internalpath, internal == NULL ? "" : internal);
	strcpy(externalpath, external == NULL ? "" : external);
	return 0;
}


/* Require a real accessible directory, creating only an app-owned child when requested. */
static int
storagedirectory(const char *path, int create)
{
	struct stat state;

	if(create && mkdir(path, 0700) != 0 && errno != EEXIST)
		return -1;
	if(stat(path, &state) != 0)
		return -1;
	if(!S_ISDIR(state.st_mode)){
		errno = ENOTDIR;
		return -1;
	}
	return access(path, R_OK|W_OK|X_OK);
}


/* Publish a complete path only after size and directory checks have succeeded. */
static int
storagecopy(char *out, size_t length, const char *base, int child, int create)
{
	char path[StoragePath];
	int bytes;

	if(base == NULL || base[0] != '/'){
		errno = EINVAL;
		return -1;
	}
	bytes = snprintf(path, sizeof(path), "%s%s", base, child ? "/Inferno" : "");
	if(bytes < 0 || (size_t)bytes >= sizeof(path) || (size_t)bytes >= length){
		errno = ENAMETOOLONG;
		return -1;
	}
	if(storagedirectory(path, create) != 0)
		return -1;
	memcpy(out, path, bytes+1);
	return 0;
}


/* Prefer the app's native-activity directories; use shared legacy storage only if accessible.
 * The launcher calls set_storage before amain on the same startup thread. */
int
inferno_android_root(char *out, size_t length)
{
	const char *override, *files, *legacy;

	if(out == NULL || length == 0){
		errno = EINVAL;
		return -1;
	}
	override = getenv("INFERNO_ROOT");
	if(override != NULL && override[0] != '\0')
		return storagecopy(out, length, override, 0, 1);
	files = internalpath[0] != '\0' ? internalpath : getenv("INFERNO_FILES_DIR");
	if(files != NULL && storagecopy(out, length, files, 1, 1) == 0)
		return 0;
	if(externalpath[0] != '\0' && storagecopy(out, length, externalpath, 1, 1) == 0)
		return 0;
	legacy = getenv("EXTERNAL_STORAGE");
	if(legacy != NULL && storagecopy(out, length, legacy, 1, 0) == 0)
		return 0;
	errno = ENOENT;
	return -1;
}
