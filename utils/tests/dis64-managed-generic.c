/* Check actual startup seeding from structural metadata, independent of user names. */
#include "named-seeds.h"

static void
managedgenericcontrol(void)
{
    const NamedSeedCase *test;
    Modlink *caller;
    SchedModule startup;
    String *text;
    unsigned int i;
    int before;

    dis64bootmanaged = 1;
    dis64bootprovider = "/managed-provider.dis64";
    dis64bootmanagedinput = "seed";
    for(i=0; i<nelem(named_seeds); i++){
        test = named_seeds+i;
        numericdrain(); before = live;
        caller = prepare64managedtrial("named-seed",test->consumer,test->consumerbytes,
            dis64bootprovider,test->provider,test->providerbytes,0);
        require(caller != nil && strcmp(caller->m->ldt[0][0].name,test->name) == 0);
        memset(&startup,0,sizeof(startup)); startup.m=caller->m; startup.ml=caller;
        schedpairseed(&startup);
        require(startup.input != nil && startup.input != H && startup.inputroot);
        if(D2H(startup.input)->t != (test->islist ? &Tlist : &Tstring)){
            fprintf(stderr,"named seed class predicate\n");
            exit(11);
        }
        text = test->islist ? managedhead((List*)startup.input) : (String*)startup.input;
        require(text != nil && text != H && text->len == 4 && memcmp(text->Sascii,"seed",4) == 0);
        numericcollect(nil);
        require(numericowner(D2H(startup.input))->magic == MAGIC_A && numericowner(D2H(text))->magic == MAGIC_A);
        ptrdel(D2H(startup.input)); destroy(startup.input);
        ptrdel(D2H(caller)); destroy(caller); numericdrain();
        require(live == before && modules == nil && Tstring.ref == 1 && Tlist.ref == 1 && Tptr.ref == 1 && worker.nerr == 0);
    }
    dis64bootmanagedinput = nil; dis64bootmanaged = 0; dis64bootprovider = nil;
    printf("PASS named startup seeds: exact String/List kinds and GC roots for arbitrary/reversed export names\n");
}
