#include "lib9.h"
#include "draw.h"
#include "kernel.h"
#include "draw-packet.h"


/* Keep all temporary images within the current display and consume them on every refusal path. */
Image*
allocimagemix(Display *display, ulong color1, ulong color3)
{
	Image *source, *result, *mask;
	Rectangle rectangle;
	Point origin;
	char reason[ERRMAX];
	int shallow, status;

	if(display == nil || display->closing || chantodepth(display->chan) == 0){
		kwerrstr("allocimagemix: invalid display");
		return nil;
	}
	shallow = display->depth <= 8;
	if(shallow && (display->opaque == nil || display->opaque->display != display)){
		kwerrstr("allocimagemix: missing display mask");
		return nil;
	}
	source = nil;
	result = nil;
	mask = nil;
	origin = Pt(0, 0);
	source = allocimage(display, Rect(0,0,1,1), display->chan, !shallow, color1);
	if(source == nil)
		goto Refuse;
	result = allocimage(display, shallow ? Rect(0,0,2,2) : Rect(0,0,1,1), display->chan, 1, color3);
	if(result == nil)
		goto Refuse;
	if(!shallow){
		mask = allocimage(display, Rect(0,0,1,1), GREY8, 1, 0x3F3F3FFF);
		if(mask == nil)
			goto Refuse;
	}
	rectangle = shallow ? Rect(0,0,1,1) : result->r;
	if(!_drawimage(result, &rectangle, source, &origin, mask, &origin, SoverD))
		goto Refuse;
	status = freeimage(mask);
	mask = nil;
	if(status < 0)
		goto Refuse;
	status = freeimage(source);
	source = nil;
	if(status < 0)
		goto Refuse;
	return result;

Refuse:
	kgerrstr(reason, sizeof(reason));
	freeimage(mask);
	freeimage(source);
	freeimage(result);
	kwerrstr("%s", reason);
	return nil;
}
