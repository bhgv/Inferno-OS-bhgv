#ifndef INFERNO_NATIVE64_SCALAR_BOOT_H
#define INFERNO_NATIVE64_SCALAR_BOOT_H // Explicit builtin-free scalar startup, not general native64 admission.
#ifdef DIS_NATIVE64_SCALAR_BOOT
#if DIS_NATIVE64_SCALAR_BOOT != 1
#error DIS_NATIVE64_SCALAR_BOOT must be 1 when explicitly selected
#endif
#if !defined(DIS_WORD32) || DIS_WORD32 != 1
#error native64 scalar startup requires DIS_WORD32=1
#endif
#if !defined(DIS64_PILOT_LOADER) || DIS64_PILOT_LOADER != 1
#error native64 scalar startup requires DIS64_PILOT_LOADER=1
#endif
#include "native64-model.h"
#endif
#endif
