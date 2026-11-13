#include "limbo.h"
#include "dis64-layout.h"
#include <assert.h>
#include <stddef.h>

static unsigned int allocations;
void *allocmem(ulong bytes) { void *p=calloc(1,bytes?bytes:1); assert(p!=nil); allocations++; return p; }
#undef free
extern void free(void*);
void layouttestfree(void *p) { if(p!=nil){ assert(allocations); allocations--; free(p); } }
#undef qsort
static int (*compareleaf)(void*,void*);
static int comparehost(const void *a,const void *b) { return compareleaf((void*)a,(void*)b); }
void infqsort(void *p,long n,long size,int (*compare)(void*,void*)) { compareleaf=compare; qsort(p,n,size,comparehost); }

typedef struct Oracle Oracle;
struct Oracle { int x; void *p; uchar b; double z; };
typedef struct Outer Outer;
struct Outer { Oracle inner; void *array; int count; };
typedef struct OracleFrame OracleFrame;
struct OracleFrame { uchar prefix[64]; int a; void *s; Outer local; };
extern int layoutbanktest(void);

int main(void)
{
	Type w={0},s={0},b={0},r={0},inner={0},outer={0},array={0},cycle={0},alt={0},pick={0},parent={0},table={0};
	Decl fields[4]={{0}},out[3]={{0}},args[2]={{0}},local={0},alias={0},tag={0},common={0},variant={0};
	Dis64Layout p={0};
	Desc desc={0}; uchar map[]={0,0x50,0x80};
	Case c={0}; Label labels[2]={{0}};
	unsigned int at=999,i; int pointers[]={Tstring,Tref,Tarray,Tlist,Tchan,Tmodule,Tany,Tpoly};
	w.kind=Tint; w.size=w.align=4; s.kind=Tstring; s.size=s.align=4;
	b.kind=Tbyte; b.size=b.align=1; r.kind=Treal; r.size=r.align=8;
	for(i=0;i<nelem(pointers);i++){
		Type t=s; t.kind=pointers[i]; assert(dis64layouttype(&t,&p));
		assert(p.bytes==8 && p.alignment==8 && p.count==1);
		assert(dis64layoutaddress(&p,0,4,1,&at) && at==0);
		assert(!dis64layoutaddress(&p,0,4,0,&at));
		assert(!dis64layoutaddress(&p,1,3,1,&at));
		dis64layoutfree(&p);
	}
	inner.kind=Tadt; inner.size=24; inner.align=8; inner.ids=fields;
	for(i=0;i<4;i++){fields[i].store=Dfield; fields[i].next=i<3?fields+i+1:nil;}
	fields[0].ty=&w; fields[1].ty=&s; fields[1].offset=4;
	fields[2].ty=&b; fields[2].offset=8; fields[3].ty=&r; fields[3].offset=16;
	assert(dis64layouttype(&inner,&p));
	assert(p.bytes==sizeof(Oracle) && p.alignment==_Alignof(Oracle));
	assert(dis64layoutaddress(&p,4,4,1,&at) && at==offsetof(Oracle,p));
	assert(dis64layoutaddress(&p,8,1,0,&at) && at==offsetof(Oracle,b));
	assert(dis64layoutaddress(&p,16,8,0,&at) && at==offsetof(Oracle,z));
	at=999; assert(!dis64layoutaddress(&p,9,1,0,&at) && at==999);
	assert(!dis64layoutaddress(&p,0,8,0,&at));
	dis64layoutfree(&p);
	array=s; array.kind=Tarray;
	outer.kind=Ttuple; outer.size=32; outer.align=8; outer.ids=out;
	for(i=0;i<3;i++){out[i].store=Dfield; out[i].next=i<2?out+i+1:nil;}
	out[0].ty=&inner; out[1].ty=&array; out[1].offset=24; out[2].ty=&w; out[2].offset=28;
	assert(dis64layouttype(&outer,&p) && p.bytes==sizeof(Outer)); dis64layoutfree(&p);
	args[0].ty=&w; args[0].store=Darg; args[0].offset=32; args[0].next=args+1;
	args[1].ty=&s; args[1].store=Darg; args[1].offset=36;
	local.ty=&outer; local.store=Dlocal; local.offset=40;
	desc.size=72; desc.map=map; desc.nmap=3;
	assert(dis64layoutstorage(args,&local,&desc,1,&p));
	assert(p.bytes==sizeof(OracleFrame));
	assert(dis64layoutaddress(&p,44,4,1,&at) && at==offsetof(OracleFrame,local)+offsetof(Outer,inner)+offsetof(Oracle,p));
	assert(dis64layoutaddress(&p,64,4,1,&at) && at==offsetof(OracleFrame,local)+offsetof(Outer,array));
	assert(!dis64layoutaddress(&p,16,4,0,&at));
	dis64layoutfree(&p);
	alias=local; alias.link=&local; local.next=&alias;
	assert(dis64layoutstorage(args,&local,&desc,1,&p)); dis64layoutfree(&p);
	alias.ty=&w; assert(!dis64layoutstorage(args,&local,&desc,1,&p)); dis64layoutfree(&p);
	local.next=nil; map[0]=1;
	assert(!dis64layoutstorage(args,&local,&desc,1,&p)); dis64layoutfree(&p); map[0]=0;
	map[2]=0; assert(!dis64layoutstorage(args,&local,&desc,1,&p)); dis64layoutfree(&p); map[2]=0x80;
	local.offset=36; assert(!dis64layoutstorage(args,&local,&desc,1,&p)); dis64layoutfree(&p); local.offset=40;
	fields[1].offset=0; assert(!dis64layouttype(&inner,&p)); dis64layoutfree(&p); fields[1].offset=4;
	cycle=inner; fields[0].ty=&cycle;
	assert(!dis64layouttype(&cycle,&p)); dis64layoutfree(&p); fields[0].ty=&w;
	s.size=8; assert(!dis64layouttype(&s,&p)); dis64layoutfree(&p); s.size=4;
	/* alt uses two different kinds of native address; only the direct channel is a heap root. */
	alt.kind=Talt; alt.size=24; alt.cse=&c; c.nlab=2; c.labs=labels; labels[0].isptr=1;
	assert(dis64layouttype(&alt,&p) && p.bytes==40 && p.count==6);
	assert(dis64layoutaddress(&p,8,4,1,&at) && at==8);
	assert(dis64layoutaddress(&p,12,4,-1,&at) && at==16);
	assert(dis64layoutaddress(&p,16,4,-1,&at) && at==24);
	assert(!dis64layoutaddress(&p,12,4,1,&at));
	assert(!dis64layoutaddress(&p,16,4,1,&at));
	assert(!dis64layoutaddress(&p,16,4,0,&at));
	dis64layoutfree(&p);
	local.ty=&alt; local.offset=32; desc.size=56; desc.nmap=2; map[0]=0; map[1]=0x20;
	assert(dis64layoutstorage(nil,&local,&desc,1,&p)); dis64layoutfree(&p);
	map[1]|=0x10; assert(!dis64layoutstorage(nil,&local,&desc,1,&p)); dis64layoutfree(&p);
	alt.size=20; assert(!dis64layouttype(&alt,&p)); dis64layoutfree(&p);
	/* Pick payloads have separate layouts; never merge all variant roots into a union. */
	parent.kind=Tadt; parent.ids=&common; parent.tags=&tag;
	common.store=Dfield; common.ty=&s; common.offset=4;
	tag.ty=&pick;
	{
		Decl owner={0}; owner.ty=&parent; tag.dot=&owner;
		pick.kind=Tadtpick; pick.size=16; pick.decl=&tag; pick.ids=&variant;
		variant.store=Dfield; variant.ty=&r; variant.offset=8;
		assert(dis64layouttype(&pick,&p) && p.bytes==24 && p.count==3); dis64layoutfree(&p);
		assert(!dis64layouttype(&parent,&p)); dis64layoutfree(&p);
	}
	for(i=0;i<3;i++){
		table.kind=i==0?Tcase:i==1?Tcasec:Tcasel; table.cse=&c; table.size=i==2?60:32;
		assert(dis64layouttype(&table,&p));
		assert(!dis64layoutaddress(&p,i==2?24:12,4,0,&at));
		if(i==1){ assert(p.bytes==64); assert(dis64layoutaddress(&p,4,4,1,&at) && at==8); }
		dis64layoutfree(&p);
	}
	table.kind=Tiface; table.size=19;
	assert(dis64layouttype(&table,&p) && p.bytes==19);
	assert(!dis64layoutaddress(&p,0,4,0,&at)); dis64layoutfree(&p);
	table.size=Dis64LayoutBytes+1;
	assert(!dis64layouttype(&table,&p)); dis64layoutfree(&p);
	{
		Type deep[129]={{0}}; Decl nested[129]={{0}};
		for(i=0;i<129;i++){
			deep[i].kind=Ttuple; deep[i].size=4; deep[i].ids=nested+i;
			nested[i].store=Dfield; nested[i].ty=i<128?deep+i+1:&s;
		}
		assert(!dis64layouttype(deep,&p)); dis64layoutfree(&p);
	}
	assert(p.leaves==nil && p.count==0 && p.reason==nil);
	assert(layoutbanktest());
	assert(allocations==0);
	return 0;
}
