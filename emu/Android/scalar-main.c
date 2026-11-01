#if !defined(ANDROID) || !defined(__aarch64__) || !defined(DIS_NATIVE64_SCALAR_BOOT)
#error Android scalar entry requires the explicit ARM64 Dis64 profile
#endif

/* Keep the normal CLI parser and native64 layout gate without exporting an APK main symbol. */
#define main inferno_android_scalar_main // Dedicated C entry for the native probe worker.
#include "../port/main.c"
#undef main

int is_run = 1; // Android reboot state; this profile never reenters a retired VM.
char calltracelog_path[MAXROOT+32] = "/sdcard/Inferno/backtrace64.log"; // Shared-storage diagnostics only.


/* Reboot cannot reenter a second VM in the same native Activity process. */
void
amain(void)
{
	fprint(2, "Android scalar probe cannot reboot in place\n");
}
