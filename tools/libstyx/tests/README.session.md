# Native Styx session acceptance

`session.c` exercises the original dispatcher and wire codecs through a copied
reply transport. The buffered malformed-frame test drives `styxprocess` itself,
so an endless disconnected-client loop is detected by the test timeout.

Covered behavior:

- Actual new clients require Tversion before other requests.
- Msize includes the complete packet, is independent per connection and cannot
  exceed MSGMAX. This implementation refuses values below 256 with Rerror.
- 9P2000 and dot-suffixed extensions negotiate to base 9P2000. Unsupported
  versions return Rversion `unknown` and cannot attach until renegotiated.
- A valid version request clunks all fids and calls backend close for open ones.
  Invalid tags and too-small limits do not replace the current session.
- Reads and stat replies respect the negotiated output capacity. Incoming frames
  above that capacity are disconnected; Tversion may renegotiate a higher limit.
- Tauth returns an explicit unsupported error rather than inventing an auth Qid.
  An attach with an authentication fid is refused: this library has no auth-fid
  provider. Packet uname/aname are routing/policy inputs, not verified credentials.
- Failed attach restores library-owned identity and publishes no fid. Existing
  live fids prevent switching the connection's user or attach name. Backend
  callbacks own rollback of any private state they change while refusing attach.
- Backend newclient failure marks the client for immediate disconnect, and
  freeclient releases any partial backend state without serving requests.
- Disconnect closes handles before releasing the client backend; current-client
  state does not retain freed clients. Malformed frames leave the dispatch loop
  and release the client immediately.

The public Client layout changed; rebuild the server and embedding consumers
against the same header. Callbacks execute synchronously on the server's owning
native thread. They must not reenter the server or stop guest workers globally.
These tests do not validate a real TCP listener, cryptographic authentication,
full listener/platform shutdown, directory pagination or a native mount client.
