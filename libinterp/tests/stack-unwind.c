#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include <stdlib.h>
#include <stdio.h>



/* Keep ownership predicates active in optimized sanitizer builds. */
static void
unwindcheck(int condition)
{
	if(!condition)
		abort();
}

REG R;
static int depth, releases, checksizes;
static int callbackmode, mutate; // Controlled metadata changes during real owner cleanup.
static REG *unwindreg; // Active target may be the global R or a saved process REG.
static int globaltarget; // Exercise the real global teardown route independently of saved REG.
static Type emptytype; // Pointer-free descriptor used to expose a late root lookup.
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
	for(i=0; i<(int)nelem(owners) && H2D(void*, owners[i])!=value; i++);
	unwindcheck(i<(int)nelem(owners) && owners[i]->ref>1);
	if(mutate && releases < 2){
		if(callbackmode == 1)extensions[releases]->reg.EX = nil;
		if(callbackmode == 2)extensions[releases]->reg.TR = &emptytype;
	}
	if(mutate && callbackmode >= 3){
		if(callbackmode == 7)
			memset(unwindreg, 0x66, sizeof(*unwindreg));
		if(callbackmode == 3 || callbackmode >= 6) unwindreg->FP = nil;
		if(callbackmode == 4 || callbackmode >= 6) unwindreg->M = H;
		if(callbackmode == 5 || callbackmode >= 6) unwindreg->EX = (uchar*)1;
	}
	owners[i]->ref--;
	observed[i]++;
}


/* Validate the heap identity reached by the production diagnostic scanner. */
int
hmsize(void *value)
{
	int i;

	for(i=0; i<(int)nelem(owners) && owners[i]!=value; i++);
	unwindcheck(i<(int)nelem(owners));
	checksizes++;
	return sizeof(Heap)+sizeof(Modlink);
}


/* Extension ownership must be released exactly once, newest segment first. */
static void
releaseextension(void *value)
{
	unwindcheck(releases<2 && value==extensions[releases]);
	releases++;
	free(value);
}

#define free(p) releaseextension(p) // Observe extension ownership without hiding real heap frees.
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
one(int firstalign, int secondalign, int mask, int roots, int typealign)
{
	Type payloadtype={0}, frametype={0};
	union {
		Heap align; // Native alignment for the packed descriptor base.
		uchar bytes[2*sizeof(Type)+16]; // Two complete Type images at independent byte shifts.
	} descriptors;
	Type *framemap, *payloadmap;
	REG reg={0};
	uchar *frames[4], *allocation[2], before[2][128];
	Modlink *module;
	int i, current, visits;

	frametype.size=sizeof(Frame)+sizeof(void*);
	frametype.np=1;
	frametype.map[0]=1<<(7-sizeof(Frame)/sizeof(void*));
	framemap=(Type*)(descriptors.bytes+typealign);
	payloadmap=(Type*)(descriptors.bytes+sizeof(Type)+8+(typealign*3)%8);
	memmove(framemap,&frametype,sizeof(frametype));
	memmove(payloadmap,&payloadtype,sizeof(payloadtype));
	for(i=0; i<(int)nelem(owners); i++){
		owners[i]=calloc(1,sizeof(Heap)+sizeof(Modlink));
		unwindcheck(owners[i] != nil);owners[i]->ref=20;owners[i]->t=payloadmap;
		expected[i]=observed[i]=0;
	}
	for(i=0; i<2; i++){
		extensions[i]=calloc(1,sizeof(Stkext)+128);
		allocation[i]=malloc(128);
		unwindcheck(extensions[i] && allocation[i]);
		memset(allocation[i],0xa5,128);
		extensions[i]->reg.TR=framemap;
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
			((i==1 && (roots&1)) || (i==3 && (roots&2))) ? nil : framemap,
			H2D(void*,owners[i]));
		expected[i]++;
		if(module!=nil){expected[current]++;current=5+i;}
	}
	expected[current]++;
	for(i=0; i<2; i++)memmove(before[i],allocation[i],128);
	reg.EX=(uchar*)extensions[0];reg.FP=frames[0];reg.M=H2D(Modlink*,owners[4]);
	R=reg;checksizes=0;mutate=0;validstk();
	unwindcheck(depth==0 && checksizes==4);
	for(i=0; i<(int)nelem(owners); i++)unwindcheck(owners[i]->ref==20 && observed[i]==0);
	unwindreg = globaltarget ? &R : &reg;
	releases=0;mutate=1;destroystack(unwindreg);mutate=0;
	unwindcheck(releases==2 && unwindreg->EX==nil && unwindreg->FP==nil && unwindreg->M==H);
	visits=0;
	for(i=0; i<(int)nelem(owners); i++){
		unwindcheck(observed[i]==expected[i] && owners[i]->ref==(ulong)(20-expected[i]));
		visits+=observed[i];free(owners[i]);
	}
	unwindcheck(visits>=5);
	for(i=0; i<2; i++){
		unwindcheck(!memcmp(before[i],allocation[i],128));
		free(allocation[i]);
	}
}


