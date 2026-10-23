# Prefab traversal and selection failure regressions

Run from the repository root:

```
python3 libinterp/tests/prefab-selection.py
```

The fixture compiles the production tagspace/tags/doselect bodies. It uses
boundary stand-ins for the surrounding element, VM heap, event and display
services. ASan and UBSan are enabled by default; CC and CFLAGS can override the
compiler. PREFAB_SELECTION_SOURCE can select an earlier prefab.c.

An independent expected traversal enumerates 704 combinations of outer/nested
list sizes, tagged and untagged elements, vector growth, and every allocation
failure in each traversal. Failure must propagate rather than return a partial
selection; all vectors must be freed. Selection tests cover ordinary and tagged
modes, every setup allocation failure, highlight rollback, unlocked display
state, returned element references and successful event delivery.

The normal libinterp mkfile currently excludes prefab.o, and its generated
prefabmod.h is absent from the checkout. Whole-file syntax checking can use a
scratch header that declares only Prefabmodtab and Prefabmodlen; all real frame
and runtime structures still come from runt.h. This does not generate a real
module table, enable Prefab in the build, or constitute a complete module build.
