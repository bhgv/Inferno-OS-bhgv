# Styx impossible-frame sizes

styx-frame-size.b is an ordinary Inferno command module. Build it with the
matching Limbo tool and run it against the rebuilt patched /dis/lib/styx.dis.
Its Draw.Context entry parameter is a declaration only; no Draw module is loaded.

The program calls the actual Tmsg.unpack and Rmsg.unpack methods. A complete
seven-byte input whose advertised size is 0..6 must return (-1,nil), not slice
away the header and raise while indexing f[4]. A physically incomplete header
returns (0,nil). A genuine wire Tread remains valid, and each truncated prefix
returns (0,nil). The byte arrays are explicitly initialized; the test does not
assume that an uninitialized scalar array happens to contain zeros.

Expected output: PASS Styx impossible-frame guards. No user file is changed and
no network or mounted filesystem service is needed by the codec checks.

The same production source and test execute in interpreter/JIT modes on the
bhgv/flashVM native Dis host with its matching compiler. Replaying the previous
Styx source produces an array-bounds error for the same input. That is codec
acceptance, not a full Styx server, namespace or platform acceptance claim.
