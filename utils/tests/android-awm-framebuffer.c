/* Run Android amain and real Tk/Draw against a headless Bionic framebuffer. */
#include <android/log.h>
#include <dlfcn.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum { Width = 640, Height = 480, Panel = 0x212b38, Menu = 0x2c3c51, Desktop = 0x777777 };
enum View { Vdesktop, Vmenu, Vcalc, Vtask, Vabout };
int Xsize = Width, Ysize = Height;
unsigned char *xscreendata;
static const char *runtime;
static unsigned char snapshot[Width * Height * 4];
static pthread_mutex_t frame_lock = PTHREAD_MUTEX_INITIALIZER;
static unsigned int flushes;

extern void amain(void);
extern void xmouse_btn(int, int, int);

int
inferno_android_root(char *out, size_t size)
{
	int n = snprintf(out, size, "%s", runtime);
	return n < 0 || (size_t)n >= size ? -1 : 0;
}

int
__android_log_print(int priority, const char *tag, const char *fmt, ...)
{
	va_list ap;
	(void)priority;
	if(strncmp(fmt, "flushmemscreen:", 15) == 0)
		return 0;
	fprintf(stderr, "%s: ", tag);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	return 0;
}

int dladdr(const void *pointer, Dl_info *info) { (void)pointer; (void)info; return 0; }
void ioa_showsplash(void) { }

void
int_refresh(int x0, int y0, int x1, int y1)
{
	(void)x0; (void)y0; (void)x1; (void)y1;
	pthread_mutex_lock(&frame_lock);
	memcpy(snapshot, xscreendata, sizeof(snapshot));
	flushes++;
	pthread_mutex_unlock(&frame_lock);
}

static uint32_t
pixel(int x, int y)
{
	const unsigned char *p = snapshot + (y * Width + x) * 4;
	return (uint32_t)p[2] << 16 | (uint32_t)p[1] << 8 | p[0];
}

static int
visible(enum View view)
{
	if(pixel(600, 15) != Panel)
		return 0;
	if(view == Vmenu)
		return pixel(600, 65) == Menu;
	if(pixel(600, 65) != Desktop)
		return 0;
	if(view == Vcalc)
		return pixel(195, 78) == 0xffffff;
	if(view == Vabout)
		return pixel(100, 65) != Desktop && pixel(195, 78) == Desktop;
	return pixel(100, 65) == Desktop &&
		(pixel(110, 11) != Panel) == (view == Vtask);
}

/* Save the actual Draw framebuffer after each visible transition. */
static int
capture(const char *path, enum View view, unsigned int after)
{
	unsigned int i, painted;
	FILE *stream;
	for(i = 0; i < 240; i++){
		pthread_mutex_lock(&frame_lock);
		painted = flushes;
		if(painted > after && visible(view)){
			stream = fopen(path, "wb");
			if(stream == NULL)
				exit(2);
			fprintf(stream, "P6\n%d %d\n255\n", Width, Height);
			for(unsigned int j = 0; j < Width * Height; j++){
				const unsigned char *p = snapshot + j * 4;
				unsigned char rgb[] = {p[2], p[1], p[0]};
				if(fwrite(rgb, 1, sizeof(rgb), stream) != sizeof(rgb))
					exit(2);
			}
			if(fclose(stream) != 0)
				exit(2);
			pthread_mutex_unlock(&frame_lock);
			printf("PASS %s at framebuffer update %u\n", path, painted);
			return painted;
		}
		pthread_mutex_unlock(&frame_lock);
		usleep(50000);
	}
	fprintf(stderr, "AWM framebuffer did not reach %s after %u updates\n", path, painted);
	exit(1);
}

static void
tap(int x, int y)
{
	xmouse_btn(x, y, 1);
	usleep(100000);
	xmouse_btn(x, y, 0);
}

/* Compare rendered digits independently of the bitmap font used by the runtime. */
static uint64_t
wait_value(uint64_t reference, int equal)
{
	uint64_t hash;
	int x,y,ink;
	for(int attempt = 0; attempt < 120; attempt++){
		pthread_mutex_lock(&frame_lock);
		hash = UINT64_C(14695981039346656037);
		ink = 0;
		for(y = 79; y < 99; y++)
			for(x = 120; x < 208; x++){
				uint32_t color = pixel(x, y);
				ink |= color != 0xffffff;
				hash = (hash ^ color) * UINT64_C(1099511628211);
			}
		int matched = visible(Vcalc) && ink && (hash == reference) == equal;
		pthread_mutex_unlock(&frame_lock);
		if(matched)
			return hash;
		usleep(50000);
	}
	fputs("Calculator display did not reach the expected value\n", stderr);
	exit(1);
}

