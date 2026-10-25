#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "runt.h"
#include "raise.h"
#include "pool.h"
#include "kernel.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static void *replacement_old; /* Observe owner retirement before its destructor runs. */
static void *replacement_slot; /* Borrowed actual result slot for replacement observation. */
static Proc worker;
static char message[128];
static int live, allocations, failat, checks;
static struct { Heap *heap; ulong size; } blocks[64];
static Type insttype, linktype, desctype;
static Type *Tinst = &insttype, *Tlink = &linktype, *Tdesc = &desctype;
Type Tarray = {1}, Tbyte = {1}, Tstring = {1};
Pool *heapmem;
int mutator, nprop;
void (*heapmonitor)(int, void*, ulong);
char exHeap[] = "heap", exNegsize[] = "negative", exNilref[] = "nil";

/* Use the actual emulator error-frame protocol around snapshot construction. */
Proc *getup(void) { return &worker; }
void nexterror(void) { assert(worker.nerr > 0); siglongjmp(worker.estack[--worker.nerr], 1); }
void error(char *s) { snprintf(message, sizeof(message), "%s", s); nexterror(); }
void kwerrstr(char *s, ...) { snprintf(message, sizeof(message), "%s", s); }
/* LeakSanitizer process inspection is unavailable in the sandbox. */
int __lsan_is_turned_off(void) { return 1; }

/* Bound the pool, fail each allocation and guard the exact requested payload. */
void*
poolalloc(Pool *pool, ulong size)
{
	Heap *h;
	int i;

	(void)pool;
	allocations++;
	if(allocations == failat || size > 65536)return nil;
	h = malloc(size+32); assert(h); memset(h, 0xa5, size+32);
	for(i = 0; i < 64 && blocks[i].heap; i++);
	assert(i < 64); blocks[i].heap = h; blocks[i].size = size; live++;
	return h;
}

/* Track references without freeing the fixture's static type descriptors. */
void
freetype(Type *t)
{
	assert(t->ref > 1); t->ref--;
}

/* Destroy partial arrays using their actual pointer map and production freeptrs. */
void
destroy(void *p)
{
	Heap *h;
	Array *a;
	WORD j;
	int i, k;

    if(p == replacement_old && replacement_old != nil) {
        void *published;
        memmove(&published, replacement_slot, sizeof(published));
        if(published != H) {
            fprintf(stderr, "Loader destroyed an owner before clearing its slot\n");
            abort();
        }
        replacement_old = nil;
    }
	if(p == H)return;
	h = D2H(p); assert(h->ref > 0);
	if(--h->ref)return;
	for(i = 0; i < 64 && blocks[i].heap != h; i++);
	assert(i < 64);
	for(k = 0; k < 32; k++)assert(((uchar*)h)[blocks[i].size+k] == 0xa5);
	if(h->t == &Tarray){
		a = p; assert(a->root == H);
		if(a->t->np)
			for(j = 0; j < a->len; j++)freeptrs(a->data+j*a->t->size, a->t);
		freetype(a->t);
	}
	freetype(h->t);
	blocks[i].heap = nil; free(h); live--;
}

/* Inject only the string allocation boundary, retaining actual array ownership. */
String*
c2string(char *text, int n)
{
	Heap *h;
	String *s;

	h = nheap(sizeof(String)+n+1);
	h->t = &Tstring; Tstring.ref++;
	s = H2D(String*, h); s->len = n;
	memmove(s->Sascii, text, n); s->Sascii[n] = 0;
	return s;
}

#define BIT(bt, nb) ((bt)&(1<<(nb)))
IMPLEMENTATION

static Module module;
static Modlink modlink;
static Inst instructions[64];
static Link exports[5];
static Type types[4], *typeptrs[4];
static Array *result;
static int operation;
static void *snapshot_slot; /* Optional borrowed byte slot for alignment/preflight cases. */
static int slot_override; /* Select the explicit slot, including nil and H. */


/* Dispatch the real generated Loader call frames through the public entry points. */
static int
dispatch(void *unused)
{
	static F_Loader_ifetch fi; /* Stable fixture frame across native error unwinds. */
	static F_Loader_link fl; /* No modified automatic call frame crosses longjmp. */
	static F_Loader_tdesc ft; /* Reset all used fields before every dispatch. */

	(void)unused;
	fi.ret = fl.ret = ft.ret = slot_override ? snapshot_slot : &result;
	fi.mp = fl.mp = ft.mp = &modlink;
	switch(operation){
	case 0: Loader_ifetch(&fi); break;
	case 1: Loader_link(&fl); break;
	case 2: Loader_tdesc(&ft); break;
	}
	return 1;
}

/* Catch baseline uncaught errors too, so ownership checks inspect the unwind. */
static void
invoke(void)
{
	kprotect(dispatch, nil);
	assert(worker.nerr == 0);
}

