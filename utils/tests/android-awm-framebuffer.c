/* Run Android amain and real Tk/Draw against a headless Bionic framebuffer. */
#include <android/asset_manager.h>
#include <android/keycodes.h>
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
#include "inferno_touch.h"
#include "../../include/keyboard.h"

enum { Width = 640, Height = 480, Panel = 0x212b38, Menu = 0x2c3c51, Desktop = 0x777777 };
enum View { Vdesktop, Vmenu, Vcalc, VcalcKeysClosed, Vcalcmoved, Vtask, Vabout,
            Vfiles, VfilesKeysClosed, Vkeys, Veditor, VeditFileMenu,
            VeditPath, VeditSource, VeditFunctions, VeditJump };
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
extern int xkbd_key(int, int, int);
extern int android_key_rune(int, int);

static void
test_keymap(void)
{
	const struct { int code, meta, rune; } keys[] = {
		{AKEYCODE_A, 0, 'a'}, {AKEYCODE_Z, AMETA_SHIFT_LEFT_ON, 'Z'},
		{AKEYCODE_A, AMETA_CAPS_LOCK_ON, 'A'},
		{AKEYCODE_A, AMETA_CAPS_LOCK_ON|AMETA_SHIFT_ON, 'a'},
		{AKEYCODE_C, AMETA_CTRL_RIGHT_ON, 3},
		{AKEYCODE_9, AMETA_SHIFT_ON, '('}, {AKEYCODE_0, 0, '0'},
		{AKEYCODE_EQUALS, AMETA_SHIFT_ON, '+'},
		{AKEYCODE_NUMPAD_5, AMETA_NUM_LOCK_ON, '5'},
		{AKEYCODE_NUMPAD_DIVIDE, 0, '/'}, {AKEYCODE_NUMPAD_ENTER, 0, '\n'},
		{AKEYCODE_DPAD_UP, 0, Up}, {AKEYCODE_DPAD_DOWN, 0, Down},
		{AKEYCODE_DPAD_LEFT, 0, Left}, {AKEYCODE_DPAD_RIGHT, 0, Right},
		{AKEYCODE_MOVE_HOME, 0, Home}, {AKEYCODE_PAGE_DOWN, 0, Pgdown},
		{AKEYCODE_TAB, AMETA_SHIFT_ON, BackTab}, {AKEYCODE_DEL, 0, '\b'},
		{AKEYCODE_BACK, 0, No}, {AKEYCODE_VOLUME_UP, 0, No},
		{AKEYCODE_Z, AMETA_ALT_ON, No}, {AKEYCODE_Z, AMETA_META_ON, No}
	};
	for(size_t i = 0; i < sizeof(keys)/sizeof(keys[0]); i++){
		int actual = android_key_rune(keys[i].code, keys[i].meta);
		if(actual != keys[i].rune){
			fprintf(stderr, "Android key %d meta %#x mapped to %d, expected %d\n",
			        keys[i].code, keys[i].meta, actual, keys[i].rune);
			exit(2);
		}
	}
	puts("PASS native Android US keyboard, modifiers, keypad and navigation runes");
}

static void
key(int code, int meta)
{
	if(!xkbd_key(code, meta, 1) || !xkbd_key(code, meta, 0)){
		fprintf(stderr, "AWM did not accept Android key %d\n", code);
		exit(2);
	}
	usleep(100000);
}

/* Model the NDK accessors while running the real motion translator on Bionic. */
typedef struct { int id; float x, y; } Contact;
struct AInputEvent {
	int32_t action;
	int64_t time;
	size_t count;
	Contact contact[3];
	int32_t source;
	int32_t buttons;
	float wheel;
};

