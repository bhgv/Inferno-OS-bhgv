#include	"lib9.h"
#include	"fcall.h"

/* Decode a counted string entirely inside the declared message. */
static
uchar*
gstring(uchar *p, uchar *ep, char **s)
{
	uint n;

	if((uint)(ep-p) < BIT16SZ)
		return nil;
	n = GBIT16(p);
	if(n > (uint)(ep-p)-BIT16SZ)
		return nil;
	p += BIT16SZ - 1;
	/* move it down, on top of count, to make room for '\0' */
	memmove(p, p + 1, n);
	p[n] = '\0';
	*s = (char*)p;
	p += n+1;
	return p;
}

/* Read a qid only after checking the remaining message bytes. */
static
uchar*
gqid(uchar *p, uchar *ep, Qid *q)
{
	if((uint)(ep-p) < QIDSZ)
		return nil;
	q->type = GBIT8(p);
	p += BIT8SZ;
	q->vers = GBIT32(p);
	p += BIT32SZ;
	q->path = GBIT64(p);
	p += BIT64SZ;
	return p;
}

/*
 * no syntactic checks.
 * three causes for error:
 *  1. message size field is incorrect
 *  2. input buffer too short for its own data (counts too long, etc.)
 *  3. too many names or qids
 * gqid() and gstring() return nil if they would reach beyond buffer.
 * main switch statement checks range and also can fall through
 * to test at end of routine.
 */
