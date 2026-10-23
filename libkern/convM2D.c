#include	"lib9.h"
#include	"fcall.h"

/* Validate every counted string before advancing beyond the available record. */
int
statcheck(uchar *buf, uint nbuf)
{
	uint left, n;
	int i;

	if(buf == nil || nbuf < STATFIXLEN || nbuf != BIT16SZ + GBIT16(buf))
		return -1;
	left = nbuf-(STATFIXLEN-4*BIT16SZ);
	buf += STATFIXLEN-4*BIT16SZ;
	for(i = 0; i < 4; i++){
		if(left < BIT16SZ)
			return -1;
		n = GBIT16(buf);
		if(n > left-BIT16SZ)
			return -1;
		buf += BIT16SZ+n;
		left -= BIT16SZ+n;
	}
	return left == 0 ? 0 : -1;
}

static char nullstring[] = "";

/* Decode only the declared first record, leaving outputs untouched on bad input. */
uint
convM2D(uchar *buf, uint nbuf, Dir *d, char *strs)
{
	uchar *p, *ebuf;
	char *sv[4];
	int i, ns;
	uint size;

	if(buf == nil || d == nil || nbuf < STATFIXLEN)
		return 0;
	size = BIT16SZ+GBIT16(buf);
	if(size > nbuf || statcheck(buf, size) < 0)
		return 0;
	nbuf = size;

	p = buf;
	ebuf = buf + nbuf;

	p += BIT16SZ;	/* size was checked before any destination writes */
	d->type = GBIT16(p);
	p += BIT16SZ;
	d->dev = GBIT32(p);
	p += BIT32SZ;
	d->qid.type = GBIT8(p);
	p += BIT8SZ;
	d->qid.vers = GBIT32(p);
	p += BIT32SZ;
	d->qid.path = GBIT64(p);
	p += BIT64SZ;
	d->mode = GBIT32(p);
	p += BIT32SZ;
	d->atime = GBIT32(p);
	p += BIT32SZ;
	d->mtime = GBIT32(p);
	p += BIT32SZ;
	d->length = GBIT64(p);
	p += BIT64SZ;

	for(i = 0; i < 4; i++){
		if(ebuf-p < BIT16SZ)
			return 0;
		ns = GBIT16(p);
		p += BIT16SZ;
		if(ns > ebuf-p)
			return 0;
		if(strs){
			sv[i] = strs;
			memmove(strs, p, ns);
			strs += ns;
			*strs++ = '\0';
		}
		p += ns;
	}

	if(strs){
		d->name = sv[0];
		d->uid = sv[1];
		d->gid = sv[2];
		d->muid = sv[3];
	}else{
		d->name = nullstring;
		d->uid = nullstring;
		d->gid = nullstring;
		d->muid = nullstring;
	}
	
	return p - buf;
}
