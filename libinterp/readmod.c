#include "lib9.h"
#include "isa.h"
#include "interp.h"
#include "kernel.h"
#include "dynld.h"
#include "raise.h"
#include "native64-scalar-boot.h"
#include "dis64-command-binding.h"


/* Validate full-width native replies before advancing a complete module image. */
Module*
readmod(char *path, Module *m, int sync)
{
	Dir *d;
	int fd, n, dynld;
	long nr;
	uchar *code;
	Module *ans;
	ulong length;

	if(path[0] == '$') {
#ifdef DIS64_PILOT_LOADER
		if(dis64launchmode()){
			if(m != nil)
				unload(m);
			kwerrstr("experimental dis64-calls does not admit builtin imports");
			return nil;
		}
#endif
#ifdef DIS_NATIVE64_SCALAR_BOOT
		/* lookmod handed us a retained module even though this profile refuses it. */
		if(m != nil)
			unload(m);
		kwerrstr("builtin modules are unavailable in native64 scalar startup");
		return nil;
#else
		if(m == nil)
			kwerrstr("module not built-in");
		return m;
#endif
	}

#ifdef DIS64_PILOT_LOADER
	/* Seal before releasing VM admission and reject cache aliases across profiles. */
	(void)dis64launchmode();
	if(m != nil && !dis64launchcompatible(m)){
		kwerrstr("module cache does not match the selected Dis64 launch profile");
		unload(m);
		return nil;
	}
#endif

	ans = nil;
	code = nil;
	length = 0;
	dynld = 0;

	if(sync)
		release();

	d = nil;
	fd = kopen(path, OREAD);
	if(fd < 0)
		goto done;

	if((d = kdirfstat(fd)) == nil)
		goto done;

	if(m != nil) {
		if(d->dev == m->dev && d->type == m->dtype &&
		   d->mtime == m->mtime &&
		   d->qid.type == m->qid.type && d->qid.path == m->qid.path && d->qid.vers == m->qid.vers) {
			ans = m;
			goto done;
		}
	}

	if(d->length < 0 || d->length >= 8*1024*1024){
		kwerrstr("implausible length");
		goto done;
	}
#ifndef DIS_NATIVE64_SCALAR_BOOT
	if(
#ifdef DIS64_PILOT_LOADER
	   !dis64launchmode() &&
#endif
	   (d->mode&0111) && dynldable(fd)){
		dynld = 1;
		goto done1;
	}
#endif
	length = d->length;
	code = mallocz(length, 0);
	if(code == nil)
		goto done;

	n = 0;
	while(n < length){
		nr = kread(fd, code+n, length-n);
		if(nr <= 0)
			break;
		if((ulong)nr > length-n){
			kwerrstr("invalid module read count");
			break;
		}
		n += (int)nr;
	}
	if(n != length) {
		free(code);
		code = nil;
	}
done:
	if(fd >= 0)
		kclose(fd);
done1:
	if(sync)
		acquire();
	if(m != nil && ans == nil)
		unload(m);
	if(code != nil) {
		ans = parsemod(path, code, length, d);
		free(code);
	}
	else if(dynld){
		kseek(fd, 0, 0);
		ans = newdyncode(fd, path, d);
		kclose(fd);
	}
	free(d);
	return ans;
}


/* Native scalar-library namespace bridge. It is deliberately uncached: every
 * load owns a fresh read/admission/proof/instance, without legacy hash aliases. */
#if defined(DIS64_PILOT_LOADER) && DIS64_PILOT_LOADER == 1
#include "dis64-call-library.h"
#include "dis64-command-binding.h"
typedef struct Dis64LibraryRead Dis64LibraryRead;
struct Dis64LibraryRead {
 char *path;
 const uchar *requests,*strings;
 ulong requestbytes,stringbytes,bytes;
 uchar *image;
 Dir *dir;
 int fd,jit;
 Modlink *link;
};

