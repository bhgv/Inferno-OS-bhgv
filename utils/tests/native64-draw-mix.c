/* Resolve each actual packet ID while all source/mask owners must still be alive. */
static Image*
miximage(Display *display, ulong id)
{
	unsigned int index;
	Image *image;

	for(index = 0; index < nelem(natives); index++){
		if(natives[index].kind != 1 || natives[index].pointer == nil)
			continue;
		image = natives[index].pointer;
		if(image->id == id){
			require(image->display == display);
			return image;
		}
	}
	require(0);
	return nil;
}


/* Inspect actual draw bytes before any following image-free command replaces the bounded buffer. */
static void
mixobserve(Display *display)
{
	Image *destination, *source, *mask;

	destination = miximage(display, BGLONG(mixpacket+1));
	source = miximage(display, BGLONG(mixpacket+5));
	mask = miximage(display, BGLONG(mixpacket+9));
	require(destination != source && destination != mask && source != mask);
	if(display->depth <= 8)
		require(mask == display->opaque);
	else
		require(mask->chan == GREY8 && mask->repl == 1);
}


/* Break only fixture default-image cycles after every mix result and temporary has retired. */
static void
mixdrop(DDisplay *display)
{
	Draw_Image *base, *white;

	base = display->drawdisplay.image;
	white = display->drawdisplay.white;
	display->drawdisplay.image = H;
	display->drawdisplay.white = H;
	display->display->image = nil;
	display->display->white = nil;
	display->display->opaque = nil;
	destroy(base);
	destroy(white);
	require(D2H(display)->ref == 1 && display->dref->ref == 1);
	destroy(display);
}


/* Alternate live displays, inject every temporary allocation and refuse packet/flush boundaries. */
static void
mixingcases(void)
{
	DDisplay *owners[2], *owner;
	Display *display;
	Image *result, *oldopaque;
	int side, shallow, failure, before, draws, calls;

	owners[0] = attachdisplay();
	owners[1] = attachdisplay();
	for(side = 0; side < 4; side++){
		owner = owners[side%2];
		display = owner->display;
		display->opaque = display->white;
		for(shallow = 0; shallow < 2; shallow++){
			display->depth = shallow ? 8 : 32;
			display->chan = shallow ? CMAP8 : RGBA32;
			for(failure = 0; failure < (shallow ? 5 : 6); failure++){
				before = nativecount;
				draws = mixdraws;
				mixwatch = 1;
				if(failure > 0 && failure <= (shallow ? 2 : 3))
					failat = allocations+failure;
				else if(failure == (shallow ? 3 : 4))
					mixrefuse = 1;
				else if(failure == (shallow ? 4 : 5))
					flushfail = 1;
				result = freshallocimagemix(display, 0x112233ff, 0x778899ff);
				mixwatch = 0;
				failat = mixrefuse = wirefail = flushfail = 0;
				if(failure){
					require(result == nil && nativecount == before);
				}else{
					require(result != nil && result->display == display && nativecount == before+1);
					require(mixdraws == draws+1 && BGLONG(mixpacket+1) == result->id);
					require(BGLONG(mixpacket+13) == 0 && BGLONG(mixpacket+17) == 0);
					require(BGLONG(mixpacket+21) == 1 && BGLONG(mixpacket+25) == 1);
					require(Dx(result->r) == (shallow ? 2 : 1) && Dy(result->r) == (shallow ? 2 : 1));
					freeimage(result);
					require(nativecount == before);
				}
			}
		}
	}
	display = owners[0]->display;
	display->depth = 8;
	oldopaque = display->opaque;
	display->opaque = owners[1]->display->white;
	before = nativecount;
	calls = nativecalls;
	require(freshallocimagemix(display, 1, 2) == nil && nativecalls == calls && nativecount == before);
	display->opaque = nil;
	require(freshallocimagemix(display, 1, 2) == nil && nativecalls == calls);
	display->opaque = oldopaque;
	mixdrop(owners[0]);
	/* A remaining live display must still construct a correct mask after another display closes. */
	owners[1]->display->depth = 32;
	owners[1]->display->chan = RGBA32;
	before = nativecount;
	mixwatch = 1;
	result = freshallocimagemix(owners[1]->display, 3, 4);
	mixwatch = 0;
	require(result != nil && nativecount == before+1);
	freeimage(result);
	mixdrop(owners[1]);
	baseline();
}


/* Compose the real Frame64 factory with native mixing, packets and managed-wrapper allocation rollback. */
static void
mixframes(void)
{
	DDisplay *display;
	Draw_Image *result;
	F_Draw_Display_colormix frame;
	uchar packed[sizeof(void*)+1];
	void *slot, *empty;
	int alias, failure, before, refs;

	for(alias = 0; alias < 2; alias++){
		for(failure = 0; failure <= 4; failure++){
			display = attachdisplay();
			display->display->depth = 32;
			display->display->chan = RGBA32;
			display->display->opaque = display->display->white;
			before = nativecount;
			refs = D2H(display)->ref;
			memset(&frame, 0, sizeof(frame));
			frame.d = &display->drawdisplay;
			frame.c1 = 0x112233ff;
			frame.c2 = 0x778899ff;
			if(alias){
				D2H(display)->ref++;
				slot = &frame.d;
				dropwatch = display;
				dropbudget = 1;
			}else{
				empty = H;
				slot = packed+1;
				memmove(slot, &empty, sizeof(empty));
			}
			frame.ret = slot;
			if(failure)
				failat = allocations+failure;
			mixwatch = 1;
			Draw_Display_colormix(&frame);
			mixwatch = 0;
			failat = 0;
			if(alias)
				require(dropbudget == 0);
			dropwatch = nil;
			memmove(&result, slot, sizeof(result));
			if(failure){
				require(result == H && nativecount == before && D2H(display)->ref == refs);
			}else{
				require(result != H && result->display == &display->drawdisplay);
				require(D2H(result)->t == TImage && nativecount == before+1 && D2H(display)->ref == refs+1);
				destroy(result);
				require(nativecount == before && D2H(display)->ref == refs);
			}
			mixdrop(display);
			baseline();
		}
	}
}


/* Check wide modulo and the defined invalid-period fallback without ever accepting a divide-by-zero trap. */
static void
replicationcases(void)
{
	unsigned int index;
	Point point;
	ReplCase *row;

	for(index = 0; index < nelem(replcases); index++){
		row = replcases+index;
		require(drawreplxy(row->low, row->high, row->point) == row->expected);
		point = drawrepl(Rect(row->low, row->low, row->high, row->high), Pt(row->point, row->point));
		require(point.x == row->expected && point.y == row->expected);
	}
}
