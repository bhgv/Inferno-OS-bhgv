/* Decode one twelve-byte decimal protocol field without depending on native long width. */
static int
drawinfointeger(char *field, int *result)
{
	uvlong value, limit;
	int position, negative, digits;

	position = 0;
	while(position < 12 && field[position] == ' ')
		position++;
	negative = 0;
	if(position < 12 && (field[position] == '-' || field[position] == '+')){
		negative = field[position] == '-';
		position++;
	}
	limit = negative ? 2147483648ULL : 2147483647ULL;
	value = 0;
	digits = 0;
	while(position < 12 && field[position] >= '0' && field[position] <= '9'){
		if(value > (limit-(field[position]-'0'))/10)
			return 0;
		value = value*10+field[position]-'0';
		digits++;
		position++;
	}
	while(position < 12 && field[position] == ' ')
		position++;
	if(digits == 0 || position != 12)
		return 0;
	if(negative)
		*result = value == 2147483648ULL ? (-2147483647-1) : -(int)value;
	else
		*result = value;
	return 1;
}


/* Validate the complete fixed-width device record before publishing any Image fields. */
static int
drawimageinfo(char *record, Image *image, int *directory)
{
	int fields[12], index, start, end;
	char channel[13];
	ulong descriptor;

	for(index = 0; index < 12; index++){
		if(index != 2 && !drawinfointeger(record+12*index, &fields[index]))
			return 0;
	}
	start = 0;
	while(start < 12 && record[24+start] == ' ')
		start++;
	end = start;
	while(end < 12 && record[24+end] != ' ')
		end++;
	memmove(channel, record+24+start, end-start);
	channel[end-start] = 0;
	while(end < 12 && record[24+end] == ' ')
		end++;
	if(end != 12 || (descriptor = strtochan(channel)) == 0 ||
	   fields[0] < 0 || fields[1] < 0 || (fields[3] != 0 && fields[3] != 1) ||
	   fields[4] > fields[6] || fields[5] > fields[7] ||
	   fields[8] > fields[10] || fields[9] > fields[11])
		return 0;
	image->chan = descriptor;
	image->depth = chantodepth(descriptor);
	image->repl = fields[3];
	image->r = Rect(fields[4], fields[5], fields[6], fields[7]);
	image->clipr = Rect(fields[8], fields[9], fields[10], fields[11]);
	if(directory != nil)
		*directory = fields[0];
	return 1;
}