int32_t AMotionEvent_getAction(const AInputEvent *e) { return e->action; }
int64_t AMotionEvent_getEventTime(const AInputEvent *e) { return e->time; }
size_t AMotionEvent_getPointerCount(const AInputEvent *e) { return e->count; }
int32_t AMotionEvent_getPointerId(const AInputEvent *e, size_t i) { return e->contact[i].id; }
float AMotionEvent_getX(const AInputEvent *e, size_t i) { return e->contact[i].x; }
float AMotionEvent_getY(const AInputEvent *e, size_t i) { return e->contact[i].y; }
int32_t AInputEvent_getSource(const AInputEvent *e) { return e->source; }
int32_t AMotionEvent_getButtonState(const AInputEvent *e) { return e->buttons; }
float AMotionEvent_getAxisValue(const AInputEvent *e, int32_t axis, size_t i)
{
	if(axis != AMOTION_EVENT_AXIS_VSCROLL || i != 0)
		abort();
	return e->wheel;
}

typedef struct { int x, y, buttons; } PointerEvent;
static const PointerEvent touch_expected[] = {
	{100,120,1}, {103,122,0}, {103,122,2}, {105,126,2},
	{106,129,0}, {106,129,4}, {107,130,0},
	{0,479,1}, {0,479,0},
	{30,60,1}, {30,60,0}, {33,63,257}, {33,63,0},
	{44,50,1}, {200,50,1}, {44,50,0}, {44,50,1}, {44,50,0}
};
static const PointerEvent *expected_pointer;
static size_t expected_count, pointer_received;

static void
expect_pointer(int x, int y, int buttons)
{
	if(pointer_received >= expected_count)
		exit(2);
	PointerEvent want = expected_pointer[pointer_received++];
	if(want.x != x || want.y != y || want.buttons != buttons){
		fprintf(stderr, "Unexpected pointer output #%zu: %d %d %d\n", pointer_received, x, y, buttons);
		exit(2);
	}
}

static void
test_touch(void)
{
	const AInputEvent samples[] = {
		{AMOTION_EVENT_ACTION_DOWN, 0, 1, {{5,100,120}}},
		{AMOTION_EVENT_ACTION_POINTER_DOWN, 0, 2, {{7,200,240},{5,103,122}}},
		{AMOTION_EVENT_ACTION_MOVE, 0, 2, {{5,105,126},{7,220,280}}},
		{AMOTION_EVENT_ACTION_POINTER_DOWN, 0, 3, {{9,300,330},{7,220,280},{5,106,129}}},
		{AMOTION_EVENT_ACTION_POINTER_UP | (1 << AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT), 0, 3,
			{{9,321,333},{5,107,130},{7,220,282}}},
		{AMOTION_EVENT_ACTION_MOVE, 0, 2, {{7,225,290},{9,324,339}}},
		{AMOTION_EVENT_ACTION_UP, 0, 1, {{7,226,290}}},
		{AMOTION_EVENT_ACTION_DOWN, 0, 1, {{6,-12,800}}},
		{AMOTION_EVENT_ACTION_CANCEL, 0, 0, {{0,0,0}}},
		{AMOTION_EVENT_ACTION_DOWN, 0, 1, {{8,30,60}}},
		{AMOTION_EVENT_ACTION_UP, 0, 1, {{8,30,60}}},
		{AMOTION_EVENT_ACTION_DOWN, 0, 1, {{2,33,63}}},
		{AMOTION_EVENT_ACTION_UP, 0, 1, {{2,33,63}}},
		{AMOTION_EVENT_ACTION_DOWN, 0, 1, {{3,44,50}}},
		{AMOTION_EVENT_ACTION_MOVE, 0, 1, {{3,200,50}}},
		{AMOTION_EVENT_ACTION_UP, 0, 1, {{3,44,50}}},
		{AMOTION_EVENT_ACTION_DOWN, 0, 1, {{4,44,50}}},
	};
	IoaTouch touch = {0};
	expected_pointer = touch_expected;
	expected_count = sizeof(touch_expected)/sizeof(touch_expected[0]);
	pointer_received = 0;
	for(size_t i = 0; i < sizeof(samples)/sizeof(samples[0]); i++){
		AInputEvent event = samples[i];
		event.time = (i+1)*INT64_C(50000000);
		if(!ioa_touch_motion(&touch, &event, Width, Height, expect_pointer))
			exit(2);
	}
	ioa_touch_reset(&touch, expect_pointer);
	ioa_touch_reset(&touch, expect_pointer);
	if(pointer_received != expected_count)
		exit(2);
	puts("PASS motion pointer IDs, button masks, chord release, cancellation, drag and double tap");
}

