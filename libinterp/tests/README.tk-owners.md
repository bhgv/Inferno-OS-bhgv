# Tk windows, display results and channel ownership

Run `python3 libinterp/tests/tk-owners.py`; the default compiler flags enable
UBSan. Checks remain active with NDEBUG. The host fixture uses actual Tk,
interpreter and builtin-frame records with bounded allocation, native widget,
context, channel and process-registry adapters. It does not render a GUI.

Each WORD64/WORD32 model passes14160 quote/bounds/top/channel/interleaving cases.
WORD32 changes VM words while retaining host pointers; actual ARM32 compilation
and linking are separate APK build checks. Top cases now require a fresh root
and widget namespace on every Tk.toplevel call. Secondary windows must not clear
the process's primary #G root. Collection resolves a weak owner by PID rather
than dereferencing a retired Prog. Cached default-display returns own a retained
reference, including aliased return replacement. Explicit display paths bypass
the default cache.

`TK_OWNER_CASE=quote`, `bounds`, `top` or `name` selects a subset.
`TK_OWNER_MODELS="32"` selects the WORD32 model. `TK_OWNER_RUNNER` supplies
an explicit target execution prefix when CC builds a cross-target fixture.
`TK_OWNER_PLATFORM=Android/arm` selects Bionic-compatible declarations instead
of the default Linux/arm host-test profile. `TK_OWNER_SOURCE` and
`TK_OWNER_DRAW_SOURCE` select alternate Tk/Draw production sources.

Run `python3 libinterp/tests/tk-window-controls.py` for two active WORD32 controls:
restoring the singleton-top return or removing the display retain must trigger
the fixture's runtime check. A compile error cannot satisfy either control.
