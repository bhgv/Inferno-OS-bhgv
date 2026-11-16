# Live directory and native lifecycle acceptance

`directories.c` runs the original dispatcher/codecs and checks:

- needfile=0 plus a read callback delegates even the cached root directory to
  that provider; the callback receives the actual 64-bit wire byte offset;
- a needfile=0 walk callback denial never falls back to hidden cached children;
- the same open root fid observes changed names/modes on subsequent reads;
- provider directory replies must consist of complete, valid stat records;
- needfile=1 retains library-tree enumeration, emitting whole records only;
- a too-small positive buffer returns an error without advancing or false EOF;
- cached continuation must use the preceding response's byte offset; offset zero
  restarts enumeration against the current tree, without freezing it;
- adding a cached child is visible on rewind without reattaching.

The live backend controls its own offset policy. A directory changing during
nonzero-offset pagination may yield the usual omissions/duplicates of a mutable
9P directory; clients can restart at zero. No directory snapshot is promised.
Walk callbacks, permissions and complete mount/client adaptation are separate
acceptance targets; a read callback does not authenticate a connection.

`lifecycle.c` uses actual POSIX readiness state and actual owned native fds, while
replacing listener creation with dup(0). It checks nested library-tree cleanup,
open-handle close before backend release, both client descriptors and the listener
closed, cleared ownership fields, idempotent end, reinitialization, and cleanup on
announce/socket-init failure. This is not a real TCP listener test.

Public styxend now lives in each transport port. POSIX releases core/library state
and its readiness allocation; Nt has the corresponding implementation but is not
covered by the Linux tests. The retained Plan9 helper-process transport returns
an explicit unsupported shutdown error: it cannot safely free shared state until
its helper processes have a join protocol. Previously styxend silently did nothing.
Do not copy that process model into the dedicated-native-thread VM.

The private styxendserver helper releases only library-owned state. Backend/file
private u payloads remain the provider's responsibility. Shutdown runs on the
owning native service thread, after callers stop submitting service operations;
it must not race dispatch or destroy backend storage before close/free callbacks.
POSIX also bounds native descriptors before FD_SET and suppresses SIGPIPE where
MSG_NOSIGNAL is available. Real TCP behavior and other platforms need their own
acceptance before claiming them as tested.

`loopback.c`/`loopback.py` add a separate actual IPv4 TCP acceptance test. The C
executable links the complete original library and POSIX transport, without
including or overriding the dispatcher. Independent Python wire packets drive
Tversion, Tattach, Twalk, Topen, directory/file Tread, Twrite and Tstat over two
connections with different msize and private views. The same open fids observe
changed names/modes/data; the second view remains unchanged. Shutdown checks four
open-handle closes and two backend releases. The test uses only synthetic data;
packet usernames route fixture views and do not implement authentication. The
split-point unit test, not TCP packet boundaries, establishes deterministic
fragmentation coverage. The real test requires native loopback networking.
