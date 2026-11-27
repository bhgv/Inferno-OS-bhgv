#define LIMBO_NATIVE64_IMPLEMENTATIONS
#include "sys-native64.h"
#include "native64-sys-fixed.h"

char exBounds[] = "bounds", exType[] = "type", exNilref[] = "nil";
Type Tarray = {.ref = 1}, Tbyte = {.ref = 1, .size = 1};
SYS_DESCRIPTORS
static int observations;

/* Unexecuted OS callbacks are identity witnesses; real UTF bodies are supplied below. */
CALLBACKS

void freeFD(Heap *heap, int swept) { USED(heap); USED(swept); abort(); }
void freeFileIO(Heap *heap, int swept) { USED(heap); USED(swept); abort(); }

/* Neither the descriptor set nor the module may be visible at an allocation boundary. */
static void sysobserve(void)
{
    check(TFD == nil && TFileIO == nil && FioTread == nil && FioTwrite == nil && Tdir == nil);
    check(modules == &baseline);
    observations++;
}

SYS_INITIALIZERS
SYS_REGISTRATION
SYS_UTF

static int initialize(void *context)
{
    USED(context);
    sysmodinit();
    return 1;
}

typedef struct SysCall SysCall;
struct SysCall { Link *link; void *frame; };

/* Invoke the published callback through the same protected VM error helper. */
static int invoke(void *context)
{
    SysCall *call = context;
    call->link->u.runt(call->frame);
    return 1;
}

static Link *find(Module *module, const char *name)
{
    int i;
    for(i = 0; i < module->ntype; i++)
        if(strcmp(module->ext[i].name, name) == 0) return module->ext+i;
    return nil;
}

/* Real UTF callbacks exercise generated frames, tuple results and byte-array bounds. */
static void utf(Module *module)
{
    static WORD runes[] = {0, 65, 0x416, 0x20ac, 0x1f600, 0x10ffff, 0xd800, -1, 0x110000};
    struct { Heap heap; Array array; } input = {0};
    uchar data[6], result[16];
    WORD encoded, decoded[3], nbytes;
    F_Sys_char2byte encode = {0};
    F_Sys_byte2char decode = {0};
    F_Sys_utfbytes extent = {0};
    SysCall call;
    int i, j, calls = 0;

    check(offsetof(typeof(input), array) == sizeof(Heap));
    input.heap.t = &Tarray; input.heap.ref = 1;
    input.array.t = &Tbyte; input.array.data = data+1;
    encode.buf = decode.buf = extent.buf = &input.array;
    encode.ret = &encoded; decode.ret = (void*)(result+1); extent.ret = &nbytes;
    for(i = 0; i < nelem(runes); i++) {
        input.array.len = 4; memset(data, 0x5a, sizeof(data));
        encode.c = runes[i];
        call = (SysCall){find(module, "char2byte"), &encode};
        check(kprotect(invoke, &call)); calls++;
        check(encoded >= 1 && encoded <= 4 && data[0] == 0x5a && data[5] == 0x5a);
        input.array.len = encoded; memset(result, 0x5a, sizeof(result));
        call = (SysCall){find(module, "byte2char"), &decode};
        check(kprotect(invoke, &call)); calls++;
        memmove(decoded, result+1, sizeof(decoded));
        check(decoded[0] == (runes[i] < 0 || runes[i] > 0x10ffff ||
              (runes[i] >= 0xd800 && runes[i] <= 0xdfff) ? 0xfffd : runes[i]));
        check(decoded[1] == encoded && decoded[2] == 1 && result[0] == 0x5a && result[13] == 0x5a);
        for(j = 0; j <= encoded; j++) {
            extent.n = j;
            call = (SysCall){find(module, "utfbytes"), &extent};
            check(kprotect(invoke, &call)); calls++;
            check(nbytes == (j == encoded ? encoded : 0));
        }
        extent.n = encoded+1; nbytes = 193;
        check(!kprotect(invoke, &call) && nbytes == 193 && worker.nerr == 0); calls++;
    }
    printf("PASS native64 Sys fixed UTF: %d published callbacks, Unicode/tuple/unaligned results and bounds nonpublication\n", calls);
}

/* Check the complete fixed set and every constructor failure; variadic exports stay absent. */
int main(void)
{
    Type *types[5], borrowed[5] = {{0}};
    Module *module, foreign = {0};
    int i, allocations, before, mask;
    baseline.ref = 7;
    check(kprotect(initialize, nil));
    allocations = attempts;
    module = modules;
    check(sysnative64typesready() && module != &baseline && module->ntype == 39 && module->ext[39].name == nil);
    check(find(module, "print") == nil && find(module, "fprint") == nil && find(module, "sprint") == nil && find(module, "aprint") == nil);
    check(FioTread->size == 24 && FioTread->map[0] == 0x20);
    check(FioTwrite->size == 32 && FioTwrite->map[0] == 0x50);
    check(Tdir->size == 80 && Tdir->np == 2 && Tdir->map[0] == 0xf0 && Tdir->map[1] == 0);
    for(i = 0; i < 39; i++) {
        const LimboNative64Entry *entry = limbo_native64_Sys_implementations[i];
        check(strcmp(module->ext[i].name, entry->name) == 0);
        check(module->ext[i].sig == entry->legacy_signature && module->ext[i].u.runt == entry->function);
        check(module->type[i]->size == entry->frame_size && module->type[i]->np == entry->map_bytes);
        check(memcmp(module->type[i]->map, entry->map, entry->map_bytes) == 0);
    }
    utf(module);
    before = attempts;
    check(kprotect(initialize, nil) && attempts == before);
    types[0] = TFD; types[1] = TFileIO; types[2] = FioTread; types[3] = FioTwrite; types[4] = Tdir;
    TFD = TFileIO = FioTread = FioTwrite = Tdir = nil;
    for(i = 0; i < 5; i++) freetype(types[i]);
    unload(module);
    check(modules == &baseline && live == 0);
    for(i = 0; i < allocations; i++) {
        fail_after = i;
        check(!kprotect(initialize, nil));
        fail_after = -1;
        check(TFD == nil && TFileIO == nil && FioTread == nil && FioTwrite == nil && Tdir == nil);
        check(modules == &baseline && live == 0 && worker.nerr == 0);
    }
    before = attempts;
    foreign.path = "$Sys"; foreign.link = &baseline; modules = &foreign;
    check(!kprotect(initialize, nil) && attempts == before && modules == &foreign && live == 0);
    modules = &baseline;
    for(mask = 1; mask < 32; mask++) {
        TFD = mask&1 ? borrowed : nil; TFileIO = mask&2 ? borrowed+1 : nil;
        FioTread = mask&4 ? borrowed+2 : nil; FioTwrite = mask&8 ? borrowed+3 : nil;
        Tdir = mask&16 ? borrowed+4 : nil;
        check(!kprotect(initialize, nil) && attempts == before && live == 0 && modules == &baseline);
        check(TFD == (mask&1 ? borrowed : nil) && TFileIO == (mask&2 ? borrowed+1 : nil));
        check(FioTread == (mask&4 ? borrowed+2 : nil) && FioTwrite == (mask&8 ? borrowed+3 : nil) && Tdir == (mask&16 ? borrowed+4 : nil));
        TFD = TFileIO = FioTread = FioTwrite = Tdir = nil;
    }
    printf("PASS native64 Sys fixed registration: 39 exports, five staged descriptors, %d allocation refusals, 32 preexisting-state refusals, %d observations; OS I/O callbacks not executed\n",
           allocations, observations);
    return 0;
}
