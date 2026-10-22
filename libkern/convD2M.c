#include	"lib9.h"
#include	"fcall.h"

/* Bound the whole stat entry before any 16-bit wire count is written. */
static uint
statsz(Dir *d, uint nsv[4])
{
	char *sv[4];
	uint n, ns, left;
	int i;

	sv[0] = d->name;
	sv[1] = d->uid;
	sv[2] = d->gid;
	sv[3] = d->muid;

	ns = STATFIXLEN;
	for(i = 0; i < 4; i++){
		left = 0xffff+BIT16SZ-ns;
		n = 0;
		if(sv[i] != nil){
			while(n <= left && sv[i][n] != '\0')
				n++;
			if(n > left)
				return 0;
		}
		nsv[i] = n;
		ns += n;
	}
	return ns;
}

/* Report zero when the stat size cannot be represented on the wire. */
uint
sizeD2M(Dir *d)
{
	uint nsv[4];

	return statsz(d, nsv);
}

/* Preserve short-buffer size reporting only for representable entries. */
uint
convD2M(Dir *d, uchar *buf, uint nbuf)
{
	uchar *p;
	char *sv[4];
	int i;
	uint ns, nsv[4], ss;

	if(nbuf < BIT16SZ)
		return 0;

	ss = statsz(d, nsv);
	if(ss == 0)
		return 0;
	p = buf;

	sv[0] = d->name;
	sv[1] = d->uid;
	sv[2] = d->gid;
	sv[3] = d->muid;

	/* Set size before returning a short buffer, so the caller can retry. */
	/* note that length excludes count field itself */
	PBIT16(p, ss-BIT16SZ);
	p += BIT16SZ;

	if(ss > nbuf)
		return BIT16SZ;

	PBIT16(p, d->type);
	p += BIT16SZ;
	PBIT32(p, d->dev);
	p += BIT32SZ;
	PBIT8(p, d->qid.type);
	p += BIT8SZ;
	PBIT32(p, d->qid.vers);
	p += BIT32SZ;
	PBIT64(p, d->qid.path);
	p += BIT64SZ;
	PBIT32(p, d->mode);
	p += BIT32SZ;
	PBIT32(p, d->atime);
	p += BIT32SZ;
	PBIT32(p, d->mtime);
	p += BIT32SZ;
	PBIT64(p, d->length);
	p += BIT64SZ;

	for(i = 0; i < 4; i++){
		ns = nsv[i];
		PBIT16(p, ns);
		p += BIT16SZ;
		if(ns)
			memmove(p, sv[i], ns);
		p += ns;
	}

	if(ss != p - buf)
		return 0;

	return p - buf;
}
