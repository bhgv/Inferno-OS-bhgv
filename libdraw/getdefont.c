#include "lib9.h"
#include "draw.h"
#include "kernel.h"
#include "drawinfo.h"
#include "pixelspan.h"


/* Decode the immutable built-in byte image completely before allocating or sending resources. */
Subfont*
getdefont(Display *display)
{
	uchar *pixels, *header, *packed;
	Image *image;
	Fontchar *info;
	Subfont *font;
	Rectangle rectangle;
	int fields[5], index, stride, bytes, count, height, ascent, remaining, previous, x;
	char reason[ERRMAX];

	if(display == nil || display->closing || sizeofdefont < 96){
		kwerrstr("getdefont: invalid display or image extent");
		return nil;
	}
	for(index = 0; index < 5; index++){
		if(!drawinfointeger((char*)defontdata+12*index, fields+index)){
			kwerrstr("getdefont: invalid image header");
			return nil;
		}
	}
	if(fields[0] < 0 || fields[0] > 3){
		kwerrstr("getdefont: invalid image depth");
		return nil;
	}
	rectangle = Rect(fields[1], fields[2], fields[3], fields[4]);
	if(!drawpixelspan(rectangle, 1<<fields[0], sizeofdefont-96, &stride, &bytes) || bytes <= 0){
		kwerrstr("getdefont: invalid pixel extent");
		return nil;
	}
	pixels = defontdata+60;
	header = pixels+bytes;
	if(!drawinfointeger((char*)header, &count) ||
	   !drawinfointeger((char*)header+12, &height) ||
	   !drawinfointeger((char*)header+24, &ascent) ||
	   count < 0 || count > 32767 || height <= 0 || height > 255 || ascent != (char)ascent){
		kwerrstr("getdefont: invalid subfont header");
		return nil;
	}
	remaining = sizeofdefont-96-bytes;
	if(remaining != 6*(count+1)){
		kwerrstr("getdefont: invalid metrics extent");
		return nil;
	}
	packed = header+36;
	previous = rectangle.min.x;
	for(index = 0; index <= count; index++){
		x = packed[6*index] | (packed[6*index+1]<<8);
		if(x < previous || x > rectangle.max.x ||
		   packed[6*index+2] > packed[6*index+3] || packed[6*index+3] > height){
			kwerrstr("getdefont: invalid character metrics");
			return nil;
		}
		previous = x;
	}
	image = allocimage(display, rectangle, drawld2chan[fields[0]], 0, 0);
	if(image == nil)
		return nil;
	info = nil;
	if(loadimage(image, rectangle, pixels, bytes) != bytes)
		goto Refuse;
	info = malloc(sizeof(Fontchar)*(count+1));
	if(info == nil)
		goto Refuse;
	_unpackinfo(info, packed, count);
	font = allocsubfont("*default*", count, height, ascent, info, image);
	if(font != nil)
		return font;

Refuse:
	kgerrstr(reason, sizeof(reason));
	free(info);
	freeimage(image);
	kwerrstr("%s", reason);
	return nil;
}
