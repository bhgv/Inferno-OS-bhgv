#include "dat.h"
#include "fns.h"
#include "interp.h"
#include <stdio.h>

typedef struct MapOwner MapOwner;

struct MapOwner {
	Heap heap; // Real native owner header, retained throughout collection.
	uchar data[sizeof(void*)]; // One native child pointer for recursive graphs.
};

static MapOwner owners[70]; // Three repeated owners or a depth-limited graph.
static Type childtype; // Aligned canonical callback descriptor for child owners.
static int callbacks[80], callbackcount; // Exact depth-first callback trace.
static int visit, gce, markdepth; // Actual collector budget and recursion state.
int mutator=1, nprop; // Production mark-barrier state.


/* Keep byte, budget, ownership and traversal checks active with NDEBUG. */
static void
gccheck(int condition)
{
	if(!condition)
		abort();
}


/* Preserve sanitizers without process-inspection leak checks in this sandbox. */
int
__lsan_is_turned_off(void)
{
	return 1;
}


/* Translate a callback payload to its exact native heap owner. */
static int
ownerindex(void *data)
{
	int i;

	for(i=0; i<70; i++)
		if(data==owners[i].data)
			return i;
	abort();
	return -1;
}

IMPLEMENTATION


/* Record collector order without substituting the markheap implementation. */
static void
observe(Type *type, void *data)
{
	gccheck(type==&childtype && callbackcount<80);
	callbacks[callbackcount++]=ownerindex(data);
}


/* Recurse through the real collector so depth and cycle behavior stay observable. */
static void
recurse(Type *type, void *data)
{
	gccheck(type==&childtype && callbackcount<80);
	callbacks[callbackcount++]=ownerindex(data);
	markheap(type,data);
}


/* Enumerate masks independently, including repeated owners and exhausted budgets. */
static unsigned long
matrix(void)
{
	struct {
		Type type; // Aligned source copied into independently shifted descriptor bytes.
		uchar extra[8]; // Up to three native pointer-map bytes.
	} descriptor;
	uchar packed[sizeof(descriptor)+8], packedbefore[sizeof(packed)];
	uchar record[256], before[sizeof(record)], *map, *base;
	Type *active;
	void *value;
	int budgets[]={-1,0,1,3,24}, depths[]={0,62,63};
	int expectedcolors[3], order[24], fields[24];
	int ds, alignment, groups, mask, phase, profile, i, slot, count;
	int budget, balance, propagated, id, depth;
	unsigned long checks=0;

	memset(&childtype,0,sizeof(childtype));childtype.mark=observe;
	for(ds=0; ds<8; ds++)for(alignment=0; alignment<8; alignment++)
	for(groups=1; groups<=3; groups+=2)for(mask=0; mask<256; mask++)
	for(phase=0; phase<3; phase++)for(profile=0; profile<5; profile++){
		if(getenv("GC_DESC_ALIGN") && ds!=atoi(getenv("GC_DESC_ALIGN")))continue;
		if(getenv("GC_RECORD_ALIGN") && alignment!=atoi(getenv("GC_RECORD_ALIGN")))continue;
		memset(&descriptor,0,sizeof(descriptor));memset(record,0xa5,sizeof(record));
		descriptor.type.np=groups;descriptor.type.size=groups*8*sizeof(void*);
		map=(uchar*)&descriptor+O(Type,map);
		for(i=0; i<groups; i++)map[i]=i==0 ? mask : i==1 ? mask^0xa5 : (mask<<1 | mask>>7)&255;
		memset(packed,0xa5,sizeof(packed));memmove(packed+ds,&descriptor,sizeof(descriptor));
		memmove(packedbefore,packed,sizeof(packed));active=(Type*)(packed+ds);
		base=record+16+alignment;
		for(slot=0; slot<groups*8; slot++){
			fields[slot]=-1;
			if(!(map[slot/8] & (0x80>>(slot%8))))continue;
			id=slot%3;value=id==phase ? H : owners[id].data;
			if(value!=H)fields[slot]=id;
			memmove(base+slot*sizeof(value),&value,sizeof(value));
		}
		memmove(before,record,sizeof(record));
		for(depth=0; depth<3; depth++){
			for(i=0; i<3; i++){
				owners[i].heap.ref=1000+i;
				owners[i].heap.t=((i+profile)%3==phase ? nil : &childtype);
				owners[i].heap.color=expectedcolors[i]=i==profile%3 ? mutator : 0;
			}
			budget=visit=budgets[profile];balance=gce=77;markdepth=depths[depth];
			count=propagated=callbackcount=nprop=0;
			for(slot=0; slot<groups*8; slot++){
				id=fields[slot];
				if(id<0 || expectedcolors[id]==mutator)continue;
				expectedcolors[id]=propagator;propagated=1;
				budget--;
				if(budget>=0 && depths[depth]+1<64){
					balance--;expectedcolors[id]=mutator;
					if(owners[id].heap.t!=nil)order[count++]=id;
				}
			}
			markheap(active,base);
			gccheck(visit==budget && gce==balance && markdepth==depths[depth] && nprop==propagated);
			gccheck(callbackcount==count && !memcmp(callbacks,order,count*sizeof(int)));
			for(i=0; i<3; i++)gccheck(owners[i].heap.color==expectedcolors[i] && owners[i].heap.ref==1000+i);
			gccheck(!memcmp(before,record,sizeof(record)) && !memcmp(packedbefore,packed,sizeof(packed)));
			checks++;
		}
	}
	return checks;
}


