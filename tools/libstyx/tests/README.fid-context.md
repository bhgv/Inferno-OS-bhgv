# Native per-fid context extension

`styxsetaux(server, aux)` adopts a non-nil native context into the current fid's
empty slot, returning 0 on success or -1 without taking ownership on failure.
It requires the appended `Styxops.freefid` consuming callback. `styxgetaux(server)`
borrows that slot only during dispatch on the owning service thread; it returns
nil outside dispatch. Neither pointer is a wire capability or a managed value.

The context is retired once at clunk/remove, session reset, disconnect or server
shutdown, before the client backend is freed. Existing Qid-based close callbacks
remain source-compatible but do not receive or own this context. Do not depend
on relative callback order between legacy close and freefid. Walk clones start
with empty context; the intended use is a separate native handle per successful
open. Set the slot only after native open succeeds. Context cleanup must not
reenter dispatch or pause guest workers.

This is an embedding C API extension, not a change to Styx/9P messages. Client and
Styxops layouts changed: rebuild consumers with the matching header/library.
`fid-context.c` uses the original dispatcher and codecs to prove independent
contexts for equal-Qid opens, refusal to overwrite ownership, scoped borrowing,
exactly-once clunk/reset cleanup and no duplicate close after reset.
