typedef struct PrintSpan PrintSpan;

struct PrintSpan
{
	String *format; // Borrowed logical format string.
	void *frame; // Numeric frame-relative alignment anchor, not dereferenced.
	void *args; // Cursor into the separately checked argument cells.
	char *output; // Borrowed bounded output region for xprint.
	char **result; // Bounded native pointer cell for bigxprint publication.
	int size; // Output capacity or initial dynamic growth capacity.
	int grow; // Whether to invoke bigxprint instead of xprint.
	int written; // Returned byte count after a successful call.
};

static PrintSpan span; // Stable state outside all VM error frames.
static uchar spanoutput[160], spanargs[32], spanresult[24]; // Independent cells and canaries.
static uchar spanbefore[160]; // Unchanged-output oracle for rejected work.
static unsigned long spanchecks; // Independent metadata, argument and publication cases.

static union {
	Heap align; // Naturally aligned origin for deliberately shifted String images.
	uchar bytes[128]; // Full String data and all byte shifts.
} spanstring;


/* Exercise the actual shared formatter and dynamic-buffer ownership boundary. */
static int
spaninvoke(void *unused)
{
	(void)unused;
	if(span.grow)
		span.written=bigxprint(nil,span.frame,span.args,span.format,span.result,span.size);
	else
		span.written=xprint(nil,span.frame,span.args,span.format,span.output,span.size);
	return 1;
}


/* Each rejection must preserve caller output and balance protected allocations. */
static void
spanreject(char *failure, int allocated)
{
	raised=nil;
	printcheck(!kprotect(spaninvoke,nil) && raised==failure);
	printcheck(worker.nerr==0 && live==0 && allocations==allocated);
	printcheck(!memcmp(spanoutput,spanbefore,sizeof(spanoutput)));
	spanchecks++;
}


/* Reset only independently owned formatting state, retaining no temporary buffer. */
static void
spanreset(void)
{
	memset(&span,0,sizeof(span));memset(spanoutput,0xa5,sizeof(spanoutput));
	memmove(spanbefore,spanoutput,sizeof(spanbefore));
	span.output=(char*)spanoutput;span.size=sizeof(spanoutput);
	allocations=0;failalloc=0;
}


/* A wrapping output region must fail before a literal or any argument is consumed. */
static void
spanbuffers(void)
{
	String *format;
	int wide,offset;

	for(wide=0;wide<2;wide++)for(offset=0;offset<64;offset++){
		spanreset();format=string("z",wide);span.format=format;
		span.output=(char*)((uintptr)-1-offset);span.size=64;
		spanreject(offset==0 ? exNilref : exBounds,0);free(format);
	}
}


/* Reject every incomplete representation, including aligned REAL/LONG near wrap. */
static void
spanarguments(void)
{
	char *formats[]={"%d","%o","%x","%X","%c","%*d","%s","%q","%H","%g","%bd"};
	String *format;
	int sizes[]={4,4,4,4,4,4,sizeof(void*),sizeof(void*),sizeof(void*),8,8};
	int kind,wide,anchor,shift,offset,full;

	for(kind=0;kind<11;kind++)for(wide=0;wide<2;wide++)for(anchor=0;anchor<8;anchor++)
	for(shift=0;shift<8;shift++)for(full=0;full<2;full++)for(offset=-1;offset<sizes[kind];offset++){
		spanreset();format=string(formats[kind],wide);span.format=format;
		span.frame=(void*)(uintptr)anchor;
		span.args=offset<0 ? nil : (void*)((uintptr)-1-offset);
		span.output=(char*)spanoutput+shift;span.size=full ? 64 : 1;
		spanreject(offset<=0 ? exNilref : exBounds,0);free(format);
	}
}


/* Header/capacity errors precede output and the borrowed argument's terminator store. */
static void
spanmetadata(void)
{
	String *format,*text,*value;
	uchar saved[64];
	int argument,offset,wide,capacity,bytes;

	for(argument=0;argument<2;argument++)for(offset=1;offset<8;offset++){
		spanreset();format=string("%s",0);value=(String*)((uintptr)-1-offset);
		span.format=argument ? format : value;span.args=spanargs;
		memmove(spanargs,&value,sizeof(value));spanreject(exHeap,0);free(format);
	}
	for(argument=0;argument<2;argument++)for(wide=0;wide<2;wide++)for(capacity=-1;capacity<2;capacity++){
		spanreset();format=string("%s",0);text=string("hi",wide);text->max=capacity;
		if(wide)text->Srune[2]=0x1234;else text->Sascii[2]=0x7d;
		bytes=sizeof(String)+3*sizeof(Rune);memmove(saved,text,bytes);
		span.format=argument ? format : text;span.args=spanargs;
		memmove(spanargs,&text,sizeof(text));spanreject(exHeap,0);
		printcheck(!memcmp(saved,text,bytes));free(text);free(format);
	}
}


