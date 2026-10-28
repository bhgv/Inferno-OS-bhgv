
static uchar publicframe[512], publicbefore[512], publicslot[32];
static void *publicold, *publicresult, *alternate;
static String *publicformat;
static int operation, fs, rs, wide, sample, failed, borrowing, previous, callbacks;
static unsigned long publiccases;


/* Identify each original-frame/publication/ownership combination on failure. */
static void
publiccheck(int test)
{
	if(!test) {
		fprintf(stderr, "print public op=%d frame=%d slot=%d wide=%d sample=%d failed=%d borrow=%d old=%d\n",
			operation, fs, rs, wide, sample, failed, borrowing, previous);
		abort();
	}
}


/* Model a result-allocation callback borrowing the original generated frame. */
static void
publicborrow(void)
{
	void *redirected = &alternate;
	String *poison = (String*)1;
	int retoffset, stringoffset;

	retoffset = operation ? offsetof(F_Sys_aprint, ret) : offsetof(F_Sys_sprint, ret);
	stringoffset = operation ? offsetof(F_Sys_aprint, s) : offsetof(F_Sys_sprint, s);
	memmove(publicframe+fs+retoffset, &redirected, sizeof(redirected));
	memmove(publicframe+fs+stringoffset, &poison, sizeof(poison));
	callbacks++;
}


/* Populate native pointer fields and classic packed varargs without typed accesses. */
static void
publicprepare(void *ret, String *format)
{
	int retoffset, stringoffset;

	memset(publicframe, 0xa5, sizeof(publicframe));
	retoffset = operation ? offsetof(F_Sys_aprint, ret) : offsetof(F_Sys_sprint, ret);
	stringoffset = operation ? offsetof(F_Sys_aprint, s) : offsetof(F_Sys_sprint, s);
	memmove(publicframe+fs+retoffset, &ret, sizeof(ret));
	memmove(publicframe+fs+stringoffset, &format, sizeof(format));
}


/* Invoke the actual public builtin under the real emulator error stack. */
static int
publiccall(void *frame)
{
	if(operation)
		Sys_aprint(frame);
	else
		Sys_sprint(frame);
	return 1;
}


/* Keep original vararg alignment and output ownership across result allocation. */
static void
checkpublic(void)
{
	char literal[1025], *formats[] = {"literal!", literal, "%d", "%bd", "x%cy"};
	char *answers[] = {"literal!", literal, "-123456", "-9223372036854775808", "x\0y"};
	int lengths[] = {8, 1024, 7, 20, 3};
	int offset, integer, ok, i;
	LONG big;
	void *framepointer;

	memset(literal, 'a', 1024);
	literal[1024] = 0;
	for(operation = 0; operation < 2; operation++)
	for(fs = 0; fs < 8; fs++)
	for(rs = 0; rs < 8; rs++)
	for(wide = 0; wide < 2; wide++)
	for(sample = 0; sample < 5; sample++)
	for(failed = 0; failed < 2; failed++)
	for(borrowing = 0; borrowing < 2; borrowing++)
	for(previous = 0; previous < 3; previous++) {
		if(getenv("PRINT_PUBLIC_OP") && operation != atoi(getenv("PRINT_PUBLIC_OP")))
			continue;
		if(getenv("PRINT_PUBLIC_BORROW") && borrowing != atoi(getenv("PRINT_PUBLIC_BORROW")))
			continue;
		vmallocationhook = nil;
		vmfail = 0;
		publicold = previous ? (void*)c2string("old", 3) : H;
		if(previous == 2)
			D2H(publicold)->ref++;
		memset(publicslot, 0xa5, sizeof(publicslot));
		memmove(publicslot+rs, &publicold, sizeof(publicold));
		publicformat = string(formats[sample], wide);
		publicprepare(publicslot+rs, publicformat);
		offset = operation ? offsetof(F_Sys_aprint, vargs) : offsetof(F_Sys_sprint, vargs);
		if(sample == 2 || sample == 4) {
			integer = sample == 2 ? -123456 : 0;
			memmove(publicframe+fs+offset, &integer, sizeof(integer));
		}
		if(sample == 3) {
			big = (-9223372036854775807LL-1);
			offset += (-offset)&7;
			memmove(publicframe+fs+offset, &big, sizeof(big));
		}
		memmove(publicbefore, publicframe, sizeof(publicbefore));
		alternate = H;
		callbacks = 0;
		framepointer = publicframe+fs;
		vmallocationhook = borrowing ? publicborrow : nil;
		vmfail = failed;
		raised = nil;
		ok = kprotect(publiccall, framepointer);
		vmfail = 0;
		vmallocationhook = nil;
		memmove(&publicresult, publicslot+rs, sizeof(publicresult));
		publiccheck(worker.nerr == 0 && live == 0 && alternate == H);
		if(failed) {
			publiccheck(!ok && raised == exNomem && publicresult == publicold && callbacks == 0);
			publiccheck(vmlive == (previous != 0));
			if(previous)
				publiccheck(D2H(publicold)->ref == (previous == 2 ? 2 : 1));
			destroy(publicold);
		} else {
			publiccheck(ok && callbacks == borrowing && publicresult != publicold);
			publiccheck(vmlive == 1+(previous == 2));
			if(operation) {
				Array *array = publicresult;
				publiccheck(array->len == lengths[sample] && memcmp(array->data, answers[sample], lengths[sample]) == 0);
			} else {
				String *text = publicresult;
				publiccheck(text->len == lengths[sample] && memcmp(text->Sascii, answers[sample], lengths[sample]) == 0);
			}
			destroy(publicresult);
		}
		if(previous == 2) {
			publiccheck(D2H(publicold)->ref == 1);
			destroy(publicold);
		}
		if(!borrowing || failed)
			publiccheck(memcmp(publicframe, publicbefore, sizeof(publicframe)) == 0);
		else {
			void *redirected = &alternate;
			String *poison = (String*)1;
			int ret = operation ? offsetof(F_Sys_aprint, ret) : offsetof(F_Sys_sprint, ret);
			int str = operation ? offsetof(F_Sys_aprint, s) : offsetof(F_Sys_sprint, s);
			memmove(publicbefore+fs+ret, &redirected, sizeof(redirected));
			memmove(publicbefore+fs+str, &poison, sizeof(poison));
			publiccheck(memcmp(publicframe, publicbefore, sizeof(publicframe)) == 0);
		}
		for(i = 0; i < sizeof(publicslot); i++)
			if(i < rs || i >= rs+sizeof(publicresult))
				publiccheck(publicslot[i] == 0xa5);
		free(publicformat);
		publiccheck(vmlive == 0 && live == 0);
		publiccases++;
	}
}


