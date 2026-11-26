/* Run Android amain and real Tk/Draw against a headless Bionic framebuffer. */
#include <android/asset_manager.h>
#include <android/log.h>
#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "inferno_storage.h"

enum { Width = 640, Height = 480, Panel = 0x212b38, Menu = 0x2c3c51, Desktop = 0x777777 };
enum View { Vdesktop, Vmenu, Vcalc, Vtask, Vabout, Vfiles };
typedef struct Area Area;
struct Area { int x0, y0, x1, y1; };
static const Area file_path = { 8, 65, 340, 80 };
static const Area file_preview = { 8, 260, 340, 324 };
static const Area file_list = { 8, 122, 340, 160 };
int Xsize = Width, Ysize = Height;
unsigned char *xscreendata;
static unsigned char snapshot[Width * Height * 4];
static pthread_mutex_t frame_lock = PTHREAD_MUTEX_INITIALIZER;
static unsigned int flushes;
static const char shared_root[] = "/sdcard/Inferno";
static const char *sdcard;
static int deny_storage;

extern void amain(void);
extern void xmouse_btn(int, int, int);

/* Map the shared Android mount onto the test's writable directory. */
static const char *
fixture_path(const char *path, char *out, size_t size)
{
	size_t base = sizeof(shared_root)-1;
	if(strncmp(path, shared_root, base) != 0 || (path[base] && path[base] != '/'))
		return path;
	int n = snprintf(out, size, "%s/Inferno%s", sdcard, path+base);
	if(n < 0 || (size_t)n >= size)
		abort();
	return out;
}

extern int __real_stat(const char *, struct stat *);
extern int __real_access(const char *, int);
extern int __real_mkdir(const char *, mode_t);
extern FILE *__real_fopen(const char *, const char *);
extern int __real_rename(const char *, const char *);
extern int __real_unlink(const char *);
extern int __real_inferno_android_root(char *, size_t);

int __wrap_stat(const char *p, struct stat *st) { char b[4096]; return __real_stat(fixture_path(p, b, sizeof(b)), st); }
int __wrap_access(const char *p, int mode) { char b[4096]; return __real_access(fixture_path(p, b, sizeof(b)), mode); }
FILE *__wrap_fopen(const char *p, const char *mode) { char b[4096]; return __real_fopen(fixture_path(p, b, sizeof(b)), mode); }
int __wrap_unlink(const char *p) { char b[4096]; return __real_unlink(fixture_path(p, b, sizeof(b))); }

int
__wrap_mkdir(const char *path, mode_t mode)
{
	char mapped[4096];
	if(deny_storage && strcmp(path, shared_root) == 0){
		errno = EACCES;
		return -1;
	}
	return __real_mkdir(fixture_path(path, mapped, sizeof(mapped)), mode);
}

int
__wrap_rename(const char *from, const char *to)
{
	char source[4096], destination[4096];
	return __real_rename(fixture_path(from, source, sizeof(source)),
	                     fixture_path(to, destination, sizeof(destination)));
}

int
__wrap_inferno_android_root(char *out, size_t size)
{
	char path[sizeof(shared_root)];
	if(__real_inferno_android_root(path, sizeof(path)) != 0)
		return -1;
	if(strcmp(path, shared_root) != 0)
		abort();
	fixture_path(path, out, size);
	return 0;
}

/* Supply the bundle bytes to the production installer without Android's Java framework. */
struct AAssetManager { const char *directory; };
struct AAsset { FILE *file; };

AAsset *
AAssetManager_open(AAssetManager *manager, const char *name, int mode)
{
	char path[4096];
	(void)mode;
	int n = snprintf(path, sizeof(path), "%s/%s", manager->directory, name);
	if(n < 0 || (size_t)n >= sizeof(path))
		return NULL;
	AAsset *asset = malloc(sizeof(*asset));
	if(asset == NULL)
		return NULL;
	asset->file = fopen(path, "rb");
	if(asset->file == NULL){
		free(asset);
		return NULL;
	}
	return asset;
}

int
AAsset_read(AAsset *asset, void *buffer, size_t count)
{
	size_t bytes = fread(buffer, 1, count, asset->file);
	return ferror(asset->file) ? -1 : (int)bytes;
}

off_t
AAsset_getLength(AAsset *asset)
{
	if(fseeko(asset->file, 0, SEEK_END) != 0)
		return -1;
	off_t length = ftello(asset->file);
	return fseeko(asset->file, 0, SEEK_SET) == 0 ? length : -1;
}

void
AAsset_close(AAsset *asset)
{
	fclose(asset->file);
	free(asset);
}

