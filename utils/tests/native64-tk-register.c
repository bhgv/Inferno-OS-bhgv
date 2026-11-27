#define LIMBO_NATIVE64_IMPLEMENTATIONS
#include "tk-native64.h"
#include "draw.h"
#include "tk.h"

Type *fakeTkTop;
static uchar TktypeMap[] = Tk_Toplevel_map;
static int formats, sorts, observations;

/* These callbacks are identity witnesses only; no GUI API is executed by this fixture. */
CALLBACKS

void tkfreetop(Heap *heap, int swept) { USED(heap); USED(swept); }
static void tkmarktop(Type *type, void *value) { USED(type); USED(value); }
int tkeventfmt(Fmt *format) { USED(format); return 0; }

/* No callback installation may run while the module or its top descriptor is partial. */
int fmtinstall(int character, int (*function)(Fmt*))
{
    check(character == 'v' && function == tkeventfmt && fakeTkTop != nil);
    check(modules != &baseline && strcmp(modules->path, "$Tk") == 0);
    formats++;
    return 0;
}

void tksorttable(void)
{
    check(fakeTkTop != nil && formats == 1);
    sorts++;
}

/* Observe every actual descriptor/module allocation before its publication boundary. */
static void tkobserve(void)
{
    check(fakeTkTop == nil && formats == 0 && sorts == 0);
    observations++;
}

INITIALIZER

static int initialize(void *context)
{
    USED(context);
    tkmodinit();
    return 1;
}

/* Verify all generated links, exact native maps, idempotence and every allocation refusal. */
int main(void)
{
    Module *module;
    int i, allocations, before;
    baseline.ref = 7;
    check(kprotect(initialize, nil));
    allocations = attempts;
    module = modules;
    check(module != &baseline && module->ntype == 10 && module->ext[10].name == nil);
    check(fakeTkTop != nil && fakeTkTop->size == sizeof(TkTop));
    check(fakeTkTop->free == tkfreetop && fakeTkTop->mark == tkmarktop);
    check(fakeTkTop->np == sizeof(TktypeMap) && fakeTkTop->map[0] == 0xf0);
    check(offsetof(TkTop, dd) == 0 && offsetof(TkTop, wreq) == 8 &&
          offsetof(TkTop, di) == 16 && offsetof(TkTop, wmctxt) == 24 &&
          offsetof(TkTop, screenr) == 32 && offsetof(TkTop, ctxt) == 48);
    for(i = 0; i < 10; i++) {
        const LimboNative64Entry *entry = limbo_native64_Tk_implementations[i];
        check(strcmp(module->ext[i].name, entry->name) == 0);
        check(module->ext[i].sig == entry->legacy_signature && module->ext[i].u.runt == entry->function);
        check(module->type[i]->size == entry->frame_size && module->type[i]->np == entry->map_bytes);
        check(memcmp(module->type[i]->map, entry->map, entry->map_bytes) == 0);
    }
    before = attempts;
    check(kprotect(initialize, nil) && attempts == before && formats == 1 && sorts == 1);
    freetype(fakeTkTop); fakeTkTop = nil;
    unload(module);
    check(modules == &baseline && live == 0 && worker.nerr == 0);
    formats = sorts = 0;
    for(i = 0; i < allocations; i++) {
        fail_after = i;
        check(!kprotect(initialize, nil));
        fail_after = -1;
        check(fakeTkTop == nil && modules == &baseline && live == 0 && worker.nerr == 0);
        check(formats == 0 && sorts == 0 && diagnostic[0] != 0);
    }
    printf("PASS native64 Tk registration: 10 generated exports, public-prefix maps, idempotence, %d allocation refusals, %d staging observations; GUI APIs not executed\n",
           allocations, observations);
    return 0;
}
