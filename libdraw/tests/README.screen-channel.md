# Native Screen channels and public-window construction

Run `python3 libdraw/tests/screen-channel.py`; omission controls are `owned`,
`public`, `channel`, `publication`, `attachment`, `validation`.

The fixture executes the actual allocscreen/publicscreen/_allocwindow/allocwindow
bodies with public native Draw headers and the actual channel/Rectangle codecs.
Native Screen allocations use genuine malloc storage with 32 independent guard
bytes. Allocation, packet reservation, flush and image construction are separate
fault/effect services; no VM Heap, collector, renderer or OS draw device is
fabricated. The image service checks every incoming channel and argument before
returning a bounded native header or refusing.

Nine valid channel formats, owned/public screens, reused/fresh window headers and
five fault schedules yield 180 cases. Eight malformed-handle cases include a
native-ulong channel with forbidden high bits on 64-bit hosts. Additional owned
screen nil/closing/channel/foreign-display checks run without effects. Public
screens deliberately have no backing image, and an owned screen's borrowed image
metadata is changed after construction to verify channel independence. Protocol
bytes are decoded independently of the writer. List order, delayed publication,
attachment count, complete retirement and the 25-attempt owned-screen bound are
checked. Local native x64 O3 UBSan: 7,245 predicates/188 cases.

Each control must fail at its exact semantic predicate with exit10. The fixture
retires its actual outstanding Screen before a deliberate failure, so LSan remains
enabled in hosted ASan+UBSan runs. Compiler, crash, timeout, sanitizer or unrelated
failures are never accepted as control evidence.

This changes the host-native Screen header and requires coherent library/runtime
rebuilding. It does not change Draw wire packets, managed Dis structures, Frame64,
shared feature/ABI IDs or admission policy. Existing external code manually
constructing a Screen must supply its channel, as both official factories do.