static void
test_mouse(void)
{
	enum {
		Primary = AMOTION_EVENT_BUTTON_PRIMARY,
		Secondary = AMOTION_EVENT_BUTTON_SECONDARY,
		Tertiary = AMOTION_EVENT_BUTTON_TERTIARY
	};
	const PointerEvent expected[] = {
		{16,63,0}, {16,63,1}, {16,63,0}, {16,63,257}, {16,63,0},
		{16,63,4}, {16,63,12}, {16,63,4}, {16,63,6}, {16,63,2}, {16,63,0},
		{16,63,16}, {16,63,0}, {16,63,1}, {16,63,0}, {16,63,4}, {16,63,0}
	};
	const struct { int action; int buttons; float wheel; int touch; } samples[] = {
		{AMOTION_EVENT_ACTION_HOVER_MOVE, 0, 0, 0},
		{AMOTION_EVENT_ACTION_DOWN, Primary, 0, 0},
		{AMOTION_EVENT_ACTION_BUTTON_PRESS, Primary, 0, 0},
		{AMOTION_EVENT_ACTION_UP, 0, 0, 0},
		{AMOTION_EVENT_ACTION_DOWN, Primary, 0, 0},
		{AMOTION_EVENT_ACTION_UP, 0, 0, 0},
		{AMOTION_EVENT_ACTION_BUTTON_PRESS, Secondary, 0, 0},
		{AMOTION_EVENT_ACTION_DOWN, 0, 0, 1},
		{AMOTION_EVENT_ACTION_SCROLL, Secondary, .5f, 0},
		{AMOTION_EVENT_ACTION_SCROLL, Secondary, .5f, 0},
		{AMOTION_EVENT_ACTION_BUTTON_PRESS, Secondary | Tertiary, 0, 0},
		{AMOTION_EVENT_ACTION_BUTTON_RELEASE, Tertiary, 0, 0},
		{AMOTION_EVENT_ACTION_CANCEL, 0, 0, 0},
		{AMOTION_EVENT_ACTION_SCROLL, 0, -.5f, 0},
		{AMOTION_EVENT_ACTION_SCROLL, 0, -.5f, 0},
		{AMOTION_EVENT_ACTION_DOWN, 0, 0, 1},
		{AMOTION_EVENT_ACTION_DOWN, Primary, 0, 0},
		{AMOTION_EVENT_ACTION_UP, 0, 0, 1},
		{AMOTION_EVENT_ACTION_MOVE, Primary, 0, 0},
		{AMOTION_EVENT_ACTION_UP, 0, 0, 0},
		{AMOTION_EVENT_ACTION_BUTTON_PRESS, Secondary, 0, 0},
	};
	IoaInput input = {0};
	expected_pointer = expected;
	expected_count = sizeof(expected)/sizeof(expected[0]);
	pointer_received = 0;
	for(size_t i = 0; i < sizeof(samples)/sizeof(samples[0]); i++){
		AInputEvent event = {
			.action = samples[i].action, .time = (i+1)*INT64_C(50000000),
			.count = 1, .contact = {{0,16,63}},
			.source = samples[i].touch ? AINPUT_SOURCE_TOUCHSCREEN : AINPUT_SOURCE_MOUSE,
			.buttons = samples[i].buttons, .wheel = samples[i].wheel
		};
		if(!ioa_pointer_motion(&input, &event, Width, Height, expect_pointer))
			exit(2);
	}
	ioa_pointer_reset(&input, expect_pointer);
	if(pointer_received != expected_count)
		exit(2);
	puts("PASS mouse buttons, double click, fractional wheel and touch/mouse grab ownership");
}

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
editor_has_color(uint32_t color)
{
	for(int y = 83; y < 222; y++)
		for(int x = 41; x < 361; x++)
			if(pixel(x, y) == color)
				return 1;
	return 0;
}