/* No Module/VM object is constructed while the caller has released exclusion. */
static int
read64libraryimage(void *argument)
{
 Dis64LibraryRead *read=argument;ulong at;long count;int fd;
 read->fd=kopen(read->path,OREAD);
 if(read->fd<0)return 0;
 read->dir=kdirfstat(read->fd);
 if(read->dir==nil)return 0;
 if(read->dir->length<=0 || (uvlong)read->dir->length>Dis64CallImageBytes){
  kwerrstr("implausible Dis64 scalar library length");return 0;
 }
 read->bytes=read->dir->length;
 read->image=mallocz(read->bytes,0);
 if(read->image==nil)return 0;
 for(at=0;at<read->bytes;at+=count){
  count=kread(read->fd,read->image+at,read->bytes-at);
  if(count<=0 || (ulong)count>read->bytes-at){kwerrstr("incomplete Dis64 scalar library read");return 0;}
 }
 fd=read->fd;read->fd=-1; /* Native kclose consumes this descriptor on every status. */
 if(kclose(fd)<0){kwerrstr("cannot close Dis64 scalar library file");return 0;}
 return 1;
}

/* Existing signed/unsigned-file policy runs under exclusion before construction. */
static int
read64libraryadmit(void *argument)
{
 Dis64LibraryRead *read=argument;
 if(mustbesigned(read->path,read->image,read->bytes,read->dir)){
  kwerrstr("security violation: not signed");return 0;
 }
 read->link=prepare64calllibrarylink(read->path,read->image,read->bytes,read->jit && !dontcompile,
  read->requests,read->requestbytes,read->strings,read->stringbytes);
 return read->link!=nil;
}
#endif

/* Return one reference and one external GC pin, with exclusion held on return.
 * Caller-owned native path/request buffers must stay immutable/live across I/O.
 * This explicit bridge grants no guest ILOAD capability or cache provenance. */
Modlink*
read64calllibrary(char *path,int jit,const uchar *requests,ulong requestbytes,
 const uchar *strings,ulong stringbytes,int sync)
{
#if defined(DIS64_PILOT_LOADER) && DIS64_PILOT_LOADER == 1
 Dis64LibraryRead read={0};DisStringPoolView pool;Dis64Bindings bindings;unsigned int n;int ok;
 if(!dis64launchmode() || (jit!=0 && jit!=1) || (sync!=0 && sync!=1) ||
    path==nil || path==H || requests==nil || requests==H || strings==nil || strings==H ||
    requestbytes>Dis64LocalBytes || stringbytes>Dis64LocalBytes ||
    (uintptr)requests>(uintptr)-1-requestbytes || (uintptr)strings>(uintptr)-1-stringbytes){
  kwerrstr("unsupported Dis64 scalar library namespace request");return nil;
 }
 for(n=0;n<1024 && path[n];n++);
 if(!n || n==1024 || path[0]=='$' ||
    !disstringpoolcheck(strings,stringbytes,65536,&pool) ||
    !dis64clientbindingscheck(requests,requestbytes,0,&pool,0,0,Dis64LocalFunctions,Dis64LocalParameters,&bindings)){
  kwerrstr("invalid Dis64 scalar library path or interface");return nil;
 }
 read.path=path;read.requests=requests;read.requestbytes=requestbytes;read.strings=strings;
 read.stringbytes=stringbytes;read.jit=jit;read.fd=-1;
 if(sync)release();
 ok=kprotect(read64libraryimage,&read);
 if(sync)acquire();
 if(ok)ok=kprotect(read64libraryadmit,&read);
 if(!ok && read.link!=nil){ptrdel(D2H(read.link));destroy(read.link);read.link=nil;}
 if(read.fd>=0)kclose(read.fd);
 free(read.dir);free(read.image);
 return ok?read.link:nil;
#else
 USED(path);USED(jit);USED(requests);USED(requestbytes);USED(strings);USED(stringbytes);USED(sync);
 kwerrstr("Dis64 scalar library namespace bridge is disabled");return nil;
#endif
}


