
static const int numericops[] = {IMOVW, IADDW, ISUBW, IMULW, IANDW, IORW, IXORW}; // Actual scalar opcode identities.
static void (*numericc[])(void) = {C_movw, C_addw, C_subw, C_mulw, C_andw, C_orw, C_xorw}; // Borrowed real handlers.
static uchar *numericcode[7]; // Owned RX mappings released after every case completes.
static int numericsize[7]; // Complete mapping lengths used at allocation and release.
static uchar numericcells[40], numericexpected[40]; // Independent full-image operands and oracle.


/* Encode an exact canonical word without a signed conversion or host byte order. */
static void
numericword(int offset, unsigned int bits)
{
    buffer[offset] = bits>>24;
    buffer[offset+1] = bits>>16;
    buffer[offset+2] = bits>>8;
    buffer[offset+3] = bits;
}


/* Build genuine production leaves and make them executable before any invocation. */
static void
numericinit(void)
{
    LeafLayout abi = {0};
    int i, count;

    abi.wordbytes = sizeof(WORD);
    abi.pc = offsetof(REG, PC);
    abi.s = offsetof(REG, s);
    abi.m = offsetof(REG, m);
    abi.d = offsetof(REG, d);
    for(i = 0; i < nelem(numericops); i++){
        count = leafemit(nil, LeafMax, &abi, numericops[i]);
        numericcheck(count > 0);
        numericsize[i] = count*sizeof(u32int);
        numericcode[i] = leafcodealloc(numericsize[i]);
        numericcheck(numericcode[i] != nil);
        numericcheck(leafemit((u32int*)numericcode[i], count, &abi, numericops[i]) == count);
        numericcheck(leafcodepublish(numericcode[i], numericsize[i]) == 0);
    }
}


/* Compute modular results independently, then compare complete C/native memory images. */
static unsigned long
numericexecute(WORD first, WORD second)
{
    UWORD left = second, right = first, answer;
    int op, alias, offset;
    unsigned long checks = 0;

    for(op = 0; op < nelem(numericops); op++){
        switch(numericops[op]){
        case IMOVW: answer = right; break;
        case IADDW: answer = left+right; break;
        case ISUBW: answer = left-right; break;
        case IMULW: answer = left*right; break;
        case IANDW: answer = left&right; break;
        case IORW: answer = left|right; break;
        default: answer = left^right; break;
        }
        for(alias = 0; alias < 3; alias++){
            offset = alias*sizeof(WORD);
            memset(numericcells, 0xa5, sizeof(numericcells));
            memmove(numericcells, &first, sizeof(first));
            memmove(numericcells+sizeof(WORD), &second, sizeof(second));
            memmove(numericexpected, numericcells, sizeof(numericcells));
            memmove(numericexpected+offset, &answer, sizeof(answer));
            R.s = numericcells;
            R.m = numericcells+sizeof(WORD);
            R.d = numericcells+offset;
            numericc[op]();
            numericcheck(!memcmp(numericcells, numericexpected, sizeof(numericcells)));
            memset(numericcells, 0xa5, sizeof(numericcells));
            memmove(numericcells, &first, sizeof(first));
            memmove(numericcells+sizeof(WORD), &second, sizeof(second));
            numericcheck(((int(*)(REG*))numericcode[op])(&R) == 1);
            numericcheck(!memcmp(numericcells, numericexpected, sizeof(numericcells)));
            checks += 2;
        }
    }
    return checks;
}


/* Invalid public storage must decline before allocating or dereferencing its image. */
static unsigned long
numericstorage(void)
{
	Dir dir = {0}, aligned[2] = {{0}};
	uintptr i;
	unsigned long checks = 0;

	fixture();
	for(i = 0; i < 64; i++){
		numericcheck(parsemod("storage", (uchar*)((uintptr)-1-i), i+1, &dir) == nil);
		numericcheck(live == 0 && modules == nil && worker.nerr == 0);
		checks++;
	}
	for(i = 1; i < sizeof(void*); i++){
		numericcheck(parsemod("storage", buffer, at, (Dir*)((uchar*)aligned+i)) == nil);
		numericcheck(live == 0 && modules == nil && worker.nerr == 0);
		checks++;
	}
	for(i = 0; i < sizeof(Dir); i++){
		numericcheck(parsemod("storage", buffer, at, (Dir*)((uintptr)-1-i)) == nil);
		numericcheck(live == 0 && modules == nil && worker.nerr == 0);
		checks++;
	}
	numericcheck(parsemod("storage", nil, at, &dir) == nil);
	numericcheck(parsemod("storage", H, at, &dir) == nil);
	numericcheck(parsemod("storage", buffer, at, nil) == nil);
	numericcheck(parsemod("storage", buffer, at, H) == nil);
	numericcheck(parsemod(nil, buffer, at, &dir) == nil);
	numericcheck(parsemod(H, buffer, at, &dir) == nil);
	numericcheck(parsemod("storage", buffer, 0x80000000UL, &dir) == nil);
	numericcheck(live == 0 && modules == nil && worker.nerr == 0);
	return checks+7;
}