/* Packed format metadata and Rune patterns must remain byte-safe at every shift. */
static void
spanpacked(void)
{
	String *format,*text,*value;
	uchar before[128];
	int wide,shift,outshift,bytes,n,i,number=7;

	for(wide=0;wide<2;wide++)for(shift=0;shift<8;shift++)for(outshift=0;outshift<8;outshift++){
		spanreset();format=string("x%dy",wide);bytes=sizeof(String)+5*sizeof(Rune);
		memset(spanstring.bytes,0xa5,sizeof(spanstring.bytes));memmove(spanstring.bytes+shift,format,bytes);
		memmove(before,spanstring.bytes,sizeof(before));span.format=(String*)(spanstring.bytes+shift);
		span.args=&number;span.output=(char*)spanoutput+outshift;span.size=32;
		printcheck(kprotect(spaninvoke,nil) && span.written==3 && !memcmp(span.output,"x7y",3));
		printcheck(!memcmp(before,spanstring.bytes,sizeof(before)) && !allocations && !live && !worker.nerr);
		for(i=0;i<outshift;i++)printcheck(spanoutput[i]==0xa5);
		for(i=outshift+3;i<(int)sizeof(spanoutput);i++)printcheck(spanoutput[i]==0xa5);
		free(format);spanchecks++;
	}
	for(wide=0;wide<2;wide++)for(shift=0;shift<8;shift++){
		spanreset();format=string("%s",0);text=string("hi",wide);bytes=sizeof(String)+3*sizeof(Rune);
		memset(spanstring.bytes,0xa5,sizeof(spanstring.bytes));memmove(spanstring.bytes+shift,text,bytes);
		value=(String*)(spanstring.bytes+shift);span.format=format;span.args=spanargs;
		memmove(spanargs,&value,sizeof(value));
		if(wide && shift%sizeof(Rune)!=0)spanreject(exHeap,0);
		else{
			printcheck(kprotect(spaninvoke,nil));n=span.written;
			printcheck(n==2 && !memcmp(spanoutput,"hi",2) && !allocations && !live && !worker.nerr);spanchecks++;
		}
		free(text);free(format);
	}
}


/* Native Heap counters must retain all bits; empty debug owners retain -1. */
static void
spanheaps(void)
{
	struct {
		Heap heap; // Naturally aligned diagnostic metadata.
		uchar data[8]; // Stable owner identity.
	} box;
	Type type={0};
	ulong counts[]={0,1,0x7fffffffUL,0x80000000UL,0xffffffffUL,(ulong)0x100000000ULL,(ulong)-2,(ulong)-1};
	String *format;
	void *owner;
	char expected[96];
	int i,offset;

	for(i=0;i<9;i++){
		spanreset();format=string("%H",0);span.format=format;span.args=spanargs;
		box.heap.t=&type;box.heap.ref=i==8 ? 0 : counts[i];owner=i==8 ? H : box.data;
		memmove(spanargs,&owner,sizeof(owner));
		if(i==8)strcpy(expected,"-1.00000000");
		else snprintf(expected,sizeof(expected),"%llu.%08llx",(unsigned long long)counts[i],(unsigned long long)(uintptr)&type);
		printcheck(kprotect(spaninvoke,nil) && span.written==(int)strlen(expected));
		printcheck(!memcmp(spanoutput,expected,strlen(expected)));free(format);spanchecks++;
	}
	for(offset=1;offset<=(int)sizeof(Heap)+7;offset++){
		spanreset();format=string("%H",0);span.format=format;span.args=spanargs;
		owner=offset<=(int)sizeof(Heap) ? (void*)(uintptr)offset : box.data+offset-sizeof(Heap);
		memmove(spanargs,&owner,sizeof(owner));spanreject(exHeap,0);free(format);
	}
}


/* Check dynamic output cells before allocation and publish unaligned cells safely. */
static void
spangrowth(void)
{
	String *format;
	char *result,*original=(char*)1;
	int offset,i;

	for(offset=-1;offset<(int)sizeof(void*);offset++){
		spanreset();format=string("ok",0);span.format=format;span.grow=1;span.size=16;
		span.result=offset<0 ? nil : (char**)((uintptr)-1-offset);
		spanreject(offset<=0 ? exNilref : exBounds,0);free(format);
	}
	for(offset=0;offset<8;offset++){
		spanreset();format=string("ok",0);span.format=format;span.grow=1;span.size=16;
		memset(spanresult,0xa5,sizeof(spanresult));span.result=(char**)(spanresult+offset);
		memmove(span.result,&original,sizeof(original));
		printcheck(kprotect(spaninvoke,nil) && span.written==2 && allocations==1 && live==1);
		memmove(&result,span.result,sizeof(result));printcheck(!memcmp(result,"ok",2));outputfree(result);
		for(i=0;i<offset;i++)printcheck(spanresult[i]==0xa5);
		for(i=offset+sizeof(result);i<(int)sizeof(spanresult);i++)printcheck(spanresult[i]==0xa5);
		printcheck(!live && !worker.nerr);free(format);spanchecks++;
	}
	spanreset();format=string("bad",0);format->max=0;span.format=format;span.grow=1;span.size=16;
	memmove(spanresult,&original,sizeof(original));span.result=(char**)spanresult;
	spanreject(exHeap,1);memmove(&result,spanresult,sizeof(result));printcheck(result==original);free(format);
}


/* Execute independent span/representation families with tracked pthread error stacks. */
static int
printspanmain(void)
{
	char *family=getenv("SYS_PRINT_CASE");

	fmtinstall('g',_efgfmt);
	if(!strcmp(family,"buffer"))spanbuffers();
	else if(!strcmp(family,"arguments"))spanarguments();
	else if(!strcmp(family,"metadata"))spanmetadata();
	else if(!strcmp(family,"packed"))spanpacked();
	else if(!strcmp(family,"heap"))spanheaps();
	else if(!strcmp(family,"growth"))spangrowth();
	else abort();
	printcheck(!live && !vmlive && !worker.nerr);
	printf("PASS Sys format spans %s: %lu cases WORD%zu\n",family,spanchecks,sizeof(WORD)*8);
	return 0;
}
