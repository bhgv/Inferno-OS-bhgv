# Native deferred byte reads / Native отложенные byte reads

The original C libstyx dispatcher can now leave a byte Tread pending while
processing other messages and clients. The wire remains standard9P2000. Existing
Styxops callbacks are unchanged; Client layout adds pending/staged read state.
Rebuild the library and all consumers together. Directory pagination is unchanged.

## Owner-thread API / Owner-thread API

Inside `Styxops.read`, `styxdeferread(server, aux, release)` returns an owned
StyxRead request or nil. Only success adopts aux. Set `*n=0` and return nil after
successful defer. The callback must not consume an event before it can arrange
its eventual reply. Returning an immediate error after defer retires the context
and sends the ordinary Rerror. Returning data and deferring the same read is
rejected. Synchronous reads allocate no request objects.

At most128 byte reads can be pending per client and one per fid. Zero-capacity,
directory, duplicate-defer and NOTAG requests cannot defer. Admission refusal
leaves aux caller-owned; the callback chooses its ordinary immediate error.
The dispatcher reserves each pending tag against all later requests, including
Tflush envelopes. Duplicate outstanding tags close the offending connection.

After the callback returns, `styxreadclient`, `styxreadaux`, `styxreadlimit` and
`styxreadoffset` expose the captured pending client, original fid context,
negotiated capacity and full64-bit offset. They are borrowed owner-thread data.
The accessors return nil/zero while staged or retired. An unrelated later
callback's `server.curc/current` is never used to choose the completion target.

`styxreadreply(request, bytes, count, error)` completes with the original tag.
Nil error means Rread; a non-nil error requires count0 and means Rerror. Bytes
remain borrowed through the call; no shared request input/output buffer is
retained. A count over the captured limit or a missing nonempty buffer returns0
without consuming the pending request. Staged or inactive requests also return0.
A valid completion returns1 after response attempt and retirement, not proof of
peer receipt. Returning count0 deliberately sends EOF; waiting is represented by
a pending request, never by a fabricated empty Rread or private wire error.

`styxreadfree` consumes the caller reference. Freeing a live caller reference
alone does not complete/cancel it. The dispatcher keeps its independent ref until
retirement. `release(aux)` runs exactly once after request detachment and before
fid/client cleanup. It may consume the caller request ref itself. It must not
wait for external work or reenter dispatch/teardown. Detached inactive handles
can outlive their former client and be released later on the native owner.

## Cancellation and notification / Отмена и уведомления

A matching Tflush retires the read before Rflush. No later Rread/Rerror with its
oldtag can be produced; unrelated flushes leave it pending. Already completed
responses retain ordinary9P ordering before a later Rflush. Accepted Tversion,
disconnect and server end cancel pending reads before freeing fid contexts.
Rejected version negotiation leaves them alone. Fid clunk/removal sends Rerror
for its pending read before freeing the fid and returning its clunk/remove reply.
This lets an independent reader retire without waiting for its own timeout.

All request APIs execute only on the socket owner, serialized with dispatch.
A producer thread never touches Client/Fid/StyxRead or guest pointers. It sends
copied readiness/data through a native mailbox/pipe. The host registers the
borrowed readiness descriptor with `styxnotifyfd`, drains it after `styxwait`,
and completes ready requests on the owner. Other messages, including standard
heartbeat Tflush, remain serviceable while a read is pending. Native teardown
never depends on guest execution or guest cooperation.

## Tests and scope / Проверки и границы

`deferred-read.c` includes the actual dispatcher and codecs. It checks full32-bit
fids with equal low bits, high64-bit Qids/offsets, negotiated read capacity,
synchronous compatibility, invalid/nonconsuming completion, delayed success/error,
independent pending fids, unrelated/matching flush, clunk ordering, immediate
callback error, mixed immediate/deferred data rejection, invalid/accepted version,
quota, duplicate tags, late replies and context-before-fid lifetime.

`deferred-read-server.c` is an actual POSIX TCP server with an independent native
producer pipe. Its arbitrary three-byte fixture records are test data, not a
public event format. `deferred-read.py` checks one record per Rread, queued
records, short-buffer preservation, other operations while pending, tag/fid reuse,
flush/reset/clunk, disconnect, shutdown and duplicate-flush-tag rejection.
DisVM acceptance additionally connects the actual native fd-owner client,
observes repeated heartbeat responses during the pending read and exercises
explicit cancellation followed by a fresh successful read without lost data.

This is the transport prerequisite for a provider event resource, not a public
event-resource implementation, path convention, telemetry schema or remote
subscription/discovery API. Those formats and publication/access policies remain
separate. Resource providers still decide visibility and operation permission.

Расширение откладывает обычный Tread до появления данных. Socket-owner продолжает
обслуживать другие сообщения; producer уведомляет его через native pipe/mailbox.
Один ответ может содержать одну запись, а короткий буфер не требует терять запись.
Формат данных выбирает backend; тестовые строки не являются форматом событий.
Tflush/Tversion/disconnect/end безопасно завершают pending requests; clunk сначала
отвечает ошибкой ожидающему read. Callback cleanup выполняется до freefid/freeclient.
API расширяет C embedding, но не сообщения Styx и не права доступа. Guest-потоки
не участвуют в продвижении и не приостанавливаются.