/* Restore static metadata and native pointer maps for each independent case. */
static void
setup(void)
{
	int i;

	assert(live == 0);
	memset(&module, 0, sizeof(module)); memset(&modlink, 0, sizeof(modlink));
	memset(instructions, 0, sizeof(instructions)); memset(exports, 0, sizeof(exports));
	memset(types, 0, sizeof(types));
	memset(&insttype, 0, sizeof(insttype)); memset(&linktype, 0, sizeof(linktype));
	memset(&desctype, 0, sizeof(desctype));
	insttype.ref = linktype.ref = desctype.ref = 1;
	insttype.size = sizeof(Loader_Inst);
	linktype.size = sizeof(Loader_Link); linktype.np = 1; linktype.map[0] = 0x80;
	desctype.size = sizeof(Loader_Typedesc); desctype.np = 1; desctype.map[0] = 0x40;
	module.prog = instructions; module.nprog = 4;
	module.ext = exports; module.type = typeptrs; module.ntype = 4;
	modlink.m = &module; result = H;
	for(i = 0; i < 4; i++){
		typeptrs[i] = &types[i]; types[i].size = 16*(i+1);
		types[i].np = i; types[i].ref = 1;
		memset(types[i].map, 0xa0+i, i);
		exports[i].name = i & 1 ? "longer name" : "name";
		exports[i].sig = 0x12345678+i;
		exports[i].frame = &types[i]; exports[i].u.pc = &instructions[i];
		instructions[i].op = IMOVW; instructions[i].add = SRC(AFP)|DST(AFP);
		instructions[i].s.imm = 10+i; instructions[i].d.imm = 20+i;
		instructions[i].reg = 30+i;
	}
	allocations = 0; failat = -1; message[0] = 0;
}

/* Successful snapshots preserve ordering, fields and all source references. */
static void
values(void)
{
	Loader_Inst *li;
	Loader_Link *ll;
	Loader_Typedesc *lt;
	int op, i;

	for(op = 0; op < 3; op++){
		setup(); operation = op; invoke();
		assert(result != H && result->len == 4);
		for(i = 0; i < 4; i++){
			if(op == 0){
				li = (Loader_Inst*)result->data+i;
				assert(li->op == IMOVW && li->addr == (SRC(AFP)|DST(AFP)));
				assert(li->src == 10+i && li->dst == 20+i && li->mid == 30+i);
			}else if(op == 1){
				ll = (Loader_Link*)result->data+i;
				assert(ll->pc == 3-i && ll->tdesc == 3-i);
				assert(ll->sig == exports[3-i].sig);
				assert(!strcmp(ll->name->Sascii, exports[3-i].name));
			}else{
				lt = (Loader_Typedesc*)result->data+i;
				assert(lt->size == types[i].size);
				if(i == 0)assert(lt->map == H);
				else assert(lt->map->len == i && !memcmp(lt->map->data, types[i].map, i));
			}
		}
		destroy(result); result = H;
		assert(live == 0 && insttype.ref == 1 && linktype.ref == 1 && desctype.ref == 1);
		checks++;
	}
}

/* Non-local failures at every allocation must release the entire partial result. */
static void
failures(void)
{
	int op, point, repeat;

	for(op = 0; op < 3; op++)for(point = 1; point <= (op == 0 ? 1 : op == 1 ? 5 : 4); point++){
		for(repeat = 0; repeat < 8; repeat++){
			setup(); operation = op; failat = point; invoke();
			assert(result == H && live == 0);
			assert(insttype.ref == 1 && linktype.ref == 1 && desctype.ref == 1);
			assert(Tarray.ref == 1 && Tbyte.ref == 1 && Tstring.ref == 1);
			checks++;
		}
	}
}

/* Replacing a live result must release its complete previous contents. */
static void
replace(void)
{
	int op;

	for(op = 0; op < 3; op++){
		setup(); operation = op; invoke(); assert(result != H);
		invoke(); assert(result != H); destroy(result); result = H;
		assert(live == 0);
		invoke(); failat = allocations+1; invoke();
		assert(result == H && live == 0); checks++;
	}
}

/* Branch targets are native Inst pointers until subtraction produces a Dis index. */
static void
branches(void)
{
	static int ops[] = {ICALL,IJMP,IBEQW,IBNEW,IBLTW,IBLEW,IBGTW,IBGEW,
		IBEQB,IBNEB,IBLTB,IBLEB,IBGTB,IBGEB,IBEQF,IBNEF,IBLTF,IBLEF,IBGTF,IBGEF,
		IBEQC,IBNEC,IBLTC,IBLEC,IBGTC,IBGEC,IBEQL,IBNEL,IBLTL,IBLEL,IBGTL,IBGEL,ISPAWN};
	Loader_Inst *li;
	Inst local[4];
	int i, target;

	for(i = 0; i < sizeof(ops)/sizeof(ops[0]); i++)for(target = 0; target < 4; target++){
		setup(); operation = 0;
		memmove(local, instructions, sizeof(local)); module.prog = local;
		local[0].op = ops[i]; local[0].add = SRC(AIMM)|DST(AIMM);
		local[0].d.ins = &local[target];
		invoke(); assert(result != H);
		li = (Loader_Inst*)result->data; assert(li->dst == target);
		destroy(result); result = H; checks++;
	}
}

