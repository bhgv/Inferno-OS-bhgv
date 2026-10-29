#ifndef INFERNO_VMCHANNEL_H
#define INFERNO_VMCHANNEL_H // Checked native C-to-VM channel entry shared by emu and kernel callers.
#include "raise.h"

#if defined(__GNUC__)
#define VmChannelInline static __inline__ // Inline small preflight helpers without a new public runtime ABI.
#else
#define VmChannelInline static // Preserve compilers without GNU inline syntax.
#endif

typedef struct VmChannelCall VmChannelCall;

/* Keep native operands outside callbacks that may overwrite the global decoded register set. */
struct VmChannelCall
{
	Channel *channel; // Original channel owner, unchanged by REG borrowing.
	void *payload; // Original send source or receive destination.
	int receive; // Nonzero invokes the existing receive instruction.
};


/* Validate the complete queue head and peer before queue mutation or scheduler access. */
VmChannelInline Prog*
vmchanqueue(Progq *queue)
{
	Prog *peer;

	if(queue == nil || queue == H || (uintptr)queue%sizeof(void*) != 0 ||
	   (uintptr)queue > (uintptr)-1-sizeof(*queue))
		error(exHeap);
	memmove(&peer, (uchar*)queue+O(Progq, prog), sizeof(peer));
	if(peer != nil && (peer == H || (uintptr)peer%sizeof(void*) != 0 ||
	   (uintptr)peer > (uintptr)-1-sizeof(*peer)))
		error(exHeap);
	return peer;
}


/* Validate both queue heads before native or bytecode callers transfer or register a waiter. */
VmChannelInline int
vmchanready(Channel *c, int receive)
{
	Prog *sender, *receiver, *peer;
	Array *buffer;
	WORD length;

	if(c == nil || c == H)
		error(exNilref);
	if((uintptr)c%sizeof(void*) != 0 || (uintptr)c > (uintptr)-1-sizeof(*c))
		error(exHeap);
	sender = vmchanqueue(c->send);
	receiver = vmchanqueue(c->recv);
	peer = receive ? sender : receiver;
	buffer = c->buf;
	if(buffer == nil)
		error(exNilref);
	if(c->size < 0)
		error(exHeap);
	if(buffer == H){
		if(c->size != 0)
			error(exHeap);
		return peer != nil;
	}
	if((uintptr)buffer%sizeof(void*) != 0 ||
	   (uintptr)buffer > (uintptr)-1-sizeof(*buffer))
		error(exHeap);
	memmove(&length, (uchar*)buffer+O(Array, len), sizeof(length));
	if(length < 0 || (uvlong)length > 0x7fffffff || c->size > length)
		error(exHeap);
	return peer != nil || (receive ? c->size != 0 : c->size < length);
}


/* Invoke authoritative C instructions using captured native addresses, never borrowed REG fields. */
VmChannelInline int
vmchanrun(void *arg)
{
	VmChannelCall *call;
	Channel *channel;

	call = arg;
	channel = call->channel;
	if(call->receive){
		R.s = &channel;
		R.d = call->payload;
		irecv();
	}else{
		R.s = call->payload;
		R.d = &channel;
		isend();
	}
	return 1;
}


/* Restore the native caller's REG image on success and on an instruction's VM exception. */
VmChannelInline void
vmchantransfer(Channel *channel, void *payload, int receive)
{
	VmChannelCall call;
	REG saved;
	int ok;

	call.channel = channel;
	call.payload = payload;
	call.receive = receive;
	memmove(&saved, &R, sizeof(saved));
	ok = kprotect(vmchanrun, &call);
	memmove(&R, &saved, sizeof(R));
	if(!ok)
		krethrow();
}
#undef VmChannelInline
#endif
