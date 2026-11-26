#include "dat.h"
#include "fns.h"
#include "keyboard.h"
#include <android/input.h>
#include <android/keycodes.h>

typedef struct Key Key;
struct Key {
	int code;
	int plain;
	int shifted;
};

static const Key keys[] = {
	{AKEYCODE_GRAVE, '`', '~'}, {AKEYCODE_MINUS, '-', '_'},
	{AKEYCODE_EQUALS, '=', '+'}, {AKEYCODE_LEFT_BRACKET, '[', '{'},
	{AKEYCODE_RIGHT_BRACKET, ']', '}'}, {AKEYCODE_BACKSLASH, '\\', '|'},
	{AKEYCODE_SEMICOLON, ';', ':'}, {AKEYCODE_APOSTROPHE, '\'', '"'},
	{AKEYCODE_COMMA, ',', '<'}, {AKEYCODE_PERIOD, '.', '>'},
	{AKEYCODE_SLASH, '/', '?'}, {AKEYCODE_SPACE, ' ', ' '},
	{AKEYCODE_AT, '@', '@'}, {AKEYCODE_STAR, '*', '*'},
	{AKEYCODE_POUND, '#', '#'}, {AKEYCODE_PLUS, '+', '+'},
	{AKEYCODE_NUMPAD_ADD, '+', '+'}, {AKEYCODE_NUMPAD_SUBTRACT, '-', '-'},
	{AKEYCODE_NUMPAD_MULTIPLY, '*', '*'}, {AKEYCODE_NUMPAD_DIVIDE, '/', '/'},
	{AKEYCODE_NUMPAD_DOT, '.', '.'}, {AKEYCODE_NUMPAD_COMMA, ',', ','},
	{AKEYCODE_NUMPAD_EQUALS, '=', '='}, {AKEYCODE_ENTER, '\n', '\n'},
	{AKEYCODE_NUMPAD_ENTER, '\n', '\n'}, {AKEYCODE_TAB, '\t', BackTab},
	{AKEYCODE_DEL, '\b', '\b'}, {AKEYCODE_FORWARD_DEL, 0x7f, 0x7f},
	{AKEYCODE_ESCAPE, Esc, Esc}, {AKEYCODE_INSERT, Ins, Ins},
	{AKEYCODE_DPAD_UP, Up, Up}, {AKEYCODE_DPAD_DOWN, Down, Down},
	{AKEYCODE_DPAD_LEFT, Left, Left}, {AKEYCODE_DPAD_RIGHT, Right, Right},
	{AKEYCODE_MOVE_HOME, Home, Home}, {AKEYCODE_MOVE_END, End, End},
	{AKEYCODE_PAGE_UP, Pgup, Pgup}, {AKEYCODE_PAGE_DOWN, Pgdown, Pgdown},
};

/* Android key codes supply a native US layout and the Inferno navigation runes. */
int
android_key_rune(int code, int meta)
{
	int shift, ctrl;
	unsigned int i;

	if(meta & (AMETA_ALT_ON|AMETA_ALT_LEFT_ON|AMETA_ALT_RIGHT_ON|
	           AMETA_META_ON|AMETA_META_LEFT_ON|AMETA_META_RIGHT_ON|
	           AMETA_FUNCTION_ON|AMETA_SYM_ON))
		return No;
	shift = (meta & (AMETA_SHIFT_ON|AMETA_SHIFT_LEFT_ON|AMETA_SHIFT_RIGHT_ON)) != 0;
	ctrl = (meta & (AMETA_CTRL_ON|AMETA_CTRL_LEFT_ON|AMETA_CTRL_RIGHT_ON)) != 0;
	if(code >= AKEYCODE_A && code <= AKEYCODE_Z){
		if(ctrl)
			return 1 + code - AKEYCODE_A;
		return (shift != ((meta & AMETA_CAPS_LOCK_ON) != 0) ? 'A' : 'a') + code - AKEYCODE_A;
	}
	if(ctrl)
		return No;
	if(code >= AKEYCODE_0 && code <= AKEYCODE_9)
		return (shift ? ")!@#$%^&*(" : "0123456789")[code - AKEYCODE_0];
	if(code >= AKEYCODE_NUMPAD_0 && code <= AKEYCODE_NUMPAD_9)
		return '0' + code - AKEYCODE_NUMPAD_0;
	for(i = 0; i < nelem(keys); i++)
		if(keys[i].code == code)
			return shift ? keys[i].shifted : keys[i].plain;
	return No;
}

int
xkbd_key(int code, int meta, int down)
{
	int rune = android_key_rune(code, meta);
	if(rune == No || gkbdq == nil)
		return 0;
	if(down)
		gkbdputc(gkbdq, rune);
	return 1;
}
