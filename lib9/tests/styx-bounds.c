#include "lib9.h"
#include "fcall.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

IMPLEMENTATION

static int checks;

/* Disable only unsupported LeakSanitizer process inspection. */
int __lsan_is_turned_off(void) { return 1; }

/* Fill all message variants with reproducible high-bit scalars and shared payloads. */
static void
setup(Fcall *f, int type, char *text, uint count, int walk)
{
	int i;

	memset(f, 0, sizeof(*f));
	f->type = type; f->tag = 0xfedc;
	f->fid = 0x89abcdef; f->afid = 0xfedcba98; f->newfid = 0x98765432;
	f->msize = 0x87654321; f->oldtag = 0xfedc;
	f->perm = 0xfedcba98; f->mode = 0xe7; f->iounit = 0x98765432;
	f->offset = -0x123456789abcdefLL;
	f->count = count; f->data = text;
	f->version = f->uname = f->aname = f->ename = f->name = text;
	f->nstat = count; f->stat = (uchar*)text;
	f->qid.type = 0x87; f->qid.vers = 0x87654321;
	f->qid.path = 0xfedcba9889abcdefULL; f->aqid = f->qid;
	f->nwname = f->nwqid = walk;
	for(i = 0; i < MAXWELEM; i++){
		f->wname[i] = text;
		f->wqid[i] = f->qid;
		f->wqid[i].path += i;
	}
}

/* Compute protocol lengths independently from the production sizing routine. */
static uint
expectedsize(int type, uint len, uint count, int walk)
{
	switch(type){
	case Tversion: case Rversion: return 13+len;
	case Tauth: return 15+2*len;
	case Tattach: return 19+2*len;
	case Rauth: case Rattach: return 20;
	case Rerror: return 9+len;
	case Tflush: return 9;
	case Rflush: case Rclunk: case Rremove: case Rwstat: return 7;
	case Twalk: return 17+walk*(2+len);
	case Rwalk: return 9+walk*13;
	case Topen: return 12;
	case Ropen: case Rcreate: return 24;
	case Tcreate: return 18+len;
	case Tread: return 23;
	case Rread: return 11+count;
	case Twrite: return 23+count;
	case Rwrite: return 11;
	case Tclunk: case Tremove: case Tstat: return 11;
	case Rstat: return 9+count;
	case Twstat: return 13+count;
	}
	abort();
}

/* Compare every field carried by the selected request or reply. */
static void
same(Fcall *a, Fcall *b)
{
	int i;

	assert(a->type == b->type && a->tag == b->tag);
	switch(a->type){
	case Tversion: case Rversion:
		assert(a->msize == b->msize && !strcmp(a->version, b->version)); break;
	case Tauth: case Tattach:
		if(a->type == Tattach)assert(a->fid == b->fid);
		assert(a->afid == b->afid);
		assert(!strcmp(a->uname, b->uname) && !strcmp(a->aname, b->aname)); break;
	case Rauth:
		assert(a->aqid.type == b->aqid.type && a->aqid.vers == b->aqid.vers);
		assert(a->aqid.path == b->aqid.path); break;
	case Rattach: case Ropen: case Rcreate:
		assert(a->qid.type == b->qid.type && a->qid.vers == b->qid.vers);
		assert(a->qid.path == b->qid.path);
		if(a->type != Rattach)assert(a->iounit == b->iounit);
		break;
	case Rerror: assert(!strcmp(a->ename, b->ename)); break;
	case Tflush: assert(a->oldtag == b->oldtag); break;
	case Twalk:
		assert(a->fid == b->fid && a->newfid == b->newfid && a->nwname == b->nwname);
		for(i = 0; i < a->nwname; i++)assert(!strcmp(a->wname[i], b->wname[i]));
		break;
	case Rwalk:
		assert(a->nwqid == b->nwqid);
		for(i = 0; i < a->nwqid; i++){
			assert(a->wqid[i].type == b->wqid[i].type);
			assert(a->wqid[i].vers == b->wqid[i].vers && a->wqid[i].path == b->wqid[i].path);
		}
		break;
	case Topen: case Tcreate:
		assert(a->fid == b->fid && a->mode == b->mode);
		if(a->type == Tcreate)assert(a->perm == b->perm && !strcmp(a->name, b->name));
		break;
	case Tread: case Twrite:
		assert(a->fid == b->fid && a->offset == b->offset);
		/* fall through */
	case Rread: case Rwrite:
		assert(a->count == b->count);
		if((a->type == Twrite || a->type == Rread) && a->count)
			assert(!memcmp(a->data, b->data, a->count));
		break;
	case Twstat:
		assert(a->fid == b->fid);
		/* fall through */
	case Rstat:
		assert(a->nstat == b->nstat);
		if(a->nstat)assert(!memcmp(a->stat, b->stat, a->nstat));
		break;
	case Tclunk: case Tremove: case Tstat: assert(a->fid == b->fid); break;
	}
}

