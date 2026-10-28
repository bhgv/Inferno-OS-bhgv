# Mapped copy operands across destructor callbacks

Run `python3 libinterp/tests/mapped-capture-native.py` from the repository root
on an AMD64 host. The driver uses ASan/UBSan and NDEBUG by default, actual VM
headers, production incmem/freeptrs and the real MOVMP, HEADMP, movtmp and
cjitexec bodies. The only VM worker runs on a joined pthread so ASan tracks its
stack. LeakSanitizer process inspection is disabled for the unavailable /proc;
compiler executable mappings and descriptors are explicitly released.

MOVMP and movtmp capture source and destination before retaining/releasing
mapped owners. HEADMP similarly captures the list payload and destination while
retaining its existing temporary list ownership. A destructor can borrow REG
without redirecting the final memmove into another record. No whole-REG restore
is added: callback changes to R.s/R.d/R.m/R.M remain visible. HEADMP retains its
historical R.s payload assignment when no callback changes it.

All existing metadata, map, span, empty-record and reference-limit preflight
remains in place. The copy order remains retain source fields, release old
result fields, copy bytes, then release HEADMP's temporary list owner. The
change neither makes incmem fallible nor introduces allocation or a native ABI.

Each scalar model passes 64,512 valid scenarios:

- MOVMP and HEADMP through C, actual fast-entry decline plus cjitexec, and the
  published complete RX entry; the channel mover movtmp is exercised directly;
- one through eight native pointer cells, three pointer-map patterns, all byte
  shifts of source and destination, duplicate source/old owners and H fields;
- callbacks disabled, first zero-reference destructor borrowing REG, and every
  zero-reference destructor borrowing REG;
- complete output/source/alternate-buffer images, source reference counts,
  complete release of old fields, the temporary list count, propagation colors
  and nprop, and exact visible REG state after callbacks.

The reference oracle builds initial old-owner counts from map occurrences and
expects source retains independently. Real production map walkers perform the
ownership operations. Destruction's zero-reference callback is modeled with
bounded static owners; this fixture does not replace a full heap/GC integration
or VM boot. WORD32 changes scalar WORD/UWORD while retaining native pointers.

Fourteen separate controls against the previous xec.c fail their explicit image
predicates: all seven execution-route/operation combinations in both scalar
models. MAPPED_CAPTURE_SOURCE selects the previous source;
MAPPED_CAPTURE_WORDS, MAPPED_CAPTURE_OPERATION and MAPPED_CAPTURE_ROUTE isolate
controls. The failure is an incorrectly copied result, not a compiler/link
failure or an intentionally invalid pointer.

Adjacent mapped-preflight-native.py checks 6,072/6,136 rejection and publication
cases. mapped-copy-native.py retains 226,576 C/RX metadata/alias/overlap/FP cases
per model. mapped-moves.py retains 13,949 ownership/FIFO/rendezvous/empty cases
per model. Complete tail entry/argument/stack/capacity instruction tests pass
16,881 cases per AMD64/ARM64 backend. Full native dispatch and ARM64 compiler
publication are also checked.

Opcode availability remains AMD64/ARM64 173/173 complete JIT+C entries and
166/173 guarded native paths. Pointer-bearing mapped copies continue through
C. Full 64-bit VM reference/frame/layout migration and physical ARM64 execution
remain separate acceptance work.
