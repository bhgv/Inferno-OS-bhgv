/* Independent bounded RV64I decoding of the actual complete-tail encoder. */
#include "isa.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint32_t
    u32int; /* Exact instruction word, independent of host pointer size. */
#include "riscv-emit.h"


/* Interpret signed I-type immediates without implementation-defined narrowing.
 * word is a copied instruction; return the exact signed12 value as int64_t.
 */
static int64_t immediate(uint32_t word) {
  unsigned bits = word >> 20; /* Independent ISA bit position. */
  return bits & 2048 ? (int64_t)bits - 4096 : (int64_t)bits;
}


/* Decode a bounded instruction stream, checking operand register effects rather
 * than comparing a second expected encoding. code contains words readable
 * through count; base is a synthetic four-byte-aligned target PC. Return1 only
 * for an actual non-linking tail to executor, with original
 * a0/ra/sp/callee-saved values intact and a1/opcode, a2/fast reconstructed
 * exactly. No target code is executed.
 */
static int decode(const uint32_t *code, unsigned count, uint64_t base,
                  uint64_t executor, unsigned opcode, uint64_t fast) {
  uint64_t registers[32]; /* Private simulated architectural register file. */
  uint64_t original[32];  /* Independent preservation oracle. */
  unsigned
      step;   /* Hard bound prevents malformed loops escaping this fixture. */
  unsigned i; /* Register index. */
  for (i = 0; i < 32; i++) {
    registers[i] = UINT64_C(0x9137a8c5f6543000) + 16 * i;
  }
  registers[0] = 0;
  memcpy(original, registers, sizeof(original));
  for (step = 0; step < count; step++) {
    uint32_t word = code[step];       /* Actual emitted instruction. */
    unsigned operation = word & 127;  /* Standard opcode field. */
    unsigned rd = (word >> 7) & 31;   /* Destination register field. */
    unsigned kind = (word >> 12) & 7; /* Standard funct3. */
    unsigned rs1 = (word >> 15) & 31; /* Source register1. */
    unsigned rs2 = (word >> 20) & 31; /* Source register2. */
    uint64_t pc = base + 4 * step; /* AUIPC observes its own instruction PC. */
    uint64_t value;                /* Pending register result. */
    if (operation == 0x13 && kind == 0) {
      value = registers[rs1] + (uint64_t)immediate(word);
    } else if (operation == 0x13 && kind == 1 && (word >> 26) == 0) {
      value = registers[rs1] << ((word >> 20) & 63);
    } else if (operation == 0x17) {
      uint64_t upper =
          word & UINT32_C(0xfffff000); /* U-type signed displacement. */
      value = pc + (upper & UINT64_C(0x80000000)
                        ? upper | UINT64_C(0xffffffff00000000)
                        : upper);
    } else if (operation == 0x33 && kind == 6 && (word >> 25) == 0) {
      value = registers[rs1] | registers[rs2];
    } else if (operation == 0x03 && (kind == 6 || kind == 2)) {
      uint64_t address = registers[rs1] + (uint64_t)immediate(word);
      uint64_t offset = address - base;
      if (address < base || offset % 4 || offset / 4 >= count) {
        return 0;
      }
      value = code[offset / 4];
      if (kind == 2 && (value & UINT64_C(0x80000000))) {
        value |= UINT64_C(0xffffffff00000000);
      }
    } else if (operation == 0x67 && kind == 0) {
      uint64_t target =
          (registers[rs1] + (uint64_t)immediate(word)) & ~UINT64_C(1);
      if (rd != 0 || target != executor || registers[11] != opcode ||
          registers[12] != fast) {
        return 0;
      }
      for (i = 0; i < 32; i++) {
        if ((i <= 4 || i == 8 || i == 9 || i == 10 || (i >= 18 && i <= 27)) &&
            registers[i] != original[i]) {
          return 0;
        }
      }
      return 1;
    } else {
      return 0;
    }
    if (rd) {
      registers[rd] = value;
    }
  }
  return 0;
}


/* Compare sizing/emission and every surrounding capacity canary for one input.
 * Addresses and op are values only. Return1 after actual decoding, or0 with no
 * hidden acceptance. Counter belongs to the caller; no callbacks or allocation.
 */
static int exercise(uint64_t executor, int op, uint64_t fast,
                    unsigned long *checks) {
  uint32_t
      words[66]; /* Guards surround a maximum-size writable encoder area. */
  int capacity;  /* Include negative capacities and exact boundary sizing. */
  for (capacity = -2; capacity <= 16; capacity++) {
    int count;  /* Actual encoder result. */
    unsigned i; /* Canary scan position. */
    memset(words, 0xa7, sizeof(words));
    count = rvbridgeemit(words + 1, capacity, executor, op, fast);
    if (count != (capacity < 15 ? -1 : 15) ||
        rvbridgeemit(NULL, capacity, executor, op, fast) != count) {
      return 0;
    }
    for (i = 0; i < 66; i++) {
      if ((i == 0 || count < 0 || i > (unsigned)count) &&
          words[i] != UINT32_C(0xa7a7a7a7)) {
        return 0;
      }
    }
    if (count > 0 && (!decode(words + 1, (unsigned)count, UINT64_C(0x100000000),
                              executor, op, fast) ||
                      !decode(words + 1, (unsigned)count, UINT64_C(0x100000004),
                              executor, op, fast))) {
      return 0;
    }
    (*checks)++;
  }
  return 1;
}