/* Private native ownership, not an on-wire table or an arbitrary caller proof. */
#if defined(DIS64_PILOT_LOADER) && DIS64_PILOT_LOADER == 1
#include "dis64-import-owner.h"
#endif

Modlink*
read64ownedimport(Module *module,WORD index,char *path)
{
#if defined(DIS64_PILOT_LOADER) && DIS64_PILOT_LOADER == 1
 Dis64ImportOwner *owner=module->native64imports;
 Native64ImportRequest *table;
 if(owner==nil || index<0 || (uint)index>=owner->count)error(exModule);
 table=owner->tables+index;
 return read64calllibrary(path,(module->rt&Dis64ScalarClient)?!!cflag:owner->jit,table->imports,table->importbytes,
  table->strings,table->stringbytes,1);
#else
 USED(module);USED(index);USED(path);kwerrstr("Dis64 structural import reader is disabled");return nil;
#endif
}

#if defined(DIS64_PILOT_LOADER) && DIS64_PILOT_LOADER == 1
static Dis64ImportOwner*
copy64importrequests(const Native64ImportRequest *tables,uint count,int jit,ulong bytes)
{
 Dis64ImportOwner *owner;const Native64ImportRequest *table;uchar *at;uint i;
 owner=mallocz(bytes,1);if(owner==nil)return nil;
 owner->count=count;owner->jit=jit;at=(uchar*)(owner+1);
 for(i=0;i<count;i++){
  table=tables+i;owner->tables[i]=*table;
  owner->tables[i].imports=at;memmove(at,table->imports,table->importbytes);at+=table->importbytes;
  owner->tables[i].strings=at;memmove(at,table->strings,table->stringbytes);at+=table->stringbytes;
 }
 return owner;
}
#endif

/* Attach all independently generated .m requests to a trusted unpublished
 * caller. One allocation owns every table/name/signature. No borrowed emitter
 * pointers, classic hashes or callback addresses come from serialized bytes.
 * Caller code/root maps remain the checked native constructor's responsibility. */
int
install64callimports(Module *module,const Native64ImportRequest *tables,uint count,int jit)
{
#if defined(DIS64_PILOT_LOADER) && DIS64_PILOT_LOADER == 1
 Dis64ImportOwner *owner;Module *cached;DisStringPoolView pool;Dis64Bindings bindings;
 const Native64ImportRequest *table;ulong bytes=sizeof(*owner);uint i;
 if(!dis64launchmode() || module==nil || module==H || module->ref!=1 ||
    module->ldt!=nil || module->native64imports!=nil || module->native64importload!=nil ||
    module->cstep!=native64typedlea8step || !count || count>64 || tables==nil || tables==(void*)H ||
    (jit!=0 && jit!=1))return 0;
 for(cached=modules;cached!=nil;cached=cached->link)if(cached==module)return 0;
 for(i=0;i<count;i++){
  table=tables+i;
  if(table->imports==nil || table->imports==(void*)H || table->strings==nil || table->strings==(void*)H ||
     table->importbytes>Dis64LocalBytes || table->stringbytes>Dis64LocalBytes ||
     (uintptr)table->imports>(uintptr)-1-table->importbytes ||
     (uintptr)table->strings>(uintptr)-1-table->stringbytes ||
     table->importbytes+table->stringbytes>8*Dis64LocalBytes-bytes ||
     !disstringpoolcheck(table->strings,table->stringbytes,65536,&pool) ||
     !dis64bindingscheck(table->imports,table->importbytes,0,&pool,0,0,
       Dis64LocalFunctions,Dis64LocalParameters,&bindings))return 0;
  bytes+=table->importbytes+table->stringbytes;
 }
 owner=copy64importrequests(tables,count,jit,bytes);if(owner==nil)return 0;
 module->native64imports=owner;
 module->native64importload=read64ownedimport;
 return 1;
#else
 USED(module);USED(tables);USED(count);USED(jit);return 0;
#endif
}


