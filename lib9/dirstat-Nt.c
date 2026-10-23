#include "lib9.h"
#include <sys/types.h>
#include <sys/stat.h>

#define ISTYPE(s, t)	(((s)->st_mode&S_IFMT) == t)

static char nullstring[] = "";
static char Enovmem[] = "out of memory";

/* Keep the basename in the returned Dir allocation, independently of its caller. */
static Dir*
statconv(struct stat *s, char *name, ulong nname)
{
	Dir *dir;

#ifdef NO
	extern char* GetNameFromID(int);

	uid = GetNameFromID(s->st_uid), NAMELEN);
	gid = GetNameFromID(s->st_gid), NAMELEN);
#endif
	if(nname > 0x7fffffff-sizeof(Dir)-1){
		werrstr(Enovmem);
		return nil;
	}
	dir = malloc(sizeof(Dir)+nname+1);
	if(dir == nil){
		werrstr(Enovmem);
		return nil;
	}
	dir->name = (char*)(dir+1);
	memmove(dir->name, name, nname);
	dir->name[nname] = '\0';
	dir->uid = dir->gid = dir->muid = nullstring;
	dir->qid.type = ISTYPE(s, _S_IFDIR)? QTDIR: QTFILE;
	dir->qid.path = s->st_ino;
	dir->qid.vers = s->st_mtime;
	dir->mode = ((ulong)dir->qid.type<<24)|(s->st_mode&0777);
	dir->atime = s->st_atime;
	dir->mtime = s->st_mtime;
	dir->length = s->st_size;
	dir->dev = s->st_dev;
	dir->type = ISTYPE(s, _S_IFIFO)? '|': 'M';
	return dir;
}

/* A descriptor stat owns an empty name when no path is available. */
Dir*
dirfstat(int fd)
{
	struct stat sbuf;

	if(fstat(fd, &sbuf) < 0)
		return nil;
	return statconv(&sbuf, nullstring, 0);
}

/* Copy the final path component, ignoring trailing separators except for root. */
Dir*
dirstat(char *f)
{
	struct stat sbuf;
	char *p, *end;

	if(stat(f, &sbuf) < 0)
		return nil;
	end = f+strlen(f);
	while(end > f+1 && end[-1] == '/')
		end--;
	p = end;
	while(p > f && p[-1] != '/')
		p--;
	if(p == end && end == f+1 && f[0] == '/')
		p = f;
	return statconv(&sbuf, p, end-p);
}
