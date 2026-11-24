implement Awmcalc;

include "sys.m";
include "draw.m";
include "tk.m";
	tk: Tk;
include "tkclient.m";
	tkclient: Tkclient;

Awmcalc: module
{
	init: fn(ctxt: ref Draw->Context, argv: list of string);
};

Limit: con 999999999;
Invalid: con -1000000000;

init(ctxt: ref Draw->Context, nil: list of string)
{
	tk = load Tk Tk->PATH;
	tkclient = load Tkclient Tkclient->PATH;
	if(ctxt == nil || tk == nil || tkclient == nil)
		raise "fail:cannot start Calculator";
	tkclient->init();
	(top, title) := tkclient->toplevel(ctxt, "", "Calculator", Tkclient->Hide);
	keys := chan of string;
	tk->namechan(top, keys, "calc");
	keypad(top);
	tkclient->startinput(top, "ptr"::nil);
	tkclient->onscreen(top, "place");
	cmd(top, "update");

	a := 0;
	b := 0;
	fresh := 1;
	failed := 0;
	pending := "";
	for(;;){
		wm := top.ctxt;
		wreq := top.wreq;
		alt{
		p := <-wm.ptr =>
			if(p != nil)
				tk->pointer(top, *p);
		s := <-wm.ctl or
		s = <-wreq or
		s = <-title =>
			if(s == "exit")
				return;
			tkclient->wmctl(top, s);
		key := <-keys =>
			if(key == "close")
				return;
			case key {
			"C" =>
				a = 0;
				b = 0;
				pending = "";
				fresh = 1;
				failed = 0;
			"+" or "-" or "*" or "/" or "=" =>
				if(failed)
					break;
				if(pending != "" && !fresh){
					b = calculate(a, b, pending);
					failed = b == Invalid;
				}
				a = b;
				pending = key;
				if(key == "=")
					pending = "";
				fresh = 1;
			* =>
				if(len key != 1 || key[0] < '0' || key[0] > '9')
					break;
				if(fresh){
					b = 0;
					fresh = 0;
				}
				if(failed){
					pending = "";
					failed = 0;
				}
				if(b <= Limit/10)
					b = b*10 + (key[0] - '0');
			}
			result := string b;
			if(failed)
				result = "Error";
			update := ".value configure -text {" + result + "}; update";
			cmd(top, update);
		}
	}
}

calculate(a, b: int, oper: string): int
{
	v := big a;
	w := big b;
	case oper {
	"+" => v += w;
	"-" => v -= w;
	"*" => v *= w;
	"/" =>
		if(b == 0)
			return Invalid;
		v /= w;
	}
	if(v < big -Limit || v > big Limit)
		return Invalid;
	return int v;
}

keypad(top: ref Tk->Toplevel)
{
	cmd(top, "label .value -text 0 -width 200 -height 36 -anchor e -bg #ffffff");
	cmd(top, "pack .value -fill x -padx 8 -pady 8");
	row(top, ".a", "7", "8", "9", "/");
	row(top, ".b", "4", "5", "6", "*");
	row(top, ".c", "1", "2", "3", "-");
	row(top, ".d", "C", "0", "=", "+");
	cmd(top, "button .close -text Close -command {send calc close}; pack .close -pady 6");
}

row(top: ref Tk->Toplevel, name, a, b, c, d: string)
{
	frame := "frame " + name;
	cmd(top, frame);
	k1 := name+".a";
	k2 := name+".b";
	k3 := name+".c";
	k4 := name+".d";
	button(top, k1, a);
	button(top, k2, b);
	button(top, k3, c);
	button(top, k4, d);
	pack := "pack " + name + " -fill x -padx 8";
	cmd(top, pack);
}

button(top: ref Tk->Toplevel, name, key: string)
{
	make := "button " + name + " -text {" + key + "} -width 44 -height 32 -command {send calc " + key + "}";
	cmd(top, make);
	pack := "pack " + name + " -side left -expand 1 -fill both -padx 2 -pady 2";
	cmd(top, pack);
}

cmd(top: ref Tk->Toplevel, command: string)
{
	err := tk->cmd(top, command);
	if(err != nil && err[0] == '!')
		raise "fail:Calculator: " + err;
}