/* Valid frames retain byte counts, fields, short-output behavior and trailing data. */
static void
valid(void)
{
	static uint lengths[] = {0, 1, 2, 7, 255, 4096, 65535};
	char *text;
	uchar *wire, *copy;
	Fcall f, got;
	uint n, len, i;
	int type, walk, k;
	uint byte;

	text = malloc(65536); wire = malloc(1100000); copy = malloc(1100000);
	assert(text && wire && copy);
	for(i = 0; i < sizeof(lengths)/sizeof(lengths[0]); i++){
		len = lengths[i]; memset(text, 'z', len); text[len] = 0;
		for(type = Tversion; type < Tmax; type++){
			if(type == Terror)continue;
			for(k = 0; k < 3; k++){
				walk = k == 0 ? 0 : k == 1 ? 1 : MAXWELEM;
				setup(&f, type, text, len, walk);
				n = expectedsize(type, len, len, walk);
				assert(sizeS2M(&f) == n);
				memset(wire, 0xa5, n+16);
				assert(convS2M(&f, wire+1, n-1) == 0);
				for(byte = 0; byte < n+16; byte++)assert(wire[byte] == 0xa5);
				assert(wire[0] == 0xa5 && wire[n] == 0xa5);
				assert(convS2M(&f, wire+1, n) == n);
				assert(wire[0] == 0xa5 && wire[n+1] == 0xa5);
				memmove(copy, wire+1, n+8);
				memset(&got, 0, sizeof(got));
				assert(convM2S(copy, n+8, &got) == n);
				assert(!memcmp(copy+n, wire+1+n, 8));
				same(&f, &got); checks++;
			}
		}
	}
	free(copy); free(wire); free(text);
}

/* Nil counted strings encode as empty strings in every string-bearing field. */
static void
nullstrings(void)
{
	Fcall f, got;
	uchar wire[1024];
	uint n;
	int type, i;

	for(type = Tversion; type < Tmax; type++){
		if(type == Terror)continue;
		setup(&f, type, nil, 0, MAXWELEM);
		n = expectedsize(type, 0, 0, MAXWELEM);
		assert(sizeS2M(&f) == n && convS2M(&f, wire, sizeof(wire)) == n);
		assert(convM2S(wire, n, &got) == n);
		f.version = f.uname = f.aname = f.ename = f.name = "";
		for(i = 0; i < MAXWELEM; i++)f.wname[i] = "";
		same(&f, &got); checks++;
	}
}

