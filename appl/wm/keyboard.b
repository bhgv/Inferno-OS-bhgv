implement Keybd;

#
# extensive revision of code originally by N. W. Knauft
#
# Copyright © 1997 Lucent Technologies Inc.  All rights reserved.
# Revisions Copyright © 1998 Vita Nuova Limited.  All rights reserved.
# Rewritten code Copyright © 2001 Vita Nuova Holdings Limited.  All rights reserved.
#

include "sys.m";
        sys: Sys;

include "draw.m";

include "tk.m";
        tk: Tk;

include "tkclient.m";
        tkclient: Tkclient;

include "arg.m";

include "keyboard.m";

Keybd: module
{
	init:	fn(nil: ref Draw->Context, nil: list of string);
};

FONT: con "/fonts/lucidasans/boldlatin1.6.font";
SPECFONT: con "/fonts/lucidasans/unicode.6.font";
Background: con "#dddddd";
Keysize: con 13;
Closekey: con -2;
Shift, Ctrl, Caps, Compose: con 1 << iota;

unshifted := array[] of {
	"Esc", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "=", "\\\\", "`", "\n",
	"Tab", "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "[", "]", "<-", "\n",
	"Ctrl", "a", "s", "d", "f", "g", "h", "j", "k", "l", ";", "'", "Enter", "\n",
	"Shift", "z", "x", "c", "v", "b", "n", "m", ",", ".", "/", "Shift", "\n",
	"Caps", "Alt", " ", "Alt", "\n",
};

shifted := array[] of {
	"Esc", "!", "@", "#", "$", "%", "^", "&", "*", "(", ")", "_", "+", "|", "~", "\n",
	"Tab", "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "\\{", "\\}", "<-", "\n",
	"Ctrl", "A", "S", "D", "F", "G", "H", "J", "K", "L", ":", "\"", "Return", "\n",
	"Shift", "Z", "X", "C", "V", "B", "N", "M", "<", ">", "?", "Shift", "\n",
	"Caps", "Alt", " ", "Alt", "\n",
};

init(ctxt: ref Draw->Context, args: list of string)
{
	sys = load Sys Sys->PATH;
	if(ctxt == nil){
		sys->fprint(sys->fildes(2), "keyboard: no window context\n");
		raise "fail:bad context";
	}
	tk = load Tk Tk->PATH;
	tkclient = load Tkclient Tkclient->PATH;
	arg := load Arg Arg->PATH;
	if(tk == nil || tkclient == nil || arg == nil)
		raise "fail:cannot start Keyboard";

	taskbar := 0;
	noexit := 0;
	docked := 0;
	winopts := Tkclient->Hide;
	arg->init(args);
	while((opt := arg->opt()) != 0){
		case opt {
		't' => taskbar = 1;
		'e' => noexit = 1; winopts = 0;
		'd' => docked = 1;
		* =>
			sys->fprint(sys->fildes(2), "usage: keyboard [-det]\n");
			raise "fail:usage";
		}
	}
	if(docked){
		winopts = Tkclient->Plain;
		taskbar = 0;
		noexit = 0;
	}

	sys->pctl(Sys->NEWPGRP, nil);
	tkclient->init();
	(t, wcmd) := tkclient->toplevel(ctxt, "-bd 0 -relief flat", "Kbd", winopts);
	keypress := chan of string;
	tk->namechan(t, keypress, "keypress");
	build(t, docked);
	redraw(t, 0);
	tkclient->startinput(t, "ptr" :: nil);
	place(t, docked);
	if(docked)
		handle(t, wcmd, keypress, taskbar, noexit, docked);
	else
		spawn handle(t, wcmd, keypress, taskbar, noexit, docked);
}

build(t: ref Tk->Toplevel, docked: int)
{
	r := t.screenr;
	width := r.max.x - r.min.x;
	height := r.max.y - r.min.y;
	keyheight := Keysize;
	keywidth := Keysize;
	if(docked){
		keyheight = 28;
		if(height > width)
			keyheight = 38;
		keywidth = (width-16)/18;
		if(keywidth > 30)
			keywidth = 30;
	}
	widgetrow := ".row";
	command: string;
	cmd(t, "frame .row");
	for(i := 0; i < len unshifted; i++){
		label := unshifted[i];
		if(label == "\n"){
			command = "pack " + widgetrow + " -fill x";
			cmd(t, command);
			if(i+1 < len unshifted){
				widgetrow = ".row" + string i;
				command = "frame " + widgetrow;
				cmd(t, command);
			}
			continue;
		}
		widget := ".b" + string i;
		w := legacywidth(label);
		font := FONT;
		if(len label > 1 || label == " ")
			font = SPECFONT;
		fontopt := " -font " + font;
		if(docked){
			fontopt = "";
			w = w * keywidth / Keysize;
		}
		command = "button " + widget + " -takefocus 0 -bd 1" + fontopt +
			" -width " + string w + " -height " + string keyheight;
		cmd(t, command);
		expand := len label > 1 && label != "\\\\";
		if(docked)
			expand = 1;
		command = "pack " + widget + " -in " + widgetrow + " -side left -fill x -expand " + string expand;
		cmd(t, command);
	}
	if(docked){
		cmd(t, "frame .nav; pack .nav -fill x");
		nav(t, "Left", Keyboard->Left, keyheight);
		nav(t, "Right", Keyboard->Right, keyheight);
		nav(t, "Up", Keyboard->Up, keyheight);
		nav(t, "Down", Keyboard->Down, keyheight);
		nav(t, "Close", Closekey, keyheight);
	}
}

