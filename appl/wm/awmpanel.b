implement Awmpanel;

include "sys.m";
include "draw.m";
include "sh.m";
include "string.m";
	str: String;
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
	str = load String String->PATH;
	if(ctxt == nil || tk == nil || tkclient == nil || str == nil)
		raise "fail:cannot start AWM panel";
	tkclient->init();
	(top, nil) := tkclient->toplevel(ctxt, "-bg #212b38", "", Tkclient->Plain);

	action := chan of string;
	keyboardclosed := chan[1] of int;
	keyboardopen := 0;
	tk->namechan(top, action, "panel");
	widgets(top);
	tkclient->startinput(top, "ptr"::"control"::nil);
	opened := 0;
	layout(top, opened);

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
			if(s == nil || control(top, s))
				break;
			tkclient->wmctl(top, s);
			if(len s >= 5 && s[0:5] == "rect ")
				layout(top, opened);
		<-keyboardclosed =>
			keyboardopen = 0;
		s := <-action =>
			if(s == "apps"){
				opened = !opened;
				showmenu(top, opened);
			}else if(s == "about" || s == "calculator" || s == "files" || s == "edit" || s == "keyboard"){
				opened = 0;
				showmenu(top, opened);
				if(s == "about")
					spawn about(ctxt);
				else if(s == "calculator")
					spawn calculator(ctxt);
				else if(s == "files")
					spawn files(ctxt);
				else if(s == "edit")
					spawn editor(ctxt);
				else if(!keyboardopen){
					keyboardopen = 1;
					spawn keyboard(ctxt, keyboardclosed);
				}
			}else if(len s >= 8 && s[0:8] == "restore "){
				id := s[8:];
				restore(top, id);
			}
		}
	}
}

widgets(top: ref Tk->Toplevel)
{
	cmd(top, "frame .bar -height 40 -bg #212b38; pack propagate .bar 0");
	cmd(top, "button .bar.apps -text Apps -bg #dce8f5 -fg #172331 " +
		"-activebackground #ffffff -takefocus 0 -command {send panel apps}");
	cmd(top, "pack .bar.apps -side left -fill y -ipadx 10 -padx 6 -pady 4");
	cmd(top, "button .bar.keyboard -text Keyboard -bg #dce8f5 -fg #172331 " +
		"-activebackground #ffffff -takefocus 0 -command {send panel keyboard}");
	cmd(top, "pack .bar.keyboard -side right -fill y -ipadx 8 -padx 6 -pady 4");
	cmd(top, "pack .bar -side top -fill x");
	cmd(top, "frame .launch -height 48 -bg #2c3c51; pack propagate .launch 0");
	cmd(top, "button .launch.calc -text Calculator -width 114 -bg #dce8f5 -fg #172331 " +
		"-activebackground #ffffff -command {send panel calculator}");
	cmd(top, "button .launch.about -text {About Inferno} -width 114 -bg #dce8f5 -fg #172331 " +
		"-activebackground #ffffff -command {send panel about}");
	cmd(top, "button .launch.files -text Files -width 84 -bg #dce8f5 -fg #172331 " +
		"-activebackground #ffffff -command {send panel files}");
	cmd(top, "button .launch.edit -text Edit -width 84 -bg #dce8f5 -fg #172331 " +
		"-activebackground #ffffff -command {send panel edit}");
	cmd(top, "pack .launch.calc .launch.about .launch.files .launch.edit -side left -fill y -padx 6 -pady 5");
	cmd(top, "pack propagate . 0");
}

showmenu(top: ref Tk->Toplevel, opened: int)
{
	if(opened)
		cmd(top, "pack .launch -side top -fill x");
	else
		cmd(top, "pack forget .launch");
	layout(top, opened);
}

calculator(ctxt: ref Draw->Context)
{
	app := load Command "/dis/wm/awmcalc.dis";
	if(app == nil)
		raise "fail:cannot load Calculator";
	app->init(ctxt, nil);
}

files(ctxt: ref Draw->Context)
{
	app := load Command "/dis/wm/awmfiles.dis";
	if(app == nil)
		raise "fail:cannot load Files";
	app->init(ctxt, nil);
}

editor(ctxt: ref Draw->Context)
{
	app := load Command "/dis/wm/edit.dis";
	if(app == nil)
		raise "fail:cannot load Edit";
	args := "edit" :: nil;
	app->init(ctxt, args);
}

keyboard(ctxt: ref Draw->Context, closed: chan of int)
{
	{
		app := load Command "/dis/wm/awmkeys.dis";
		if(app == nil)
			raise "fail:cannot load Keyboard";
		app->init(ctxt, nil);
	} exception {
		"*" =>
			closed <-= 1;
			raise;
	}
	closed <-= 1;
}

control(top: ref Tk->Toplevel, message: string): int
{
	args := str->unquoted(message);
	if(len args < 2)
		return 0;
	kind := hd args;
	args = tl args;
	id := hd args;
	client := int id;
	if(client < 0 || string client != id)
		return 0;
	if(kind == "newclient")
		return 1;
	if(kind == "delclient"){
		removetask(top, id);
		return 1;
	}
	if(kind != "request")
		return 0;
	args = tl args;
	if(len args == 2 && hd args == "task"){
		label := hd tl args;
		addtask(top, id, label);
	}else if(args != nil && (hd args == "untask" || hd args == "unhide"))
		removetask(top, id);
	return 1;
}

addtask(top: ref Tk->Toplevel, id, name: string)
{
	removetask(top, id);
	label := "Window " + id;
	if(name == "Calculator" || name == "Files")
		label = name;
	widget := ".bar.task" + id;
	button := "button " + widget + " -text {" + label + "} -bg #dce8f5 -fg #172331 " +
		"-activebackground #ffffff -command {send panel restore " + id + "} -takefocus 0";
	cmd(top, button);
	pack := "pack " + widget + " -side left -fill y -padx 4 -pady 4";
	cmd(top, pack);
	# Reattach the panel image after changing the packed task buttons.
	tkclient->onscreen(top, "exact");
	cmd(top, "update");
}

removetask(top: ref Tk->Toplevel, id: string)
{
	destroy := "destroy .bar.task" + id;
	if(tk->cmd(top, destroy) == nil)
		cmd(top, "update");
}

restore(top: ref Tk->Toplevel, id: string)
{
	removetask(top, id);
	request := "ctl " + id + " untask";
	tkclient->wmctl(top, request);
}

layout(top: ref Tk->Toplevel, opened: int)
{
	r := top.screenr;
	w := r.max.x - r.min.x;
	h := r.max.y - r.min.y;
	if(w <= 0 || h <= 0)
		return;
	size := 40+48*opened;
	if(h > size)
		h = size;
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
