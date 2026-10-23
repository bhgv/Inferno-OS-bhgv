#include "lib9.h"
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <assert.h>
#include <stdio.h>

#ifdef NT_STAT
#define _S_IFDIR S_IFDIR
#define _S_IFIFO S_IFIFO
#endif

static struct stat metadata;
static struct passwd user;
static struct group group;
static char username[300], groupname[300];
static int nouser, nogroup, failalloc, failstat, live, checks, calls, errors;
static ulong ownedsize;

/* Disable unsupported process inspection while retaining allocator checks. */
int __lsan_is_turned_off(void) { return 1; }

/* Record the one allocation that must own every nonempty metadata string. */
static void*
dirmalloc(ulong n)
{
	void *p;
	calls++;
	ownedsize = n;
	if(failalloc)return nil;
	p = malloc(n); assert(p != nil); live++;
	return p;
}

/* Drop the complete snapshot without any separately allocated name strings. */
static void dirfree(void *p) { assert(p != nil && live == 1); live--; free(p); }

/* Return controlled native stat/NSS records without consulting the host account. */
static int pathstat(char *path, struct stat *s) { assert(path != nil); if(failstat)return -1; *s = metadata; return 0; }
static int fdstat(int fd, struct stat *s) { assert(fd == 7); if(failstat)return -1; *s = metadata; return 0; }
static struct passwd* userlookup(uid_t id) { assert(id == metadata.st_uid); user.pw_name = username; return nouser ? nil : &user; }
static struct group* grouplookup(gid_t id) { assert(id == metadata.st_gid); group.gr_name = groupname; return nogroup ? nil : &group; }
void werrstr(char *fmt, ...) { assert(fmt != nil); errors++; }

#define malloc dirmalloc
#define stat(path, output) pathstat(path, output)
#define fstat(fd, output) fdstat(fd, output)
#define getpwuid userlookup
#define getgrgid grouplookup
IMPLEMENTATION
#undef malloc
#undef stat
#undef fstat
#undef getpwuid
#undef getgrgid

/* Check that a string lies wholly in the single snapshot allocation. */
static void
inside(Dir *d, char *p)
{
	assert(p >= (char*)(d+1) && p+strlen(p) < (char*)d+ownedsize);
}

/* Modify/free the caller's path and NSS storage after taking a snapshot. */
static void
check(char *path, char *expected, int missing)
{
	char *input;
	Dir *d;
	int failure;

	for(failure = 0; failure <= 1; failure++){
		nouser = missing&1; nogroup = missing&2;
#ifdef NT_STAT
		nouser = nogroup = 1;
#endif
		strcpy(username, "account"); strcpy(groupname, "group");
		input = strdup(path); assert(input != nil);
		calls = errors = 0; failalloc = failure;
		d = dirstat(input);
		free(input);
		if(failure){
			assert(d == nil && errors == 1 && live == 0 && calls == 1);
		}else{
			assert(d != nil && calls == 1 && errors == 0);
			assert(strcmp(d->name, expected) == 0);
			inside(d, d->name);
			memset(username, 'x', 7); memset(groupname, 'y', 5);
			assert(strcmp(d->uid, nouser ? "" : "account") == 0);
			assert(strcmp(d->gid, nogroup ? "" : "group") == 0);
			if(!nouser)inside(d, d->uid);
			if(!nogroup)inside(d, d->gid);
			assert(strcmp(d->muid, "") == 0);
			assert(d->length == metadata.st_size && d->qid.path == metadata.st_ino);
			assert(d->mode == ((ulong)(S_ISDIR(metadata.st_mode) ? QTDIR : QTFILE)<<24 | (metadata.st_mode&0777)));
			dirfree(d);
		}
		checks++;
	}
	failalloc = 0;
}

/* Cover no-slash, root and trailing-slash basenames without altering their bytes. */
int
main(void)
{
	static char *paths[] = {"file", "тест", "/path/file", "dir/", "a/b///", "/", "////", "./name", "../name", "/a//b/"};
	static char *names[] = {"file", "тест", "file", "dir", "b", "/", "/", "name", "name", "b"};
	char path[5000];
	Dir *d;
	int i, missing;
	memset(&metadata, 0, sizeof metadata);
	metadata.st_uid = 42; metadata.st_gid = 43; metadata.st_size = 123456789;
	metadata.st_ino = 192837; metadata.st_mode = S_IFREG|0644;
	for(i = 0; i < sizeof paths/sizeof paths[0]; i++)
		for(missing = 0; missing < 4; missing++)check(paths[i], names[i], missing);
	memset(path, 'a', 4000); strcpy(path+4000, "/final/");
	metadata.st_mode = S_IFDIR|0755;
	check(path, "final", 0);
	strcpy(username, "account"); strcpy(groupname, "group");
	nouser = nogroup = 0;
	d = dirfstat(7);
	assert(d != nil && strcmp(d->name, "") == 0); inside(d, d->name); dirfree(d); checks++;
	failstat = 1; calls = 0;
	assert(dirstat("missing") == nil && dirfstat(7) == nil && calls == 0 && live == 0);
	checks++;
	printf("owned dirstat: %d checks passed\n", checks);
	return 0;
}
