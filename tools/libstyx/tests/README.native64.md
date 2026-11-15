# Native64 C server acceptance

`native64.c` includes the original dispatcher, encodes requests with lib9 and
captures responses at the transport boundary. It does not replace protocol codecs
or require a listening socket. Link with the complete native lib9 and POSIX
libstyx archives; the test translation unit supplies the original server symbols.

Checks cover distinct full-width fids with equal low 16 bits, high-bit 64-bit
Qid paths, zero-element same-fid walks, refusal to replace another live fid,
64-bit read offsets, read/write open modes, bounded read capacity, zero/partial
results and rejection of callback counts exceeding the request (including counts
above 32 bits on LP64). Checks remain active with NDEBUG. Run both normally and
with address/undefined-behaviour sanitizers.

This is a focused dispatch/LP64 regression test. It does not validate the complete
network server, authentication, negotiated msize, fragmented TCP transport,
directory pagination or shutdown lifetime. Those require further transport and
backend acceptance before deploying the library against untrusted peers.

The public `styxqid` helper now accepts `Path`; rebuild callers and the library
together. Wire format and callback signatures remain unchanged. Native callbacks
receive a real `ulong` temporary and must never write beyond the input capacity.
