/* Allocate ordinary publication metadata without changing the VM heap fixture. */
void*
mallocz(ulong size, int clear)
{
	void *p;

	p = malloc(size);
	if(p != nil && clear)
		memset(p, 0, size);
	return p;
}


/* Exercise the production compiler's normal executable publication boundary. */
int
kprotect(int (*fn)(void*), void *arg)
{
	return fn(arg);
}

int mutator, nprop;


/* Select independently enumerated IEEE encodings whose %g spelling is a plain integer. */
static int
nativevalue(uvlong bits)
{
    static uvlong values[] = {
        0x0ULL, 0x3ff0000000000000ULL, 0xbff0000000000000ULL, 0x4000000000000000ULL,
        0xc000000000000000ULL, 0x4010000000000000ULL, 0xc010000000000000ULL, 0x4020000000000000ULL,
        0xc020000000000000ULL, 0x4030000000000000ULL, 0xc030000000000000ULL, 0x4040000000000000ULL,
        0xc040000000000000ULL, 0x4050000000000000ULL, 0xc050000000000000ULL, 0x4060000000000000ULL,
        0xc060000000000000ULL, 0x4070000000000000ULL, 0xc070000000000000ULL, 0x4080000000000000ULL,
        0xc080000000000000ULL, 0x4090000000000000ULL, 0xc090000000000000ULL, 0x40a0000000000000ULL,
        0xc0a0000000000000ULL, 0x40b0000000000000ULL, 0xc0b0000000000000ULL, 0x40c0000000000000ULL,
        0xc0c0000000000000ULL, 0x40d0000000000000ULL, 0xc0d0000000000000ULL, 0x40e0000000000000ULL,
        0xc0e0000000000000ULL, 0x40f0000000000000ULL, 0xc0f0000000000000ULL, 0x4100000000000000ULL,
        0xc100000000000000ULL, 0x4110000000000000ULL, 0xc110000000000000ULL, 0x4120000000000000ULL,
        0xc120000000000000ULL
    };
    int i;

    for(i = 0; i < sizeof(values)/sizeof(values[0]); i++)
        if(values[i] == bits)
            return 1;
    return 0;
}


