#include "lib9.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <sys/mman.h>

char *native_cistrstr(char*,char*);
int native_cistrncmp(char*,char*,int);
char *native_strstr(char*,char*);
long native_strlen(char*);
char *native_strchr(char*,int);
int native_strncmp(char*,char*,long);
#define cistrstr native_cistrstr
#define cistrncmp native_cistrncmp
#define strstr native_strstr
#define strlen native_strlen
#define strchr native_strchr
#define strncmp native_strncmp
IMPLEMENTATION
#undef cistrstr
#undef cistrncmp
#undef strstr
#undef strlen
#undef strchr
#undef strncmp

static int checks;
static char alphabet[] = {'A','a','B','b',(char)0x80,(char)0xff};

/* Canonicalize independently through a lookup table for the host libc oracle. */
static void
fold(char *to, char *from)
{
	uchar mapping[256];
	int i;

	for(i=0;i<256;i++)mapping[i]=i;
	for(i=0;i<26;i++)mapping['A'+i]='a'+i;
	for(i=0;from[i]!=0;i++)to[i]=mapping[(uchar)from[i]];
	to[i]=0;
}

/* Generate every bounded byte string over the mixed-case/high-bit alphabet. */
static int
strings(char data[][6], int maximum)
{
	int n=0,length,count,index,i,k;

	for(length=0;length<=maximum;length++){
		count=1;
		for(i=0;i<length;i++)count*=6;
		for(index=0;index<count;index++){
			k=index;
			for(i=0;i<length;i++){data[n][i]=alphabet[k%6];k/=6;}
			data[n++][length]=0;
		}
	}
	return n;
}

/* Compare complete first-match positions against independent host search calls. */
static void
small(void)
{
	char haystacks[1555][6], needles[259][6], lowerhay[6], lowerneedle[6];
	char *found,*actual;
	int nh,nn,h,n;

	nh=strings(haystacks,4);
	nn=strings(needles,3);
	assert(nh==1555 && nn==259);
	for(h=0;h<nh;h++)for(n=0;n<nn;n++){
		fold(lowerhay,haystacks[h]);
		fold(lowerneedle,needles[n]);
		found=strstr(lowerhay,lowerneedle);
		actual=native_cistrstr(haystacks[h],needles[n]);
		assert((found==nil && actual==nil) ||
			(found!=nil && actual==haystacks[h]+(found-lowerhay)));
		assert(native_strstr(haystacks[h],needles[n])==strstr(haystacks[h],needles[n]));
		checks++;
	}
}

/* Build a long NUL-terminated string from owned repeated shared mappings. */
static void
wide(void)
{
	ulong length,chunk,capacity,offset,page,lastpage;
	char *base,*piece;
	FILE *file;

	if(sizeof(long)<=sizeof(int)){
		puts("wide substring fixture requires LP64");
		return;
	}
	length=(ulong)INT_MAX+34;
	chunk=2*1024*1024;
	page=sysconf(_SC_PAGESIZE);
	capacity=(length/chunk+1)*chunk;
	base=mmap(nil,capacity,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
	assert(base!=MAP_FAILED);
	file=tmpfile();
	assert(file!=nil && ftruncate(fileno(file),chunk)==0);
	piece=mmap(nil,chunk,PROT_READ|PROT_WRITE,MAP_SHARED,fileno(file),0);
	assert(piece!=MAP_FAILED);
	memset(piece,'a',chunk);
	assert(munmap(piece,chunk)==0);
	for(offset=0;offset<capacity;offset+=chunk)
		assert(mmap(base+offset,chunk,PROT_READ,MAP_SHARED|MAP_FIXED,fileno(file),0)==base+offset);
	/* A private last page prevents the terminator appearing in earlier aliases. */
	lastpage=length/page*page;
	assert(mmap(base+lastpage,page,PROT_READ|PROT_WRITE,
		MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0)==base+lastpage);
	memset(base+lastpage,'a',page);
	base[length]=0;
	assert(native_strlen(base)==length);
	assert(native_cistrstr("aaa",base)==nil);
	assert(native_strstr("aaa",base)==nil);
	assert(native_cistrstr(base,"AAA")==base);
	assert(native_strstr(base,"aaa")==base);
	assert(munmap(base,capacity)==0 && fclose(file)==0);
	printf("substring search: %lu-byte needle passed\n",length);
}

/* Run exact small strings or the optional genuine >INT_MAX string span. */
int
main(int argc, char **argv)
{
	if(argc>1 && strcmp(argv[1],"wide")==0){
		wide();
		return 0;
	}
	small();
	printf("substring search: %d byte/case scenarios passed\n",checks);
	return 0;
}
