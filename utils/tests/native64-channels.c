#include <stdarg.h>
char exNegsize[]="negative size"; // Ordinary String allocation refusal identity.
static void *oldwatch, *slotwatch; // Observe publication before actual displaced String destruction.
static int cases; // Completed independent descriptor/frame/path scenarios.
char deffontname[]="*default*"; // Existing native default-subfont identity.

struct ChannelCase {
	char *text; // Complete source token, including intentionally malformed forms.
	ulong expected; // Independently computed canonical protocol descriptor or zero.
};
typedef struct ChannelCase ChannelCase;

struct DepthCase {
	uvlong value; // Native-width raw descriptor, including overwide values.
	int expected; // Independent protocol depth; zero denotes invalid encoding.
};
typedef struct DepthCase DepthCase;

DATA_ROWS


/* Resolve allocator capacities without replacing String/Heap construction or destruction. */
ulong
poolmsize(Pool *pool, void *pointer)
{
	void *block;
	int i;

	USED(pool);
	block=(uchar*)pointer-O(Bhdr,u.data);
	for(i=0; i<RuntimeBlocks; i++)
		if(blocks[i].pointer==block)
			return blocks[i].bytes-O(Bhdr,u.data);
	require(0);
	return 0;
}


/* Native UTF-cache buffers have their own exact allocator identity and capacity. */
ulong
msize(void *pointer)
{
	int i;

	for(i=0; i<RuntimeBlocks; i++)
		if(blocks[i].pointer==pointer)
			return blocks[i].bytes;
	require(0);
	return 0;
}


/* Capture the existing diagnostic across publication and old-owner cleanup. */
void
kgerrstr(char *target, unsigned int capacity)
{
	if(capacity)
		snprintf(target,capacity,"%s",message);
}


/* An actual pool free must see the caller's completed replacement, never the old owner. */
static void
observe(void *pointer)
{
	void *value;

	if(oldwatch!=nil && oldwatch!=H && pointer==D2H(oldwatch)){
		memmove(&value,slotwatch,sizeof(value));
		require(value!=oldwatch);
	}
}

static char *expectedbase; // Full path expected at the file-probe boundary.
static int probedepth, probes, closes, wanted; // Independent native probing order and selected successful probe.


/* Compare every complete probe path; long inputs must never be truncated to fixed buffers. */
int
libopen(char *path, int mode)
{
	int i, expected=-1, ordinal=0;
	size_t size;

	require(mode==OREAD && expectedbase!=nil);
	for(i=3; i>=0; i--) if((1<<i)<=probedepth){
		if(ordinal++==probes){expected=i;break;}
	}
	require(expected>=0);
	size=strlen(expectedbase);
	require(strlen(path)==size+2 && memcmp(path,expectedbase,size)==0);
	require(path[size]=='.' && path[size+1]=='0'+expected);
	probes++;
	return probes-1==wanted ? 73 : -1;
}


/* Only the successfully opened probe owns a native FD that needs closing. */
int
libclose(int descriptor)
{
	require(descriptor==73);
	closes++;
	return 0;
}

#define malloc(n) trackedallocate(n,0) // Track actual String cache and native path allocations.
REAL_IMPLEMENTATION
#undef malloc


/* Adapt channel allocation errors to the real VM protected boundary. */
static int
textcall(void *frame)
{
	Draw_Chans_text(frame);
	return 1;
}


/* Adapt source-cache allocation errors without permitting partial scalar output. */
static int
makecall(void *frame)
{
	Draw_Chans_mk(frame);
	return 1;
}


/* Guard both ends of caller-owned byte-aligned result storage. */
static void
canary(uchar *bytes, int size, int width)
{
	int i;

	for(i=0; i<size; i++)
		if(i<1 || i>=1+width)
			require(bytes[i]==0xa5);
}


/* Real ASCII/Rune Strings feed the actual generated channel consumer, including an overlapping scalar result. */
static void
makecases(void)
{
	F_Draw_Chans_mk frame;
	String *source;
	Draw_Chans result;
	uchar output[24];
	int i, wide, alias, j, length;

	for(i=0; i<nelem(channels); i++) for(wide=0; wide<2; wide++) for(alias=0; alias<2; alias++){
		length=strlen(channels[i].text);
		if(wide && length){
			source=newrunes(length);
			for(j=0; j<length; j++) source->Srune[j]=(uchar)channels[i].text[j];
		}else source=c2string(channels[i].text,length);
		memset(&frame,0,sizeof(frame));
		memset(output,0xa5,sizeof(output));
		frame.s=source;
		frame.ret=alias ? (Draw_Chans*)&frame.s : (Draw_Chans*)(output+1);
		Draw_Chans_mk(&frame);
		memmove(&result,frame.ret,sizeof(result));
		require((ulong)result.desc==channels[i].expected && D2H(source)->ref==1);
		if(!alias) canary(output,sizeof(output),sizeof(result));
		destroy(source);
		require(live==0 && Tstring.ref==1);
		cases++;
	}
	memset(&frame,0,sizeof(frame));
	memset(output,0xa5,sizeof(output));
	source=newrunes(2);
	source->Srune[0]='r'; source->Srune[1]='8';
	frame.s=source; frame.ret=(Draw_Chans*)(output+1);
	failat=allocations+1;
	require(!kprotect(makecall,&frame) && strcmp(message,exNomem)==0);
	failat=0;
	for(i=0; i<sizeof(output); i++) require(output[i]==0xa5);
	require(source->tmp==nil && D2H(source)->ref==1);
	destroy(source);
	require(live==0 && Tstring.ref==1);
	cases++;
}


