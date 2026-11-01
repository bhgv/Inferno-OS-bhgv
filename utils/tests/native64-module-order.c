/* Independent FlashVM review oracle for request order, alias ownership and arbitrary retirement order. */
#ifndef NATIVE64_REFUSED


/* Exercise all request/unload permutations with and without a pre-existing registry owner.
 * The fixture owns all input storage; successful output cells borrow module owners.
 * No concurrency, initializer rollback or file-call admission is implied.
 */
static void
moduleorder(void)
{
    static const unsigned int orders[6][3] = {
        {0, 1, 2}, {0, 2, 1}, {1, 0, 2},
        {1, 2, 0}, {2, 0, 1}, {2, 1, 0}
    };
    const LimboNative64Entry *entries[] = {&limbo_native64_Probe_add_entry};
    Native64ModuleSpec specs[3];
    Module *output[3];
    Module *previous;
    Module *cursor;
    char names[3][16] = {"$OrderA", "$OrderB", "$OrderC"};
    unsigned int alive[3];
    unsigned int outer;
    unsigned int request;
    unsigned int retire;
    unsigned int step;
    unsigned int i;
    unsigned int removed;
    unsigned int cases;
    int before;
    F_Probe_add frame;
    WORD result;

    cases = 0;
    for(outer = 0; outer < 2; outer++){
        for(request = 0; request < 6; request++){
            for(retire = 0; retire < 6; retire++){
                previous = outer ? builtinmod64("$Older", limbo_native64_Probe_specs, 2, entries, 1) : nil;
                check(!outer || previous != nil);
                for(i = 0; i < 3; i++){
                    specs[i].name = names[orders[request][i]];
                    specs[i].specs = limbo_native64_Probe_specs;
                    specs[i].count = 2;
                    specs[i].entries = entries;
                    specs[i].selected = 1;
                    output[i] = nil;
                    alive[i] = 1;
                }
                check(builtinmods64(specs, 3, output));
                /* The zero-size transaction must neither inspect spans nor touch an existing chain. */
                before = attempts;
                cursor = modules;
                check(builtinmods64((Native64ModuleSpec*)1, 0, (Module**)1));
                check(attempts == before && modules == cursor);
                for(step = 0; step <= 3; step++){
                    cursor = modules;
                    for(i = 3; i != 0; i--){
                        if(alive[i-1]){
                            check(cursor == output[i-1]);
                            check(cursor->ref == 1);
                            check(strcmp(cursor->name, specs[i-1].name) == 0);
                            memset(&frame, 0, sizeof(frame));
                            result = 0;
                            frame.ret = &result;
                            frame.x = 17;
                            frame.y = -9;
                            cursor->ext[0].u.runt(&frame);
                            check(result == 8);
                            cursor = cursor->link;
                        }
                    }
                    check(cursor == (previous != nil ? previous : &baseline));
                    check(previous == nil || (previous->ref == 1 && previous->link == &baseline));
                    if(step != 3){
                        removed = orders[retire][step];
                        unload(output[removed]);
                        alive[removed] = 0;
                        /* A borrowed output is invalid after unload: clear it before any later observation. */
                        output[removed] = nil;
                    }
                }
                if(previous != nil){
                    unload(previous);
                }
                balanced();
                cases++;
            }
        }
    }
    check(cases == 72);
    printf("PASS independent module order: %u request/retirement permutations, aliases, surviving calls and empty transactions\n", cases);
}
#endif


/* Keep all owning upstream failure/removal controls and add the independent ordering oracle. */
int
main(void)
{
    native64_set_main();
#ifndef NATIVE64_REFUSED
    moduleorder();
#endif
    return 0;
}
