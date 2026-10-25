#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "runt.h"
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>

char exHeap[]="heap",exNomem[]="memory";
static jmp_buf boundary;
static char *raised;
static String strings[4];
static char *texts[]={"","ascii","u\303\251","group\360\237\230\200"};
static char *wantednames[4];
static ulong lengths[4],expectedsize,allocatedsize;
static int conversions,queries,allocations,failallocation,expectedoverflow;
static uchar *allocation;

/* Preserve the exact VM error without permitting a partial native record. */
void
error(char *message)
{
	raised=message;
	longjmp(boundary,1);
}

/* Keep address/stack instrumentation without unavailable leak-process inspection. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

/* Return stable conversion buffers and verify every captured VM string owner. */
char*
string2c(String *string)
{
	int i;

	assert(conversions<4);
	for(i=0;i<4 && string!=&strings[i];i++);
	assert(i<4);
	assert(texts[i]==wantednames[conversions]);
	conversions++;
	return texts[i];
}

/* Model huge lengths without creating huge buffers or reading beyond real strings. */
static ulong
measuredlength(char *text)
{
	assert(queries<4 && text==wantednames[queries]);
	return lengths[queries++];
}

/* Observe the complete native size, then allocate guarded storage or raise allocation failure. */
void*
smalloc(size_t size)
{
	assert(!expectedoverflow && ++allocations==1 && conversions==4 && queries==4 && size==expectedsize);
	if(failallocation || size>4096)
		error(exNomem);
	allocation=malloc(size+32);
	assert(allocation);
	allocatedsize=size;
	memset(allocation,0xa5,size+32);
	return allocation;
}

#define strlen measuredlength
IMPLEMENTATION
#undef strlen

/* Keep nonlocal error handling outside automatic oracle variables. */
static Dir*
invoke(void *slot)
{
	raised=nil;
	if(setjmp(boundary))
		return nil;
	return packdir(slot);
}

/* Check real byte copies, source alignment, all metadata and simulated native-width limits. */
int
main(void)
{
	union { uvlong align; uchar bytes[512]; } memory;
	uchar before[512];
	Sys_Dir source;
	Dir *result;
	unsigned __int128 total,prefix;
	ulong sum;
	int pattern,profile,offset,fail,i,which,overflow,firstoverflow;
	unsigned long checks=0;

	for(profile=0;profile<8;profile++)
	for(pattern=0;pattern<(profile ? 1 : 256);pattern++)
	for(offset=0;offset<8;offset++)for(fail=0;fail<2;fail++){
		if(getenv("PACKDIR_PROFILE") && profile!=atoi(getenv("PACKDIR_PROFILE")))continue;
		if(getenv("PACKDIR_OFFSET") && offset!=atoi(getenv("PACKDIR_OFFSET")))continue;
		memset(&source,0xa5,sizeof(source));
		for(i=0;i<4;i++){
			which=(pattern>>(i*2))&3;
			wantednames[i]=texts[which];
			lengths[i]=strlen(texts[which]);
		}
		source.name=&strings[(pattern>>0)&3];source.uid=&strings[(pattern>>2)&3];
		source.gid=&strings[(pattern>>4)&3];source.muid=&strings[(pattern>>6)&3];
		source.qid.path=((uvlong)1<<50)+pattern;
		source.qid.vers=(WORD)-17;source.qid.qtype=(WORD)pattern;
		source.mode=0640+pattern;source.atime=(WORD)-1;source.mtime=0x7fffffff;
		source.length=((vlong)1<<45)+pattern;source.dtype=7;source.dev=11;
		if(profile==1)lengths[0]=0x7fffffffUL;
		if(profile==2)lengths[0]=0x80000000UL;
		if(profile==3)lengths[0]=(ulong)((uvlong)1<<32)+17;
		if(profile==4)lengths[0]=(ulong)-1;
		if(profile==5){lengths[0]=(ulong)-1-sizeof(Dir)-2;lengths[1]=1;}
		if(profile==6)lengths[0]=lengths[1]=(ulong)-1/2;
		if(profile==7)lengths[0]=(ulong)-1-sizeof(Dir)-4;
		total=prefix=sizeof(Dir);firstoverflow=0;
		for(i=0;i<4;i++){
			prefix+=(unsigned __int128)lengths[i]+1;
			if(prefix>(ulong)-1 && !firstoverflow)firstoverflow=i+1;
			total+=(unsigned __int128)lengths[i]+1;
		}
		overflow=total>(ulong)-1;expectedoverflow=overflow;
		expectedsize=(ulong)total;
		failallocation=fail;
		allocation=nil;allocatedsize=0;conversions=queries=allocations=0;
		memset(memory.bytes,0xa5,sizeof(memory.bytes));
		memmove(memory.bytes+16+offset,&source,sizeof(source));
		memmove(before,memory.bytes,sizeof(before));
		result=invoke(memory.bytes+16+offset);
		assert(!memcmp(before,memory.bytes,sizeof(before)) && conversions==4);
		assert(queries==(overflow ? firstoverflow : 4));
		assert(allocations==!overflow);
		if(overflow)
			assert(!result && !allocation && raised==exHeap);
		else if(fail || total>4096)
			assert(!result && !allocation && raised==exNomem);
		else{
			assert(result==(Dir*)allocation && !raised && allocatedsize==expectedsize);
			char *names[]={result->name,result->uid,result->gid,result->muid};
			sum=sizeof(Dir);
			for(i=0;i<4;i++){
				assert(names[i]==(char*)allocation+sum);
				assert(!memcmp(names[i],wantednames[i],lengths[i]+1));
				sum+=lengths[i]+1;
			}
			assert(sum==expectedsize);
			assert(result->qid.path==source.qid.path && result->qid.vers==source.qid.vers && result->qid.type==source.qid.qtype);
			assert(result->mode==source.mode && result->atime==source.atime && result->mtime==source.mtime);
			assert(result->length==source.length && result->type==source.dtype && result->dev==source.dev);
			for(i=0;i<32;i++)assert(allocation[allocatedsize+i]==0xa5);
			free(allocation);
		}
		checks++;
	}
	printf("PASS Sys packdir: %lu alignment/metadata/native-size/overflow/failure cases WORD%zu\n",checks,sizeof(WORD)*8);
	return 0;
}
