#include "comp-amd64.c"
#include "tests/leaf-cons-stub.h"
#include "tests/leaf-plain-stubs.h"
/* Supply the builtin identity used by guarded numeric formatting leaves. */
Type Tstring = {1}, Tarray = {1};
#include <stdio.h>
#include <setjmp.h>
#include <fenv.h>

#define ownercheck(condition) ownerpredicate(!!(condition)) // Keep pointer/scalar oracles active under NDEBUG.


/* Retain every lifetime/GC predicate in optimized test builds. */
static void
ownerpredicate(int condition)
{
	if(!condition)
		abort();
}


REG R;
int mutator, nprop;
char exNilref[]="nil", exModule[]="module", exHeap[]="heap", exBounds[]="bounds";
static jmp_buf boundary;
static char *raised;
static int releases;

/* Supply descriptor allocation while retaining production executable publication. */
void*
mallocz(ulong size, int clear)
{
	void *value=malloc(size);
	if(value && clear)
		memset(value,0,size);
	return value;
}

/* Compiler publication is infallible in this lifetime differential fixture. */
int
kprotect(int (*callback)(void*), void *argument)
{
	return callback(argument);
}

/* Observe the production handler's exception before comparing ownership bytes. */
void
error(char *message)
{
	raised=message;
	longjmp(boundary,1);
}

/* Model owner release without freeing storage needed for the second execution. */
void
destroy(void *value)
{
	if(value != H){
		ownercheck(value != nil && D2H(value)->ref > 0);
		D2H(value)->ref--;
		releases++;
	}
}

/* Preserve ASan stack checks without unavailable process-based leak inspection. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

#define OP(name) void name(void)
IMPLEMENTATION

/* Keep VM errors outside the caller's matrix loop variables. */
static int
ownerchecked(void (*operation)(void))
{
	raised = nil;
	if(setjmp(boundary))
		return 0;
	operation();
	return 1;
}


/* Failed owner preflight cannot retain, mark, release or publish any heap bytes. */
static void
checkpreflight(Module *module)
{
	union {
		Heap heap; // Natural alignment and the actual native Heap metadata.
		uchar bytes[512]; // Opaque payload and canaries for an owner or List.
	} source, previous, container;
	uchar sourceimage[512], previousimage[512], listimage[512], slots[128], before[128];
	void *value, *old, *node;
	List *list;
	REG saved, expected;
	uintptr pointer;
	int op, family, mode, limit, alignment, total = 0;
	char *why;
	void (*handlers[])(void) = {movp, self, headp, tail};
	int codes[] = {IMOVP, ISELF, IHEADP, ITAIL};

	for(op = 0; op < 4; op++)
	for(family = 0; family < 6; family++){
		if(family == 2 && op < 2)
			continue;
		limit = family == 0 ? 8 : family == 1 ? sizeof(Heap) :
			family == 2 ? (op == 2 ? offsetof(List, data) : offsetof(List, tail))+sizeof(void*)-1 :
			family == 3 ? 4 : family == 4 ? sizeof(Heap)+2 : 1;
		for(mode = 0; mode < limit; mode++)
		for(alignment = 0; alignment < 8; alignment++){
			if((family == 0 && op == 1 && mode < 4) || (family == 3 && op != 1 && mode == 1))
				continue;
			if(getenv("OWNER_PREFLIGHT_FAMILY") && family != atoi(getenv("OWNER_PREFLIGHT_FAMILY")))
				continue;
			memset(&source, 0, sizeof(source)); memset(&previous, 0, sizeof(previous)); memset(&container, 0, sizeof(container));
			source.heap.ref = previous.heap.ref = container.heap.ref = 1;
			value = H2D(void*, &source.heap); old = H; node = H2D(void*, &container.heap);
			why = exHeap;
			memset(slots, 0xa5, sizeof(slots)); memset(&R, 0, sizeof(R));
			R.s = slots+8+alignment; R.d = slots+64+alignment;
			if(family == 1)value = (void*)(uintptr)(mode+1);
			if(family == 2){ node = (void*)((uintptr)-2-mode); why = exBounds; }
			if(family == 3){
				if(mode < 2){ value = mode == 0 ? nil : H; why = op == 1 ? exModule : exNilref; }
				else source.heap.ref = mode == 2 ? 0 : (ulong)-1;
			}
			if(family == 4){
				if(mode <= sizeof(Heap))old = (void*)(uintptr)mode;
				else { old = H2D(void*, &previous.heap); previous.heap.ref = 0; }
			}
			list = H2D(List*, &container.heap);
			memmove(list->data, &value, sizeof(value)); list->tail = value;
			memmove(R.s, op >= 2 ? &node : &value, sizeof(value)); memmove(R.d, &old, sizeof(old));
			R.M = value;
			if(family == 0){
				pointer = (mode&3) == 0 ? 0 : (mode&3) == 1 ? (uintptr)H : (uintptr)-1-((mode&3)-1);
				if(mode < 4)R.s = (void*)pointer; else R.d = (void*)pointer;
			}
			if(family == 3)R.d = (void*)1; // Source errors must precede this unused unmapped old-owner slot.
			if(family == 5)R.d = &source.heap.ref; // A malformed destination aliases the source's reference cell.
			saved = expected = R;
			if(op >= 2 && family != 0 && family != 2)
				expected.s = op == 2 ? (void*)list->data : (void*)&list->tail;
			memmove(before, slots, sizeof(before)); memmove(sourceimage, &source, sizeof(source));
			memmove(previousimage, &previous, sizeof(previous)); memmove(listimage, &container, sizeof(container));
			mutator = 1; nprop = 7; releases = 0;
			ownercheck(module->jit->entry[codes[op]](&R) == 0 && !memcmp(&R, &saved, sizeof(R)));
			ownercheck(!ownerchecked(handlers[op]) && raised == why && !memcmp(&R, &expected, sizeof(R)));
			ownercheck(nprop == 7 && releases == 0 && !memcmp(slots, before, sizeof(slots)) &&
				!memcmp(&source, sourceimage, sizeof(source)) && !memcmp(&previous, previousimage, sizeof(previous)) &&
				!memcmp(&container, listimage, sizeof(container)));
			total++;
		}
	}
	printf("PASS owner preflight: %d slot/header/previous-owner/source-priority/GC cases WORD%zu\n", total, sizeof(WORD)*8);
}


