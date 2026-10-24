/* RV64 uses private RW-to-RX pages on Linux; embedded targets keep their cache API. */
#if defined(__linux__) && defined(__riscv)
#define RV_CODE_PAGES
#endif
#define RVXLEN 64
#include "comp-riscv.c"
