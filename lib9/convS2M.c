#include	"lib9.h"
#include	"fcall.h"

/* Encode a string whose 16-bit length was validated by sizeS2M. */
static
uchar*
pstring(uchar *p, char *s)
{
	uint n;

	if(s == nil){
		PBIT16(p, 0);
		p += BIT16SZ;
		return p;
	}

	n = strlen(s);
	PBIT16(p, n);
	p += BIT16SZ;
	memmove(p, s, n);
	p += n;
	return p;
}

/* Encode a qid in its fixed-width wire representation. */
static
uchar*
pqid(uchar *p, Qid *q)
{
	PBIT8(p, q->type);
	p += BIT8SZ;
	PBIT32(p, q->vers);
	p += BIT32SZ;
	PBIT64(p, q->path);
	p += BIT64SZ;
	return p;
}

/* Return zero for a string that cannot fit its 16-bit wire count. */
static uint
stringsz(char *s)
{
	uint n;

	if(s == nil)
		return BIT16SZ;
	for(n = 0; n <= 0xffff; n++)
		if(s[n] == '\0')
			return BIT16SZ+n;
	return 0;
}

/* Validate variable fields before sizing or touching the output buffer. */
uint
sizeS2M(Fcall *f)
{
	uint n, ns;
	int i;

	n = 0;
	n += BIT32SZ;	/* size */
	n += BIT8SZ;	/* type */
	n += BIT16SZ;	/* tag */

	switch(f->type)
	{
	default:
		return 0;

	case Tversion:
		n += BIT32SZ;
		if((ns = stringsz(f->version)) == 0)
			return 0;
		n += ns;
		break;

	case Tflush:
		n += BIT16SZ;
		break;

	case Tauth:
		n += BIT32SZ;
		if((ns = stringsz(f->uname)) == 0)
			return 0;
		n += ns;
		if((ns = stringsz(f->aname)) == 0)
			return 0;
		n += ns;
		break;

	case Tattach:
		n += BIT32SZ;
		n += BIT32SZ;
		if((ns = stringsz(f->uname)) == 0)
			return 0;
		n += ns;
		if((ns = stringsz(f->aname)) == 0)
			return 0;
		n += ns;
		break;

	case Twalk:
		if(f->nwname > MAXWELEM)
			return 0;
		n += BIT32SZ;
		n += BIT32SZ;
		n += BIT16SZ;
		for(i=0; i<f->nwname; i++){
			if((ns = stringsz(f->wname[i])) == 0)
				return 0;
			n += ns;
		}
		break;

	case Topen:
		n += BIT32SZ;
		n += BIT8SZ;
		break;

	case Tcreate:
		n += BIT32SZ;
		if((ns = stringsz(f->name)) == 0)
			return 0;
		n += ns;
		n += BIT32SZ;
		n += BIT8SZ;
		break;

	case Tread:
		n += BIT32SZ;
		n += BIT64SZ;
		n += BIT32SZ;
		break;

	case Twrite:
		n += BIT32SZ;
		n += BIT64SZ;
		n += BIT32SZ;
		if(f->count > (uint)~0-n)
			return 0;
		n += f->count;
		break;

	case Tclunk:
	case Tremove:
		n += BIT32SZ;
		break;

	case Tstat:
		n += BIT32SZ;
		break;

	case Twstat:
		n += BIT32SZ;
		n += BIT16SZ;
		n += f->nstat;
		break;
/*
 */

	case Rversion:
		n += BIT32SZ;
		if((ns = stringsz(f->version)) == 0)
			return 0;
		n += ns;
		break;

	case Rerror:
		if((ns = stringsz(f->ename)) == 0)
			return 0;
		n += ns;
		break;

	case Rflush:
		break;

	case Rauth:
		n += QIDSZ;
		break;

	case Rattach:
		n += QIDSZ;
		break;

	case Rwalk:
		if(f->nwqid > MAXWELEM)
			return 0;
		n += BIT16SZ;
		n += f->nwqid*QIDSZ;
		break;

	case Ropen:
	case Rcreate:
		n += QIDSZ;
		n += BIT32SZ;
		break;

	case Rread:
		n += BIT32SZ;
		if(f->count > (uint)~0-n)
			return 0;
		n += f->count;
		break;

	case Rwrite:
		n += BIT32SZ;
		break;

	case Rclunk:
		break;

	case Rremove:
		break;

	case Rstat:
		n += BIT16SZ;
		n += f->nstat;
		break;

	case Rwstat:
		break;
	}
	return n;
}