static int
visible(enum View view)
{
	if(pixel(500, 15) != Panel)
		return 0;
	if(view == Vmenu)
		return pixel(600, 65) == Menu;
	if(pixel(600, 65) != Desktop)
		return 0;
	if(view == Vcalc)
		return pixel(195, 78) == 0xffffff;
	if(view == VcalcKeysClosed)
		return visible(Vcalc) && pixel(300, 465) == Desktop;
	if(view == Vcalcmoved)
		return pixel(195, 78) == Desktop && pixel(415, 178) == 0xffffff;
	if(view == Vabout)
		return pixel(100, 65) != Desktop && pixel(195, 78) == Desktop;
	if(view == Vfiles)
		return pixel(380, 70) == 0xdce8f5 && pixel(330, 275) == 0xffffff;
	if(view == VfilesKeysClosed)
		return visible(Vfiles) && pixel(300, 465) == Desktop;
	if(view >= Veditor){
		if(view != VeditFileMenu && view != VeditPath && pixel(40, 82) != 0x009f9f)
			return 0;
		switch(view){
		case Veditor:
			return pixel(280, 160) == 0xffffff;
		case VeditFileMenu:
			return pixel(50, 126) == 0xdddddd;
		case VeditPath:
			return pixel(300, 258) == 0xdddddd;
		case VeditSource:
			return pixel(280, 160) == 0xffffff && pixel(300, 258) == Desktop &&
				editor_has_color(0x204bab) && editor_has_color(0x387141) &&
				editor_has_color(0x9b3223) && editor_has_color(0x86468c) &&
				editor_has_color(0x046782);
		case VeditFunctions:
			return pixel(280, 160) == 0xdddddd;
		case VeditJump:
			return pixel(280, 160) == 0xffffff && editor_has_color(0x046782) &&
				!editor_has_color(0x204bab);
		default:
			return 0;
		}
	}

	if(view == Vkeys)
		return pixel(300, 303) != Desktop && pixel(300, 465) != Desktop;
	return pixel(100, 65) == Desktop &&
		pixel(300, 465) == Desktop &&
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
			for(unsigned int y = 0; y < Height; y++){
				unsigned char rgb[Width * 3];
				for(unsigned int x = 0; x < Width; x++){
					const unsigned char *p = snapshot + (y * Width + x) * 4;
					rgb[3*x] = p[2];
					rgb[3*x+1] = p[1];
					rgb[3*x+2] = p[0];
				}
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
pointer_motion(int32_t action, int x, int y, int source, int buttons, float wheel)
{
	static IoaInput input;
	static int64_t time;
	AInputEvent event = {action, time += 100000000, 1, {{0,x,y}}, source, buttons, wheel};
	ioa_pointer_motion(&input, &event, Width, Height, xmouse_btn);
}

static void
motion(int32_t action, int x, int y)
{
	pointer_motion(action, x, y, AINPUT_SOURCE_TOUCHSCREEN, 0, 0);
}

static void
mouse_click(int x, int y)
{
	pointer_motion(AMOTION_EVENT_ACTION_DOWN, x, y, AINPUT_SOURCE_MOUSE, AMOTION_EVENT_BUTTON_PRIMARY, 0);
	usleep(100000);
	pointer_motion(AMOTION_EVENT_ACTION_UP, x, y, AINPUT_SOURCE_MOUSE, 0, 0);
}

static void
scroll(int x, int y, float delta)
{
	pointer_motion(AMOTION_EVENT_ACTION_SCROLL, x, y, AINPUT_SOURCE_MOUSE, 0, delta);
}

static void
tap(int x, int y)
{
	motion(AMOTION_EVENT_ACTION_DOWN, x, y);
	usleep(100000);
	motion(AMOTION_EVENT_ACTION_UP, x, y);
}

static void
drag(int x, int y, int tox, int toy)
{
	motion(AMOTION_EVENT_ACTION_DOWN, x, y);
	usleep(100000);
	motion(AMOTION_EVENT_ACTION_MOVE, tox, toy);
	usleep(100000);
	motion(AMOTION_EVENT_ACTION_UP, tox, toy);
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
	tap(40, 131);
	wait_files(file_path, home, 0, "opening Documents with a double tap");
	list = wait_files(file_list, 0, 0, "listing Documents");
	scroll(90, 175, -1);
	wait_files(file_list, list, 0, "scrolling down in the file list");
	scroll(90, 175, 1);
	wait_files(file_list, list, 1, "scrolling back to the first file");
	tap(40, 131);
	list = wait_files(file_list, list, 0, "selecting the first file");
	tap(40, 131);
	first = wait_files(file_preview, empty, 0, "previewing the first file with a double tap");
	mouse_click(40, 148);
	wait_files(file_list, list, 0, "selecting the second file");
	mouse_click(40, 148);
	second = wait_files(file_preview, first, 0, "previewing the second file");
	if(second == empty){
		fputs("Files cleared the preview instead of showing a second file\n", stderr);
		exit(1);
	}
	scroll(70, 286, -.5f);
	scroll(70, 286, -.5f);
	wait_files(file_preview, second, 0, "scrolling down in the preview");
	scroll(70, 286, 1);
	wait_files(file_preview, second, 1, "scrolling back to the top of the preview");
	puts("PASS mouse opened a file by double click and scrolled both Files panes");
	painted = capture("awm-files-preview.ppm", Vfiles, painted);
	tap(22, 102);
	wait_files(file_path, home, 1, "going up to Home");
	wait_files(file_preview, empty, 1, "clearing the preview");
	key(AKEYCODE_ENTER, 0);
	wait_files(file_path, home, 0, "opening Documents with Enter");
	key(AKEYCODE_DPAD_DOWN, 0);
	key(AKEYCODE_NUMPAD_ENTER, 0);
	wait_files(file_preview, second, 1, "opening the second entry with keyboard");
	key(AKEYCODE_DPAD_UP, 0);
	key(AKEYCODE_ENTER, 0);
	wait_files(file_preview, first, 1, "opening the first entry with keyboard");
	key(AKEYCODE_DEL, 0);
	wait_files(file_path, home, 1, "going up with Backspace");
	wait_files(file_preview, empty, 1, "clearing the preview from keyboard");
	puts("PASS keyboard navigated Files, opened both entries and returned to Home");
	tap(600, 19);
	painted = capture("awm-files-screen-keyboard.ppm", Vkeys, painted);
	tap(451, 464);                    /* Enter opens the active Documents row. */
	wait_files(file_path, home, 0, "opening Documents with the screen keyboard");
	tap(320, 464);
	tap(451, 464);                    /* Down + Enter opens the second file. */
	tap(516, 464);
	painted = capture("awm-files-screen-keyboard-closed.ppm", VfilesKeysClosed, painted);
	wait_files(file_preview, second, 1, "previewing from the screen keyboard");
	tap(600, 19);
	painted = capture("awm-files-screen-keyboard.ppm", Vkeys, painted);
	tap(385, 464);
	wait_files(file_path, home, 1, "going up with the screen keyboard");
	tap(516, 464);
	painted = capture("awm-files-screen-keyboard-closed.ppm", VfilesKeysClosed, painted);
	wait_files(file_preview, empty, 1, "clearing the preview after virtual Back");
	puts("PASS screen keyboard navigated Files and remained reusable after closing");
	tap(283, 103);
	capture("awm-files-closed.ppm", Vdesktop, painted);
	puts("PASS Files opened Documents and previewed a file by double tap, then returned to Home");
}

static void
type_path(const char *path)
{
	for(; *path; path++){
		int code, meta = 0;
		if(*path >= 'a' && *path <= 'z')
			code = AKEYCODE_A + *path - 'a';
		else if(*path >= 'A' && *path <= 'Z'){
			code = AKEYCODE_A + *path - 'A';
			meta = AMETA_SHIFT_ON;
		}else if(*path == '/')
			code = AKEYCODE_SLASH;
		else if(*path == '.')
			code = AKEYCODE_PERIOD;
		else
			abort();
		key(code, meta);
	}
}

static void
wait_editor_save(void)
{
	for(int attempt = 0; attempt < 120; attempt++){
		char line[256];
		FILE *file = fopen("/sdcard/Inferno/usr/inferno/Documents/zsample.b", "r");
		if(file != NULL){
			int complete = 0;
			while(fgets(line, sizeof(line), file) != NULL)
				if(strcmp(line, "\tcompletion_target;\n") == 0){
					complete = 1;
					break;
				}
			fclose(file);
			if(complete)
				return;
		}
		usleep(50000);
	}
	fputs("Edit did not save the completed identifier in shared storage\n", stderr);
	exit(1);
}

static void
test_editor(void)
{
	unsigned int painted;

	tap(27, 19);
	painted = capture("awm-editor-menu.ppm", Vmenu, 0);
	tap(390, 61);
	painted = capture("awm-editor-window.ppm", Veditor, painted);
	tap(20, 71);
	painted = capture("awm-editor-file-menu.ppm", VeditFileMenu, painted);
	tap(40, 108);
	painted = capture("awm-editor-open-path.ppm", VeditPath, painted);
	tap(180, 258);
	type_path("/usr/inferno/Documents/zsample.b");
	key(AKEYCODE_ENTER, 0);
	painted = capture("awm-editor-source.ppm", VeditSource, painted);

	key(AKEYCODE_G, AMETA_CTRL_ON);
	painted = capture("awm-editor-functions.ppm", VeditFunctions, painted);
	key(AKEYCODE_DPAD_DOWN, 0);
	key(AKEYCODE_ENTER, 0);
	painted = capture("awm-editor-jump.ppm", VeditJump, painted);
	key(AKEYCODE_G, AMETA_CTRL_ON);
	painted = capture("awm-editor-functions-again.ppm", VeditFunctions, painted);
	key(AKEYCODE_ENTER, 0);
	scroll(200, 125, 6.0f);
	painted = capture("awm-editor-first-function.ppm", VeditSource, painted);

	tap(123, 172);                   /* The partial identifier on line 7. */
	key(AKEYCODE_MOVE_END, 0);
	key(AKEYCODE_DPAD_LEFT, 0);
	key(AKEYCODE_T, AMETA_CTRL_ON);
	tap(20, 71);
	painted = capture("awm-editor-save-menu.ppm", VeditFileMenu, painted);
	tap(27, 129);
	wait_editor_save();
	tap(350, 52);
	capture("awm-editor-closed.ppm", Vdesktop, painted);
	puts("PASS native Edit opened a Limbo file, highlighted it, jumped to a function, completed an identifier and saved it");
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
	key(AKEYCODE_C, 0);
	wait_value(zero, 1);
	key(AKEYCODE_NUMPAD_2, 0);
	two = wait_value(zero, 0);
	key(AKEYCODE_EQUALS, AMETA_SHIFT_ON);
	key(AKEYCODE_3, 0);
	three = wait_value(two, 0);
	if(three == five){
		fputs("Calculator displayed the result before equals\n", stderr);
		exit(1);
	}
	key(AKEYCODE_ENTER, 0);
	wait_value(five, 1);
	puts("PASS keyboard Calculator 2 + 3 equals the directly entered 5");

	tap(185, 52);
	painted = capture("awm-minimized.ppm", Vtask, painted);
	key(AKEYCODE_C, 0);
	tap(120, 19);
	painted = capture("awm-restored.ppm", Vcalc, painted);
	wait_value(five, 1);
	key(AKEYCODE_C, 0);
	wait_value(zero, 1);
	key(AKEYCODE_NUMPAD_8, 0);
	key(AKEYCODE_NUMPAD_SUBTRACT, 0);
	key(AKEYCODE_NUMPAD_3, 0);
	key(AKEYCODE_NUMPAD_ENTER, 0);
	wait_value(five, 1);
	puts("PASS minimized Calculator ignored keys and calculated 8 - 3 after restoring focus");
	drag(40, 51, 260, 151);
	painted = capture("awm-calculator-dragged.ppm", Vcalcmoved, painted);
	drag(260, 151, 40, 51);
	painted = capture("awm-calculator-dragged-back.ppm", Vcalc, painted);
	wait_value(five, 1);
	puts("PASS touch moved the Calculator window and kept its value");
	tap(600, 19);
	painted = capture("awm-screen-keyboard.ppm", Vkeys, painted);
	tap(600, 19);                     /* Opening again must keep one window. */
	tap(253, 399);                    /* c */
	wait_value(zero, 1);
	tap(159, 304);                    /* 2 */
	wait_value(two, 1);
	tap(118, 430);                    /* + */
	tap(205, 304);                    /* 3 */
	wait_value(three, 1);
	tap(451, 464);                    /* Enter */
	wait_value(five, 1);
	tap(516, 464);
	painted = capture("awm-screen-keyboard-closed.ppm", VcalcKeysClosed, painted);
	puts("PASS screen keyboard computed 2 + 3 in the focused Calculator and opened only once");
	tap(110, 283);
	painted = capture("awm-calculator-closed.ppm", Vdesktop, painted);
	tap(27, 19);
	painted = capture("awm-menu.ppm", Vmenu, painted);
	tap(186, 61);
	painted = capture("awm-about.ppm", Vabout, painted);
	tap(65, 125);
	painted = capture("awm-closed.ppm", Vdesktop, painted);
	test_files(painted);
	test_editor();
	exit(0);
}

int
main(int argc, char **argv)
{
	pthread_t worker;
	if(argc != 3)
		return 2;
	test_touch();
	test_mouse();
	test_keymap();
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
	const char *panel = "/sdcard/Inferno/dis/wm/awmpanel-keyboard.dis";
	const char *oldpanel = "/sdcard/Inferno/dis/wm/awmpanel.dis";
	const char *keys = "/sdcard/Inferno/dis/wm/awmkeys.dis";
	const char marker[] = "Previous AWM panel";
	const char local[] = "Local document preserved by AWM\n"
		"01 local line\n02 local line\n03 local line\n04 local line\n"
		"05 local line\n06 local line\n07 local line\n08 local line\n"
		"09 local line\n10 local line\n11 local line\n12 local line\n";
	FILE *file = fopen(oldpanel, "w");
	if(file == NULL || fputs(marker, file) < 0 || fclose(file) != 0 ||
	   unlink(keys) != 0 || ioa_prepare_storage(NULL) != -1 ||
	   ioa_prepare_storage(&manager) != 0 || ioa_prepare_storage(NULL) != 0){
		perror("Restore missing screen keyboard");
		return 2;
	}
	file = fopen(document, "w");
	if(file == NULL || fputs(local, file) < 0 || fclose(file) != 0 ||
	   unlink(panel) != 0 || ioa_prepare_storage(NULL) != -1 ||
	   ioa_prepare_storage(&manager) != 0 || ioa_prepare_storage(NULL) != 0){
		perror("Restore missing AWM panel");
		return 2;
	}
	char content[sizeof(local)-1];
	file = fopen(document, "r");
	if(file == NULL || fread(content, 1, sizeof(content), file) != sizeof(content) ||
	   memcmp(content, local, sizeof(content)) != 0 || fgetc(file) != EOF || fclose(file) != 0){
		fputs("AWM panel repair overwrote a local document\n", stderr);
		return 2;
	}
	puts("PASS shared AWM panel repair preserved the user's document");
	char previous[sizeof(marker)-1];
	file = fopen(oldpanel, "r");
	if(file == NULL || fread(previous, 1, sizeof(previous), file) != sizeof(previous) ||
	   memcmp(previous, marker, sizeof(previous)) != 0 || fgetc(file) != EOF || fclose(file) != 0){
		fputs("Screen keyboard installation overwrote the previous AWM panel\n", stderr);
		return 2;
	}
	puts("PASS keyboard and new AWM panel installed beside the previous panel");
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