static int
cancel_install(unsigned int done, unsigned int total, const char *path, void *argument)
{
	(void)total;
	(void)path;
	(void)argument;
	return done == 0;
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
	if(view == Vfiles)
		return pixel(380, 70) == 0xdce8f5 && pixel(330, 355) == 0xffffff;
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

/* Wait for a specific widget to repaint, independently of the font bitmap. */
static uint64_t
wait_files(Area area, uint64_t reference, int equal, const char *action)
{
	for(int attempt = 0; attempt < 120; attempt++){
		uint64_t hash = UINT64_C(14695981039346656037);
		pthread_mutex_lock(&frame_lock);
		for(int y = area.y0; y < area.y1; y++)
			for(int x = area.x0; x < area.x1; x++)
				hash = (hash ^ pixel(x, y)) * UINT64_C(1099511628211);
		int matched = visible(Vfiles) && (hash == reference) == equal;
		pthread_mutex_unlock(&frame_lock);
		if(matched)
			return hash;
		usleep(50000);
	}
	fprintf(stderr, "Files did not repaint as expected after %s\n", action);
	exit(1);
}

static void
test_files(unsigned int painted)
{
	uint64_t home, empty, list, first, second;
	tap(27, 19);
	painted = capture("awm-menu.ppm", Vmenu, painted);
	tap(303, 62);
	painted = capture("awm-files.ppm", Vfiles, painted);
	home = wait_files(file_path, 0, 0, "opening Home");
	empty = wait_files(file_preview, 0, 0, "opening Files");
	list = wait_files(file_list, 0, 0, "listing Home");
	tap(40, 131);
	wait_files(file_list, list, 0, "selecting Documents");
	tap(158, 103);
	wait_files(file_path, home, 0, "opening Documents");
	list = wait_files(file_list, 0, 0, "listing Documents");
	tap(40, 131);
	list = wait_files(file_list, list, 0, "selecting the first file");
	tap(158, 103);
	first = wait_files(file_preview, empty, 0, "previewing the first file");
	tap(40, 148);
	wait_files(file_list, list, 0, "selecting the second file");
	tap(158, 103);
	second = wait_files(file_preview, first, 0, "previewing the second file");
	if(second == empty){
		fputs("Files cleared the preview instead of showing a second file\n", stderr);
		exit(1);
	}
	painted = capture("awm-files-preview.ppm", Vfiles, painted);
	tap(22, 102);
	wait_files(file_path, home, 1, "going up to Home");
	wait_files(file_preview, empty, 1, "clearing the preview");
	tap(283, 103);
	capture("awm-files-closed.ppm", Vdesktop, painted);
	puts("PASS Files opened Documents, previewed two files and returned to Home");
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
	painted = capture("awm-closed.ppm", Vdesktop, painted);
	test_files(painted);
	exit(0);
}

int
main(int argc, char **argv)
{
	pthread_t worker;
	if(argc != 3)
		return 2;
	AAssetManager manager = {argv[1]};
	sdcard = argv[2];
	deny_storage = 1;
	if(ioa_prepare_storage(&manager) != -1 || errno != EACCES){
		fputs("The installer ignored the shared-storage denial\n", stderr);
		return 2;
	}
	deny_storage = 0;
	if(ioa_prepare_storage_progress(&manager, cancel_install, NULL) != -1 ||
	   errno != ECANCELED || ioa_prepare_storage(NULL) != -1){
		fputs("Interrupted extraction was accepted as a complete runtime\n", stderr);
		return 2;
	}
	if(ioa_prepare_storage(&manager) != 0 || ioa_prepare_storage(NULL) != 0){
		perror("Bundled Inferno runtime");
		return 2;
	}
	puts("PASS shared storage denial, interrupted extraction and ready-tree reuse without assets");
	const char *document = "/sdcard/Inferno/usr/inferno/Documents/alpha.txt";
	const char *panel = "/sdcard/Inferno/dis/wm/awmpanel.dis";
	const char local[] = "Local document preserved by AWM\n";
	FILE *file = fopen(document, "w");
	if(file == NULL || fputs(local, file) < 0 || fclose(file) != 0 ||
	   unlink(panel) != 0 || ioa_prepare_storage(NULL) != -1 ||
	   ioa_prepare_storage(&manager) != 0 || ioa_prepare_storage(NULL) != 0){
		perror("Restore missing AWM panel");
		return 2;
	}
	char content[sizeof(local)];
	file = fopen(document, "r");
	if(file == NULL || fgets(content, sizeof(content), file) == NULL ||
	   strcmp(content, local) != 0 || fclose(file) != 0){
		fputs("AWM panel repair overwrote a local document\n", stderr);
		return 2;
	}
	puts("PASS shared AWM panel repair preserved the user's document");
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
