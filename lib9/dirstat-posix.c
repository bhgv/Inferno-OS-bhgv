#include "lib9.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>

static char nullstring[] = "";
static char Enovmem[] = "out of memory";

/* Store every variable string in the returned Dir's single owned allocation. */
static Dir*
statconv(struct stat *s, char *name, ulong nname)
{
	struct passwd *p;
	struct group *g;
	Dir *dir;
	ulong str, nuid, ngid, limit;
	char *n;

	limit = 0x7fffffff-sizeof(Dir);
	if(nname >= limit)
		goto Nomem;
	str = nname+1;
	p = getpwuid(s->st_uid);
	nuid = p == nil ? 0 : strlen(p->pw_name);
	g = getgrgid(s->st_gid);
	ngid = g == nil ? 0 : strlen(g->gr_name);
	if(p != nil){
		if(nuid >= limit-str)
			goto Nomem;
		str += nuid+1;
	}
	if(g != nil){
		if(ngid >= limit-str)
			goto Nomem;
		str += ngid+1;
	}
	dir = malloc(sizeof(Dir)+str);
	if(dir == nil)
		goto Nomem;
	n = (char*)(dir+1);
	dir->name = n;
	memmove(n, name, nname);
	n[nname] = '\0';
	n += nname+1;
	dir->uid = dir->gid = dir->muid = nullstring;
	if(p != nil){
		dir->uid = n;
		memmove(n, p->pw_name, nuid+1);
		n += nuid+1;
	}
	if(g != nil){
		dir->gid = n;
		memmove(n, g->gr_name, ngid+1);
	}
	dir->qid.type = S_ISDIR(s->st_mode)? QTDIR: QTFILE;
	dir->qid.path = s->st_ino;
	dir->qid.vers = s->st_mtime;
	dir->mode = ((ulong)dir->qid.type<<24)|(s->st_mode&0777);
	dir->atime = s->st_atime;
	dir->mtime = s->st_mtime;
	dir->length = s->st_size;
	dir->dev = s->st_dev;
	dir->type = S_ISFIFO(s->st_mode)? '|': 'M';
	return dir;

Nomem:
	werrstr(Enovmem);
	return nil;
}

/* Descriptor metadata has an empty, owned name when no path is available. */
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