/* Compare real formatting, output lifetime, FP flags and reuse with published RX code. */
int
main(void)
{
    uvlong values[] = {
        0x0ULL, 0x8000000000000000ULL, 0x3ff0000000000000ULL, 0xbff0000000000000ULL,
        0x4000000000000000ULL, 0xc000000000000000ULL, 0x4120000000000000ULL, 0xc120000000000000ULL,
        0x3fe0000000000000ULL, 0x4008000000000000ULL, 0x412e847e00000000ULL, 0x412e848000000000ULL,
        0x1a56e1fc2f8f359ULL, 0x7e37e43c8800759cULL, 0x1ULL, 0x7fefffffffffffffULL,
        0x7ff0000000000000ULL, 0xfff0000000000000ULL, 0x7ff8000000000042ULL, 0x7ff0000000000042ULL
    };
    Module module = {0};
    Inst code = {0};
    String *owner, *slot, *result;
    REAL value;
    uvlong captured, capacity;
    ulong refs;
    REG saved;

    union {
        uvlong alignment; // Stable base for every scalar and owner slot byte offset.
        uchar bytes[128]; // Complete operand image including surrounding canaries.
    } slots;
    uchar before[128], expected[128], ownerbefore[128], ownerexpected[128];
    char decimal[64];
    int sample, mode, sa, da, alias, round, seed, pass, length, allocations;
    int invalid, accepted, reused, wide, wantflags, referenceflags, seedflags;
    int rounds[] = {FE_TONEAREST, FE_DOWNWARD};
    int imagebytes = sizeof(String)+65;
    unsigned long total = 0;

    code.op = ICVTFC; module.prog = &code; module.nprog = 1;
    if(!compile(&module, 1, nil) || !module.jit || !module.jit->entry[ICVTFC]) abort();
    for(sample = 0; sample < sizeof(values)/sizeof(values[0]); sample++) for(mode = 0; mode < 18; mode++)
    for(sa = 0; sa < 8; sa++) for(da = 0; da < 8; da++) for(alias = 0; alias < 3; alias++)
    for(round = 0; round < 2; round++) for(seed = 0; seed < 2; seed++){
        if(getenv("REAL_FORMAT_MODE") && mode != atoi(getenv("REAL_FORMAT_MODE"))) continue;
        wide = mode == 1 || mode == 3 || mode == 5 || mode == 10 || mode == 17;
        owner = wide ? newrunes(16) : newstring(64);
        owner->len = wide ? -7 : 7;
        memset(owner->Sascii, 0xa5, 65);
        owner->tmp = malloc(16); if(owner->tmp == nil) abort();
        memmove(owner->tmp, "old-real-cache", 15);
        if(mode == 3){owner->len = -1; owner->max = 1;}
        if(mode == 4 || mode == 5 || mode == 13 || mode == 16 || mode == 17) D2H(owner)->ref = 2;
        if(mode == 8) D2H(owner)->ref = 0;
        if(mode == 9) owner->len = (-0x7fffffff-1);
        if(mode == 10){owner->len = -1; owner->max = 0x20000000;}
        if(mode == 11) owner->len = 65;
        if(mode == 14 || mode == 16) Tstring.ref = 0;
        if(mode == 15 || mode == 17) Tstring.ref = 0x7fffffff;
        slot = mode == 6 ? H : mode == 7 ? nil : owner;
        memset(slots.bytes, 0xa5, sizeof(slots.bytes));
        R.s = alias == 1 ? (void*)(owner->Sascii+sa) : alias == 2 ? (void*)(slots.bytes+80+da) : (void*)(slots.bytes+sa);
        R.d = slots.bytes+80+da;
        memmove(R.s, &values[sample], sizeof(values[sample])); memmove(R.d, &slot, sizeof(slot));
        memmove(&captured, R.s, sizeof(captured)); memmove(&value, &captured, sizeof(value));
        if(fesetround(rounds[round])) abort();
        feclearexcept(FE_ALL_EXCEPT); if(seed) feraiseexcept(FE_INVALID|FE_INEXACT);
        seedflags = fetestexcept(FE_ALL_EXCEPT);
        length = snprintf(decimal, sizeof(decimal), "%g", value);
        referenceflags = fetestexcept(FE_ALL_EXCEPT);
        if(mode == 2){owner->len = 0; owner->max = length;}
        refs = D2H(owner)->ref;
        capacity = owner->len < 0 ? (uvlong)(uint)owner->max*sizeof(Rune) : (uvlong)(uint)owner->max;
        invalid = (mode >= 7 && mode <= 11) || mode == 13 || mode == 16 || mode == 17;
        reused = !invalid && slot == owner && refs == 1 && capacity <= 0x7fffffff &&
            capacity >= (nativevalue(captured) ? length : 32);
        accepted = reused && nativevalue(captured);
        saved = R; memmove(before, slots.bytes, sizeof(before)); memmove(ownerbefore, owner, imagebytes);
        for(pass = 0; pass < 2; pass++){
            R = saved; memmove(slots.bytes, before, sizeof(before)); memmove(owner, ownerbefore, imagebytes);
            memmove(expected, before, sizeof(expected)); memmove(ownerexpected, ownerbefore, imagebytes);
            allocations = calls; raised = nil;
            if(fesetround(rounds[round])) abort();
            feclearexcept(FE_ALL_EXCEPT); if(seed) feraiseexcept(FE_INVALID|FE_INEXACT);
            if(pass == 0){
                if(module.jit->entry[ICVTFC](&R) != accepted) abort();
                if(accepted){
                    ((String*)ownerexpected)->len = length; ((String*)ownerexpected)->max = capacity;
                    memmove(((String*)ownerexpected)->Sascii, decimal, length+1);
                }
                if(calls != allocations || memcmp(owner, ownerexpected, imagebytes) || memcmp(slots.bytes, expected, sizeof(expected))) abort();
                wantflags = seedflags;
            }else{
                failure = mode == 12 || mode == 13 ? 0 : -1;
                if(setjmp(boundary) == 0){cvtfc(); if(invalid) abort();}
                else if(!invalid || raised != (mode == 7 ? exNilref : mode == 13 ? exNomem : exHeap)) abort();
                failure = -1;
                memmove(&result, R.d, sizeof(result));
                if(invalid){
                    if(calls != allocations || memcmp(owner, ownerbefore, imagebytes) || memcmp(slots.bytes, before, sizeof(before))) abort();
                }else{
                    if(result == H || result == nil || result->len != length || memcmp(result->Sascii, decimal, length+1)) abort();
                    memmove(expected+80+da, &result, sizeof(result));
                    if(reused){
                        if(result != owner || calls != allocations || result->max != capacity || strcmp(result->tmp, "old-real-cache")) abort();
                        ((String*)ownerexpected)->len = length; ((String*)ownerexpected)->max = capacity;
                        memmove(((String*)ownerexpected)->Sascii, decimal, length+1);
                        if(memcmp(owner, ownerexpected, imagebytes)) abort();
                    }else{
                        if(result == owner || calls != allocations+1) abort();
                        destroy(result);
                        if(slot == owner && refs > 1 && (D2H(owner)->ref != refs-1 || memcmp(owner, ownerbefore, imagebytes))) abort();
                        if(slot == owner && refs == 1) owner = H;
                    }
                    if(memcmp(slots.bytes, expected, sizeof(expected))) abort();
                }
                wantflags = invalid || nativevalue(captured) ? seedflags : referenceflags;
            }
            if(memcmp(&R, &saved, sizeof(R)) || fegetround() != rounds[round] || fetestexcept(FE_ALL_EXCEPT) != wantflags) abort();
        }
        if(mode >= 14) Tstring.ref = 2;
        if(owner != H){owner->len = 7; owner->max = 64; D2H(owner)->ref = 1; destroy(owner);}
        if(live || Tstring.ref != 1) abort();
        total++;
    }
    module.jit->freecode(module.jit->code, module.jit->size); free(module.jit);
    printf("PASS real formatting C/RX: %lu lifetime/reuse/image/FP cases WORD%zu\n", total, sizeof(WORD)*8);
    return 0;
}
