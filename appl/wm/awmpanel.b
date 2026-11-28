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
			if(s == nil || control(top, s))
				break;
			tkclient->wmctl(top, s);
			if(len s >= 5 && s[0:5] == "rect ")
				layout(top);
		<-keyboardclosed =>
			keyboardopen = 0;
		s := <-action =>
			if(s == "apps"){
				opened = (opened == 0);
				showmenu(top, opened);
			}else if(s == "tools"){
				opened = 2;
				showmenu(top, opened);
			}else if(s == "about" || s == "keyboard" || s == "calculator" ||
				s == "files" || s == "edit" || s == "shell" || s == "processes" || s == "debug" || s == "help"){
				opened = 0;
				showmenu(top, opened);
				if(s == "about")
					spawn about(ctxt);
				else if(s == "keyboard"){
					if(!keyboardopen){
						keyboardopen = 1;
						spawn keyboard(ctxt, keyboardclosed);
					}
				}else
					spawn runapp(ctxt, s);
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
	appsbutton(top, ".bar.apps");
	cmd(top, "pack .bar.apps -side left -fill y -padx 6 -pady 4");
	cmd(top, "button .bar.keyboard -text Keyboard -bg #dce8f5 -fg #172331 " +
		"-activebackground #ffffff -takefocus 0 -command {send panel keyboard}");
	cmd(top, "pack .bar.keyboard -side right -fill y -ipadx 8 -padx 6 -pady 4");
	cmd(top, "pack .bar -side top -fill x");
	cmd(top, "frame .launch -height 40 -bg #2c3c51; pack propagate .launch 0");
	appsbutton(top, ".launch.apps");
	cmd(top, "button .launch.calc -text Calculator -width 100 -bg #dce8f5 -fg #172331 " +
		"-activebackground #ffffff -command {send panel calculator}");
	cmd(top, "button .launch.tools -text Tools -width 86 -bg #dce8f5 -fg #172331 " +
		"-activebackground #ffffff -command {send panel tools}");
	cmd(top, "button .launch.files -text Files -width 64 -bg #dce8f5 -fg #172331 " +
		"-activebackground #ffffff -command {send panel files}");
	cmd(top, "button .launch.edit -text Edit -width 64 -bg #dce8f5 -fg #172331 " +
		"-activebackground #ffffff -command {send panel edit}");
	cmd(top, "pack .launch.apps .launch.calc .launch.tools .launch.files .launch.edit " +
		"-side left -fill y -padx 4 -pady 4");
	cmd(top, "frame .tools -height 40 -bg #344355; pack propagate .tools 0");
	appsbutton(top, ".tools.apps");
	cmd(top, "button .tools.shell -text Shell -width 52 -bg #dce8f5 -fg #172331 -command {send panel shell}");
	cmd(top, "button .tools.proc -text Processes -width 78 -bg #dce8f5 -fg #172331 -command {send panel processes}");
	cmd(top, "button .tools.debug -text Debug -width 54 -bg #dce8f5 -fg #172331 -command {send panel debug}");
	cmd(top, "button .tools.help -text Help -width 48 -bg #dce8f5 -fg #172331 -command {send panel help}");
	cmd(top, "button .tools.about -text About -width 52 -bg #dce8f5 -fg #172331 -command {send panel about}");
	cmd(top, "pack .tools.apps .tools.shell .tools.proc .tools.debug .tools.help .tools.about " +
		"-side left -fill y -padx 4 -pady 4");
	cmd(top, "pack propagate . 0");
}

appsbutton(top: ref Tk->Toplevel, name: string)
{
	make := "button " + name + " -text Apps -width 50 -bg #dce8f5 -fg #172331 " +
		"-activebackground #ffffff -takefocus 0 -command {send panel apps}";
	cmd(top, make);
	icon := name + " configure -bitmap :26:vitabig.png";
	tk->cmd(top, icon);
}

showmenu(top: ref Tk->Toplevel, opened: int)
{
	case opened {
	1 =>
		cmd(top, "pack forget .bar .tools; pack .launch -side top -fill x; update");
	2 =>
		cmd(top, "pack forget .bar .launch; pack .tools -side top -fill x; update");
	* =>
		cmd(top, "pack forget .launch .tools; pack .bar -side top -fill x; update");
	}
}

runapp(ctxt: ref Draw->Context, name: string)
{
	path: string;
	case name {
	"calculator" => path = "/dis/wm/awmcalc.dis";
	"files" => path = "/dis/wm/awmfiles.dis";
	"edit" => path = "/dis/wm/edit.dis";
	"shell" => path = "/dis/wm/awmsh-history.dis";
	"processes" => path = "/dis/wm/awmproc.dis";
	"debug" => path = "/dis/wm/awmdebug.dis";
	"help" => path = "/dis/wm/help.dis";
	}
	app := load Command path;
	if(app == nil)
		raise "fail:cannot load " + name;
	args := name :: nil;
	app->init(ctxt, args);
}

keyboard(ctxt: ref Draw->Context, closed: chan of int)
{
	{
		app := load Command "/dis/wm/keyboard.dis";
		if(app == nil)
			raise "fail:cannot load Keyboard";
		args := "keyboard" :: "-d" :: nil;
		app->init(ctxt, args);
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

layout(top: ref Tk->Toplevel)
{
	r := top.screenr;
	w := r.max.x - r.min.x;
	h := r.max.y - r.min.y;
	if(w <= 0 || h <= 0)
		return;
	if(h > 40)
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
