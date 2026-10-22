#include <lib9.h>

static char base32[] = "23456789abcdefghijkmnpqrstuvwxyz";

/*
 * Decode complete base32 byte groups. Validate the whole input and capacity
 * before writing, so malformed text or short buffers leave dest unchanged.
 */
int
dec32(uchar *dest, int ndest, char *src, int nsrc)
{
	uvlong required;
	uint value;
	int i, bits, n, tail;
	char *digit;

	if(nsrc < 0 || ndest < 0)
		return -1;
	tail = nsrc%8;
	if(tail == 1 || tail == 3 || tail == 6)
		return -1;
	required = (uvlong)nsrc*5/8;
	if(required > (uvlong)ndest)
		return -1;
	for(i = 0; i < nsrc; i++)
		if(src[i] == 0 || strchr(base32, (uchar)src[i]) == nil)
			return -1;

	value = 0;
	bits = 0;
	n = 0;
	for(i = 0; i < nsrc; i++){
		digit = strchr(base32, (uchar)src[i]);
		value = (value<<5) | (digit-base32);
		bits += 5;
		if(bits >= 8){
			bits -= 8;
			dest[n++] = value>>bits;
		}
	}
	return n;
}

/* Encode every tail length, reserving space for the terminating NUL. */
int
enc32(char *dest, int ndest, uchar *src, int nsrc)
{
	uvlong required;
	uint value;
	int i, bits, n;

	if(nsrc < 0 || ndest <= 0)
		return -1;
	required = ((uvlong)nsrc*8+4)/5;
	if(required >= (uvlong)ndest)
		return -1;

	value = 0;
	bits = 0;
	n = 0;
	for(i = 0; i < nsrc; i++){
		value = (value<<8) | src[i];
		bits += 8;
		while(bits >= 5){
			bits -= 5;
			dest[n++] = base32[(value>>bits)&31];
		}
	}
	if(bits)
		dest[n++] = base32[(value<<(5-bits))&31];
	dest[n] = 0;
	return n;
}
