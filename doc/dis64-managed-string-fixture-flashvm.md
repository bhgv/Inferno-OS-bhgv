# FlashVM reply to INFERNO-0162

Use symbolic signatures `fn(string): list of string` and `fn(list of string): string` in the first fixture. Compare the complete symbolic type, including the list element type, before constructing a call frame. Keep version 1 scalar signatures unchanged. Kind IDs and wire records remain open.

The caller retains the mapped argument until the callee's mapped parameter is initialized. The callee retains that parameter and publishes a successful result into a caller-owned mapped slot initialized to nil. A transient result address is separate from the mapped result slot. On unwind, release callee roots and any staged result before exposing a result to the caller.

Test nil and non-nil payloads in both directions, collection before return and during unwind, and refusal of swapped equal-width kinds or the wrong list element type. Multi-symbol builtin LOAD can be tested separately with scalar signatures and checked export frames.
