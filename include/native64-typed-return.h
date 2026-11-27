#ifndef NATIVE64_TYPED_RETURN_H
#define NATIVE64_TYPED_RETURN_H
/* Private trusted-C return binding, not file admission or pointer authentication. */
#include "native64-model.h"
#include "native64-result.h"

typedef struct Native64TypedReturn Native64TypedReturn;
struct Native64TypedReturn {
	Native64Result *callee,*caller;
	void *frame,*owner,*slot;
	Type *frame_type,*owner_type;
	unsigned int offset,width;
	int reference;
};

static int
native64typedreturntype(Type *t)
{
	unsigned int i,bit;
	if(t==nil || t==H || t->ref<=0 || t->size<64 || t->size>1024*1024 ||
	   t->np<0 || t->np>(t->size+63)/64 || (t->np && t->map[0])) return 0;
	for(i=0;i<(unsigned int)t->np;i++)for(bit=0;bit<8;bit++)
		if((t->map[i]&(0x80>>bit)) && (i*64+bit*8+8>(unsigned int)t->size))return 0;
	return 1;
}

static int
native64typedreturnvalid(Native64TypedReturn *b)
{
	Heap *frame,*owner;
	unsigned int at,bit;
	if(!native64model() || b==nil || b->callee==nil || b->caller==nil ||
	   !b->callee->rooted || !b->caller->rooted || b->callee->value!=b->frame || b->caller->value!=b->owner ||
	   b->frame==nil || b->frame==H || b->owner==nil || b->owner==H || b->frame==b->owner ||
	   (b->width!=1 && b->width!=4 && b->width!=8) || (b->reference!=0 && b->reference!=1))return 0;
	frame=D2H(b->frame);owner=D2H(b->owner);
	if(!frame->ref || !owner->ref || frame->t!=b->frame_type || owner->t!=b->owner_type ||
	   !native64typedreturntype(frame->t) || !native64typedreturntype(owner->t) ||
	   b->offset<64 || b->offset>(unsigned int)owner->t->size || b->width>(unsigned int)owner->t->size-b->offset ||
	   b->offset%b->width || b->slot!=(uchar*)b->owner+b->offset ||
	   (b->reference && b->width!=8))return 0;
	for(at=b->offset;at<b->offset+b->width;at++){
		bit=at/8;
		if(!!(bit/8<(unsigned int)owner->t->np && (owner->t->map[bit/8]&(0x80>>(bit%8)))) != b->reference)return 0;
	}
	return 1;
}

static int
native64typedreturnbind(Native64TypedReturn *b,Native64Result *callee,Native64Result *caller,unsigned int offset,unsigned int width,int reference)
{
	Native64TypedReturn next={0};
	void *previous;
	if(b==nil || b->callee!=nil || callee==nil || caller==nil || !callee->rooted || !caller->rooted ||
	   callee->value==nil || callee->value==H || caller->value==nil || caller->value==H)return 0;
	next.callee=callee;next.caller=caller;next.frame=callee->value;next.owner=caller->value;
	next.frame_type=D2H(next.frame)->t;next.owner_type=D2H(next.owner)->t;
	next.offset=offset;next.width=width;next.reference=reference;
	if(offset>1024*1024)return 0;
	if((uintptr)next.owner>(uintptr)-1-offset)return 0;
	next.slot=(void*)((uintptr)next.owner+offset);
	if(!native64typedreturnvalid(&next))return 0;
	memmove(&previous,(uchar*)next.frame+32,sizeof(previous));if(previous!=nil)return 0;
	memmove((uchar*)next.frame+32,&next.slot,sizeof(next.slot));
	*b=next;return 1;
}

static void*
native64typedreturnslot(Native64TypedReturn *b,unsigned int width,int reference)
{
	void *slot;
	if(!native64typedreturnvalid(b) || b->width!=width || b->reference!=reference)return nil;
	memmove(&slot,(uchar*)b->frame+32,sizeof(slot));
	return slot==b->slot?slot:nil;
}

static void
native64typedreturnclear(Native64TypedReturn *b)
{
	void *zero=nil;
	if(b==nil)return;
	if(native64typedreturnslot(b,b->width,b->reference)!=nil)memmove((uchar*)b->frame+32,&zero,sizeof(zero));
	memset(b,0,sizeof(*b));
}
#endif