/* Reject invalid list identities without changing operand pointers or owner slots. */
static unsigned long
badlists(Module *module)
{
	union {uvlong align;uchar bytes[64];} slots;
	uchar expected[64];
	void *owner, *empty;
	int op, kind, sa, da;
	unsigned long checks=0;
	void (*handlers[])(void)={headp,tail};
	int codes[]={IHEADP,ITAIL};

	for(op=0;op<2;op++)for(kind=0;kind<2;kind++)
	for(sa=0;sa<8;sa++)for(da=0;da<8;da++){
		owner=kind ? H : nil; empty=H;
		memset(&R,0,sizeof(R));memset(slots.bytes,0xa5,sizeof(slots.bytes));
		R.s=slots.bytes+8+sa;R.d=slots.bytes+40+da;
		memmove(R.s,&owner,sizeof(owner));memmove(R.d,&empty,sizeof(empty));
		memmove(expected,slots.bytes,sizeof(expected));
		nprop=0;releases=0;raised=nil;
		ownercheck(!module->jit->entry[codes[op]](&R));
		ownercheck(!memcmp(expected,slots.bytes,sizeof(expected)) && R.s==slots.bytes+8+sa);
		if(setjmp(boundary)==0){
			handlers[op]();ownercheck(0);
		}else
			ownercheck(raised==exNilref);
		ownercheck(!memcmp(expected,slots.bytes,sizeof(expected)) && R.s==slots.bytes+8+sa);
		ownercheck(nprop==0 && releases==0);
		checks++;
	}
	/* Overlapping nil slots exercise the no-header path at every byte displacement. */
	for(sa=0;sa<8;sa++)for(da=0;da<8;da++){
		memset(slots.bytes,0xff,sizeof(slots.bytes));memmove(expected,slots.bytes,sizeof(expected));
		memset(&R,0,sizeof(R));R.s=slots.bytes+8+sa;R.d=slots.bytes+8+da;
		nprop=0;releases=0;
		ownercheck(module->jit->entry[IMOVP](&R));movp();
		ownercheck(!memcmp(expected,slots.bytes,sizeof(expected)) && nprop==0 && releases==0);
		checks++;
	}
	return checks;
}

