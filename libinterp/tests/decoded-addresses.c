
char exNilref[] = "nil"; // Distinct address-decoder error identity.
static uchar decmp[256], decfp[256], decpayload[256]; // Native-pointer cells and backing payloads.
static uchar decimage[sizeof(Inst)+8]; // Complete instruction image at every byte alignment.
static int decselected; // Addressing byte invoked inside the real VM error protector.


/* Construct the operand address independently of the generated decoder. */
static uchar*
decoracle(int mode, int destination, Inst *pc, Inst *original, uchar *mp, uchar *fp)
{
    switch(mode){
    case AMP: return mp+(destination ? pc->d.ind : pc->s.ind);
    case AFP: return fp+(destination ? pc->d.ind : pc->s.ind);
    case AIMM: return (uchar*)original+(destination ? O(Inst, d) : O(Inst, s));
    case AMP|AIND:
    case AFP|AIND: return decpayload+(destination ? 128+47 : 32+31);
    }
    return nil;
}


/* Cover all 256 addressing bytes, unused fields, high pointers and both alignment axes. */
static unsigned long
decoderimages(void)
{
    static const ushort registers[] = {0, 1, 63, 32767, 32768, 65535}; // Signed immediate and unsigned offset boundaries.
    Inst pc;
    REG expected;
    uchar *mp, *fp, *value, saved[sizeof(Inst)];
    int addr, align, framealign, reg, source, destination;
    unsigned long checks = 0;

    for(framealign = 0; framealign < 8; framealign++){
        if(getenv("DECODE_FRAME_ALIGN") && framealign != atoi(getenv("DECODE_FRAME_ALIGN")))continue;
        mp = decmp+64+framealign;
        fp = decfp+64+framealign;
        value = decpayload+32;
        memmove(mp+3, &value, sizeof(value));
        memmove(fp+3, &value, sizeof(value));
        value = decpayload+128;
        memmove(mp+19, &value, sizeof(value));
        memmove(fp+19, &value, sizeof(value));
        for(align = 0; align < 8; align++)for(addr = 0; addr < 256; addr++)for(reg = 0; reg < nelem(registers); reg++){
            if(getenv("DECODE_IMAGE_ALIGN") && align != atoi(getenv("DECODE_IMAGE_ALIGN")))continue;
            memset(&pc, 0, sizeof(pc));
            pc.op = IADDW;
            pc.add = addr;
            pc.reg = registers[reg];
            pc.s.ind = reg&1 ? -16 : 32;
            pc.d.ind = reg&1 ? -8 : 48;
            source = USRC(addr);
            destination = UDST(addr);
            if(source == (AMP|AIND) || source == (AFP|AIND)){
                pc.s.i.f = 3;
                pc.s.i.s = 31;
            }
            if(destination == (AMP|AIND) || destination == (AFP|AIND)){
                pc.d.i.f = 19;
                pc.d.i.s = 47;
            }
            memset(&R, 0, sizeof(R));
            R.MP = mp;
            R.FP = fp;
            R.PC = (Inst*)(decimage+align);
            R.s = decpayload+1;
            R.d = decpayload+2;
            R.m = decpayload+3;
            R.t = 123456;
            memmove(decimage+align, &pc, sizeof(pc));
            memmove(saved, decimage+align, sizeof(saved));
            memmove(&expected, &R, sizeof(expected));
            if(source != AXXX && source <= (AFP|AIND))
                expected.s = decoracle(source, 0, &pc, R.PC, mp, fp);
            if(destination != AXXX && destination <= (AFP|AIND)){
                expected.d = decoracle(destination, 1, &pc, R.PC, mp, fp);
                switch(addr&ARM){
                case AXNON: expected.m = expected.d; break;
                case AXIMM:
                    expected.t = pc.reg < 32768 ? pc.reg : (int)pc.reg-65536;
                    expected.m = &R.t;
                    break;
                case AXINF: expected.m = (void*)((uintptr)fp+pc.reg); break;
                case AXINM: expected.m = (void*)((uintptr)mp+pc.reg); break;
                }
            }
            dec[addr]();
            numericcheck(!memcmp(&R, &expected, sizeof(R)));
            numericcheck(!memcmp(saved, decimage+align, sizeof(saved)));
            checks++;
        }
    }
    return checks;
}


/* Invoke the selected generated function through the actual Proc error frames. */
static int
decoderinvoke(void *unused)
{
    (void)unused;
    dec[decselected]();
    return 1;
}