/* Counts without an inline payload remain full-width; size arithmetic is exact. */
static void
countlimits(void)
{
	Fcall f, got;
	uchar wire[64];
	int types[] = {Tread, Rwrite};
	int i;
	uint n;

	for(i = 0; i < 2; i++){
		setup(&f, types[i], nil, (uint)~0, 0);
		n = expectedsize(types[i], 0, 0, 0);
		assert(sizeS2M(&f) == n && convS2M(&f, wire, sizeof(wire)) == n);
		assert(convM2S(wire, n, &got) == n); same(&f, &got); checks++;
	}
	setup(&f, Twrite, nil, (uint)~0-23, 0);
	assert(sizeS2M(&f) == (uint)~0);
	f.count++;
	assert(sizeS2M(&f) == 0); checks++;
	setup(&f, Rread, nil, (uint)~0-11, 0);
	assert(sizeS2M(&f) == (uint)~0);
	f.count++;
	assert(sizeS2M(&f) == 0); checks++;
}

/* Invalid Fcall sizes must be rejected before modifying any output bytes. */
static void
reject(Fcall *f)
{
	uchar wire[64], saved[64];

	memset(wire, 0xa5, sizeof(wire)); memmove(saved, wire, sizeof(wire));
	assert(sizeS2M(f) == 0);
	assert(convS2M(f, wire+1, sizeof(wire)-2) == 0);
	assert(!memcmp(wire, saved, sizeof(wire))); checks++;
}

/* Sizing remains possible before the opaque payload storage is assigned. */
static void
missingpayload(Fcall *f)
{
	uchar wire[64], saved[64];

	memset(wire, 0xa5, sizeof(wire)); memmove(saved, wire, sizeof(wire));
	assert(sizeS2M(f) == expectedsize(f->type, 0, 1, 0));
	assert(convS2M(f, wire, sizeof(wire)) == 0);
	assert(!memcmp(wire, saved, sizeof(wire))); checks++;
}

/* Payload addition must not wrap the unsigned 32-bit wire size. */
static void
overflowerrors(void)
{
	Fcall f;
	uint count;
	int type, i;

	for(type = Twrite; type != 0; type = type == Twrite ? Rread : 0){
		for(i = 0; i < 11; i++){
			count = (uint)~0-i;
			setup(&f, type, "x", count, 0); reject(&f);
		}
	}
}

/* Reject oversized walk counts before reading the fixed Fcall arrays. */
static void
walkerrors(void)
{
	Fcall f;
	int i;

	for(i = MAXWELEM+1; i <= MAXWELEM+32; i++){
		setup(&f, Twalk, "x", 0, i); reject(&f);
		setup(&f, Rwalk, "x", 0, i); reject(&f);
	}
}

/* Strings longer than the wire count are invalid in every string-bearing type. */
static void
stringerrors(void)
{
	Fcall f;
	char *longtext;
	int type, length;

	longtext = malloc(65538); assert(longtext);
	for(length = 65536; length <= 65537; length++){
		memset(longtext, 'x', length); longtext[length] = 0;
		for(type = Tversion; type < Tmax; type++){
			if(type != Tversion && type != Rversion && type != Tauth && type != Tattach &&
			   type != Tcreate && type != Rerror && type != Twalk)continue;
			setup(&f, type, longtext, 0, 1); reject(&f);
		}
	}
	free(longtext);
}

/* Missing nonempty payloads and unknown types must not partially write headers. */
static void
payloaderrors(void)
{
	Fcall f;
	int type;

	for(type = Twrite; type != 0; type = type == Twrite ? Rread : 0){
		setup(&f, type, nil, 1, 0); missingpayload(&f);
	}
	setup(&f, Twstat, nil, 1, 0); missingpayload(&f);
	setup(&f, Rstat, nil, 1, 0); missingpayload(&f);
	setup(&f, Terror, "x", 0, 0); reject(&f);
}

/* Each invalid-input family can reproduce its original failure independently. */
static void
encodeerrors(void)
{
	char *only;

	only = getenv("STYX_ENCODE");
	if(only == nil || !strcmp(only, "overflow"))overflowerrors();
	if(only == nil || !strcmp(only, "walk"))walkerrors();
	if(only == nil || !strcmp(only, "string"))stringerrors();
	if(only == nil || !strcmp(only, "payload"))payloaderrors();
}

