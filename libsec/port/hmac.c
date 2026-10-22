#include "os.h"
#include <libsec.h>

/* RFC 2104 streaming HMAC, with allocation-free outer finalization. */
static DigestState*
hmac_x(uchar *p, ulong len, uchar *key, ulong klen, uchar *digest, DigestState *s,
	DigestState*(*x)(uchar*, ulong, uchar*, DigestState*), int xlen)
{
	int i;
	DigestState outer;
	uchar pad[65], innerdigest[256];

	if(xlen > sizeof(innerdigest))
		return nil;

	if(klen>64)
		return nil;

	/* first time through */
	if(s == nil || s->seeded == 0){
		for(i=0; i<64; i++)
			pad[i] = 0x36;
		pad[64] = 0;
		for(i=0; i<klen; i++)
			pad[i] ^= key[i];
		s = (*x)(pad, 64, nil, s);
		if(s == nil)
			return nil;
	}

	s = (*x)(p, len, nil, s);
	if(digest == nil)
		return s;

	/* last time through */
	for(i=0; i<64; i++)
		pad[i] = 0x5c;
	pad[64] = 0;
	for(i=0; i<klen; i++)
		pad[i] ^= key[i];
	(*x)(nil, 0, innerdigest, s);
	/* Finalization must not allocate another native state after consuming s. */
	memset(&outer, 0, sizeof(outer));
	(*x)(pad, 64, nil, &outer);
	(*x)(innerdigest, xlen, digest, &outer);
	return nil;
}

DigestState*
hmac_sha1(uchar *p, ulong len, uchar *key, ulong klen, uchar *digest, DigestState *s)
{
	return hmac_x(p, len, key, klen, digest, s, sha1, SHA1dlen);
}

DigestState*
hmac_md5(uchar *p, ulong len, uchar *key, ulong klen, uchar *digest, DigestState *s)
{
	return hmac_x(p, len, key, klen, digest, s, md5, MD5dlen);
}
