#define LIMBO_NATIVE64_IMPLEMENTATIONS
#include "draw-native64.h"
#include "draw.h"
#include "nativeptrmap.h"
char exType[] = "bad type";
char exZdiv[] = "zero divide";

typedef struct DRef DRef;
typedef struct DDisplay DDisplay;
typedef struct DImage DImage;
typedef struct DScreen DScreen;
typedef struct DFont DFont;
WRAPPERS
GLOBALS
static int observations;

/* Non-geometry callbacks are identity witnesses; real geom.c supplies geometry calls. */
CALLBACKS

static void freedrawfont(Heap *heap, int swept) { USED(heap); USED(swept); }
static void freedrawimage(Heap *heap, int swept) { USED(heap); USED(swept); }
static void freedrawscreen(Heap *heap, int swept) { USED(heap); USED(swept); }
static void freedrawdisplay(Heap *heap, int swept) { USED(heap); USED(swept); }

/* Every allocator boundary must see the old descriptor publication state. */
static void drawobserve(void)
{
    check(TFont == nil && TImage == nil && TScreen == nil && TDisplay == nil);
    observations++;
}

INITIALIZER

static int initialize(void *context)
{
    USED(context);
    drawmodinit();
    return 1;
}

typedef struct GeometryCall GeometryCall;
struct GeometryCall { Link *link; void *frame; };

/* Enter a real published C export through the owning VM error boundary. */
static int geometrycall(void *context)
{
    GeometryCall *call = context;
    call->link->u.runt(call->frame);
    return 1;
}

static Link *geometrylink(Module *module, const char *name)
{
    int i;
    for(i = 0; i < module->ntype; i++)
        if(strcmp(module->ext[i].name, name) == 0) return module->ext+i;
    check(0);
    return nil;
}

/* Exercise real Frame64 arithmetic, unaligned/aliased results and zero-divide nonpublication. */
static void geometry(Module *module)
{
    static WORD values[] = {0, 1, -1, (-2147483647-1), 2147483647};
    F_Draw_Point_add add = {0};
    F_Draw_Point_sub sub = {0};
    F_Draw_Point_mul mul = {0};
    F_Draw_Point_div div = {0};
    F_Draw_Rect_clip clip = {0};
    struct { Draw_Rect t0; WORD t1; } clipped;
    Draw_Point result;
    uchar bytes[sizeof(clipped)+2];
    GeometryCall call;
    int i, j, k, count = 0;
    for(i = 0; i < nelem(values); i++)
        for(j = 0; j < nelem(values); j++) {
            add.p = (Draw_Point){values[i], values[j]};
            add.q = (Draw_Point){values[j], values[i]};
            memset(bytes, 0x5a, sizeof(bytes));
            add.ret = (void*)(bytes+1);
            call = (GeometryCall){geometrylink(module, "Point.add"), &add};
            check(kprotect(geometrycall, &call));
            memmove(&result, bytes+1, sizeof(result));
            check((UWORD)result.x == (UWORD)values[i]+(UWORD)values[j] &&
                  (UWORD)result.y == (UWORD)values[j]+(UWORD)values[i]);
            check(bytes[0] == 0x5a && bytes[1+sizeof(result)] == 0x5a);
            sub.p = add.p; sub.q = add.q; sub.ret = &sub.p;
            call = (GeometryCall){geometrylink(module, "Point.sub"), &sub};
            check(kprotect(geometrycall, &call));
            check((UWORD)sub.p.x == (UWORD)values[i]-(UWORD)values[j] &&
                  (UWORD)sub.p.y == (UWORD)values[j]-(UWORD)values[i]);
            count += 2;
            for(k = 0; k < nelem(values); k++) {
                mul.p = add.p; mul.i = values[k]; mul.ret = &result;
                call = (GeometryCall){geometrylink(module, "Point.mul"), &mul};
                check(kprotect(geometrycall, &call));
                check((UWORD)result.x == (UWORD)values[i]*(UWORD)values[k] &&
                      (UWORD)result.y == (UWORD)values[j]*(UWORD)values[k]);
                div.p = add.p; div.i = values[k]; div.ret = &result;
                result = (Draw_Point){71, 93};
                call = (GeometryCall){geometrylink(module, "Point.div"), &div};
                check(kprotect(geometrycall, &call) == (values[k] != 0));
                if(values[k] == 0) check(result.x == 71 && result.y == 93);
                else {
                    check(result.x == (values[k] == -1 ? (WORD)((UWORD)0-(UWORD)values[i]) : values[i]/values[k]));
                    check(result.y == (values[k] == -1 ? (WORD)((UWORD)0-(UWORD)values[j]) : values[j]/values[k]));
                }
                count += 2;
            }
        }
    check(sizeof(clipped) == sizeof(*clip.ret));
    clip.r = (Draw_Rect){{0,0},{10,20}};
    clip.s = (Draw_Rect){{5,-2},{15,4}};
    memset(bytes, 0x5a, sizeof(bytes)); clip.ret = (void*)(bytes+1);
    call = (GeometryCall){geometrylink(module, "Rect.clip"), &clip};
    check(kprotect(geometrycall, &call));
    memmove(&clipped, bytes+1, sizeof(clipped));
    check(clipped.t0.min.x == 5 && clipped.t0.min.y == 0 &&
          clipped.t0.max.x == 10 && clipped.t0.max.y == 4 && clipped.t1 == 1);
    check(bytes[0] == 0x5a && bytes[sizeof(bytes)-1] == 0x5a);
    printf("PASS native64 Draw geometry: %d published callbacks, wrap/zero-divide and unaligned/aliased results\n", count+1);
}