static void*
observe(void *unused)
{
	unsigned int painted;
	uint64_t zero, five, two, three;
	(void)unused;
	painted = capture("awm-desktop.ppm", Vdesktop, 0);
	tap(27, 19);
	painted = capture("awm-menu.ppm", Vmenu, painted);
	tap(53, 61);
	painted = capture("awm-calculator.ppm", Vcalc, painted);
	zero = wait_value(0, 0);
	tap(84, 171);                    /* Render a directly entered 5 as an oracle. */
	five = wait_value(zero, 0);
	tap(33, 248);
	wait_value(zero, 1);
	tap(84, 210);
	two = wait_value(zero, 0);
	tap(184, 248);
	tap(135, 209);
	three = wait_value(two, 0);
	if(three == five){
		fputs("Calculator displayed the result before equals\n", stderr);
		exit(1);
	}
	tap(134, 248);
	wait_value(five, 1);
	puts("PASS Calculator 2 + 3 equals the directly entered 5");

	tap(185, 52);
	painted = capture("awm-minimized.ppm", Vtask, painted);
	tap(120, 19);
	painted = capture("awm-restored.ppm", Vcalc, painted);
	wait_value(five, 1);
	puts("PASS Calculator retained its value when restored from the taskbar");
	tap(110, 283);
	painted = capture("awm-calculator-closed.ppm", Vdesktop, painted);
	tap(27, 19);
	painted = capture("awm-menu.ppm", Vmenu, painted);
	tap(186, 61);
	painted = capture("awm-about.ppm", Vabout, painted);
	tap(65, 125);
	capture("awm-closed.ppm", Vdesktop, painted);
	exit(0);
}

int
main(int argc, char **argv)
{
	pthread_t worker;
	if(argc != 2)
		return 2;
	runtime = argv[1];
	xscreendata = calloc((size_t)Width * Height, 4);
	if(xscreendata == NULL || pthread_create(&worker, NULL, observe, NULL) != 0)
		return 2;
	amain();
	pthread_join(worker, NULL);
	return 2;
}

/* NativeActivity audio interfaces are unused by the desktop smoke test. */
void NativeAudio_createEngine(void) { }
void NativeAudio_shutdown(void) { }
bool NativeAudio_createUriAudioPlayer(char *s) { (void)s; return false; }
void NativeAudio_shutdownUri(void) { }
void NativeAudio_setPlayingUriAudioPlayer(bool v) { (void)v; }
void NativeAudio_setLoopingUriAudioPlayer(bool v) { (void)v; }
void NativeAudio_setChannelMuteUriAudioPlayer(int c, bool v) { (void)c; (void)v; }
void NativeAudio_setChannelSoloUriAudioPlayer(int c, bool v) { (void)c; (void)v; }
int NativeAudio_getNumChannelsUriAudioPlayer(void) { return 0; }
void NativeAudio_setVolumeUriAudioPlayer(int v) { (void)v; }
void NativeAudio_setMuteUriAudioPlayer(bool v) { (void)v; }
void NativeAudio_enableStereoPositionUriAudioPlayer(bool v) { (void)v; }
void NativeAudio_setStereoPositionUriAudioPlayer(int v) { (void)v; }
bool NativeAudio_enableReverb(bool v) { (void)v; return false; }
void NativeAudio_createBufferQueueAudioPlayer(int rate, int bytes) { (void)rate; (void)bytes; }
void NativeAudio_shutdownBufferQueueAudioPlayer(void) { }
bool NativeAudio_setBufClip(int *n, char *buf, int size) { (void)n; (void)buf; (void)size; return false; }
bool NativeAudio_createFileAudioPlayer(char *name) { (void)name; return false; }
void NativeAudio_shutdownFile(void) { }
void NativeAudio_setPlayingFileAudioPlayer(bool v) { (void)v; }
void NativeAudio_setLoopingFileAudioPlayer(bool v) { (void)v; }
bool NativeAudio_createAudioRecorder(void) { return false; }
void NativeAudio_shutdownAudioRecorder(void) { }
bool NativeAudio_startRecording(int *n) { (void)n; return false; }
int NativeAudio_getRecordingResult(int *n, char **buf) { (void)n; (void)buf; return 0; }
