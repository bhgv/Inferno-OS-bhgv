#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include <assert.h>
#include <stdio.h>

REG R;
static int depth, releases, checksizes;
static Heap *owners[9];
static Stkext *extensions[2];
static int expected[9], observed[9];

/* Preserve address/stack instrumentation without unavailable process inspection. */
int __lsan_is_turned_off(void) { return 1; }

/* Record actual cleanup order and reference changes without freeing fixture owners. */
void
destroy(void *value)
{
	int i;

	if(value == H)return;
	for(i=0; i<nelem(owners) && H2D(void*, owners[i])!=value; i++);
	assert(i<nelem(owners) && owners[i]->ref>1);
	owners[i]->ref--;
	observed[i]++;
}

/* Validate the heap identity reached by the production diagnostic scanner. */
int
hmsize(void *value)
{
	int i;

	for(i=0; i<nelem(owners) && owners[i]!=value; i++);
	assert(i<nelem(owners));
	checksizes++;
	return sizeof(Heap)+sizeof(Modlink);
}

/* Extension ownership must be released exactly once, newest segment first. */
static void
releaseextension(void *value)
{
	assert(releases<2 && value==extensions[releases]);
	releases++;
	free(value);
}

#define free(p) releaseextension(p)
IMPLEMENTATION
#undef free

/* Build frames solely through byte copies so the fixture has no alignment UB. */
static void
putframe(uchar *frame, uchar *previous, Modlink *module, Type *type, void *payload)
{
	Inst *lr=nil;

	memmove(frame+O(Frame, lr), &lr, sizeof(lr));
	memmove(frame+O(Frame, fp), &previous, sizeof(previous));
	memmove(frame+O(Frame, mr), &module, sizeof(module));
	memmove(frame+O(Frame, t), &type, sizeof(type));
	memmove(frame+sizeof(Frame), &payload, sizeof(payload));
}

/* Independently count releases across four frames and two stack extensions. */
static void
one(int firstalign, int secondalign, int mask, int roots)
{
	Type payloadtype={0}, frametype={0};
	REG reg={0};
	uchar *frames[4], *allocation[2], before[2][128];
	Modlink *module;
	int i, current, visits;

	frametype.size=sizeof(Frame)+sizeof(void*);
	frametype.np=1;
	frametype.map[0]=1<<(7-sizeof(Frame)/sizeof(void*));
	for(i=0; i<nelem(owners); i++){
		owners[i]=calloc(1,sizeof(Heap)+sizeof(Modlink));
		assert(owners[i]);owners[i]->ref=20;owners[i]->t=&payloadtype;
		expected[i]=observed[i]=0;
	}
	for(i=0; i<2; i++){
		extensions[i]=calloc(1,sizeof(Stkext)+128);
		allocation[i]=malloc(128);
		assert(extensions[i] && allocation[i]);
		memset(allocation[i],0xa5,128);
		extensions[i]->reg.TR=&frametype;
	}
	extensions[0]->reg.EX=(uchar*)extensions[1];
	frames[0]=allocation[0]+16+firstalign;
	frames[1]=(uchar*)extensions[0]->reg.tos.fr;
	frames[2]=allocation[1]+16+secondalign;
	frames[3]=(uchar*)extensions[1]->reg.tos.fr;
	current=4;
	for(i=0; i<4; i++){
		module=(mask&(1<<i)) ? H2D(Modlink*,owners[5+i]) : nil;
		putframe(frames[i],i==3 ? nil : frames[i+1],module,
			(i==1 && (roots&1) || i==3 && (roots&2)) ? nil : &frametype,
			H2D(void*,owners[i]));
		expected[i]++;
		if(module!=nil){expected[current]++;current=5+i;}
	}
	expected[current]++;
	for(i=0; i<2; i++)memmove(before[i],allocation[i],128);
	reg.EX=(uchar*)extensions[0];reg.FP=frames[0];reg.M=H2D(Modlink*,owners[4]);
	R=reg;checksizes=0;validstk();
	assert(depth==0 && checksizes==4);
	for(i=0; i<nelem(owners); i++)assert(owners[i]->ref==20 && observed[i]==0);
	releases=0;destroystack(&reg);
	assert(releases==2 && reg.EX==nil && reg.FP==nil && reg.M==H);
	visits=0;
	for(i=0; i<nelem(owners); i++){
		assert(observed[i]==expected[i] && owners[i]->ref==20-expected[i]);
		visits+=observed[i];free(owners[i]);
	}
	assert(visits>=5);
	for(i=0; i<2; i++){
		assert(!memcmp(before[i],allocation[i],128));
		free(allocation[i]);
	}
}

/* Vary both ordinary-frame alignments, all module switches and root descriptors. */
int
main(void)
{
	int a,b,mask,roots,checks=0;

	for(a=0; a<8; a++)for(b=0; b<8; b++)
	for(mask=0; mask<16; mask++)for(roots=0; roots<4; roots++){
		one(a,b,mask,roots);checks++;
	}
	printf("PASS stack unwind: %d alignment/extension/type/owner checks WORD%zu\n",checks,sizeof(WORD)*8);
	return 0;
}