nav(t: ref Tk->Toplevel, label: string, code, height: int)
{
	widget := ".nav." + label;
	command := "button " + widget + " -takefocus 0 -height " + string height +
		" -bg " + Background + " -text {" + label + "} -command {send keypress " + string code + "}";
	cmd(t, command);
	command = "pack " + widget + " -side left -expand 1 -fill x";
	cmd(t, command);
}

legacywidth(label: string): int
{
	case label {
	"<-" => return 28;
	"Tab" => return 26;
	"Caps" => return 40;
	"Enter" or "Return" or "Ctrl" => return 36;
	"Shift" => return 45;
	"Esc" => return 21;
	"Alt" => return 22;
	" " => return 140;
	}
	return Keysize;
}

keycode(label: string): int
{
	case label {
	"<-" => return '\b';
	"Tab" => return '\t';
	"Caps" => return Keyboard->Caps;
	"Enter" or "Return" => return '\n';
	"Shift" => return Keyboard->LShift;
	"Esc" => return Keyboard->Esc;
	"Ctrl" => return Keyboard->LCtrl;
	"Alt" => return Keyboard->LAlt;
	}
	c := label[0];
	if(c == '\\' && len label > 1)
		c = label[1];
	return c;
}

place(t: ref Tk->Toplevel, docked: int)
{
	r := t.screenr;
	size := tk->rect(t, ".", Tk->Border|Tk->Required);
	x := r.min.x + (r.max.x-r.min.x-(size.max.x-size.min.x))/2;
	y := r.max.y - (size.max.y-size.min.y);
	command := ". configure -x " + string x + " -y " + string y;
	cmd(t, command);
	if(docked)
		tkclient->onscreen(t, "onscreen");
	else
		tkclient->onscreen(t, nil);
	cmd(t, "update");
}

handle(t: ref Tk->Toplevel, wcmd, keypress: chan of string, taskbar, noexit, docked: int)
{
	if(taskbar)
		tkclient->wmctl(t, "task");
	mods := 0;
	collected := "";
	for(;;){
		wm := t.ctxt;
		wreq := t.wreq;
		alt{
		m := <-wm.ptr =>
			if(m != nil)
				tk->pointer(t, *m);
		s := <-wm.ctl or s = <-wreq or s = <-wcmd =>
			if(s == "exit"){
				if(docked)
					return;
				if(noexit)
					s = "task";
			}
			if(s != nil)
				tkclient->wmctl(t, s);
			if(docked && len s >= 5 && s[0:5] == "rect ")
				place(t, docked);
		msg := <-keypress =>
			c := int msg;
			case c {
			Closekey =>
				return;
			Keyboard->Caps =>
				mods = (mods & Caps) ^ Caps;
			Keyboard->LShift =>
				mods ^= Shift;
			Keyboard->LCtrl =>
				mods = (mods & (Caps|Ctrl)) ^ Ctrl;
			Keyboard->LAlt =>
				mods = (mods & (Caps|Compose)) ^ Compose;
				collected = "";
			* =>
				previous := mods;
				if(mods & Compose){
					collected[len collected] = c;
					c = latin1(collected);
					if(c < -1)
						break;
					mods &= ~Compose;
					if(c == -1){
						mods &= Caps;
						redraw(t, mods);
						for(i := 0; i < len collected; i++){
							rune := collected[i];
							sendkey(t, rune);
						}
						break;
					}
				}
				if(mods & Ctrl)
					c &= 16r1f;
				mods &= Caps;
				if(previous != mods)
					redraw(t, mods);
				sendkey(t, c);
				break;
			}
			if(c == Keyboard->Caps || c == Keyboard->LShift || c == Keyboard->LCtrl || c == Keyboard->LAlt)
				redraw(t, mods);
		}
	}
}

sendkey(t: ref Tk->Toplevel, code: int)
{
	tkclient->wmctl(t, "key " + string code);
}