/* Preserve every encoded byte while checking exact signed values and cursor movement. */
static void
numericoperand(int bytes, int expected)
{
	uchar before[4], *cursor = buffer;

	memmove(before, buffer, sizeof(before));
	numericcheck(operand(&cursor) == expected && cursor == buffer+bytes);
	numericcheck(!memcmp(before, buffer, sizeof(before)));
}


/* Exhaust short encodings, then cover deterministic full-width signed operand patterns. */
static unsigned long
numericoperands(void)
{
	unsigned int i, bits, state = 0x9abcdef0U;
	int expected;

	for(i = 0; i < 128; i++){
		buffer[0] = i;
		expected = i < 64 ? (int)i : (int)i-128;
		numericoperand(1, expected);
	}
	for(i = 0; i < 16384; i++){
		buffer[0] = 0x80|(i>>8);
		buffer[1] = i;
		expected = i < 8192 ? (int)i : (int)i-16384;
		numericoperand(2, expected);
	}
	for(i = 0; i < 50000; i++){
		state = state*1664525U+1013904223U;
		bits = state&0x3fffffffU;
		numericword(0, bits);
		buffer[0] |= 0xc0;
		expected = bits < 0x20000000U ? (int)bits : (int)bits-1073741824;
		numericoperand(4, expected);
	}
	return 66512;
}


/* Exercise sign boundaries and deterministic canonical bit patterns through actual parsing. */
int
main(void)
{
    static const unsigned int edge[] = {
        0, 1, 63, 64, 127, 128, 255, 256, 32767, 32768, 65535, 65536,
        0x7ffffffeU, 0x7fffffffU, 0x80000000U, 0x80000001U, 0xfffffffeU, 0xffffffffU
    }; // Positive, negative and encoded-width boundaries.
    Module *m;
    Array *array;
    WORD expected[2], actual[2];
    unsigned int bits[2], state = 0x12345678U;
    unsigned long checks = 0;
    int i, j;
    char *selected = getenv("DIS_NUMERIC_CASE");

    if(selected == nil || !strcmp(selected, "storage"))
        printf("PASS parser storage: %lu invalid pointer/extent cases before publication\n", numericstorage());
    if(selected != nil && !strcmp(selected, "storage"))
        return 0;
    printf("PASS signed operands: %lu exact decode/cursor/unchanged-image cases\n", numericoperands());
    numericinit();
    for(i = 0; i < 4096+nelem(edge); i++){
        state = state*1664525U+1013904223U;
        bits[0] = i < nelem(edge) ? edge[i] : state;
        bits[1] = ~bits[0];
        for(j = 0; j < 2; j++){
            if(bits[j] <= 0x7fffffffU)
                expected[j] = bits[j];
            else
                expected[j] = (LONG)bits[j]-4294967296LL;
        }
        fixture();
        numericword(literal_at, bits[0]);
        numericword(literal_at+4, bits[1]);
        m = parse(at);
        numericcheck(m != nil);
        array = A(m->origmp);
        numericcheck(array->len == 2 && array->t->size == sizeof(WORD));
        memmove(actual, array->data, sizeof(actual));
        numericcheck(actual[0] == expected[0] && actual[1] == expected[1]);
        checks += numericexecute(actual[0], actual[1]);
        drop(m);
    }
    for(i = 0; i < nelem(numericops); i++)
        leafcodefree(numericcode[i], numericsize[i]);
    numericcheck(live == 0 && worker.nerr == 0);
    printf("PASS canonical literals WORD%zu: 4114 parsed pairs, %lu real C/RX image checks, balanced owners\n", sizeof(WORD)*8, checks);
    return 0;
}
