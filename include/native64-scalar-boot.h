#ifndef INFERNO_NATIVE64_SCALAR_BOOT_H
#define INFERNO_NATIVE64_SCALAR_BOOT_H // Explicit scalar startup profile, not general native64 admission.
#ifdef DIS_NATIVE64_AWM_BOOT
#if DIS_NATIVE64_AWM_BOOT != 1 || !defined(__ANDROID__) || !defined(__aarch64__) || \
    !defined(DIS_WORD32) || DIS_WORD32 != 1 || !defined(DIS64_PILOT_LOADER) || DIS64_PILOT_LOADER != 1 || \
    !defined(DIS_NATIVE64_GUI_SYS_FIXED) || !defined(DIS_NATIVE64_GUI_DRAW) || \
    !defined(DIS_NATIVE64_GUI_TK) || !defined(DIS_NATIVE64_FORMATTER) || defined(DIS_NATIVE64_SCALAR_BOOT)
#error native64 AWM startup requires the Android ARM64 framebuffer GUI profile
#endif
#endif
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
#ifdef DIS_NATIVE64_BOOT_MATH
#if DIS_NATIVE64_BOOT_MATH != 1
#error DIS_NATIVE64_BOOT_MATH must be 1 when explicitly selected
#endif
#ifndef DIS_NATIVE64_SCALAR_BOOT
#error native64 Math startup requires DIS_NATIVE64_SCALAR_BOOT=1
#endif
#endif
#ifdef DIS_NATIVE64_BOOT_SYS_IO
#if DIS_NATIVE64_BOOT_SYS_IO != 1
#error DIS_NATIVE64_BOOT_SYS_IO must be 1 when explicitly selected
#endif
#ifndef DIS_NATIVE64_SCALAR_BOOT
#error native64 Sys I/O startup requires DIS_NATIVE64_SCALAR_BOOT=1
#endif
#endif
#ifdef DIS_NATIVE64_BOOT_FREETYPE
#if DIS_NATIVE64_BOOT_FREETYPE != 1
#error DIS_NATIVE64_BOOT_FREETYPE must be 1 when explicitly selected
#endif
#ifndef DIS_NATIVE64_SCALAR_BOOT
#error native64 Freetype startup requires DIS_NATIVE64_SCALAR_BOOT=1
#endif
#endif
#ifdef DIS_NATIVE64_BOOT_EMUINIT
#if DIS_NATIVE64_BOOT_EMUINIT != 1 || !defined(DIS_NATIVE64_BOOT_SYS_IO) || !defined(DIS_NATIVE64_FORMATTER)
#error checked emuinit boot requires selected Sys I/O and typed formatter
#endif
#endif
#endif
