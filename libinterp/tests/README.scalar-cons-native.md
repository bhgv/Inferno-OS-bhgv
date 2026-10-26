# Scalar list construction through a native allocator ABI

The subsequent raw/pointer constructors and stricter heap-prefix boundaries are
described in [README.payload-cons-native.md](README.payload-cons-native.md).

AMD64 and ARM64 now compile ICONSB, ICONSW, ICONSL and ICONSF. Active opcode
availability rises from 149 to 153 of 173 IDs; 20 remain interpreter-only. RV64
counts are unchanged. Native operation availability is distinct from full VM
or operating-system port acceptance.

The handler captures the source bits before the destination can be overwritten,
stages them on an aligned native stack frame, calls the existing cons allocator
and writes the captured head with native byte/word/qword stores. This bypasses
opcode dispatch while keeping allocation, descriptor retention, tail transfer,
GC Setmark and VM errors in the established runtime helper. These constructors
are not allocation-free leaves and do not introduce a separate pool allocator.
Pointer/mapped/raw aggregate constructors remain on their existing C paths.

The exact allocator address is passed in LeafLayout and emitted at full native
pointer width. The adapter binds it to the production cons function. ICONSB
allocates the historical four-byte payload but writes exactly one byte. ICONSW
uses sizeof(WORD); ICONSL and ICONSF copy eight bytes as raw bits. REAL values
never pass through floating-point arithmetic, preserving NaN payloads, signaling
NaNs and signed zeros. Native scalar instructions add no FP effects; runtime
callbacks retain their usual C semantics.

The AMD64 handler subtracts 24 bytes from the SysV entry stack, aligns the
allocator call and keeps the captured value outside the pushed return address.
The ARM64 handler saves scalar bits and LR in a 16-byte aligned frame and restores
both before returning. Neither depends on caller-saved registers surviving the
allocator. Nonvolatile registers and the original Dis PC/register pointers stay
intact. Native VM errors unwind through the same outer boundary as C; actual
AMD64 longjmp behavior is tested, while ARM64 runtime unwinding still needs
physical-target acceptance.

The common cons helper now rejects nil/sentinel/wrapping destination spans and
low, unaligned or dead tails before requesting memory. Tail headers are captured
with memmove rather than dereferenced before alignment/lower-bound checks.
Reference/size errors retain precedence. This fixes invalid-address accesses
shared by interpreter and native callers; the new preflight oracle fails with
UBSan against the parent helper. Saturated live tail references remain valid
because ownership transfer does not increment them.

Validation:

- scalar-cons-native.py: 245,760 C/actual AMD64 RX cases over WORD32/native
  pointers and WORD64. Coverage includes all scalar bits, NaNs, operand aliases,
  exact payload/padding and allocation canaries, GC colors/propagation,
  descriptor/tail saturation, allocation failure, VM-error unwind and FP state.
  Another 32 address/error-order cases execute both C and RX paths.
- scalar-cons-jit.py: 295,176 AMD64/ARM64 instruction-emulation checks. The runtime
  allocator model uses a high 64-bit function address, validates arguments and
  stack alignment, and deliberately clobbers caller-saved registers. Successful
  paths verify stack/LR restoration, nonvolatile registers, exact VM images and
  bounded stores. Error models stop at allocator entry; real AMD64 error unwind
  is covered by the native driver, not inferred from these models.
- scalar-cons-layouts.py: 2,808,400 ASan/UBSan sizing/publication/canary checks per
  backend across all four commands and both WORD widths. Invalid or undersized
  output buffers stay unchanged.
- The successful C/RX subset passes ASan/UBSan (122,880 cases). Complete errors
  run with UBSan and NDEBUG. Explicit live-owner counts and trailing guards
  accompany the container's ASan process/stack-discovery warnings.
- Native publication, private C/ctypes layout probes and the existing list,
  library, compiler and interpreter checkpoint drivers provide regression checks.

Unrelated linked fixtures provide an aborting cons identity only to satisfy
publication linkage; they never execute these constructors. The dedicated
constructor driver links the actual production allocator and allocation logic.
ARM64 execution evidence is emulated; physical ARM64 and complete Inferno boot
acceptance remain separate.