/* Require preflight failures before formatting, VM allocation or result publication. */
static void
checkpublicerrors(void)
{
	Heap dead = {0};
	uintptr address;
	void *frame;
	int mode, ok, bytes;
	char *why;

	wide = sample = failed = borrowing = previous = 0;
	for(operation = 0; operation < 2; operation++)
	for(fs = 0; fs < 8; fs++)
	for(rs = 0; rs < 8; rs++)
	for(mode = 0; mode < 6; mode++) {
		if(getenv("PRINT_PUBLIC_OP") && operation != atoi(getenv("PRINT_PUBLIC_OP")))
			continue;
		if(getenv("PRINT_PUBLIC_ERROR") && mode != atoi(getenv("PRINT_PUBLIC_ERROR")))
			continue;
		publicold = H;
		publicformat = string("text", 0);
		if(mode == 2)
			publicold = nil;
		if(mode == 3)
			publicold = (void*)1;
		if(mode == 4)
			publicold = &dead+1;
		memmove(publicslot+rs, &publicold, sizeof(publicold));
		publicprepare(mode == 0 ? (void*)((uintptr)-1-sizeof(void*)+1) : publicslot+rs, publicformat);
		frame = publicframe+fs;
		why = mode == 2 ? exNilref : mode >= 3 && mode <= 4 ? exHeap : exBounds;
		if(mode == 1) {
			bytes = operation ? offsetof(F_Sys_aprint, vargs) : offsetof(F_Sys_sprint, vargs);
			frame = (void*)((uintptr)-1-bytes+1);
		}
		if(mode == 5) {
			frame = H;
			why = exNilref;
		}
		allocations = 0;
		raised = nil;
		ok = kprotect(publiccall, frame);
		publiccheck(!ok && raised == why && allocations == 0 && live == 0 && vmlive == 0 && worker.nerr == 0);
		memmove(&publicresult, publicslot+rs, sizeof(publicresult));
		publiccheck(publicresult == publicold);
		free(publicformat);
		publiccases++;
	}
	if(getenv("PRINT_PUBLIC_ERROR"))
		return;
	for(operation = 0; operation < 2; operation++)
	for(address = (uintptr)-1-sizeof(void*)+1; address != (uintptr)-1; address++) {
		if(getenv("PRINT_PUBLIC_OP") && operation != atoi(getenv("PRINT_PUBLIC_OP")))
			continue;
		publicprepare((void*)address, (String*)1);
		ok = kprotect(publiccall, publicframe+fs);
		publiccheck(!ok && raised == exBounds && live == 0 && vmlive == 0);
		publiccases++;
	}
}


/* The shared driver already runs this entry on an ASan-registered pthread stack. */
static int
printspanmain(void)
{
	char *family;

	family = getenv("SYS_PRINT_CASE");
	if(getenv("SYS_PRINT_CASE") && strcmp(getenv("SYS_PRINT_CASE"), "preflight") == 0)
		checkpublicerrors();
	else
		checkpublic();
	if(getenv("PRINT_PUBLIC_OP") == nil && getenv("PRINT_PUBLIC_BORROW") == nil && getenv("PRINT_PUBLIC_ERROR") == nil)
		publiccheck(publiccases == (family && strcmp(family, "preflight") == 0 ? 782 : 15360));
	printf("PASS public print: %lu frame/callback/result/owner cases WORD%zu\n", publiccases, sizeof(WORD)*8);
	return 0;
}
