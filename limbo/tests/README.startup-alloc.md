# Compiler startup and implementation registration allocation

Run `python3 limbo/tests/startup-alloc.py` from the repository root.

Both limbo.y and its tracked y.tab.c epilogue run the same production rootincludes
helper; the driver verifies identical bodies and that main calls the helper.
The compiler's production allocmem and moddecled are included too. Thirty cases
across the parser copies cover absent/empty roots, slash and drive forms, spaces
and UTF-8, one to four implemented modules, allocation failures and a synthetic
oversized root. Returned paths and implementation-list owners are counted.

Root path construction uses allocmem before copying and bounds the joined
length within the compiler's signed size limit. It frees the owned root and
passes the independently allocated root/module path to addinclude, which retains
its argument. Missing roots still install INCPATH. Implementation-module list
nodes also use allocmem before being dereferenced. Former raw allocations
crashed on failure instead of invoking the compiler's fatal error convention.

STARTUP_DRIVER and STARTUP_TYPES select baseline sources independently.
STARTUP_CASE selects paths, modules or failure. The original driver block is
extracted unchanged into a small callable wrapper for baseline comparison.
Original startup and module registration both independently trigger null-access
UBSan errors. Fatal allocation cases run in children and require the diagnostic
exit code. The harness defaults to strict UBSan and uses actual compiler layouts.

Module binding, scope and interface-name dependencies are bounded adapters;
this does not run full source translation or regenerate parser tables. The
oversized-root branch uses a virtual strlen result without allocating gigabytes.
Grammar rules, generated parser tables, Dis output and JIT interfaces are unchanged.