redraw(t: ref Tk->Toplevel, mods: int)
{
	bank := unshifted;
	if(mods & Shift)
		bank = shifted;
	for(i := 0; i < len bank; i++){
		label := bank[i];
		if(label == "\n")
			continue;
		c := keycode(label);
		widget := ".b" + string i;
		colour := Background;
		case c {
		Keyboard->LShift => if(mods & Shift) colour = "white";
		Keyboard->LCtrl => if(mods & Ctrl) colour = "white";
		Keyboard->Caps => if(mods & Caps) colour = "white";
		Keyboard->LAlt => if(mods & Compose) colour = "white";
		}
		if(mods & Caps){
			if(c >= 'A' && c <= 'Z')
				c += 'a' - 'A';
			else if(c >= 'a' && c <= 'z')
				c -= 'a' - 'A';
		}
		if(len label == 1 || label[0] == '\\'){
			label = "";
			label[0] = c;
		}
		command := widget + " configure -bg " + colour + " -activebackground " + colour +
			" -command {send keypress " + string c + "} -text '" + label;
		cmd(t, command);
	}
	cmd(t, "update");
}

# Each compose entry stores its prefix, input characters and output runes.
latintab := array[] of {
	" ", " i", "␣ı",
	"!~", "-=~", "≄≇≉",
	"!", "!<=>?bmp", "¡≮≠≯‽⊄∉⊅",
	"\"*", "IUiu", "ΪΫϊϋ",
	"\"", "\"AEIOUYaeiouy", "¨ÄËÏÖÜŸäëïöüÿ",
	"$*", "fhk", "ϕϑϰ",
	"$", "BEFHILMRVaefglopv", "ℬℰℱℋℐℒℳℛƲɑℯƒℊℓℴ℘ʋ",
	"\'\"", "Uu", "Ǘǘ",
	"\'", "\'ACEILNORSUYZacegilnorsuyz", "´ÁĆÉÍĹŃÓŔŚÚÝŹáćéģíĺńóŕśúýź",
	"*", "*ABCDEFGHIKLMNOPQRSTUWXYZabcdefghiklmnopqrstuwxyz", "∗ΑΒΞΔΕΦΓΘΙΚΛΜΝΟΠΨΡΣΤΥΩΧΗΖαβξδεφγθικλμνοπψρστυωχηζ",
	"+", "-O", "±⊕",
	",", ",ACEGIKLNORSTUacegiklnorstu", "¸ĄÇĘĢĮĶĻŅǪŖŞŢŲąçęģįķļņǫŗşţų",
	"-*", "l", "ƛ",
	"-", "+-2:>DGHILOTZbdghiltuz~", "∓­ƻ÷→ÐǤĦƗŁ⊖ŦƵƀðǥℏɨłŧʉƶ≂",
	".", ".CEGILOZceglz", "·ĊĖĠİĿ⊙Żċėġŀż",
	"/", "Oo", "Øø",
	"1", "234568", "½⅓¼⅕⅙⅛",
	"2", "-35", "ƻ⅔⅖",
	"3", "458", "¾⅗⅜",
	"4", "5", "⅘",
	"5", "68", "⅚⅝",
	"7", "8", "⅞",
	":", ")-=", "☺÷≔",
	"<!", "=~", "≨⋦",
	"<", "-<=>~", "←«≤≶≲",
	"=", ":<=>OV", "≕⋜≡⋝⊜⇒",
	">!", "=~", "≩⋧",
	">", "<=>~", "≷≥»≳",
	"?", "!?", "‽¿",
	"@\'", "\'", "ъ",
	"@@", "\'EKSTYZekstyz", "ьЕКСТЫЗекстыз",
	"@C", "Hh", "ЧЧ",
	"@E", "Hh", "ЭЭ",
	"@K", "Hh", "ХХ",
	"@S", "CHch", "ЩШЩШ",
	"@T", "Ss", "ЦЦ",
	"@Y", "AEOUaeou", "ЯЕЁЮЯЕЁЮ",
	"@Z", "Hh", "ЖЖ",
	"@c", "h", "ч",
	"@e", "h", "э",
	"@k", "h", "х",
	"@s", "ch", "щш",
	"@t", "s", "ц",
	"@y", "aeou", "яеёю",
	"@z", "h", "ж",
	"@", "ABDFGIJLMNOPRUVXabdfgijlmnopruvx", "АБДФГИЙЛМНОПРУВХабдфгийлмнопрувх",
	"A", "E", "Æ",
	"C", "ACU", "⋂ℂ⋃",
	"Dv", "Zz", "Ǆǅ",
	"D", "-e", "Ð∆",
	"G", "-", "Ǥ",
	"H", "-H", "Ħℍ",
	"I", "-J", "ƗĲ",
	"L", "&-Jj|", "⋀ŁǇǈ⋁",
	"N", "JNj", "Ǌℕǋ",
	"O", "*+-./=EIcoprx", "⊛⊕⊖⊙⊘⊜ŒƢ©⊚℗®⊗",
	"P", "P", "ℙ",
	"Q", "Q", "ℚ",
	"R", "R", "ℝ",
	"S", "123S", "¹²³§",
	"T", "-u", "Ŧ⊨",
	"V", "=", "⇐",
	"Y", "R", "Ʀ",
	"Z", "-ACSZ", "Ƶℤ",
	"^", "ACEGHIJOSUWYaceghijosuwy", "ÂĈÊĜĤÎĴÔŜÛŴŶâĉêĝĥîĵôŝûŵŷ",
	"_\"", "AUau", "ǞǕǟǖ",
	"_,", "Oo", "Ǭǭ",
	"_.", "Aa", "Ǡǡ",
	"_", "AEIOU_aeiou", "ĀĒĪŌŪ¯āēīōū",
	"`\"", "Uu", "Ǜǜ",
	"`", "AEIOUaeiou", "ÀÈÌÒÙàèìòù",
	"a", "ben", "↔æ∠",
	"b", "()+-0123456789=bknpqru", "₍₎₊₋₀₁₂₃₄₅₆₇₈₉₌♝♚♞♟♛♜•",
	"c", "$Oagu", "¢©∩≅∪",
	"dv", "z", "ǆ",
	"d", "-adegz", "ð↓‡°†ʣ",
	"e", "$lmns", "€⋯—–∅",
	"f", "a", "∀",
	"g", "$-r", "¤ǥ∇",
	"h", "-v", "ℏƕ",
	"i", "-bfjps", "ɨ⊆∞ĳ⊇∫",
	"l", "\"$&\'-jz|", "“£∧‘łǉ⋄∨",
	"m", "iou", "µ∈×",
	"n", "jo", "ǌ¬",
	"o", "AOUaeiu", "Å⊚Ůåœƣů",
	"p", "Odgrt", "℗∂¶∏∝",
	"r", "\"\'O", "”’®",
	"s", "()+-0123456789=abnoprstu", "⁽⁾⁺⁻⁰ⁱ⁲⁳⁴⁵⁶⁷⁸⁹⁼ª⊂ⁿº⊃√ß∍∑",
	"t", "-efmsu", "ŧ∃∴™ς⊢",
	"u", "-AEGIOUaegiou", "ʉĂĔĞĬŎŬ↑ĕğĭŏŭ",
	"v\"", "Uu", "Ǚǚ",
	"v", "ACDEGIKLNORSTUZacdegijklnorstuz", "ǍČĎĚǦǏǨĽŇǑŘŠŤǓŽǎčďěǧǐǰǩľňǒřšťǔž",
	"w", "bknpqr", "♗♔♘♙♕♖",
	"x", "O", "⊗",
	"y", "$", "¥",
	"z", "-", "ƶ",
	"|", "Pp|", "Þþ¦",
	"~!", "=", "≆",
	"~", "-=AINOUainou~", "≃≅ÃĨÑÕŨãĩñõũ≈",
};

