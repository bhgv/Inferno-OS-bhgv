/* Private compiler/console agreement. These values have no registry assignment. */
#ifndef DIS64_MANAGED_TRIAL_H
#define DIS64_MANAGED_TRIAL_H

#define Dis64ManagedFile (1U<<30) // Native-only cache provenance; never a wire runtime flag.

#define Dis64ManagedLiterals 32U // Independent mapped String roots in the private file profile.
#define Dis64ManagedLiteralRunes 1024U // Aggregate Rune32 resource limit.

#define Dis64ManagedCalls 32U // Bounded private String caller; no new wire tags.

#define Dis64TrialVersion 0xface0010U
#define Dis64TrialString 0x71U
#define Dis64TrialList 0x72U

#endif
