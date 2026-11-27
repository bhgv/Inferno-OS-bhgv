/* Run Android amain and real Tk/Draw against a headless Bionic framebuffer. */
#include <android/asset_manager.h>
#include <android/keycodes.h>
#include <android/log.h>
#include <android/native_window.h>
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
#include "inferno_fb_pixels.h"
#include "../../include/keyboard.h"

enum { Width = 640, Height = 480, Panel = 0x212b38, Menu = 0x2c3c51, Tools = 0x344355, Desktop = 0x777777 };
enum View { Vdesktop, Vmenu, Vtools, Vcalc, VcalcKeysClosed, Vcalcmoved, Vtask, Vabout,
            Vfiles, VfilesKeysClosed, Vkeys, Veditor, VeditFileMenu,
            VeditPicker, VeditConfirm, VeditSource, VeditFunctions, VeditJump,
            VfilesEditor, VfilesEditorSave, VtoolShell, VtoolProc, VtoolDebug, VtoolHelp, VhelpFromShell,
            VportraitDesktop, VportraitFiles, VportraitKeys, VclassicCalc, VclassicHelp };
typedef struct Area Area;
struct Area { int x0, y0, x1, y1; };
static const IoaViewport wide_view = {{0, 0, Width, Height}, Width, Height};
static const Area file_path = { 8, 65, 340, 80 };
static const Area file_preview = { 8, 260, 340, 324 };
static const Area file_list = { 8, 122, 340, 160 };
static const Area picker_path = { 8, 250, 340, 264 };
static const Area picker_list = { 8, 305, 340, 438 };
int Xsize = Width, Ysize = Height;
unsigned char *xscreendata;
static unsigned char *snapshot;
static IoaViewport input_view;
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
	{6,472,1}, {0,479,1}, {0,479,0},
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
		{AMOTION_EVENT_ACTION_DOWN, 0, 1, {{6,6,472}}},
		{AMOTION_EVENT_ACTION_MOVE, 0, 1, {{6,-12,800}}},
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
		if(!ioa_touch_motion(&touch, &event, &wide_view, expect_pointer))
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
		if(!ioa_pointer_motion(&input, &event, &wide_view, expect_pointer))
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
	memcpy(snapshot, xscreendata, (size_t)Xsize * Ysize * 4);
	flushes++;
	pthread_mutex_unlock(&frame_lock);
}

