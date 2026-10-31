#ifndef INFERNO_ANDROID_FDLIMIT_H
#define INFERNO_ANDROID_FDLIMIT_H // Private native helper, not an interposed Android libc symbol.

/* Return a representable descriptor limit or the native sysconf failure value. */
int inferno_android_fdlimit(void);
#endif
