# Deferred native attach / Отложенный native attach

This is a native C embedding extension. Styx/9P2000 messages and tags are unchanged.
Rebuild consumers because `Client` now stores an opaque pending transaction.
`attach.c` is an implementation fragment included by the original dispatcher;
it is not a second server or an extra object in the original mkfile.

An existing `Styxops.attach` callback remains synchronous unless it explicitly
calls `styxdeferattach(server, aux, release)`. On success this returns one owned
`StyxAttach*` and adopts `aux` for exactly-once `release(aux)` at retirement.
A second defer or a defer outside that callback returns nil without taking aux.
The callback must return nil to keep the transaction pending. Returning an error
retires it immediately, including the release callback. The caller must still
release its owned request handle. A request cannot be completed inside admission.

All request/client APIs run on the native socket owner, serialized against
dispatch and teardown. Worker threads use their own native mailbox/notification;
they must not access `Client`, `StyxAttach` or server pointers concurrently.
No request pointer is a wire capability or a managed VM value. Callbacks must
not reenter dispatch, client destruction or server teardown, run guests, or wait
for external work. Native workers never pause a guest for this API.

`styxattachclient(request)` borrows the pending client after admission returns;
it returns nil once the request retires, even after disconnect/server end.
On its owner thread a service checks this before installing a chosen root in
its session. `styxattachreply(request, nil)` commits the copied attach identity,
creates the reserved fid and attempts Rattach with the original tag. An error
string attempts Rerror without creating a fid/changing committed identity.
Return1 means consumed/completion attempted; it does not acknowledge transport
receipt. Return0 means inactive/staged, with no response or fid allocation.

`styxattachfree` drops the caller reference. The server has its own reference
until retirement. Before abandoning live work, explicitly reply with an error;
dropping the caller reference alone does not answer/cancel a pending transaction.
A release callback may consume the caller reference if its owner arranged that
ownership, as in the VFS acceptance adapter. It runs once on success, denial,
flush, version reset, disconnect or shutdown, before the client backend is freed.
An inactive caller handle can safely outlive its client, but must eventually be
freed. `release` is a nonwaiting native cleanup, not a foreign guest callback.

Only one attach can be pending per client; another Tattach gets Rerror without
replacing it. Other requests continue. The pending fid is reserved against walk
allocation. Its tag must not be reused while outstanding; a duplicate tag closes
the invalid stream. Tflush receives Rflush immediately with the flush tag. A match
cancels the pending attach first, preventing any later original-tag response.
After Rflush the client may reuse that tag/fid. Unrelated/completed/repeated flushes
still receive Rflush. Accepted Tversion and disconnect discard pending work.

For POSIX, `styxnotifyfd(server, fd)` registers one borrowed native readiness fd;
`-1` removes it, return0 means success, -1 invalid/unsupported. Register/drain on
the owner thread. A worker writes its pipe/mailbox endpoint, waking `styxwait`;
the host drains its notifications and completes ready requests on the socket
owner. Unregister before closing the borrowed fd or close it after `styxend`.
The library never reads/closes that descriptor. Nt/Plan9 currently return -1;
no portability/wakeup guarantee is claimed for those adapters.

Отложенный attach не меняет wire-протокол. Callback принимает native request,
а отдельный worker уведомляет socket-owner через fd. Ответ выполняется только
socket-owner. Tflush немедленно отменяет запрос и возвращает Rflush; после него
старый Rattach невозможен. Tversion/disconnect/shutdown также отменяют pending.
Гостевые потоки не участвуют в продвижении и не приостанавливаются.

`deferred-attach.c` exercises the real original dispatcher and codecs: no premature
reply/fid; preserved tag; duplicate defer; pending fid reservation; unrelated and
matching flush; exact fid/tag reuse; success/error retirement; version reset;
immediate callback error after defer; duplicate tag rejection; safe late completion
after disconnect. VFS TCP acceptance additionally exercises native reviewer
notifications and publication ticket cancellation against the same implementation.
