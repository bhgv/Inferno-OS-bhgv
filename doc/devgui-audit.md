# GUI device audit (experiment)

This report distinguishes regression-tested device fixes from integration work
that remains. It does not claim that all Inferno bugs or all GUI bugs are fixed.

## Closed defects

- Reply reads preserve unread bytes; zero reads do not consume replies. Queues
  are FIFO, belong to the individual open channel, compare the full Qid
  path/version and use the VM lock for read, write and close. Independent opens
  cannot consume or drain each other's replies; close drains its own reply nodes.
- Missing-widget reads unwind the error stack. Repeated failures do not leave
  stale handlers or held locks.
- Snapshots use growing lib9 formatting and a per-open cache. Short reads and
  offsets return exact bytes without a terminating NUL; getter commands and
  snapshots are no longer truncated to fixed stack buffers. Heap cleanup covers
  formatter/getter failures, invalid widget/canvas types and absent canvas focus.
- Snapshot command envelopes escape quotes and backslashes, preserving embedded
  newlines. Writes copy bounded input, reject embedded NUL/malformed envelopes,
  parse the entire request before execution and reject more than 99 commands.
  Input is never modified. Positive stream offsets remain accepted.
- Widget Qids use a lazy monotonic identity stored in Tk, not the sibling-list
  position. Deleted widgets, reused object memory and replaced tops cannot
  inherit an old widget Qid. The 64-bit path reserves 32 PID bits, 31 identity
  bits and one control flag; identity exhaustion fails without wrapping. The
  process directory itself intentionally follows the current top.
- Directory lookup does not write into the supplied name. High PID bits survive
  Qid construction and parent traversal. All generation paths, including `..`,
  balance locking/error handlers. Overlong directory names fail explicitly
  rather than aliasing through truncation.
- Create rejects unknown types instead of selecting the last table entry. Create
  and remove use dynamic commands and one ownership/lock cleanup path. Missing
  processes/widgets, allocation failures and Tk errors are reported. Removal of
  the device root does not release an unheld lock. Created widget lookup uses
  its full name after callbacks rather than a stale parent pointer.
- GUI bootstrap calls use typed, zero-initialized module frames and check failed
  results before dereferencing them. Temporary Dis references are destroyed;
  window ownership transfers to the published top. Creating the first GUI of a
  different Prog is rejected: the underlying module APIs operate on `currun()`.
  Existing target GUI instances remain accessible.
- lib9 dynamic formatting saves the used offset before realloc and checks
  capacity growth for overflow.

## Verification

From the repository root:

```sh
python3 emu/port/tests/guiread.py
CFLAGS='-O1 -g -fsanitize=undefined' python3 emu/port/tests/guiread.py
CFLAGS='-O1 -g -fsanitize=address,undefined --param=asan-stack=0' \
  python3 emu/port/tests/guiread.py
cc -Werror=incompatible-pointer-types -fsyntax-only \
  -I Linux/386/include -I emu/port -I include -I libtk emu/port/devgui.c
```

`guiwrite.py` is a compatibility entry point for the same suite. The harness
extracts production device functions and links production lib9 formatters with
real Chan/Tk/Dis headers. Scheduler, process lookup and Tk command execution are
boundary stubs. Allocation cutoffs exercise read, write, create and remove;
explicit accounting checks heap cleanup, and assertions check lock/error-stack
balance. Directory tests include PID 100000, immutable lookup and long names. Identity tests delete/reorder siblings, reuse
widget storage, replace the top and exhaust the ID counter. Independent opens
are tested with partial reads and close in both orders.
Bootstrap tests cover the ownership guard and display-allocation failure; the
successful Draw/Tk bootstrap and its later failures still require integration
coverage. Dis destruction is stubbed and is not a GC/refcount integration test.

The sandbox lacks usable `/proc` information for ASan stack/LSan handling across
Inferno's siglongjmp error paths. The ASan run therefore checks the heap with
stack instrumentation disabled; it is not a full stack/LeakSanitizer result.

## Remaining work

- Run the complete emulator with real Tk, display resources, scheduling and GC;
  exercise first-top creation, allocation failure at every bootstrap stage,
  repeated open/close and concurrent clients.
- Tk requests are not transactions: preceding successful commands/widgets are
  not rolled back after a later command fails. Bootstrap may leave a published
  top after a later failure.
- Review device permissions and wstat separately. Existing GUI access policy is
  not established as correct by these memory/error-path fixes.