#if defined(DIS64_PILOT_LOADER) && DIS64_PILOT_LOADER == 1
/* Match pool Runes to bounded UTF-8 native names without locale or truncation. */
static int
match64typedname(const DisStringPoolView *pool,uint symbol,const char *name)
{
 DisRuneView view;uint i,c,j,n,at=0;uchar utf[4];
 if(name==nil || name==H || !disstringpoolget(pool,symbol,&view) || !view.count || view.count>255)return 0;
 for(i=0;i<view.count;i++){
  if(!disruneat(&view,i,&c) || !c)return 0;
  if(c<0x80){utf[0]=c;n=1;}
  else if(c<0x800){utf[0]=0xc0|(c>>6);utf[1]=0x80|(c&63);n=2;}
  else if(c<0x10000){utf[0]=0xe0|(c>>12);utf[1]=0x80|((c>>6)&63);utf[2]=0x80|(c&63);n=3;}
  else{utf[0]=0xf0|(c>>18);utf[1]=0x80|((c>>12)&63);utf[2]=0x80|((c>>6)&63);utf[3]=0x80|(c&63);n=4;}
  for(j=0;j<n;j++)if(at==255 || (uchar)name[at++]!=utf[j])return 0;
 }
 return name[at]==0;
}
#endif

/* Structural file requests plus independent .m hashes join an existing checked
 * ldt. Nothing is published until every table/name/hash/scalar frame agrees. */
int
install64typedimports(Module *module,const Native64TypedImportRequest *tables,uint count,int jit)
{
#if defined(DIS64_PILOT_LOADER) && DIS64_PILOT_LOADER == 1
 Native64ImportRequest requests[64];Dis64ImportOwner *owner;Module *cached;DisStringPoolView pool;
 Dis64Bindings bindings;Dis64Binding binding;const Native64TypedImportRequest *entry;
 ulong bytes=sizeof(*owner);uint i,j,k,total=0,base,frame,width,hash;Type *type;Import *ldt;
 if(sizeof(void*)!=8 || sizeof(WORD)!=4 || sizeof(uint)!=4 || !dis64launchmode() ||
    module==nil || module==H || module->ref!=1 || module->compiled ||
    module->cstep!=native64typedlea8step || module->native64imports!=nil || module->native64importload!=read64ownedimport ||
    module->ldt==nil || module->ldt==H || module->type==nil || module->type==H || module->ntype<1 || module->ntype>5121 ||
    !count || count>64 || tables==nil || tables==(void*)H || (jit!=0 && jit!=1))return 0;
 for(cached=modules;cached!=nil;cached=cached->link)if(cached==module)return 0;
 for(i=0;i<count;i++){
  entry=tables+i;requests[i]=entry->request;
  if(!entry->count || entry->count>64 || entry->count>4096-total || entry->hashes==nil || entry->hashes==(void*)H ||
     (uintptr)entry->hashes>(uintptr)-1-entry->count*4 ||
     requests[i].imports==nil || requests[i].imports==(void*)H || requests[i].strings==nil || requests[i].strings==(void*)H ||
     requests[i].importbytes>Dis64LocalBytes || requests[i].stringbytes>Dis64LocalBytes ||
     (uintptr)requests[i].imports>(uintptr)-1-requests[i].importbytes || (uintptr)requests[i].strings>(uintptr)-1-requests[i].stringbytes ||
     requests[i].importbytes+requests[i].stringbytes>8*Dis64LocalBytes-bytes ||
     !disstringpoolcheck(requests[i].strings,requests[i].stringbytes,65536,&pool) ||
     !dis64bindingscheck(requests[i].imports,requests[i].importbytes,0,&pool,0,0,64,64,&bindings) || bindings.count!=entry->count)return 0;
  ldt=module->ldt[i];if(ldt==nil || ldt==H)return 0;
  for(j=0;j<entry->count;j++){
   if(!dis64bindingget(&bindings,j,&binding) || !match64typedname(&pool,binding.symbol,ldt[j].name))return 0;
   memmove(&hash,entry->hashes+j,4);if(memcmp(&hash,&ldt[j].sig,4))return 0;
  }
  if(ldt[entry->count].name!=nil)return 0;
  bytes+=requests[i].importbytes+requests[i].stringbytes;total+=entry->count;
 }
 if(module->ldt[count]!=nil || total>=(uint)module->ntype)return 0;
 base=module->ntype-total;
 for(i=0;i<count;i++){
  if(!disstringpoolcheck(requests[i].strings,requests[i].stringbytes,65536,&pool) ||
     !dis64bindingscheck(requests[i].imports,requests[i].importbytes,0,&pool,0,0,64,64,&bindings))return 0;
  for(j=0;j<bindings.count;j++){
   if(!dis64bindingget(&bindings,j,&binding))return 0;
   frame=64;
   for(k=0;k<binding.signature.parameters;k++){
    width=dis64scalarwidth(dis64u32(binding.signature.data+12+4*k));frame=(frame+width-1)&~(width-1);frame+=width;
   }
   frame=(frame+7)&~7;type=module->type[base++];
   if(type==nil || type==H || type->ref<=0 || type->size!=(int)frame || type->np!=(int)((frame+63)/64))return 0;
   for(k=0;k<(uint)type->np;k++)if(type->map[k])return 0;
  }
 }
 owner=copy64importrequests(requests,count,jit,bytes);if(owner==nil)return 0;
 module->native64imports=owner;return 1;
#else
 USED(module);USED(tables);USED(count);USED(jit);return 0;
#endif
}


