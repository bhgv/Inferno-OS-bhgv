static Type *payload;
static int buffered, mapped;


/* Invoke the real protected constructor for each descriptor-alias combination. */
static int
retainchannel(void *unused)
{
	(void)unused;
	result = cnewc(payload, mapped ? movtmp : scalar_move, buffered);
	return 1;
}


/* No descriptor failure may allocate queues, retain a Type or publish an owner. */
int
main(void)
{
	int alias, bad, limit, before, ok, valid, needed;
	Type savedpayload, savedchannel, savedarray;
	ulong total = 0;

	for(alias=0; alias<3; alias++)
	for(buffered=0; buffered<2; buffered++)
	for(mapped=0; mapped<2; mapped++)
	for(bad=0; bad<6; bad++){
		Tchannel.ref = Tarray.ref = element.ref = 1;
		payload = alias == 0 ? &element : alias == 1 ? &Tchannel : &Tarray;
		needed = buffered+mapped+(alias == 1)+(buffered && alias == 2);
		limit = 0x7fffffff-needed;
		if(bad==0)payload->ref=0;
		if(bad==1)payload->ref=-1;
		if(bad==2)payload->ref=0x7fffffff;
		if(bad==3)payload->ref=needed ? limit+1 : limit;
		if(bad==4)payload->ref=limit;
		if(bad==5)Tarray.ref=0;
		valid = bad==4 || (!buffered && !mapped && alias!=1 && bad<4) || (!buffered && bad==5 && !(mapped && alias==2));
		memmove(&savedpayload,payload,sizeof(Type));memmove(&savedchannel,&Tchannel,sizeof(Type));memmove(&savedarray,&Tarray,sizeof(Type));
		before=calls;result=H;last_error=nil;
		ok=kprotect(retainchannel,nil);
		if(ok!=valid || worker.nerr)abort();
		if(!ok){
			if(last_error!=exHeap || calls!=before || live || result!=H ||
			   memcmp(payload,&savedpayload,sizeof(Type)) || memcmp(&Tchannel,&savedchannel,sizeof(Type)) || memcmp(&Tarray,&savedarray,sizeof(Type)))abort();
		}else{
			if(payload->ref!=savedpayload.ref+needed || result==H)abort();
			destroy(result);
			if(payload->ref!=savedpayload.ref || Tchannel.ref!=savedchannel.ref || Tarray.ref!=savedarray.ref || live)abort();
		}
		total++;
	}
	Tchannel.ref=Tarray.ref=element.ref=1;
	printf("PASS channel retains: %lu descriptor/alias/buffer/mover cases WORD%zu\n",total,sizeof(WORD)*8);
	return 0;
}