/* Reject unsupported native-PC snapshots and absent type-table membership. */
static void
metadata(void)
{
	int op;

	for(op = 0; op < 2; op++){
		setup(); operation = op; module.compiled = 1; invoke();
		assert(result == H && allocations == 0 && !strcmp(message, "compiled module")); checks++;
	}
	setup(); operation = 1; exports[0].frame = (Type*)&module; invoke();
	assert(result == H && live == 0 && !strcmp(message, "bad export frame")); checks++;
	setup(); operation = 1; exports[0].frame = nil; exports[0].u.pc = nil; exports[0].name = ".mp"; invoke();
	assert(result != H);
	{ Loader_Link *ll = (Loader_Link*)result->data+3; assert(ll->pc == -1 && ll->tdesc == -1); }
	destroy(result); result = H; checks++;
	setup(); operation = 1; module.prog = nil; invoke();
	assert(result != H);
	for(op = 0; op < result->len; op++){
		Loader_Link *ll = (Loader_Link*)result->data+op;
		assert(ll->pc == -1 && ll->tdesc == -1);
	}
	destroy(result); result = H; checks++;
	for(op = 0; op < 3; op++){
		setup(); operation = op; module.nprog = module.ntype = 0; module.ext = nil; invoke();
		assert(result != H && result->len == 0); destroy(result); result = H;
		assert(live == 0); checks++;
	}
}

/* Impossible counts fail before pool allocation or table traversal. */
static void
bounds(void)
{
	int op, i, count[] = {-1, 0x7fffffff, 0x40000000};

	for(op = 0; op < 3; op += 2)for(i = 0; i < 3; i++){
		setup(); operation = op;
		if(op == 0)module.nprog = count[i]; else module.ntype = count[i];
		invoke(); assert(result == H && allocations == 0 && live == 0); checks++;
	}
}

/* Check every byte alignment, replacement publication, failure rollback and nil result slot.
 * No arguments; returns zero with a diagnostic if any owner, canary or error frame differs. */
static int
slots(void)
{
    static uchar bytes[40]; /* Stable fixture storage survives native error unwinds. */
    static uchar expected[40]; /* Independent expected image, including all outer canaries. */
    void *empty = H;
    Array *value;
    int op, offset, pass, ok;

    for(op = 0; op < 3; op++) {
        for(offset = 0; offset < 8; offset++) {
            for(pass = 0; pass < 2; pass++) {
                setup();
                operation = op;
                memset(bytes, 0xa5, sizeof(bytes));
                snapshot_slot = bytes + 8 + offset;
                slot_override = 1;
                memmove(snapshot_slot, &empty, sizeof(empty));
                ok = kprotect(dispatch, nil);
                memmove(&value, snapshot_slot, sizeof(value));
                if(!ok || value == H) {
                    fprintf(stderr, "Loader unaligned snapshot failed\n");
                    return 0;
                }
                replacement_old = value;
                replacement_slot = snapshot_slot;
                if(pass) {
                    failat = allocations + 1;
                }
                memcpy(expected, bytes, sizeof(bytes));
                kprotect(dispatch, nil);
                memmove(&value, snapshot_slot, sizeof(value));
                memmove(expected + 8 + offset, &value, sizeof(value));
                if(replacement_old != nil || (value == H) != pass ||
                   memcmp(bytes, expected, sizeof(bytes)) || worker.nerr) {
                    fprintf(stderr, "Loader snapshot replacement/canary/rollback failed\n");
                    return 0;
                }
                destroy(value);
                slot_override = 0;
                if(live || Tarray.ref != 1 || Tstring.ref != 1 || Tbyte.ref != 1) {
                    fprintf(stderr, "Loader slot test leaked a snapshot owner\n");
                    return 0;
                }
                checks++;
            }
        }
        for(pass = 0; pass < 2; pass++) {
            setup();
            operation = op;
            slot_override = 1;
            snapshot_slot = pass ? H : nil;
            ok = kprotect(dispatch, nil);
            slot_override = 0;
            if(ok || strcmp(message, exNilref) || allocations || live || worker.nerr) {
                fprintf(stderr, "Loader invalid slot bypassed preflight\n");
                return 0;
            }
            checks++;
        }
    }
    return 1;
}

/* Isolate baseline failures as well as exercising the full ownership matrix. */
int
main(void)
{
	char *only;

	only = getenv("LOADER_SNAPSHOT_CASES");
    if((only == nil || !strcmp(only, "slots")) && !slots()) {
        return 1;
    }
	if(only == nil || !strcmp(only, "values"))values();
	if(only == nil || !strcmp(only, "failures"))failures();
	if(only == nil || !strcmp(only, "replace"))replace();
	if(only == nil || !strcmp(only, "branches"))branches();
	if(only == nil || !strcmp(only, "metadata"))metadata();
	if(only == nil || !strcmp(only, "bounds"))bounds();
	assert(live == 0 && worker.nerr == 0);
	printf("Loader snapshots: %d cases, WORD=%zu bytes\n", checks, sizeof(WORD));
	return 0;
}