#if defined(DIS_NATIVE64_SCALAR_BOOT) && defined(DIS64_PILOT_LOADER)
typedef struct Dis64PairRead Dis64PairRead;

struct Dis64PairRead {
	char *consumer; // Explicit namespace path for the startup module.
	char *provider; // Explicit or resolved namespace path for its dependency.
	char sibling[1024]; // Bounded path assembled from consumer directory and import.
	uchar *images[2]; // Owned read buffers, kept through linking.
	ulong sizes[2]; // Complete file extents.
	int fd; // Current open namespace descriptor, or -1.
	Dir *dir; // Current file metadata for the existing admission check.
	Modlink *link; // Rooted consumer returned by the private builder.
};


/* Read a complete bounded file and apply the existing Dis admission policy. */
static int
read64pairimage(Dis64PairRead *read, char *path, int index)
{
	ulong offset, length;
	long count;

	read->fd = kopen(path, OREAD);
	if(read->fd < 0)
		return 0;
	read->dir = kdirfstat(read->fd);
	if(read->dir == nil)
		return 0;
	if(read->dir->length <= 0 || read->dir->length >= 8*1024*1024){
		kwerrstr("implausible Dis64 pair length");
		return 0;
	}
	length = read->dir->length;
	read->images[index] = mallocz(length, 0);
	if(read->images[index] == nil)
		return 0;
	for(offset=0; offset<length; offset+=count){
		count = kread(read->fd, read->images[index]+offset, length-offset);
		if(count <= 0 || (ulong)count > length-offset){
			kwerrstr("incomplete Dis64 pair read");
			return 0;
		}
	}
	if(mustbesigned(path, read->images[index], length, read->dir)){
		kwerrstr("security violation: not signed");
		return 0;
	}
	kclose(read->fd);
	read->fd = -1;
	free(read->dir);
	read->dir = nil;
	read->sizes[index] = length;
	return 1;
}