unicode(k: string): int
{
	c := 0;
	for(i:=1; i<5; i++){
		r := k[i];
		c <<= 4;
		if('0'<=r && r<='9')
			c += r-'0';
		else if('a'<=r && r<='f')
			c += 10 + r-'a';
		else if('A'<=r && r<='F')
			c += 10 + r-'A';
		else
			return -1;
	}
	return c;
}

#
# Given n characters k[0]..k[n-1], find the corresponding rune or return -1 for
# failure, or something < -1 if n is too small.  In the latter case, the result
# is minus the required n.
#
latin1(k: string): int
{
	n := len k;
	first := k[0];
	if(first == 'X' || n > 1 && first == 'x' && k[1] != 'O'){
		if(n >= 5)
			return unicode(k);
		return -5;
	}
	for(i := 0; i < len latintab; i += 3){
		prefix := latintab[i];
		if(first != prefix[0])
			continue;
		if(n == 1)
			return -2;
		c := k[1];
		if(len prefix == 2){
			if(prefix[1] != c)
				continue;
			if(n == 2)
				return -3;
			c = k[2];
		}
		input := latintab[i+1];
		output := latintab[i+2];
		for(p := 0; p < len input && p < len output; p++)
			if(input[p] == c){
				rune := output[p];
				return rune;
			}
		return -1;
	}
	return -1;
}

cmd(top: ref Tk->Toplevel, c: string): string
{
	e := tk->cmd(top, c);
	if (e != nil && e[0] == '!')
		sys->fprint(sys->fildes(2), "keyboard: tk error on '%s': %s\n", c, e);
	return e;
}
