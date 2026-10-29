/* Shared classic/Dis64 signed30 operand emission; no new wire IDs are assigned. */
#ifndef DIS_OPERAND_H
#define DIS_OPERAND_H // Shared compact operand encoder guard.


/* Emit the shortest representation using unsigned shifts; failure writes nothing. */
static unsigned int
disoperand(unsigned char *output, unsigned int capacity, long value)
{
	unsigned long bits;
	unsigned int length, i, mask, tag;

	if(value < -536870912L || value > 536870911L)
		return 0;
	length = value >= -64 && value <= 63 ? 1 :
		value >= -8192 && value <= 8191 ? 2 : 4;
	if(output == 0 || capacity < length)
		return 0;
	bits = (unsigned long)value;
	mask = length == 1 ? 0x7f : 0x3f;
	tag = length == 1 ? 0 : length == 2 ? 0x80 : 0xc0;
	for(i=0; i<length; i++)
		output[i] = (bits>>(8*(length-i-1)))&0xff;
	output[0] = (output[0]&mask)|tag;
	return length;
}
#endif