static uint32_t
pixel(int x, int y)
{
	const unsigned char *p = snapshot + (y * Xsize + x) * 4;
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
	if(view == Vmenu)
		return pixel(Xsize - 2, 15) == Menu;
	if(view == Vtools)
		return pixel(Xsize - 2, 15) == Tools;
	if(pixel(Xsize / 2, 1) != Panel)
		return 0;
	if(view == VportraitDesktop)
		return pixel(Xsize / 2, 65) == Desktop && pixel(Xsize / 2, Ysize - 14) == Desktop &&
			pixel(27, 12) == 0xee0000;
	if(view == VportraitFiles)
		return pixel(380, 70) == 0xdce8f5 && pixel(Xsize / 2, Ysize - 14) == Desktop;
	if(view == VportraitKeys)
		return pixel(380, 70) == 0xdce8f5 && pixel(Xsize / 2, Ysize - 220) != Desktop &&
			pixel(Xsize / 2, Ysize - 14) != Desktop;
	if(pixel(600, 65) != Desktop)
		return 0;
	if(view == VtoolShell)
		return pixel(180, 52) == 0x0000cc && pixel(300, 150) == 0xffffff;
	if(view == VtoolProc)
		return pixel(180, 72) == 0x0000cc && pixel(300, 265) == 0xdddddd;
	if(view == VtoolDebug)
		return pixel(180, 92) == 0x0000cc && pixel(300, 265) == 0xffffff;
	if(view == VtoolHelp)
		return pixel(180, 52) == 0x0000cc && pixel(320, 260) == 0xffffff;
	if(view == VhelpFromShell)
		return pixel(180, 72) == 0x0000cc && pixel(320, 280) == 0xffffff;
	if(view == Vcalc)
		return pixel(195, 78) == 0xffffff;
	if(view == VclassicCalc)
		return pixel(175, 52) == 0x666699 && pixel(150, 100) == 0xffffff;
	if(view == VclassicHelp)
		return pixel(180, 52) == 0x666699 && pixel(320, 260) == 0xffffff;
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
	if(view == VfilesEditor)
		return pixel(40, 102) == 0x009f9f && pixel(280, 182) == 0xffffff &&
			editor_has_color(0x046782);
	if(view == VfilesEditorSave)
		return pixel(50, 146) == 0xdddddd && editor_has_color(0x046782);

	if(view >= Veditor){
		if(view != VeditFileMenu && pixel(40, 82) != 0x009f9f)
			return 0;
		switch(view){
		case Veditor:
			return pixel(280, 160) == 0xffffff;
		case VeditFileMenu:
			return pixel(50, 126) == 0xdddddd;
		case VeditPicker:
			return pixel(380, 254) == 0xdce8f5 && pixel(300, 350) == 0xdddddd;
		case VeditConfirm:
			return pixel(390, 145) == 0xdddddd;
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
		pixel(27, 12) == 0xee0000 &&
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
		int ready = painted > after && visible(view);
		if(ready || i == 239){
			stream = fopen(path, "wb");
			if(stream == NULL)
				exit(2);
			fprintf(stream, "P6\n%d %d\n255\n", Xsize, Ysize);
			for(int y = 0; y < Ysize; y++){
				unsigned char rgb[Xsize * 3];
				for(int x = 0; x < Xsize; x++){
					const unsigned char *p = snapshot + (y * Xsize + x) * 4;
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
			if(!ready)
				break;
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
	ARect frame = input_view.frame;
	float device_x = frame.left + (x + .5f) * (frame.right - frame.left) / Xsize;
	float device_y = frame.top + (y + .5f) * (frame.bottom - frame.top) / Ysize;
	AInputEvent event = {action, time += 100000000, 1, {{0,device_x,device_y}}, source, buttons, wheel};
	ioa_pointer_motion(&input, &event, &input_view, xmouse_btn);
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
wait_region(enum View view, Area area, uint64_t reference, int equal, const char *action)
{
	for(int attempt = 0; attempt < 120; attempt++){
		uint64_t hash = UINT64_C(14695981039346656037);
		pthread_mutex_lock(&frame_lock);
		for(int y = area.y0; y < area.y1; y++)
			for(int x = area.x0; x < area.x1; x++)
				hash = (hash ^ pixel(x, y)) * UINT64_C(1099511628211);
		int matched = visible(view) && (hash == reference) == equal;
		pthread_mutex_unlock(&frame_lock);
		if(matched)
			return hash;
		usleep(50000);
	}
	fprintf(stderr, "AWM did not repaint as expected after %s\n", action);
	exit(1);
}

static uint64_t
wait_files(Area area, uint64_t reference, int equal, const char *action)
{
	return wait_region(Vfiles, area, reference, equal, action);
}

static void
test_files(unsigned int painted)
{
	uint64_t home, empty, list, first, second;
	tap(27, 19);
	painted = capture("awm-menu.ppm", Vmenu, painted);
	tap(301, 20);
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
	tap(540, 374);                    /* Enter opens the active Documents row. */
	wait_files(file_path, home, 0, "opening Documents with the screen keyboard");
	tap(420, 464);
	tap(540, 374);                    /* Down + Enter opens the second file. */
	tap(528, 464);
	painted = capture("awm-files-screen-keyboard-closed.ppm", VfilesKeysClosed, painted);
	wait_files(file_preview, second, 1, "previewing from the screen keyboard");
	tap(600, 19);
	painted = capture("awm-files-screen-keyboard.ppm", Vkeys, painted);
	tap(551, 344);
	wait_files(file_path, home, 1, "going up with the screen keyboard");
	tap(528, 464);
	painted = capture("awm-files-screen-keyboard-closed.ppm", VfilesKeysClosed, painted);
	wait_files(file_preview, empty, 1, "clearing the preview after virtual Back");
	puts("PASS screen keyboard navigated Files and remained reusable after closing");
	tap(283, 103);
	capture("awm-files-closed.ppm", Vdesktop, painted);
	puts("PASS Files opened Documents and previewed a file by double tap, then returned to Home");
}

static void
wait_editor_save(const char *path)
{
	for(int attempt = 0; attempt < 120; attempt++){
		char line[256];
		FILE *file = fopen(path, "r");
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
	uint64_t home, documents, first_page;

	tap(27, 19);
	painted = capture("awm-editor-menu.ppm", Vmenu, 0);
	tap(369, 20);
	painted = capture("awm-editor-window.ppm", Veditor, painted);
	tap(20, 71);
	painted = capture("awm-editor-file-menu.ppm", VeditFileMenu, painted);
	tap(40, 108);
	painted = capture("awm-editor-open-picker.ppm", VeditPicker, painted);
	home = wait_region(VeditPicker, picker_path, 0, 0, "opening the chooser at Home");
	tap(55, 314);
	tap(160, 285);
	documents = wait_region(VeditPicker, picker_path, home, 0, "opening Documents in the chooser");
	first_page = wait_region(VeditPicker, picker_list, 0, 0, "listing Documents in the chooser");
	scroll(150, 370, -9.0f);
	scroll(150, 370, -9.0f);
	wait_region(VeditPicker, picker_list, first_page, 0, "scrolling to the last file in the chooser");
	tap(60, 426);                   /* zsample.b is the last row. */
	tap(160, 285);
	painted = capture("awm-editor-source.ppm", VeditSource, painted);

	tap(600, 19);
	painted = capture("awm-editor-keyboard.ppm", Vkeys, painted);
	tap(96, 374);                    /* Ctrl */
	tap(283, 374);                   /* g */
	painted = capture("awm-editor-functions.ppm", VeditFunctions, painted);
	tap(420, 464);                   /* Down */
	tap(540, 374);                   /* Enter */
	painted = capture("awm-editor-jump.ppm", VeditJump, painted);
	tap(96, 374);
	tap(283, 374);
	painted = capture("awm-editor-functions-again.ppm", VeditFunctions, painted);
	tap(540, 374);
	scroll(200, 125, 6.0f);
	painted = capture("awm-editor-first-function.ppm", VeditSource, painted);

	const Area caret_area = {133, 164, 153, 179};
	const Area completed_area = {168, 164, 252, 179};
	uint64_t before = wait_region(VeditSource, caret_area, 0, 0, "recording the identifier before moving the caret");
	tap(141, 172);
	wait_region(VeditSource, caret_area, before, 0, "placing the caret in the unique prefix");
	before = wait_region(VeditSource, completed_area, 0, 0, "recording the empty end of the line");
	tap(96, 374);                    /* Ctrl */
	tap(265, 344);                   /* t */
	wait_region(VeditSource, completed_area, before, 0, "completing the identifier");

	tap(210, 129);                   /* Append to the comment on line 4. */
	tap(108, 404);
	painted = capture("awm-editor-keyboard-shift.ppm", Vkeys, painted);
	tap(465, 344);                   /* { */
	tap(108, 404);
	tap(500, 344);                   /* } */
	tap(465, 344);                   /* [ */
	tap(500, 344);                   /* ] */
	tap(108, 404);
	tap(442, 374);                   /* : */
	tap(461, 374);                   /* ; */

	tap(528, 464);                   /* Close */
	painted = capture("awm-editor-keyboard-closed.ppm", VeditSource, painted);
	tap(20, 71);
	painted = capture("awm-editor-reopen-menu.ppm", VeditFileMenu, painted);
	tap(40, 108);
	painted = capture("awm-editor-confirm.ppm", VeditConfirm, painted);
	key(AKEYCODE_ENTER, 0);
	painted = capture("awm-editor-reopen-picker.ppm", VeditPicker, painted);
	wait_region(VeditPicker, picker_path, documents, 1, "reopening in the current document directory");
	tap(286, 285);
	painted = capture("awm-editor-picker-cancelled.ppm", VeditSource, painted);
	tap(20, 71);
	painted = capture("awm-editor-save-menu.ppm", VeditFileMenu, painted);
	tap(27, 129);
	wait_editor_save("/sdcard/Inferno/usr/inferno/Documents/zsample.b");
	tap(350, 52);
	capture("awm-editor-closed.ppm", Vdesktop, painted);
	puts("PASS stock screen keyboard typed Limbo punctuation, selected functions and completed an identifier in Edit");
}

static void
test_files_editor(void)
{
	unsigned int painted;
	uint64_t home, directory, selection, before;
	const Area source_preview = { 8, 260, 340, 359 };

	tap(27, 19);
	painted = capture("awm-files-editor-menu.ppm", Vmenu, 0);
	tap(301, 20);
	painted = capture("awm-files-editor-home.ppm", Vfiles, painted);
	home = wait_files(file_path, 0, 0, "opening Home for Edit");
	tap(62, 148);				/* Limbo is the second directory. */
	tap(332, 103);				/* Edit should leave the directory selected. */
	wait_files(file_path, home, 1, "using Edit on a directory");
	tap(160, 103);
	directory = wait_files(file_path, home, 0, "opening the Limbo directory");
	tap(62, 131);
	selection = wait_files(file_list, 0, 0, "selecting the Limbo source");
	before = wait_files(source_preview, 0, 0, "reading the empty preview");
	tap(332, 103);
	painted = capture("awm-files-editor-open.ppm", VfilesEditor, painted);
	before = wait_files(source_preview, before, 0, "previewing the file launched in Edit");

	tap(123, 192);				/* The prefix on line 7 in the child editor. */
	key(AKEYCODE_MOVE_END, 0);
	key(AKEYCODE_DPAD_LEFT, 0);
	key(AKEYCODE_T, AMETA_CTRL_ON);
	tap(20, 91);
	painted = capture("awm-files-editor-save.ppm", VfilesEditorSave, painted);
	tap(27, 149);
	wait_editor_save("/sdcard/Inferno/usr/inferno/Limbo/example ;[one].b");
	tap(350, 72);
	wait_files(source_preview, before, 0, "refreshing the preview after closing Edit");
	painted = capture("awm-files-editor-return.ppm", Vfiles, painted);
	wait_files(file_path, directory, 1, "preserving the selected directory");
	wait_files(file_list, selection, 1, "preserving the selected file");
	tap(283, 103);
	capture("awm-files-editor-closed.ppm", Vdesktop, painted);
	puts("PASS Files opened the selected source in Edit and refreshed its preview after save and close");
}

static void
shell_command(const char *line)
{
	for(; *line; line++){
		if(*line >= 'a' && *line <= 'z')
			key(AKEYCODE_A + *line - 'a', 0);
		else if(*line >= '0' && *line <= '9')
			key(AKEYCODE_0 + *line - '0', 0);
		else if(*line == ' ')
			key(AKEYCODE_SPACE, 0);
		else if(*line == '/')
			key(AKEYCODE_SLASH, 0);
		else if(*line == '.')
			key(AKEYCODE_PERIOD, 0);
		else if(*line == '(')
			key(AKEYCODE_9, AMETA_SHIFT_ON);
		else if(*line == ')')
			key(AKEYCODE_0, AMETA_SHIFT_ON);
		else if(*line == '-')
			key(AKEYCODE_MINUS, 0);
		else
			abort();
	}
	key(AKEYCODE_ENTER, 0);
}
static void
wait_shell_echo(void)
{
	for(int attempt = 0; attempt < 120; attempt++){
		int equal = 1, ink = 0;
		pthread_mutex_lock(&frame_lock);
		/* Compare the argument in the command transcript to Echo's output. */
		for(int y = 0; y < 14; y++)
			for(int x = 0; x < 56; x++){
				uint32_t output = pixel(x + 2, y + 92);
				equal &= output == pixel(x + 58, y + 78);
				ink |= output == 0;
			}
		pthread_mutex_unlock(&frame_lock);
		if(equal && ink)
			return;
		usleep(50000);
	}
	fputs("Shell did not render the output from the Inferno Echo module\n", stderr);
	exit(1);
}

static void*
observe_tools(void *unused)
{
	(void)unused;
	const Area stack = {10, 156, 295, 240};
	const Area debug_status = {1, 131, 287, 145};
	const Area pwd_output = {2, 121, 104, 133};
	unsigned int painted;
	uint64_t before, blank;

	painted = capture("awm-dev-desktop.ppm", Vdesktop, 0);
	tap(27, 19);
	painted = capture("awm-dev-apps.ppm", Vmenu, painted);
	tap(207, 20);
	painted = capture("awm-dev-tools.ppm", Vtools, painted);
	tap(100, 20);
	painted = capture("awm-dev-shell.ppm", VtoolShell, painted);
	shell_command("echo inferno");
	wait_shell_echo();
	painted = capture("awm-dev-shell-echo.ppm", VtoolShell, painted);
	blank = wait_region(VtoolShell, pwd_output, 0, 0, "recording the empty output line");

	tap(27, 19);
	painted = capture("awm-dev-apps-again.ppm", Vmenu, painted);
	tap(207, 20);
	painted = capture("awm-dev-tools-again.ppm", Vtools, painted);
	tap(189, 20);
	painted = capture("awm-dev-processes.ppm", VtoolProc, painted);
	tap(55, 225);                   /* The idle Shell worker is listed as Awmsh. */
	tap(100, 98);                   /* Debug the selected process. */
	painted = capture("awm-dev-debugger.ppm", VtoolDebug, painted);
	before = wait_region(VtoolDebug, stack, 0, 0, "reading the selected process stack");

	uint64_t attached = wait_region(VtoolDebug, debug_status, 0, 0, "recording the attached debugger status");
	tap(155, 116);                  /* Step, then wake the Shell blocked in alt. */
	wait_region(VtoolDebug, debug_status, attached, 0, "waiting for the step request");
	tap(32, 52);
	tap(215, 273);                  /* Raise the debugger from its visible bottom edge. */
	wait_region(VtoolDebug, stack, before, 0, "stepping the process and refreshing its stack");
	painted = capture("awm-dev-debugger-step.ppm", VtoolDebug, painted);
	tap(215, 115);                  /* Continue the Shell. */
	tap(32, 52);
	painted = capture("awm-dev-shell-continued.ppm", VtoolShell, painted);
	shell_command("pwd");
	wait_region(VtoolShell, pwd_output, blank, 0, "running a command after continuing");
	puts("PASS tree launcher, Inferno command output, live process list, debugger attach, step and continue");
	exit(0);
}

static void*
observe_help(void *unused)
{
	(void)unused;
	const Area manual = {2, 106, 339, 245};
	const Area shell_manual = {2, 126, 339, 265};
	const Area command_output = {2, 79, 290, 92};
	unsigned int painted = capture("awm-help-desktop.ppm", Vdesktop, 0);
	tap(27, 19);
	painted = capture("awm-help-apps.ppm", Vmenu, painted);
	tap(207, 20);
	painted = capture("awm-help-tools.ppm", Vtools, painted);
	tap(292, 20);
	painted = capture("awm-help-home.ppm", VtoolHelp, painted);
	uint64_t home = wait_region(VtoolHelp, manual, 0, 0, "opening the Help home page");
	shell_command("wm/man");
	uint64_t man = wait_region(VtoolHelp, manual, home, 0, "opening the wm/man alias");
	painted = capture("awm-help-man.ppm", VtoolHelp, painted);
	tap(280, 77);
	wait_region(VtoolHelp, manual, home, 1, "returning to the Help home page");
	shell_command("man(1)");
	wait_region(VtoolHelp, manual, man, 1, "opening the same manual page by section");
	tap(280, 77);
	wait_region(VtoolHelp, manual, home, 1, "resetting the manual search");
	shell_command("sys(2)");
	uint64_t sys = wait_region(VtoolHelp, manual, home, 0, "opening an indexed Sys manual page");
	if(sys == man){
		fputs("Help opened the same page for man(1) and sys(2)\n", stderr);
		exit(1);
	}
	painted = capture("awm-help-sys.ppm", VtoolHelp, painted);
	tap(373, 52);
	painted = capture("awm-help-closed.ppm", Vdesktop, painted);
	tap(27, 19);
	painted = capture("awm-commands-apps.ppm", Vmenu, painted);
	tap(207, 20);
	painted = capture("awm-commands-tools.ppm", Vtools, painted);
	tap(100, 20);
	painted = capture("awm-commands-shell.ppm", VtoolShell, painted);

	shell_command("clear");
	uint64_t blank = wait_region(VtoolShell, command_output, 0, 0, "clearing the Shell");
	const char *commands[][2] = {
		{"echo man", "basename /man/1/man"},
		{"echo /man", "cleanname /man/./1/.."},
		{"echo inferno command output", "stream /tmp/command-check.txt"},
	};
	for(size_t i = 0; i < sizeof(commands)/sizeof(commands[0]); i++){
		shell_command(commands[i][0]);
		uint64_t expected = wait_region(VtoolShell, command_output, blank, 0, commands[i][0]);
		shell_command("clear");
		wait_region(VtoolShell, command_output, blank, 1, "clearing before the comparison");
		shell_command(commands[i][1]);
		wait_region(VtoolShell, command_output, expected, 1, commands[i][1]);
		shell_command("clear");
		wait_region(VtoolShell, command_output, blank, 1, "clearing after the comparison");
	}
	shell_command("mkdir awm-help-check");
	shell_command("help wm/man");
	wait_region(VhelpFromShell, shell_manual, man, 1, "opening the same manual page from Shell");
	capture("awm-commands-man.ppm", VhelpFromShell, painted);
	puts("PASS indexed Inferno Help, Shell Help, stock command output and directory creation");
	exit(0);
}

static void*
observe_classic(void *unused)
{
	(void)unused;
	const Area display = {120, 81, 208, 114};
	const Area manual = {2, 118, 339, 242};
	unsigned int painted = capture("classic-desktop.ppm", Vdesktop, 0);
	tap(27, 19);
	painted = capture("classic-apps.ppm", Vmenu, painted);
	tap(110, 20);
	painted = capture("classic-calculator.ppm", VclassicCalc, painted);
	uint64_t zero = wait_region(VclassicCalc, display, 0, 0, "starting the ARMv7 Calculator");
	tap(84, 180);
	uint64_t five = wait_region(VclassicCalc, display, zero, 0, "entering five by touch");
	key(AKEYCODE_C, 0);
	wait_region(VclassicCalc, display, zero, 1, "clearing the ARMv7 Calculator");
	key(AKEYCODE_2, 0);
	key(AKEYCODE_EQUALS, AMETA_SHIFT_ON);
	key(AKEYCODE_3, 0);
	key(AKEYCODE_ENTER, 0);
	wait_region(VclassicCalc, display, five, 1, "calculating two plus three with the Android keyboard");
	tap(203, 54);
	painted = capture("classic-calculator-closed.ppm", Vdesktop, painted);
	tap(27, 19);
	painted = capture("classic-apps-again.ppm", Vmenu, painted);
	tap(207, 20);
	painted = capture("classic-tools.ppm", Vtools, painted);
	tap(292, 20);
	painted = capture("classic-help.ppm", VclassicHelp, painted);
	uint64_t home = wait_region(VclassicHelp, manual, 0, 0, "opening the ARMv7 Help home page");
	shell_command("wm/man");
	wait_region(VclassicHelp, manual, home, 0, "opening the Inferno manual from the shared tree");
	capture("classic-man.ppm", VclassicHelp, painted);
	puts("PASS ARMv7 classic Dis desktop, Calculator touch and key input, and indexed Help");
	exit(0);
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
	tap(110, 20);
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
	tap(239, 404);                    /* c */
	wait_value(zero, 1);
	tap(158, 314);                    /* 2 */
	wait_value(two, 1);
	tap(108, 404);                    /* Shift */
	tap(496, 314);                    /* + */
	tap(191, 314);                    /* 3 */
	wait_value(three, 1);
	tap(540, 374);                    /* Enter */
	wait_value(five, 1);
	tap(528, 464);
	painted = capture("awm-screen-keyboard-closed.ppm", VcalcKeysClosed, painted);
	puts("PASS screen keyboard computed 2 + 3 in the focused Calculator and opened only once");
	tap(110, 283);
	painted = capture("awm-calculator-closed.ppm", Vdesktop, painted);
	tap(27, 19);
	painted = capture("awm-menu.ppm", Vmenu, painted);
	tap(207, 20);
	painted = capture("awm-tools.ppm", Vtools, painted);
	tap(365, 20);
	painted = capture("awm-about.ppm", Vabout, painted);
	tap(65, 125);
	painted = capture("awm-closed.ppm", Vdesktop, painted);
	test_files(painted);
	test_files_editor();
	test_editor();
	exit(0);
}

static void
test_viewport(void)
{
	int width, height;
	ARect bounds = ioa_content_bounds(945, 2048, (ARect){0, 90, 945, 1970});
	if(!ioa_screen_size(bounds, 420, &width, &height) || width != 416 || height != 828)
		abort();
	ARect resized = {0, 90, 945, 1845};
	IoaViewport fitted = ioa_viewport(resized, width, height);
	if(fitted.frame.top != resized.top || fitted.frame.bottom != resized.bottom ||
	   fitted.frame.left <= resized.left || fitted.frame.right >= resized.right)
		abort();

	IoaViewport view = ioa_viewport((ARect){2, 1, 6, 5}, 2, 2);
	const unsigned char source[16] = {1, 2, 3, 0, 4, 5, 6, 0, 7, 8, 9, 0, 10, 11, 12, 0};
	unsigned char target[6][9][4];
	memset(target, 0xa5, sizeof(target));
	ioa_fb_copy(target, 8, 6, 9, WINDOW_FORMAT_RGBX_8888, source, &view);
	for(int y = 0; y < 6; y++)
		for(int x = 0; x < 9; x++){
			const unsigned char *dest = target[y][x];
			if(x == 8){
				if(memcmp(dest, "\245\245\245\245", 4) != 0)
					abort();
			}else if(ioa_viewport_contains(&view, x, y)){
				const unsigned char *pixel = source + 4 * (((y - 1) / 2) * 2 + (x - 2) / 2);
				if(dest[0] != pixel[2] || dest[1] != pixel[1] || dest[2] != pixel[0] || dest[3] != 255)
					abort();
			}else if(memcmp(dest, "\0\0\0\377", 4) != 0)
				abort();
		}
	puts("PASS density-based desktop sizing, content insets, resizing, scaled pixels and untouched stride padding");
}

static void*
observe_portrait(void *unused)
{
	(void)unused;
	unsigned int painted;
	uint64_t home;

	painted = capture("portrait-desktop.ppm", VportraitDesktop, 0);
	tap(27, 19);
	painted = capture("portrait-apps.ppm", Vmenu, painted);
	tap(301, 20);
	painted = capture("portrait-files.ppm", VportraitFiles, painted);
	home = wait_region(VportraitFiles, file_path, 0, 0, "opening Files in portrait");
	tap(27, 19);
	painted = capture("portrait-apps-over-files.ppm", Vmenu, painted);
	wait_region(Vmenu, file_path, home, 1, "keeping the Files path visible under Apps");
	tap(27, 19);
	painted = capture("portrait-files-after-apps.ppm", VportraitFiles, painted);
	wait_region(VportraitFiles, file_path, home, 1, "keeping the Files path below Apps");

	tap(Xsize - 40, 20);
	painted = capture("portrait-keyboard.ppm", VportraitKeys, painted);
	tap(374, Ysize - 140);
	wait_region(VportraitKeys, file_path, home, 0, "opening Documents with the scaled bottom keyboard");
	tap(365, Ysize - 20);
	capture("portrait-keyboard-closed.ppm", VportraitFiles, painted);
	puts("PASS portrait Apps, Files and scaled keyboard with Android navigation insets");
	exit(0);
}

int
main(int argc, char **argv)
{
	pthread_t worker;
	int portrait = argc == 4 && strcmp(argv[3], "portrait") == 0;
	int tools = argc == 4 && strcmp(argv[3], "tools") == 0;
	int help = argc == 4 && strcmp(argv[3], "help") == 0;
	int classic = argc == 4 && strcmp(argv[3], "classic") == 0;
	if(argc != 3 && !portrait && !tools && !help && !classic)
		return 2;
	test_viewport();
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
	const char *manual = "/sdcard/Inferno/man/1/INDEX";
	const char *oldpanel = "/sdcard/Inferno/dis/wm/awmpanel.dis";
	const char *keys = "/sdcard/Inferno/dis/wm/keyboard.dis";
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
	if(unlink(manual) != 0 || ioa_prepare_storage(NULL) != -1 ||
	   ioa_prepare_storage(&manager) != 0 || ioa_prepare_storage(NULL) != 0){
		perror("Restore missing manual pages in an existing shared tree");
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
	input_view = wide_view;
	if(portrait){
		ARect bounds = ioa_content_bounds(945, 2048, (ARect){0, 90, 945, 1970});
		if(!ioa_screen_size(bounds, 420, &Xsize, &Ysize))
			return 2;
		input_view = ioa_viewport(bounds, Xsize, Ysize);
	}
	xscreendata = calloc((size_t)Xsize * Ysize, 4);
	snapshot = calloc((size_t)Xsize * Ysize, 4);
	void *(*scenario)(void *) = observe;
	if(portrait)
		scenario = observe_portrait;
	if(tools)
		scenario = observe_tools;
	if(help)
		scenario = observe_help;
	if(classic)
		scenario = observe_classic;
	if(xscreendata == NULL || snapshot == NULL || pthread_create(&worker, NULL, scenario, NULL) != 0)
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