/* Every truncated frame must fail without modifying the following frame. */
static void
decodeerrors(void)
{
	Fcall f, got;
	uchar wire[1024], copy[1024], *shortbuf;
	uint n, cut, declared;
	int type;

	for(type = Tversion; type < Tmax; type++){
		if(type == Terror)continue;
		setup(&f, type, "abcdefg", 7, MAXWELEM);
		n = sizeS2M(&f);
		memset(wire, 0xa5, sizeof(wire));
		assert(convS2M(&f, wire, sizeof(wire)) == n);
		for(declared = n+1; declared <= n+2; declared++){
			memmove(copy, wire, sizeof(wire)); PBIT32(copy, declared);
			memmove(wire, copy, sizeof(wire));
			assert(convM2S(copy, n, &got) == 0);
			assert(!memcmp(copy, wire, sizeof(wire))); checks++;
		}
		PBIT32(wire, n);
		for(cut = 0; cut < n; cut++){
			shortbuf = malloc(cut ? cut : 1); assert(shortbuf);
			memmove(shortbuf, wire, cut);
			assert(convM2S(shortbuf, cut, &got) == 0);
			free(shortbuf); checks++;
			if(cut < 7)continue;
			memmove(copy, wire, sizeof(wire)); PBIT32(copy, cut);
			assert(convM2S(copy, sizeof(copy), &got) == 0);
			assert(!memcmp(copy+cut, wire+cut, sizeof(wire)-cut)); checks++;
		}
	}
}

/* Malformed count fields and mixed bytes cannot cross the declared frame. */
static void
malformed(void)
{
	Fcall f, got;
	uchar wire[256], saved[256];
	uint seed, n, declared, i, j;
	int type;

	for(type = Twrite; type != 0; type = type == Twrite ? Rread : 0){
		setup(&f, type, "x", 1, 0);
		n = convS2M(&f, wire, sizeof(wire)); assert(n > 0);
		i = type == Twrite ? 19 : 7;
		memset(wire+i, 0xff, 4);
		assert(convM2S(wire, n, &got) == 0); checks++;
	}
	seed = 0x12345678;
	for(j = 0; j < 4096; j++){
		for(i = 0; i < sizeof(wire); i++){
			seed = seed*1664525U+1013904223U;
			wire[i] = seed>>24;
		}
		declared = 7+seed%(sizeof(wire)-6);
		PBIT32(wire, declared);
		memmove(saved, wire, sizeof(wire));
		n = convM2S(wire, sizeof(wire), &got);
		assert(n == 0 || n == declared);
		assert(!memcmp(wire+declared, saved+declared, sizeof(wire)-declared)); checks++;
	}
}

/* Empty payloads permit nil storage without a null memmove call. */
static void
empty(void)
{
	Fcall f, got;
	uchar wire[64];
	int types[] = {Twrite, Rread, Twstat, Rstat};
	int i;
	uint n;

	assert(convM2S(nil, 0, &got) == 0);
	for(i = 0; i < 4; i++){
		setup(&f, types[i], nil, 0, 0);
		n = sizeS2M(&f); assert(n != 0);
		assert(convS2M(&f, wire, sizeof(wire)) == n);
		assert(convM2S(wire, n, &got) == n); same(&f, &got); checks++;
	}
}

/* Keep failure families independently runnable for before/after comparisons. */
int
main(void)
{
	char *only;

	only = getenv("STYX_CASES");
	if(only == nil || !strcmp(only, "valid")){
		valid(); nullstrings(); countlimits();
	}
	if(only == nil || !strcmp(only, "encode"))encodeerrors();
	if(only == nil || !strcmp(only, "decode")){
		decodeerrors(); malformed();
	}
	if(only == nil || !strcmp(only, "empty"))empty();
	printf("Styx frame bounds: %d cases\n", checks);
	return 0;
}