/* Preserve recursion limits, cycle suppression, nil Types and zero-map laziness. */
static unsigned long
graphs(void)
{
	Type descriptor={0};
	uchar packed[sizeof(Type)+8];
	void *root, *value;
	int lengths[]={1,2,7,64,70};
	int ds, budget, cycle, length, i, expected, calls, steps, colors[70], count;
	unsigned long checks=0;

	childtype.np=1;childtype.size=sizeof(void*);childtype.map[0]=0x80;childtype.mark=recurse;
	descriptor.np=1;descriptor.size=sizeof(void*);descriptor.map[0]=0x80;
	for(ds=0; ds<8; ds++)for(cycle=0; cycle<2; cycle++)
	for(length=0; length<5; length++)for(budget=0; budget<74; budget++){
		for(i=0; i<70; i++){
			owners[i].heap.color=colors[i]=0;owners[i].heap.ref=1000+i;owners[i].heap.t=&childtype;
			value=i<lengths[length]-1 ? owners[i+1].data : cycle ? owners[0].data : H;
			memmove(owners[i].data,&value,sizeof(value));
		}
		memmove(packed+ds,&descriptor,sizeof(descriptor));root=owners[0].data;
		visit=budget;gce=100;markdepth=callbackcount=nprop=0;
		expected=budget;calls=steps=0;
		for(i=0; i<lengths[length]; i++){
			steps++;expected--;colors[i]=propagator;
			if(expected<0 || i>=63)break;
			colors[i]=mutator;calls++;
		}
		markheap((Type*)(packed+ds),&root);
		gccheck(visit==budget-steps && visit==expected && gce==100-calls && callbackcount==calls && markdepth==0 && nprop==1);
		for(i=0; i<70; i++)gccheck(owners[i].heap.color==colors[i] && owners[i].heap.ref==1000+i);
		for(i=0; i<calls; i++)gccheck(callbacks[i]==i);
		checks++;
	}
	memset(&descriptor,0,sizeof(descriptor));
	for(ds=0; ds<8; ds++)for(count=0; count<=3; count+=3)for(i=0; i<3; i++){
		descriptor.np=count;memmove(packed+ds,&descriptor,sizeof(descriptor));
		value=i==0 ? nil : i==1 ? H : (void*)1;
		visit=7;gce=11;markdepth=callbackcount=nprop=0;
		markheap((Type*)(packed+ds),value);markheap(nil,value);
		gccheck(visit==7 && gce==11 && markdepth==0 && callbackcount==0 && nprop==0);
		checks++;
	}
	return checks;
}


/* Run packed object maps and depth-limited recursive marking under both WORD models. */
int
main(void)
{
	unsigned long maps=0, chains=0;
	char *family=getenv("GC_MAP_CASE");

	if(family==nil || !strcmp(family,"matrix"))maps=matrix();
	if(family==nil || !strcmp(family,"graphs"))chains=graphs();
	gccheck(maps || chains);
	printf("PASS GC maps WORD%zu: %lu packed-map/budget/depth checks, %lu recursive/empty checks\n",sizeof(WORD)*8,maps,chains);
	return 0;
}
