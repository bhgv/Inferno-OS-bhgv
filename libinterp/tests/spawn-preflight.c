
static uchar *spawncode[2]; // Owned complete-entry RX mappings for SPAWN/MSPAWN.
static int spawnsize[2]; // Exact byte extents used at allocation and release.
static int spawnroute; // Direct C or the real complete native tail.
static Inst *spawntarget; // Source target cell, including invalid-pointer probes.


/* Invoke real handlers without replacing their frame allocation or publication logic. */
static int
spawninvoke(void *unused)
{
    (void)unused;
    if(spawnroute)spawncheck(((int(*)(REG*))spawncode[operation-3])(&R)==1);
    else if(operation==3)test_spawn();
    else test_mspawn();
    return 1;
}


/* Reject before stack allocation, owner acquisition or scheduler publication. */
static void
spawnreject(char *wanted)
{
    static REG before;
    static uchar frame[512];
    int baseline=live;

    memmove(&before,&R,sizeof(R));memmove(frame,framebuf.bytes,sizeof(frame));
    spawncheck(!kprotect(spawninvoke,nil));
    spawncheck(raised==wanted && allocations==0 && unframes==0 && created==nil && live==baseline);
    spawncheck(!memcmp(&before,&R,sizeof(R)) && !memcmp(frame,framebuf.bytes,sizeof(frame)));
    spawncheck(D2H(ml)->ref==1 && worker.nerr==0);
    destroy(ml);ml=nil;spawncheck(live==0);checks++;
}


/* Prepare ordinary source frames and native module owners for independent failures. */
static void
spawnimage(int imported)
{
    setup();
    operation=imported ? 4 : 3;
    operand=0;
    spawntarget=code;
    if(imported){R.d=&ml;R.m=&operand;}
    else R.d=&spawntarget;
}


/* Check complete operand cells before the first borrowed native-pointer read. */
static void
spawnslots(void)
{
    int imported,route,field,offset;
    void *bad;

    for(imported=0;imported<2;imported++)for(route=0;route<2;route++)
    for(field=0;field<2;field++)for(offset=0;offset<=sizeof(void*);offset++){
        if(!imported && field==1)continue;
        if(field==1 && offset>sizeof(WORD))continue;
        if(getenv("SPAWN_IMPORTED") && imported!=atoi(getenv("SPAWN_IMPORTED")))continue;
        if(getenv("SPAWN_ROUTE") && route!=atoi(getenv("SPAWN_ROUTE")))continue;
        spawnimage(imported);spawnroute=route;
        bad=offset==0 ? nil : (void*)((uintptr)0-(uintptr)offset);
        if(field==0)R.d=bad;else R.m=bad;
        spawnreject(exBounds);
    }
}


/* Check nil, sentinel and wrapping target PCs before creating a child. */
static void
spawntargets(void)
{
    int imported,route,offset;
    Inst *bad;

    for(imported=0;imported<2;imported++)for(route=0;route<2;route++)
    for(offset=0;offset<=sizeof(Inst);offset++){
        if(getenv("SPAWN_IMPORTED") && imported!=atoi(getenv("SPAWN_IMPORTED")))continue;
        if(getenv("SPAWN_ROUTE") && route!=atoi(getenv("SPAWN_ROUTE")))continue;
        spawnimage(imported);spawnroute=route;
        bad=offset==0 ? nil : (Inst*)((uintptr)0-(uintptr)offset);
        if(imported)ml->links[0].u.pc=bad;else spawntarget=bad;
        spawnreject(exSpawn);
    }
    for(route=0;route<2;route++){
        if(getenv("SPAWN_IMPORTED") && atoi(getenv("SPAWN_IMPORTED"))!=1)continue;
        if(getenv("SPAWN_ROUTE") && route!=atoi(getenv("SPAWN_ROUTE")))continue;
        spawnimage(1);spawnroute=route;ml->prog=H;spawnreject(exSpawn);
    }
}