/* An invalid address must report the exact error and balance the native unwind stack. */
static void
decoderreject(char *expected)
{
    decodelast = nil;
    numericcheck(!kprotect(decoderinvoke, nil));
    numericcheck(decodelast == expected && worker.nerr == 0 && live == 0);
}


/* Exercise nil, wrapping, minimum signed offsets and invalid indirect pointer spans. */
static unsigned long
decodererrors(void)
{
    Inst pc = {0};
    uchar *target;
    uintptr i;
    unsigned long checks = 0;

    memset(&R, 0, sizeof(R));
    R.PC = &pc;
    decselected = SRC(AFP)|DST(AXXX);
    R.FP = nil;
    decoderreject(exNilref);
    R.FP = H;
    decoderreject(exNilref);
    R.FP = (uchar*)((uintptr)-16);
    pc.s.ind = 32;
    decoderreject(exBounds);
    R.FP = (uchar*)1;
    pc.s.ind = -2;
    decoderreject(exBounds);
    pc.s.ind = -(WORD)(((UWORD)1<<(sizeof(WORD)*8-1))-1)-1;
    decoderreject(exBounds);
    checks += 5;
    decselected = SRC(AFP|AIND)|DST(AXXX);
    pc.s.i.f = 3;
    pc.s.i.s = 31;
    R.FP = (uchar*)((uintptr)-11);
    decoderreject(exBounds);
    R.FP = decfp+64;
    target = nil;
    memmove(R.FP+3, &target, sizeof(target));
    decoderreject(exNilref);
    target = H;
    memmove(R.FP+3, &target, sizeof(target));
    decoderreject(exNilref);
    target = (uchar*)((uintptr)-16);
    memmove(R.FP+3, &target, sizeof(target));
    decoderreject(exBounds);
    checks += 4;
    decselected = SRC(AXXX)|DST(AXXX);
    R.FP = nil;
    R.MP = H;
    dec[decselected]();
    checks++;
    for(i = 0; i < sizeof(Inst); i++){
        R.PC = (Inst*)((uintptr)-1-i);
        decoderreject(i == 0 ? exNilref : exBounds);
        checks++;
    }
    R.PC = nil;
    decoderreject(exNilref);
    return checks+1;
}


/* Execute a decoded MOVW from a genuinely unaligned pointer cell through C and RX code. */
static unsigned long
decodernative(void)
{
    Inst pc = {0};
    WORD input = -1234567, result;
    uchar *target = decpayload+31;
    int align, imagealign;
    unsigned long checks = 0;

    numericinit();
    for(align = 0; align < 8; align++)for(imagealign = 0; imagealign < 8; imagealign++){
        memset(&R, 0, sizeof(R));
        R.FP = decfp+64+align;
        pc.op = IMOVW;
        pc.add = SRC(AFP|AIND)|DST(AFP);
        pc.s.i.f = 3;
        pc.s.i.s = 0;
        pc.d.ind = 48;
        R.PC = (Inst*)(decimage+imagealign);
        memmove(R.FP+3, &target, sizeof(target));
        memmove(target, &input, sizeof(input));
        memmove(decimage+imagealign, &pc, sizeof(pc));
        dec[pc.add]();
        C_movw();
        memmove(&result, R.FP+48, sizeof(result));
        numericcheck(result == input);
        memset(R.FP+48, 0, sizeof(result));
        numericcheck(((int(*)(REG*))numericcode[0])(&R) == 1);
        memmove(&result, R.FP+48, sizeof(result));
        numericcheck(result == input);
        checks += 2;
    }
    for(align = 0; align < nelem(numericops); align++)
        leafcodefree(numericcode[align], numericsize[align]);
    return checks;
}


/* Keep every decoder check active with NDEBUG in both native-pointer WORD models. */
int
main(void)
{
    char *selected = getenv("DECODE_CASE");
    unsigned long images = 0, errors = 0, native = 0;

    if(selected == nil || !strcmp(selected, "images"))images = decoderimages();
    if(selected == nil || !strcmp(selected, "errors"))errors = decodererrors();
    if(selected == nil || !strcmp(selected, "native"))native = decodernative();
    printf("PASS decoded addresses WORD%zu: %lu full REG/instruction images, %lu exact errors, %lu C/RX executions\n",
        sizeof(WORD)*8, images, errors, native);
    return 0;
}
