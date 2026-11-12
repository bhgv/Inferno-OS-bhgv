/* Own serialized String roots through actual constructors and the production collector. */
#include "literal-images.h"
char exNegsize[] = "negative size";

/* The fixture uses fixed arena blocks; expose their real payload extent. */
int
hmsize(void *heap)
{
    return numericowner((Heap*)heap)->size-O(Bhdr,u.data);
}

/* Execute unseeded serialized calls with collection after every VM quantum.
 * The final String (or list head) must be the original owned MP literal. */
static void
managedliteralexecute(const ManagedLiteralCase *test, int jit)
{
    ManagedImageCase fixture = {0};
    Prog process = {0};
    Modlink *caller, *loaded;
    Type *frame;
    Frame *entry;
    String *literal, *answer;
    void *result;
    Inst *pc;
    int step;

    require(modules == nil);
    fixture.consumer = test->consumer; fixture.consumerbytes = test->consumerbytes;
    fixture.provider = test->provider; fixture.providerbytes = test->providerbytes;
    managednamespace = &fixture; managedreplacement = nil;
    cacheopens = cachecloses = cachereads = cacheparses = cachechecks = 0;
    cachereleases = cacheacquires = cachefd = cacheoffset = cachekind = 0;
    dis64bootmanaged = 1;
    dis64bootprovider = "/managed-provider.dis64";
    cflag = jit;
    caller = prepare64managedtrial("literal-execution",test->consumer,test->consumerbytes,
        dis64bootprovider,test->provider,test->providerbytes,jit);
    require(caller != nil);
    memmove(&literal,caller->MP+24,sizeof(literal));
    frame = caller->m->entryt;
    caller->m->ss = 4096;
    process.exval = H; process.state = Pready; process.quanta = 1;
    process.R.M = caller; process.R.PC = caller->prog;
    entry = mallocz(frame->size,1);
    require(entry != nil);
    entry->t = frame; R.s = &entry;
    newstack(&process); free(entry); initmem(frame,process.R.FP);
    ptrdel(D2H(caller));
    managedprocess = &process;
    for(step=0; step<1024; step++){
        pc = process.R.PC;
        if(process.R.M == caller && pc == caller->prog+caller->m->nprog-1) break;
        require(kprotect(execute,&process));
        if(pc->op == ILOAD && jit){
            memmove(&loaded,caller->MP,sizeof(loaded));
            require(loaded != nil && loaded != H && loaded->m->jit != nil);
            rxbegin = (uintptr)loaded->m->jit->code;
            rxend = rxbegin+loaded->m->jit->size;
        }
        numericcollect(&process);
        require(numericowner(D2H(literal))->magic == MAGIC_A);
    }
    require(step < 1024 && cachefd == 0 && cacheopens == cachecloses && cachereleases == cacheacquires);
    memmove(&result,process.R.FP+72,sizeof(result));
    require(result != nil && result != H);
    answer = test->listresult ? managedhead((List*)result) : (String*)result;
    require(answer == literal);
    require(!kprotect(execute,&process) && message[0] == 0);
    destroystack(&R); numericdrain();
    require(modules == nil && Tstring.ref == 1 && Tlist.ref == 1 && Tptr.ref == 1 && Tmodlink.ref == 1);
    managedprocess = nil; managednamespace = nil;
    dis64bootmanaged = 0; dis64bootprovider = nil; cflag = 0;
}

/* Verify Rune32 payloads, GC reachability and every partial-construction failure.
 * Images and expected values are independently generated; no input is seeded. */
static void
managedliteralcontrol(void)
{
    const ManagedLiteralCase *test;
    Modlink *caller;
    String *text;
    unsigned int i, j, value;
    int before, first, attempts, failure, stringrefs, refusals = 0;

    dis64bootmanaged = 1;
    dis64bootprovider = "/managed-provider.dis64";
    for(i=0; i<nelem(literal_cases); i++){
        test = literal_cases+i;
        numericdrain();
        require(modules == nil);
        before = live;
        stringrefs = Tstring.ref;
        first = allocations;
        caller = prepare64managedtrial("literal-caller",test->consumer,test->consumerbytes,
            dis64bootprovider,test->provider,test->providerbytes,0);
        require(caller != nil);
        attempts = allocations-first;
        memmove(&text,caller->MP+24,sizeof(text));
        if(text == nil || text == H || text->len != -(int)test->count){
            fprintf(stderr,"literal root publication predicate\n");
            exit(10);
        }
        for(j=0; j<test->count; j++){
            memmove(&value,text->Srune+j,sizeof(value));
            require(value == test->values[j]);
        }
        numericcollect(nil);
        require(numericowner(D2H(text))->magic == MAGIC_A);
        for(j=0; j<test->count; j++){
            memmove(&value,text->Srune+j,sizeof(value));
            require(value == test->values[j]);
        }
        ptrdel(D2H(caller));
        destroy(caller);
        numericdrain();
        require(live == before && modules == nil && Tstring.ref == stringrefs);
        for(failure=1; failure<=attempts; failure++){
            failat = allocations+failure;
            require(prepare64managedtrial("literal-caller",test->consumer,test->consumerbytes,
                dis64bootprovider,test->provider,test->providerbytes,0) == nil);
            failat = 0;
            numericdrain();
            require(live == before && modules == nil && worker.nerr == 0 && Tstring.ref == stringrefs);
        }
        refusals += attempts;
    }
    for(i=0; i<nelem(literal_cases); i++){
        managedliteralexecute(literal_cases+i,0);
        managedliteralexecute(literal_cases+i,1);
    }
    dis64bootmanaged = 0;
    printf("PASS unseeded literal execution: 32 C/JIT/GC cases, exact final pointer identity\n");
    printf("PASS serialized String literals: %u exact Rune32/GC roots, %d allocation rollback refusals\n",(unsigned int)nelem(literal_cases),refusals);
}
