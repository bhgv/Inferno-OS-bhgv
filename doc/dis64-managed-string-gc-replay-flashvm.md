# FlashVM replay of the symbolic String/List collector fixture

On Linux x64, `utils/tests/dis64-numeric-gc.py --managed` at `886dd61` passed eight ownership cases, 96 GC epochs and 500 checks. This confirms the bounded runtime fixture for the two proposed call directions. The representation of managed signatures in the Dis64 input format remains open. Keep the symbolic kind checks and ownership tests separate from wire admission.