/* Compare both executions against independent reference, mark and byte expectations. */
int
main(void)
{
	Module module={0}; Inst instructions[4]={{0}};
	int codes[4]={IMOVP,ISELF,IHEADP,ITAIL};
	void (*handlers[4])(void)={movp,self,headp,tail};
	Heap *heap, *previous;
	List *list;
	void *value, *old, *source, *destination, *result;
	union {uvlong align;uchar bytes[128];} slots;
	uchar before[128], expected[128];
	ulong counts[]={0,1,2,0xffffffffUL,0x100000000UL,(ulong)-2,(ulong)-1};
	ulong wanted, oldwanted, refs;
	int op,sa,da,v,d,ri,color,live,alias,pass,valid,native,marked,oldrelease,seed;
	int selectedop=-1, selectedref=-1;
	char *selection;
	unsigned long checks=0;
	char *failure;
	Inst sentinel;

	selection=getenv("OWNER_OPERATION");
	if(selection)selectedop=atoi(selection);
	selection=getenv("OWNER_REF_INDEX");
	if(selection)selectedref=atoi(selection);
	heap=calloc(1,sizeof(Heap)+sizeof(Modlink));
	previous=calloc(1,sizeof(Heap)+sizeof(Modlink));
	list=calloc(1,sizeof(List)+sizeof(void*));
	ownercheck(heap && previous && list);
	for(op=0;op<4;op++)
		instructions[op].op=codes[op];
	module.prog=instructions; module.nprog=4;
	ownercheck(compile(&module,4,nil));
	for(op=0;op<4;op++)
		ownercheck(module.jit->entry[codes[op]]);

	checkpreflight(&module);
	if(getenv("OWNER_PREFLIGHT_ONLY"))goto finished;
	for(op=0;op<4;op++)for(sa=0;sa<8;sa++)for(da=0;da<8;da++)
	for(v=0;v<3;v++)for(d=0;d<4;d++)for(ri=0;ri<7;ri++)
	for(color=0;color<4;color++)for(live=0;live<3;live++)for(alias=0;alias<2;alias++)for(seed=0;seed<2;seed++){
		if((selectedop>=0 && op!=selectedop) || (selectedref>=0 && ri!=selectedref))
			continue;
		value=v==0 ? nil : v==1 ? H : H2D(void*,heap);
		old=d==0 ? H : d==1 ? H2D(void*,previous) : d==2 ? H2D(void*,heap) : nil;
		source=slots.bytes+8+sa; destination=slots.bytes+64+da;
		if(alias){
			if(op!=0 || sa!=da)
				continue;
			destination=source; old=value;
		}
		/* C-null destinations are corrupt owners; keep their destruction out of scope. */
		if(old==nil)
			continue;
		refs=counts[ri]; mutator=live;
		if(old==H2D(void*,heap) && refs==0 && value!=H2D(void*,heap))
			continue;
		valid=value!=nil && (op!=1 || value!=H) && (value==H || (refs!=0 && refs!=(ulong)-1));
		failure=value==nil || (op==1 && value==H) ? (op==1 ? exModule : exNilref) : exHeap;
		native=valid && (old==H || old==value);
		marked=valid && value!=H && color!=mutator;
		oldrelease=valid && old!=H;
		wanted=refs; oldwanted=10;
		if(valid){
			if(value!=H)wanted++;
			if(old==H2D(void*,heap))wanted--;
			if(old==H2D(void*,previous))oldwanted--;
		}
		memset(slots.bytes,0xa5,sizeof(slots.bytes));
		result=op>=2 ? list : value;
		memmove(source,&result,sizeof(result)); memmove(destination,&old,sizeof(old));
		memmove(before,slots.bytes,sizeof(before)); memmove(expected,before,sizeof(expected));
		if(valid)memmove(expected+((uchar*)destination-slots.bytes),&value,sizeof(value));
		for(pass=0;pass<2;pass++){
			memmove(slots.bytes,before,sizeof(before));
			memset(&R,0,sizeof(R)); R.s=source;R.d=destination;R.M=value;R.PC=&sentinel;
			heap->ref=refs; heap->color=color; previous->ref=10; previous->color=2;
			memmove(list->data,&value,sizeof(value));list->tail=value;
			nprop=seed; releases=0; raised=nil;
			feclearexcept(FE_ALL_EXCEPT); feraiseexcept(FE_DIVBYZERO);
			if(pass==0){
				ownercheck(module.jit->entry[codes[op]](&R)==native);
				if(!native){
					ownercheck(!memcmp(before,slots.bytes,sizeof(before)) && heap->ref==refs && previous->ref==10);
					ownercheck(R.s==source && heap->color==color && nprop==seed && releases==0);
					ownercheck(fetestexcept(FE_ALL_EXCEPT)==FE_DIVBYZERO);
					continue;
				}
			}else if(setjmp(boundary)==0){
				handlers[op](); ownercheck(valid);
			}else
				ownercheck(!valid && raised==failure);
			ownercheck(!memcmp(expected,slots.bytes,sizeof(expected)));
			ownercheck(heap->ref==wanted && previous->ref==oldwanted && releases==(pass ? oldrelease : 0));
			ownercheck(heap->color==(marked ? propagator : color) && nprop==(marked ? 1 : seed) && previous->color==2);
			ownercheck(R.PC==&sentinel && R.M==value && R.d==destination);
			ownercheck(R.s==(op>=2 ? (op==2 ? (void*)list->data : (void*)&list->tail) : source));
			ownercheck(fetestexcept(FE_ALL_EXCEPT)==FE_DIVBYZERO);
		}
		checks++;
	}
	checks+=badlists(&module);
finished:
	module.jit->freecode(module.jit->code,module.jit->size);free(module.jit);
	free(heap);free(previous);free(list);
	printf("PASS native owner C differential: %lu reference/GC/slot/error/FP cases WORD%zu\n",checks,sizeof(WORD)*8);
	return 0;
}
