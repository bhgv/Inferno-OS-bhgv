#ifndef INFERNO_ANDROID_STORAGE_H
#define INFERNO_ANDROID_STORAGE_H // Shared /sdcard/Inferno only; no private-directory fallback.
#include <stddef.h>

int inferno_android_root(char*, size_t);
#endif
