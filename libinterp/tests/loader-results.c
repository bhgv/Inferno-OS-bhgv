
/* Reusable generated call frames; no automatic mutated frame crosses a VM error unwind. */
static F_Loader_tnew result_tnew; /* Descriptor construction result. */
static F_Loader_dnew result_dnew; /* Heap construction owner result. */
static F_Loader_ext result_ext; /* Export installation status. */
static F_Loader_compile result_compile; /* Compiler status. */
static F_Loader_newmod result_newmod; /* Module construction owner result. */
static int result_operation; /* Selected public method, 0..4. */


/* Call the selected production API under kprotect; unused is borrowed and ignored.
 * Returns one after normal completion; native errors unwind to the test protector. */
static int
result_dispatch(void *unused)
{
    (void)unused;
    switch(result_operation) {
    case 0:
        Loader_tnew(&result_tnew);
        break;
    case 1:
        Loader_ext(&result_ext);
        break;
    case 2:
        Loader_compile(&result_compile);
        break;
    case 3:
        Loader_dnew(&result_dnew);
        break;
    default:
        Loader_newmod(&result_newmod);
        break;
    }
    return 1;
}


/* Set only the borrowed output field of the selected reusable frame; no owner is acquired. */
static void
result_slot(void *slot)
{
    result_tnew.ret = slot;
    result_ext.ret = slot;
    result_compile.ret = slot;
    result_dnew.ret = slot;
    result_newmod.ret = slot;
}


/* Exercise all output byte alignments, nil slots and heap-owner replacement.
 * No arguments; returns zero with a diagnostic if output, canaries or ownership differ. */
static int
result_tests(void)
{
    static uchar bytes[40]; /* Stable output image across native error unwinds. */
    static uchar expected[40]; /* Independent surrounding canaries. */
    void *empty = H;
    void *value;
    WORD status;
    Loader_Niladt *backing = H; /* Temporary module-data owner, retained separately from module results. */
    static Loader_Inst code; /* One ordinary RET is enough to build a valid nonexecuted module. */
    static Type instruction_type; /* Borrowed pointer-free Loader_Inst element layout. */
    static Array instructions; /* Borrowed construction input; never enters the VM heap. */
    static String name; /* string2c is an injected native conversion boundary in this fixture. */
    int offset;
    int before;
    int checks = 0;

    for(result_operation = 0; result_operation < 5; result_operation++) {
        memset(&result_tnew, 0, sizeof(result_tnew));
        memset(&result_ext, 0, sizeof(result_ext));
        memset(&result_compile, 0, sizeof(result_compile));
        memset(&result_dnew, 0, sizeof(result_dnew));
        memset(&result_newmod, 0, sizeof(result_newmod));
        result_tnew.mp = H;
        result_ext.mp = H;
        result_compile.mp = H;
        result_dnew.size = 8;
        result_dnew.map = H;
        result_newmod.inst = H;
        result_newmod.name = H;
        result_newmod.data = H;
        for(offset = 0; offset < 8; offset++) {
            memset(bytes, 0xa5, sizeof(bytes));
            result_slot(bytes + 8 + offset);
            memcpy(expected, bytes, sizeof(bytes));
            before = live;
            if(result_operation == 4) {
                result_dnew.ret = &backing;
                Loader_dnew(&result_dnew);
                instruction_type.size = sizeof(Loader_Inst);
                instructions.t = &instruction_type;
                instructions.data = (uchar*)&code;
                instructions.len = 1;
                code.op = IRET;
                code.addr = SRC(AXXX) | DST(AXXX);
                result_newmod.inst = &instructions;
                result_newmod.name = &name;
                result_newmod.data = backing;
                result_newmod.nlink = 0;
                result_newmod.ss = 1024;
            }
            if(result_operation < 3) {
                status = -1;
                memmove(expected + 8 + offset, &status, sizeof(status));
            } else {
                memmove(bytes + 8 + offset, &empty, sizeof(empty));
            }
            if(!kprotect(result_dispatch, nil)) {
                fprintf(stderr, "Loader result unexpectedly raised at an admitted slot\n");
                return 0;
            }
            if(result_operation >= 3) {
                memmove(&value, bytes + 8 + offset, sizeof(value));
                if(value == H) {
                    fprintf(stderr, "Loader owner result differs\n");
                    return 0;
                }
                memmove(expected + 8 + offset, &value, sizeof(value));
                if(result_operation >= 3) {
                    watchedowner = value;
                    watchedslot = bytes + 8 + offset;
                    /* Replace a live heap result; the destructor observes H before the new owner. */
                    if(!kprotect(result_dispatch, nil) || watchedowner != nil) {
                        fprintf(stderr, "Loader construction replacement did not retire its output first\n");
                        return 0;
                    }
                    memmove(&value, bytes + 8 + offset, sizeof(value));
                    memmove(expected + 8 + offset, &value, sizeof(value));
                }
                destroy(value);
                if(result_operation == 4) {
                    destroy(backing);
                    backing = H;
                }
            }
            if(memcmp(bytes, expected, sizeof(bytes)) || live != before || worker.nerr) {
                fprintf(stderr, "Loader result changed canaries, width or ownership: op=%d offset=%d\n", result_operation, offset);
                return 0;
            }
            checks++;
        }
    }
    printf("PASS Loader construction/scalar result slots: %d cases, WORD%zu\n", checks, sizeof(WORD)*8);
    return 1;
}


/* Run legacy or output-slot acceptance in a fresh process selected by argv.
 * argc/argv select optional slots mode; zero means every private allocation/error frame was retired. */
int
main(int argc, char **argv)
{
    if(argc == 1) {
        existingtests();
        return 0;
    }
    if(argc == 2 && strcmp(argv[1], "slots") == 0) {
        return result_tests() ? 0 : 1;
    }
    if(argc == 2 && strlen(argv[1]) == 4 && !strncmp(argv[1], "nil", 3) &&
       argv[1][3] >= '0' && argv[1][3] <= '9') {
        int probe = argv[1][3] - '0'; /* One native error unwind per process keeps sanitizer stack discovery independent. */
        result_operation = probe / 2;
        result_slot((probe % 2) ? H : nil);
        if(kprotect(result_dispatch, nil) || live || worker.nerr) {
            fprintf(stderr, "Loader accepted nil result probe %d\n", probe);
            return 1;
        }
        printf("PASS Loader nil result probe %d, WORD%zu\n", probe, sizeof(WORD)*8);
        return 0;
    }
    fprintf(stderr, "usage: loader-runtime [slots|nil0..nil9]\n");
    return 2;
}
