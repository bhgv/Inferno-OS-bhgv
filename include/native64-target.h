#ifndef INFERNO_NATIVE64_TARGET_H
#define INFERNO_NATIVE64_TARGET_H // Reject a host compiler accidentally selected for another target.
#if !defined(__SIZEOF_POINTER__) || __SIZEOF_POINTER__ != 8 || __SIZEOF_LONG__ != 8
#error native64 profiles require an LP64 target compiler
#endif
#if defined(LINUX_AMD64) && !defined(__x86_64__)
#error amd64 profile requires an x86-64 compiler
#endif
#if defined(LINUX_ARM64) && !defined(__aarch64__)
#error arm64 profile requires an AArch64 compiler
#endif
#if defined(LINUX_RISCV64) && (!defined(__riscv) || __riscv_xlen != 64)
#error riscv64 profile requires an RV64 compiler
#endif
#endif
