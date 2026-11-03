#ifndef DRAW_PACKET_H
#define DRAW_PACKET_H // Private bounded reservation, no public or wire layout change.


/* Reserve operator and command together so a refused command cannot leave a pending operator. */
static uchar*
drawpacket(Display *display, int bytes, Drawop op)
{
	uchar *packet;
	int prefix;

	if(display == nil || bytes < 0 || bytes > 2147483645 || (unsigned int)op >= Ncomp)
		return nil;
	prefix = op != SoverD ? 2 : 0;
	packet = bufimage(display, bytes+prefix);
	if(packet == nil)
		return nil;
	if(prefix != 0){
		packet[0] = 'O';
		packet[1] = op;
	}
	return packet+prefix;
}

#endif