/* Construct a bounded namespace sibling path for either layout. */
static int
read64pairsibling(Dis64PairRead *read, int ordinal)
{
	char *slash;
	size_t prefix, capacity;
	int count;

	slash = strrchr(read->consumer, '/');
	prefix = slash == nil ? 0 : slash-read->consumer+1;
	if(prefix >= sizeof(read->sibling)-sizeof(".dis64"))
		return 0;
	capacity = sizeof(read->sibling)-prefix-sizeof(".dis64")+1;
	if(ordinal < 0)
		count = dis64pairdependency(read->images[0], read->sizes[0],
			read->sibling+prefix, capacity);
	else
		count = dis64siblingdependency(read->images[0], read->sizes[0], ordinal,
			read->sibling+prefix, capacity);
	if(count == 0)
		return 0;
	memmove(read->sibling, read->consumer, prefix);
	strcat(read->sibling+prefix, ".dis64");
	read->provider = read->sibling;
	return count;
}


/* Publish sibling links only in the private rooted consumer, one mapped slot at a time. */
static int
read64siblinggraph(Dis64PairRead *read)
{
	Modlink *provider;
	int count, i;

	count = read64pairsibling(read, 0);
	if(count == 0){
		kwerrstr("invalid Dis64 sibling dependency name or path");
		return 0;
	}
	read->link = prepare64siblings(read->consumer, read->images[0], read->sizes[0], cflag != 0);
	if(read->link == nil)
		return 0;
	for(i=0; i<count; i++){
		if(!read64pairsibling(read, i)){
			kwerrstr("invalid Dis64 sibling dependency name or path");
			return 0;
		}
		if(!read64pairimage(read, read->provider, 1))
			return 0;
		provider = prepare64providerlink(read->provider, read->images[1], read->sizes[1],
			cflag != 0, read->images[0], read->sizes[0], i);
		free(read->images[1]);
		read->images[1] = nil;
		if(provider == nil)
			return 0;
		memmove(read->link->MP+i*sizeof(provider), &provider, sizeof(provider));
		Setmark(D2H(provider));
		ptrdel(D2H(provider));
	}
	return 1;
}


/* Build the linked instance only after both namespace files pass admission. */
static int
read64pairprotected(void *argument)
{
	Dis64PairRead *read;

	read = argument;
	if(!read64pairimage(read, read->consumer, 0))
		return 0;
	if(dis64bootmanaged){
#if defined(DIS_NATIVE64_BOOT_SYS_IO) && defined(DIS_NATIVE64_MANAGED_FILES)
        if(read->provider != nil && strcmp(read->provider,"$Sys") == 0){
            read->link = prepare64sysstring(read->consumer,read->images[0],read->sizes[0],cflag != 0);
            return read->link != nil;
        }
#endif
		if(!read64pairimage(read, read->provider, 1))
			return 0;
		read->link = prepare64managedtrial(read->consumer, read->images[0], read->sizes[0],
			read->provider, read->images[1], read->sizes[1], cflag != 0);
		return read->link != nil;
	}
#ifdef DIS_NATIVE64_BOOT_SYS_IO
	if(read->provider != nil && strcmp(read->provider, "$Sys") == 0){
		read->link = prepare64sysclockconsumer(read->consumer, read->images[0],
			read->sizes[0], cflag != 0);
		return read->link != nil;
	}
#endif
	if(read->provider == nil && !read64pairsibling(read, -1))
		return read64siblinggraph(read);
	if(!read64pairimage(read, read->provider, 1))
		return 0;
	read->link = prepare64consumerlink(read->consumer, read->images[0], read->sizes[0],
		read->provider, read->images[1], read->sizes[1], cflag != 0);
	return read->link != nil;
}


/* Return one rooted linked instance for the explicit scalar-console boot. */
Modlink*
read64pair(char *consumer, char *provider)
{
	Dis64PairRead read;

	memset(&read, 0, sizeof(read));
	read.consumer = consumer;
	read.provider = provider;
	read.fd = -1;
	if(!kprotect(read64pairprotected, &read) && read.link != nil){
		ptrdel(D2H(read.link));
		destroy(read.link);
		read.link = nil;
	}
	if(read.fd >= 0)
		kclose(read.fd);
	free(read.dir);
	free(read.images[0]);
	free(read.images[1]);
	return read.link;
}
#endif