/* Bind every generated method, then sweep failures across wrapper and module construction. */
int main(void)
{
    Module *module;
    int i, allocations, before;
    baseline.ref = 7;
    check(kprotect(initialize, nil));
    allocations = attempts;
    module = modules;
    check(module != &baseline && module->ntype == 89 && module->ext[89].name == nil);
    check(TFont != nil && TImage != nil && TScreen != nil && TDisplay != nil);
    check(TFont->size == sizeof(DFont) && TFont->free == freedrawfont);
    check(TImage->size == sizeof(DImage) && TImage->free == freedrawimage);
    check(TScreen->size == sizeof(DScreen) && TScreen->free == freedrawscreen);
    check(TDisplay->size == sizeof(DDisplay) && TDisplay->free == freedrawdisplay);
    check(memcmp(TFont->map, fontmap, sizeof(fontmap)) == 0);
    check(memcmp(TImage->map, imagemap, sizeof(imagemap)) == 0);
    check(memcmp(TScreen->map, screenmap, sizeof(screenmap)) == 0);
    check(memcmp(TDisplay->map, displaymap, sizeof(displaymap)) == 0);
    for(i = 0; i < 89; i++) {
        const LimboNative64Entry *entry = limbo_native64_Draw_implementations[i];
        check(strcmp(module->ext[i].name, entry->name) == 0);
        check(module->ext[i].sig == entry->legacy_signature && module->ext[i].u.runt == entry->function);
        check(module->type[i]->size == entry->frame_size && module->type[i]->np == entry->map_bytes);
        check(memcmp(module->type[i]->map, entry->map, entry->map_bytes) == 0);
    }
    geometry(module);
    before = attempts;
    check(kprotect(initialize, nil) && attempts == before);
    freetype(TFont); freetype(TImage); freetype(TScreen); freetype(TDisplay);
    TFont = TImage = TScreen = TDisplay = nil;
    unload(module);
    check(modules == &baseline && live == 0 && worker.nerr == 0);
    for(i = 0; i < allocations; i++) {
        fail_after = i;
        check(!kprotect(initialize, nil));
        fail_after = -1;
        check(TFont == nil && TImage == nil && TScreen == nil && TDisplay == nil);
        check(modules == &baseline && live == 0 && worker.nerr == 0 && diagnostic[0] != 0);
    }
    printf("PASS native64 Draw registration: 89 generated exports, four mapped wrappers, idempotence, %d allocation refusals, %d staging observations; window/device APIs not executed\n",
           allocations, observations);
    return 0;
}
