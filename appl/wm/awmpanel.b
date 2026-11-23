implement Awmpanel;

include "sys.m";
include "draw.m";
include "tk.m";
	tk: Tk;
include "tkclient.m";
	tkclient: Tkclient;

Awmpanel: module
{
	init: fn(ctxt: ref Draw->Context, argv: list of string);
};

init(ctxt: ref Draw->Context, nil: list of string)
{
	tk = load Tk Tk->PATH;
	tkclient = load Tkclient Tkclient->PATH;
	if(ctxt == nil || tk == nil || tkclient == nil)
		raise "fail:cannot start AWM panel";
	tkclient->init();
	(top, nil) := tkclient->toplevel(ctxt, "-bg #212b38", "", Tkclient->Plain);

	action := chan of string;
	tk->namechan(top, action, "panel");
	cmd(top, "button .about -text {About Inferno} -bg #dce8f5 -fg #172331 " +
		"-activebackground #ffffff -command {send panel about} -takefocus 0");
	cmd(top, "pack .about -side left -fill y -padx 6 -pady 4");
	cmd(top, "pack propagate . 0");
	tkclient->startinput(top, "ptr"::nil);
	layout(top);

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
			tkclient->wmctl(top, s);
			if(len s >= 5 && s[0:5] == "rect ")
				layout(top);
		s := <-action =>
			if(s == "about")
				spawn about(ctxt);
		}
	}
}

layout(top: ref Tk->Toplevel)
{
	r := top.screenr;
	w := r.max.x - r.min.x;
	h := r.max.y - r.min.y;
	if(w <= 0 || h <= 0)
		return;
	if(h >= 40)
		h = 40;
	config := ". configure -x " + string r.min.x + " -y " + string r.min.y +
		" -width " + string w + " -height " + string h;
	cmd(top, config);
	tkclient->onscreen(top, "exact");
	cmd(top, "update");
}

about(ctxt: ref Draw->Context)
{
	(top, title) := tkclient->toplevel(ctxt, "", "About Inferno", 0);
	close := chan of string;
	tk->namechan(top, close, "about");
	cmd(top, "label .label -text {Inferno\nAWM desktop}");
	cmd(top, "button .close -text Close -command {send about close}");
	cmd(top, "pack .label .close -padx 16 -pady 8");
	tkclient->startinput(top, "ptr"::nil);
	tkclient->onscreen(top, "place");
	cmd(top, "update");

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
		<-close =>
			return;
		}
	}
}

cmd(top: ref Tk->Toplevel, command: string)
{
	err := tk->cmd(top, command);
	if(err != nil && err[0] == '!')
		raise "fail:AWM panel: " + err;
}