/* Release a module-only process image even when its final destructor borrows REG. */
static void
emptyunwind(void)
{
	REG reg;
	int i;

	for(i = 0; i < nelem(owners); i++){
		owners[i] = calloc(1, sizeof(Heap)+sizeof(Modlink));
		unwindcheck(owners[i] != nil);
		owners[i]->ref = 20;
		expected[i] = observed[i] = 0;
	}
	expected[4] = 1;
	memset(&reg, 0, sizeof(reg));
	reg.M = H2D(Modlink*, owners[4]);
	memmove(&R, &reg, sizeof(reg));
	unwindreg = globaltarget ? &R : &reg;
	releases = 0; mutate = 1;
	destroystack(unwindreg);
	mutate = 0;
	unwindcheck(releases == 0 && unwindreg->EX == nil && unwindreg->FP == nil && unwindreg->M == H);
	for(i = 0; i < nelem(owners); i++){
		unwindcheck(observed[i] == expected[i] && owners[i]->ref == (ulong)(20-expected[i]));
		free(owners[i]);
	}
}


/* Vary both ordinary-frame alignments, all module switches and root descriptors. */
int
main(void)
{
	int a,b,mask,roots,typealign,checks=0,emptychecks=0;
	char *family;

	family = getenv("UNWIND_CASE");
	if(family == nil || !strcmp(family, "frames"))
	for(callbackmode=0; callbackmode<8; callbackmode++)
	for(globaltarget=0; globaltarget<2; globaltarget++)
	for(typealign=0; typealign<8; typealign++)
	for(a=0; a<8; a++)for(b=0; b<8; b++)
	for(mask=0; mask<16; mask++)for(roots=0; roots<4; roots++){
		if(callbackmode < 3 && globaltarget != 0)continue;
		if(getenv("UNWIND_GLOBAL_TARGET") && globaltarget != atoi(getenv("UNWIND_GLOBAL_TARGET")))continue;
		if(getenv("UNWIND_TYPE_SHIFT") && typealign != atoi(getenv("UNWIND_TYPE_SHIFT")))continue;
		if(getenv("UNWIND_CALLBACK_MODE") && callbackmode != atoi(getenv("UNWIND_CALLBACK_MODE")))continue;
		one(a,b,mask,roots,typealign);checks++;
	}
	if(family == nil || !strcmp(family, "empty"))
	for(callbackmode = 0; callbackmode < 8; callbackmode++)
	for(globaltarget = 0; globaltarget < 2; globaltarget++){
		if(callbackmode == 1 || callbackmode == 2) continue;
		if(getenv("UNWIND_GLOBAL_TARGET") && globaltarget != atoi(getenv("UNWIND_GLOBAL_TARGET"))) continue;
		if(getenv("UNWIND_CALLBACK_MODE") && callbackmode != atoi(getenv("UNWIND_CALLBACK_MODE"))) continue;
		emptyunwind(); emptychecks++;
	}
	printf("PASS empty stack unwind: %d module-only cleanup checks WORD%zu\n", emptychecks, sizeof(WORD)*8);
	printf("PASS stack unwind: %d alignment/extension/type/owner checks WORD%zu\n",checks,sizeof(WORD)*8);
	return 0;
}
