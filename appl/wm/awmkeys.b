implement Awmkeys;

include "sys.m";
include "draw.m";
include "keyboard.m";
include "tk.m";
	tk: Tk;
include "tkclient.m";
	tkclient: Tkclient;

Awmkeys: module
{
	init: fn(ctxt: ref Draw->Context, argv: list of string);
};

init(ctxt: ref Draw->Context, nil: list of string)
{
	tk = load Tk Tk->PATH;
	tkclient = load Tkclient Tkclient->PATH;
	if(ctxt == nil || tk == nil || tkclient == nil)
		raise "fail:cannot start Keyboard";
	tkclient->init();
	(top, nil) := tkclient->toplevel(ctxt, "-bg #212b38", "", Tkclient->Plain);
	keys := chan of string;
	tk->namechan(top, keys, "onscreenkeys");
	widgets(top);
	# A pointer-only client leaves keyboard focus with the active application.
	tkclient->startinput(top, "ptr"::nil);
	dock(top);
	shifted := 0;

	for(;;){
		wm := top.ctxt;
		wreq := top.wreq;
		alt{
		p := <-wm.ptr =>
			if(p != nil)
				tk->pointer(top, *p);
		s := <-wm.ctl or
		s = <-wreq =>
			if(s == "exit")
				return;
			if(s != nil)
				tkclient->wmctl(top, s);
			if(len s >= 5 && s[0:5] == "rect ")
				dock(top);
		key := <-keys =>
			case key {
			"close" =>
				return;
			"shift" =>
				shifted = !shifted;
				if(shifted)
					cmd(top, ".tools.shift configure -relief sunken; update");
				else
					cmd(top, ".tools.shift configure -relief raised; update");
			* =>
				code := int key;
				if(code <= 0)
					break;
				if(shifted && code >= 'a' && code <= 'z')
					code -= 'a'-'A';
				request := "key " + string code;
				tkclient->wmctl(top, request);
				if(shifted){
					shifted = 0;
					cmd(top, ".tools.shift configure -relief raised; update");
				}
			}
		}
	}
}

widgets(top: ref Tk->Toplevel)
{
	row(top, ".digits", "1234567890");
	row(top, ".qwerty", "qwertyuiop");
	row(top, ".asdf", "asdfghjkl");
	row(top, ".zxc", "zxcvbnm");
	row(top, ".symbols", "+-*/=.,_");
	cmd(top, "frame .tools -bg #212b38; pack .tools -fill x");
	button(top, ".tools.shift", "Shift", "shift");
	button(top, ".tools.space", "Space", "32");
	up := string Keyboard->Up;
	down := string Keyboard->Down;
	button(top, ".tools.up", "Up", up);
	button(top, ".tools.down", "Down", down);
	button(top, ".tools.back", "Back", "8");
	button(top, ".tools.enter", "Enter", "10");
	button(top, ".tools.close", "Close", "close");
}

row(top: ref Tk->Toplevel, name, letters: string)
{
	frame := "frame " + name + " -bg #212b38; pack " + name + " -fill x";
	cmd(top, frame);
	for(i := 0; i < len letters; i++){
		widget := name + ".k" + string i;
		label := letters[i:i+1];
		code := string letters[i];
		button(top, widget, label, code);
	}
}

button(top: ref Tk->Toplevel, name, label, code: string)
{
	r := top.screenr;
	width := (r.max.x-r.min.x)/10-4;
	if(width > 42)
		width = 42;
	if(width < 20)
		width = 20;
	height := 28;
	if(r.max.y-r.min.y > r.max.x-r.min.x)
		height = 38;
	size := " -width " + string width + " -height " + string height;
	make := "button " + name + " -text {" + label + "}" + size +
		" -bg #dce8f5 -fg #172331 -activebackground #ffffff" +
		" -takefocus 0 -command {send onscreenkeys " + code + "}";
	cmd(top, make);
	pack := "pack " + name + " -side left -expand 1 -fill x -padx 1 -pady 1";
	cmd(top, pack);
}

dock(top: ref Tk->Toplevel)
{
	r := top.screenr;
	size := tk->rect(top, ".", Tk->Border|Tk->Required);
	x := r.min.x+(r.max.x-r.min.x-(size.max.x-size.min.x))/2;
	y := r.max.y-(size.max.y-size.min.y);
	config := ". configure -x " + string x + " -y " + string y;
	cmd(top, config);
	tkclient->onscreen(top, "onscreen");
	cmd(top, "update");
}

cmd(top: ref Tk->Toplevel, command: string)
{
	err := tk->cmd(top, command);
	if(err != nil && err[0] == '!')
		raise "fail:Keyboard: " + err;
}