uint
convM2S(uchar *ap, uint nap, Fcall *f)
{
	uchar *p, *ep;
	uint i, size;

	if(nap < BIT32SZ+BIT8SZ+BIT16SZ)
		return 0;
	size = GBIT32(ap);
	if(size < BIT32SZ+BIT8SZ+BIT16SZ || size > nap)
		return 0;

	p = ap + BIT32SZ;
	ep = ap + size;

	f->type = GBIT8(p);
	p += BIT8SZ;
	f->tag = GBIT16(p);
	p += BIT16SZ;

	switch(f->type)
	{
	default:
		return 0;

	case Tversion:
		if((uint)(ep-p) < BIT32SZ)
			return 0;
		f->msize = GBIT32(p);
		p += BIT32SZ;
		p = gstring(p, ep, &f->version);
		break;

	case Tflush:
		if((uint)(ep-p) < BIT16SZ)
			return 0;
		f->oldtag = GBIT16(p);
		p += BIT16SZ;
		break;

	case Tauth:
		if((uint)(ep-p) < BIT32SZ)
			return 0;
		f->afid = GBIT32(p);
		p += BIT32SZ;
		p = gstring(p, ep, &f->uname);
		if(p == nil)
			break;
		p = gstring(p, ep, &f->aname);
		if(p == nil)
			break;
		break;

	case Tattach:
		if((uint)(ep-p) < BIT32SZ)
			return 0;
		f->fid = GBIT32(p);
		p += BIT32SZ;
		if((uint)(ep-p) < BIT32SZ)
			return 0;
		f->afid = GBIT32(p);
		p += BIT32SZ;
		p = gstring(p, ep, &f->uname);
		if(p == nil)
			break;
		p = gstring(p, ep, &f->aname);
		if(p == nil)
			break;
		break;

	case Twalk:
		if((uint)(ep-p) < BIT32SZ+BIT32SZ+BIT16SZ)
			return 0;
		f->fid = GBIT32(p);
		p += BIT32SZ;
		f->newfid = GBIT32(p);
		p += BIT32SZ;
		f->nwname = GBIT16(p);
		p += BIT16SZ;
		if(f->nwname > MAXWELEM)
			return 0;
		for(i=0; i<f->nwname; i++){
			p = gstring(p, ep, &f->wname[i]);
			if(p == nil)
				break;
		}
		break;

	case Topen:
		if((uint)(ep-p) < BIT32SZ+BIT8SZ)
			return 0;
		f->fid = GBIT32(p);
		p += BIT32SZ;
		f->mode = GBIT8(p);
		p += BIT8SZ;
		break;

	case Tcreate:
		if((uint)(ep-p) < BIT32SZ)
			return 0;
		f->fid = GBIT32(p);
		p += BIT32SZ;
		p = gstring(p, ep, &f->name);
		if(p == nil)
			break;
		if((uint)(ep-p) < BIT32SZ+BIT8SZ)
			return 0;
		f->perm = GBIT32(p);
		p += BIT32SZ;
		f->mode = GBIT8(p);
		p += BIT8SZ;
		break;

	case Tread:
		if((uint)(ep-p) < BIT32SZ+BIT64SZ+BIT32SZ)
			return 0;
		f->fid = GBIT32(p);
		p += BIT32SZ;
		f->offset = GBIT64(p);
		p += BIT64SZ;
		f->count = GBIT32(p);
		p += BIT32SZ;
		break;

	case Twrite:
		if((uint)(ep-p) < BIT32SZ+BIT64SZ+BIT32SZ)
			return 0;
		f->fid = GBIT32(p);
		p += BIT32SZ;
		f->offset = GBIT64(p);
		p += BIT64SZ;
		f->count = GBIT32(p);
		p += BIT32SZ;
		if((uint)(ep-p) < f->count)
			return 0;
		f->data = (char*)p;
		p += f->count;
		break;

	case Tclunk:
	case Tremove:
		if((uint)(ep-p) < BIT32SZ)
			return 0;
		f->fid = GBIT32(p);
		p += BIT32SZ;
		break;

	case Tstat:
		if((uint)(ep-p) < BIT32SZ)
			return 0;
		f->fid = GBIT32(p);
		p += BIT32SZ;
		break;

	case Twstat:
		if((uint)(ep-p) < BIT32SZ+BIT16SZ)
			return 0;
		f->fid = GBIT32(p);
		p += BIT32SZ;
		f->nstat = GBIT16(p);
		p += BIT16SZ;
		if((uint)(ep-p) < f->nstat)
			return 0;
		f->stat = p;
		p += f->nstat;
		break;

/*
 */
	case Rversion:
		if((uint)(ep-p) < BIT32SZ)
			return 0;
		f->msize = GBIT32(p);
		p += BIT32SZ;
		p = gstring(p, ep, &f->version);
		break;

	case Rerror:
		p = gstring(p, ep, &f->ename);
		break;

	case Rflush:
		break;

	case Rauth:
		p = gqid(p, ep, &f->aqid);
		if(p == nil)
			break;
		break;

	case Rattach:
		p = gqid(p, ep, &f->qid);
		if(p == nil)
			break;
		break;

	case Rwalk:
		if((uint)(ep-p) < BIT16SZ)
			return 0;
		f->nwqid = GBIT16(p);
		p += BIT16SZ;
		if(f->nwqid > MAXWELEM)
			return 0;
		for(i=0; i<f->nwqid; i++){
			p = gqid(p, ep, &f->wqid[i]);
			if(p == nil)
				break;
		}
		break;

	case Ropen:
	case Rcreate:
		p = gqid(p, ep, &f->qid);
		if(p == nil)
			break;
		if((uint)(ep-p) < BIT32SZ)
			return 0;
		f->iounit = GBIT32(p);
		p += BIT32SZ;
		break;

	case Rread:
		if((uint)(ep-p) < BIT32SZ)
			return 0;
		f->count = GBIT32(p);
		p += BIT32SZ;
		if((uint)(ep-p) < f->count)
			return 0;
		f->data = (char*)p;
		p += f->count;
		break;

	case Rwrite:
		if((uint)(ep-p) < BIT32SZ)
			return 0;
		f->count = GBIT32(p);
		p += BIT32SZ;
		break;

	case Rclunk:
	case Rremove:
		break;

	case Rstat:
		if((uint)(ep-p) < BIT16SZ)
			return 0;
		f->nstat = GBIT16(p);
		p += BIT16SZ;
		if((uint)(ep-p) < f->nstat)
			return 0;
		f->stat = p;
		p += f->nstat;
		break;

	case Rwstat:
		break;
	}

	if(p==nil || p>ep)
		return 0;
	if(p == ep)
		return size;
	return 0;
}