/* Serialize only messages whose complete wire size has been validated. */
uint
convS2M(Fcall *f, uchar *ap, uint nap)
{
	uchar *p;
	uint i, size;

	size = sizeS2M(f);
	if(size == 0)
		return 0;
	if(size > nap)
		return 0;
	if((f->type == Twrite || f->type == Rread) && f->count != 0 && f->data == nil)
		return 0;
	if((f->type == Twstat || f->type == Rstat) && f->nstat != 0 && f->stat == nil)
		return 0;

	p = (uchar*)ap;

	PBIT32(p, size);
	p += BIT32SZ;
	PBIT8(p, f->type);
	p += BIT8SZ;
	PBIT16(p, f->tag);
	p += BIT16SZ;

	switch(f->type)
	{
	default:
		return 0;

	case Tversion:
		PBIT32(p, f->msize);
		p += BIT32SZ;
		p = pstring(p, f->version);
		break;

	case Tflush:
		PBIT16(p, f->oldtag);
		p += BIT16SZ;
		break;

	case Tauth:
		PBIT32(p, f->afid);
		p += BIT32SZ;
		p  = pstring(p, f->uname);
		p  = pstring(p, f->aname);
		break;

	case Tattach:
		PBIT32(p, f->fid);
		p += BIT32SZ;
		PBIT32(p, f->afid);
		p += BIT32SZ;
		p  = pstring(p, f->uname);
		p  = pstring(p, f->aname);
		break;

	case Twalk:
		PBIT32(p, f->fid);
		p += BIT32SZ;
		PBIT32(p, f->newfid);
		p += BIT32SZ;
		PBIT16(p, f->nwname);
		p += BIT16SZ;
		if(f->nwname > MAXWELEM)
			return 0;
		for(i=0; i<f->nwname; i++)
			p = pstring(p, f->wname[i]);
		break;

	case Topen:
		PBIT32(p, f->fid);
		p += BIT32SZ;
		PBIT8(p, f->mode);
		p += BIT8SZ;
		break;

	case Tcreate:
		PBIT32(p, f->fid);
		p += BIT32SZ;
		p = pstring(p, f->name);
		PBIT32(p, f->perm);
		p += BIT32SZ;
		PBIT8(p, f->mode);
		p += BIT8SZ;
		break;

	case Tread:
		PBIT32(p, f->fid);
		p += BIT32SZ;
		PBIT64(p, f->offset);
		p += BIT64SZ;
		PBIT32(p, f->count);
		p += BIT32SZ;
		break;

	case Twrite:
		PBIT32(p, f->fid);
		p += BIT32SZ;
		PBIT64(p, f->offset);
		p += BIT64SZ;
		PBIT32(p, f->count);
		p += BIT32SZ;
		if(f->count != 0)
			memmove(p, f->data, f->count);
		p += f->count;
		break;

	case Tclunk:
	case Tremove:
		PBIT32(p, f->fid);
		p += BIT32SZ;
		break;

	case Tstat:
		PBIT32(p, f->fid);
		p += BIT32SZ;
		break;

	case Twstat:
		PBIT32(p, f->fid);
		p += BIT32SZ;
		PBIT16(p, f->nstat);
		p += BIT16SZ;
		if(f->nstat != 0)
			memmove(p, f->stat, f->nstat);
		p += f->nstat;
		break;
/*
 */

	case Rversion:
		PBIT32(p, f->msize);
		p += BIT32SZ;
		p = pstring(p, f->version);
		break;

	case Rerror:
		p = pstring(p, f->ename);
		break;

	case Rflush:
		break;

	case Rauth:
		p = pqid(p, &f->aqid);
		break;

	case Rattach:
		p = pqid(p, &f->qid);
		break;

	case Rwalk:
		PBIT16(p, f->nwqid);
		p += BIT16SZ;
		if(f->nwqid > MAXWELEM)
			return 0;
		for(i=0; i<f->nwqid; i++)
			p = pqid(p, &f->wqid[i]);
		break;

	case Ropen:
	case Rcreate:
		p = pqid(p, &f->qid);
		PBIT32(p, f->iounit);
		p += BIT32SZ;
		break;

	case Rread:
		PBIT32(p, f->count);
		p += BIT32SZ;
		if(f->count != 0)
			memmove(p, f->data, f->count);
		p += f->count;
		break;

	case Rwrite:
		PBIT32(p, f->count);
		p += BIT32SZ;
		break;

	case Rclunk:
		break;

	case Rremove:
		break;

	case Rstat:
		PBIT16(p, f->nstat);
		p += BIT16SZ;
		if(f->nstat != 0)
			memmove(p, f->stat, f->nstat);
		p += f->nstat;
		break;

	case Rwstat:
		break;
	}
	if(size != p-ap)
		return 0;
	return size;
}