/* Reject inactive opcodes and malformed numerical pointer sentinels before any
 * output write, even with a short capacity. Return0 on changed guards/status.
 */
static int refusals(void) {
  static const uint64_t bad_executor[] = {0, 1, UINT64_MAX, 0x1000, 0x1000};
  static const uint64_t bad_fast[] = {0, 0, 0, 1, UINT64_MAX};
  uint32_t words[66]; /* Complete refusal canary, not merely adjacent words. */
  int op;             /* Invalid/inactive opcode candidate. */
  unsigned scenario;  /* Pointer sentinel pair. */
  unsigned index;     /* Canary position. */
  for (op = -1; op <= 256; op++) {
    if (op > INOP && op < MAXDIS && op != IECLR) {
      continue;
    }
    memset(words, 0xa7, sizeof(words));
    if (rvbridgeemit(words + 1, 64, 0x1000, op, 0) != 0 ||
        rvbridgeemit(NULL, 64, 0x1000, op, 0) != 0) {
      return 0;
    }
    for (index = 0; index < 66; index++) {
      if (words[index] != UINT32_C(0xa7a7a7a7)) {
        return 0;
      }
    }
  }
  for (scenario = 0; scenario < sizeof(bad_executor) / sizeof(bad_executor[0]);
       scenario++) {
    int capacity; /* Refusal precedes insufficient output capacity. */
    for (capacity = -1; capacity <= 64; capacity++) {
      memset(words, 0xa7, sizeof(words));
      if (rvbridgeemit(words + 1, capacity, bad_executor[scenario], IADDW,
                       bad_fast[scenario]) != 0) {
        return 0;
      }
      for (index = 0; index < 66; index++) {
        if (words[index] != UINT32_C(0xa7a7a7a7)) {
          return 0;
        }
      }
    }
  }
  return 1;
}


/* Exercise every pointer bit and active opcode, then reject malformed inputs
 * atomically. Numerical pointer decoding is not pointer authentication or
 * actual C dispatch, which stay covered by separate target/host owning tests.
 */
int main(void) {
  unsigned long checks =
      0;         /* Sizing, guard and independent decode transactions. */
  int op;        /* Existing Dis opcode value, never a new wire allocation. */
  unsigned bit;  /* Every pointer bit permitted by the encoder's even-address
                    contract. */
  unsigned mode; /* Nil or full-width optional native target. */
  static const uint64_t patterns[] = {
      UINT64_C(0xfffffffe), UINT64_C(0x80000000), UINT64_C(0x8000000080000000),
      UINT64_C(0xfffffffffffffffe),
      UINT64_C(0x12345678fedcba98)}; /* Mixed-word and sign-extension edge
                                        patterns, never dereferenced. */
  (void)rvemit; /* Retain the owning leaf encoder declaration without executing
                   it here. */
  for (op = 1; op < MAXDIS; op++) {
    if (op == IECLR) {
      continue;
    }
    for (bit = 1; bit < 64; bit++) {
      uint64_t executor = UINT64_C(0x1000) | (UINT64_C(1) << bit);
      for (mode = 0; mode < 2; mode++) {
        uint64_t fast =
            mode ? UINT64_C(0x2000) | (UINT64_C(1) << ((bit + 30) % 63 + 1))
                 : 0;
        if (!exercise(executor, op, fast, &checks)) {
          fprintf(stderr, "FAIL RV bridge decode op%d bit%u mode%u\n", op, bit,
                  mode);
          return 1;
        }
      }
    }
  }
  for (op = 1; op < MAXDIS; op++) {
    if (op == IECLR) {
      continue;
    }
    for (bit = 0; bit < sizeof(patterns) / sizeof(patterns[0]); bit++) {
      for (mode = 0; mode < 2; mode++) {
        uint64_t fast = mode ? patterns[4 - bit] : 0;
        if (!exercise(patterns[bit], op, fast, &checks)) {
          fputs("FAIL RV bridge decode mixed pointer pattern\n", stderr);
          return 1;
        }
      }
    }
  }
  if (!refusals()) {
    fputs("FAIL RV bridge refusal changed output\n", stderr);
    return 1;
  }
  printf("PASS independent RV64 bridge decoder: %lu capacity/pointer/opcode "
         "transactions, both entry alignments, preserved ABI registers\n",
         checks);
  return 0;
}
