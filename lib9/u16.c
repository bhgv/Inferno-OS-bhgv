#include <lib9.h>
static char t16e[] = "0123456789ABCDEF";

/* Decode hexadecimal pairs, retaining the legacy ignored-delimiter behavior. */
int
dec16(uchar *out, int lim, char *in, int n)
{
	int c, high, pending, used;

	if(lim <= 0 || n <= 0)
		return 0;
	high = 0;
	pending = 0;
	used = 0;
	while(n > 0 && used < lim){
		n--;
		c = (uchar)*in++;
		if(c >= '0' && c <= '9')
			c -= '0';
		else if(c >= 'a' && c <= 'f')
			c = c-'a'+10;
		else if(c >= 'A' && c <= 'F')
			c = c-'A'+10;
		else
			continue;
		if(pending){
			out[used++] = (high<<4)|c;
			pending = 0;
		}else{
			high = c;
			pending = 1;
		}
	}
	return used;
}

/* Reserve a terminator and stop before reading a byte that cannot be encoded. */
int
enc16(char *out, int lim, uchar *in, int n)
{
	uint c;
	int used;

	if(lim <= 0)
		return 0;
	used = 0;
	while(n > 0 && lim-used > 2){
		n--;
		c = *in++;
		out[used++] = t16e[c>>4];
		out[used++] = t16e[c&0xf];
	}
	out[used] = 0;
	return used;
}
