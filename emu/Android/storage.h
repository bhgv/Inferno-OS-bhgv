#ifndef INFERNO_ANDROID_STORAGE_H
#define INFERNO_ANDROID_STORAGE_H // Launcher-supplied app directories; no storage permission bypass.
#include <stddef.h>

int inferno_android_set_storage(const char*, const char*);
int inferno_android_root(char*, size_t);
#endif