/* Native raw widths are checked before formatting, preventing an overwide descriptor from overflowing output. */
static void
textcases(void)
{
	F_Draw_Chans_text frame;
	String *old, *result;
	uchar bytes[24], native[24];
	char *expected;
	int i, j, alias;

	for(i=0; i<nelem(depths); i++){
		require(chantodepth((ulong)depths[i].value)==depths[i].expected);
		memset(native,0xa5,sizeof(native));
		expected=chantostr((char*)native+1,(ulong)depths[i].value);
		if(!depths[i].expected){
			require(expected==nil);
			for(j=0; j<sizeof(native); j++) require(native[j]==0xa5);
		}else{
			require(expected==(char*)native+1 && strlen(expected)<=8);
			require(strtochan(expected)==depths[i].value);
		}
		cases++;
	}
	for(i=0; i<nelem(depths); i++) for(alias=0; alias<2; alias++){
		old=c2string("old",3);
		memset(&frame,0,sizeof(frame));
		memset(bytes,0xa5,sizeof(bytes));
		frame.c.desc=(WORD)depths[i].value;
		frame.ret=alias ? (String**)&frame.c.desc : (String**)(bytes+1);
		memmove(frame.ret,&old,sizeof(old));
		if(!alias) frame.c.desc=(WORD)depths[i].value;
		expected=chantostr((char*)native,(ulong)frame.c.desc);
		oldwatch=old; slotwatch=frame.ret;
		Draw_Chans_text(&frame);
		oldwatch=nil;
		memmove(&result,frame.ret,sizeof(result));
		require((result==H)==(expected==nil));
		if(result!=H) require(strcmp(string2c(result),expected)==0 && D2H(result)->ref==1);
		if(!alias) canary(bytes,sizeof(bytes),sizeof(result));
		destroy(result);
		require(live==0 && Tstring.ref==1);
		cases++;
	}
	old=c2string("retained",8);
	memset(&frame,0,sizeof(frame));
	frame.ret=&old; frame.c.desc=RGBA32;
	failat=allocations+1;
	require(!kprotect(textcall,&frame) && strcmp(message,exHeap)==0);
	failat=0;
	require(old!=H && strcmp(string2c(old),"retained")==0 && D2H(old)->ref==1);
	destroy(old);
	require(live==0 && Tstring.ref==1);
	cases++;
}


/* Probe long complete relative/absolute/default paths at every native depth and allocation-refusal boundary. */
static void
pathcases(void)
{
	char *font, *name, *base, *actual;
	int length, depth, choice, expectedprobes, i;

	for(length=4; length<=8192; length*=4){
		font=trackedallocate(length+8,0);
		name=trackedallocate(length+1,0);
		base=trackedallocate(2*length+2,0);
		memset(font,'d',length); strcpy(font+length,"/font");
		memset(name,'s',length); name[length]=0;
		memset(base,'d',length); base[length]='/';
		memmove(base+length+1,name,length+1);
		for(depth=0; depth<=10; depth++) for(choice=-1; choice<4; choice++){
			expectedbase=base; probedepth=depth>8 ? 8 : depth;
			probes=closes=0; wanted=choice;
			expectedprobes=0;
			for(i=3; i>=0; i--) if((1<<i)<=probedepth) expectedprobes++;
			actual=subfontname(name,font,depth);
			require(actual!=nil);
			if(choice>=0 && choice<expectedprobes){
				require(probes==choice+1 && closes==1 && strlen(actual)==strlen(base)+2);
			}else require(probes==expectedprobes && closes==0 && strcmp(actual,base)==0);
			free(actual);
			cases++;
		}
		probes=closes=0; failat=allocations+1;
		require(subfontname(name,font,8)==nil && probes==0);
		failat=0;
		free(base); free(name); free(font);
		require(live==0);
		cases++;
	}
	expectedbase="/root/subfont"; probedepth=8; wanted=-1; probes=closes=0;
	actual=subfontname("/root/subfont",nil,8);
	require(actual!=nil && strcmp(actual,expectedbase)==0 && probes==4);
	free(actual);
	actual=subfontname(deffontname,nil,8);
	require(actual!=nil && strcmp(actual,deffontname)==0);
	free(actual);
	require(subfontname(nil,"f",8)==nil && subfontname("relative",nil,8)==nil && live==0);
	cases+=3;
	{
		char *children[]={"sub", "sub", "sub", "€", ""}; // Native child names, including an empty and UTF-8 name.
		char *parents[]={"font", "/font", "dir/font", "шрифт/font", "font"}; // Independent directory boundaries.
		char *expected[]={"./sub", "/sub", "dir/sub", "шрифт/€", "./"}; // Complete expected fallback paths.

		for(i=0; i<nelem(children); i++){
			probes=closes=0;
			actual=subfontname(children[i],parents[i],0);
			require(actual!=nil && strcmp(actual,expected[i])==0 && probes==0 && closes==0);
			free(actual);
			require(live==0);
			cases++;
		}
	}
}


/* Restore all actual String Type/Heap owners after both generated-frame consumers and native path probes. */
int
main(void)
{
	Tstring.size=sizeof(String); Tstring.free=freestring; Tstring.np=0;
	makecases(); textcases(); pathcases();
	require(live==0 && Tstring.ref==1 && worker.nerr==0);
	printf("PASS native64 channels: %d cases, %d predicates, real String/Heap ownership; protocol32 and full paths; all allocations retired\n",cases,checks);
	return 0;
}