/* Bound native module prefixes before reading entry points or stack-size hints. */
static void
spawnowners(void)
{
    int imported,route,offset;
    static Modlink *badlink;

    for(imported=0;imported<2;imported++)for(route=0;route<2;route++)
    for(offset=1;offset<=sizeof(Modlink);offset++){
        if(getenv("SPAWN_IMPORTED") && imported!=atoi(getenv("SPAWN_IMPORTED")))continue;
        if(getenv("SPAWN_ROUTE") && route!=atoi(getenv("SPAWN_ROUTE")))continue;
        spawnimage(imported);spawnroute=route;
        badlink=(Modlink*)((uintptr)0-(uintptr)offset);
        if(imported)R.d=&badlink;else R.M=badlink;
        spawnreject(exModule);
    }
    for(imported=0;imported<2;imported++)for(route=0;route<2;route++)for(offset=1;offset<sizeof(void*);offset++){
        static union {
            uvlong align; // Native alignment before shifting the borrowed link image.
            uchar bytes[sizeof(Modlink)+8]; // Complete misaligned Modlink prefixes.
        } image;
        if(getenv("SPAWN_IMPORTED") && imported!=atoi(getenv("SPAWN_IMPORTED")))continue;
        if(getenv("SPAWN_ROUTE") && route!=atoi(getenv("SPAWN_ROUTE")))continue;
        spawnimage(imported);spawnroute=route;memmove(image.bytes+offset,ml,sizeof(*ml));
        badlink=(Modlink*)(image.bytes+offset);
        if(imported)R.d=&badlink;else R.M=badlink;
        spawnreject(exModule);
    }
    for(route=0;route<2;route++)for(offset=1;offset<=sizeof(Module);offset++){
        if(getenv("SPAWN_IMPORTED") && atoi(getenv("SPAWN_IMPORTED"))!=1)continue;
        if(getenv("SPAWN_ROUTE") && route!=atoi(getenv("SPAWN_ROUTE")))continue;
        spawnimage(1);spawnroute=route;ml->m=(Module*)((uintptr)0-(uintptr)offset);
        /* Restore the separately retained owner before the fixture destructor runs. */
        {
            Module *bad=ml->m;
            static REG before;
            int baseline=live;

            memmove(&before,&R,sizeof(R));
            spawncheck(!kprotect(spawninvoke,nil));
            spawncheck(raised==exModule && ml->m==bad && allocations==0 && live==baseline && created==nil);
            spawncheck(!memcmp(&before,&R,sizeof(R)) && worker.nerr==0);
            ml->m=&module;destroy(ml);ml=nil;spawncheck(live==0);checks++;
        }
    }
}


/* Legacy compiled targets are native-PC tokens, not complete Inst records. */
static void
spawnlegacytarget(void)
{
    int imported,route;
    Inst *nativepc=(Inst*)((uintptr)-2);

    for(imported=0;imported<2;imported++)for(route=0;route<2;route++){
        if(getenv("SPAWN_IMPORTED") && imported!=atoi(getenv("SPAWN_IMPORTED")))continue;
        if(getenv("SPAWN_ROUTE") && route!=atoi(getenv("SPAWN_ROUTE")))continue;
        spawnimage(imported);spawnroute=route;ml->compiled=1;
        if(imported)ml->links[0].u.pc=nativepc;else spawntarget=nativepc;
        spawncheck(kprotect(spawninvoke,nil) && created!=nil && created->R.PC==nativepc && unframes==1);
        discard(created);destroy(ml);ml=nil;spawncheck(live==0 && worker.nerr==0);checks++;
    }
}


/* Cover both emulator/kernel error boundaries and release all complete native tails. */
int
main(void)
{
    int i,op,n;
    char *family=getenv("SPAWN_PREFLIGHT_CASE");

    if(getenv("STACK_CASES") && !strcmp(getenv("STACK_CASES"),"pid"))return spawnlegacy();
    optab[ISPAWN]=test_spawn;optab[IMSPAWN]=test_mspawn;
    for(i=0;i<2;i++){
        op=i ? IMSPAWN : ISPAWN;
        n=leafbridgeemit(nil,LeafMax,(void*)cjitexec,op,nil);
        spawncheck(n>0);spawnsize[i]=n*sizeof(u32int);spawncode[i]=leafcodealloc(spawnsize[i]);
        spawncheck(spawncode[i]!=nil && leafbridgeemit((u32int*)spawncode[i],n,(void*)cjitexec,op,nil)==n);
        spawncheck(leafcodepublish(spawncode[i],spawnsize[i])==0);
    }
    if(family==nil || !strcmp(family,"slots"))spawnslots();
    if(family==nil || !strcmp(family,"targets"))spawntargets();
    if(family==nil || !strcmp(family,"owners"))spawnowners();
    if(family!=nil && !strcmp(family,"legacy"))spawnlegacytarget();
    for(i=0;i<2;i++)leafcodefree(spawncode[i],spawnsize[i]);
    spawncheck(checks>0 && live==0 && worker.nerr==0);
    printf("PASS spawn preflight WORD%zu: %d C/RX %s cases\n",sizeof(WORD)*8,checks,
        family && !strcmp(family,"legacy") ? "native-PC construction" : "pre-publication rejection");
    return 0;
}
