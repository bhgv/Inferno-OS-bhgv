#!/usr/bin/env python3
"""Independently check the private location reader's actual byte budget and stream lifetime."""
from io import BytesIO
from pathlib import Path
from types import SimpleNamespace
import runpy

ROOT = Path(__file__).resolve().parents[2]  # Owning tracked source root.


class GrowingInput:
    """Supply stale small metadata and a larger stream without filesystem timing races."""

    # Retain immutable fixture bytes; no file or stream is opened yet.
    def __init__(self, payload):
        self.payload = payload  # Immutable borrowed fixture bytes.
        self.stream = None  # Last owned stream, inspected after the reader closes it.
        self.requests = []  # Actual requested read sizes, independent of returned size.

    # Model metadata obtained before a concurrent append.
    def stat(self):
        return SimpleNamespace(st_size=0)

    # Supply the former unbounded API so its contract removal fails actively.
    def read_bytes(self):
        return self.payload

    # Transfer one recorded stream to the helper; require read-only admission.
    def open(self, mode):
        if mode != "rb":
            raise RuntimeError("Diagnostic reader attempted a write")
        owner = self

        class RecordedStream(BytesIO):
            # Record the actual bound before delegating to a private byte buffer.
            def read(self, size=-1):
                owner.requests.append(size)
                return super().read(size)

        self.stream = RecordedStream(self.payload)
        return self.stream


# Execute the real helper with a tiny bound. A virtual growth control makes the
# old stat-then-read implementation admit excess bytes, so the oracle is active.
def main():
    module = runpy.run_path(str(ROOT / "utils/dis64-locations.py"))
    read = module["read"]
    read.__globals__["LIMIT"] = 3
    for payload in (b"", b"a", b"abc", b"abcd", b"abcde"):
        source = GrowingInput(payload)
        accepted = True
        try:
            result = read(source)
        except ValueError:
            accepted = False
        if accepted != (len(payload) <= 3):
            raise RuntimeError("Growing input escaped the actual byte budget")
        if accepted and result != payload:
            raise RuntimeError("Bounded input changed")
        if source.requests != [4] or source.stream is None or not source.stream.closed:
            raise RuntimeError("Reader did not bound and close its single stream")
    print("PASS location reader: exact bound, stale metadata/growth, EOF and closed stream")


if __name__ == "__main__":
    main()
