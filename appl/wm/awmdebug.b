implement Awmdebug;

include "sys.m";
	sys: Sys;
include "draw.m";
include "tk.m";
	tk: Tk;
include "tkclient.m";
	tkclient: Tkclient;

Awmdebug: module
{
	init: fn(ctxt: ref Draw->Context, argv: list of string);
};

init(ctxt: ref Draw->Context, argv: list of string)
{
	sys = load Sys Sys->PATH;
	tk = load Tk Tk->PATH;
	tkclient = load Tkclient Tkclient->PATH;
	if(ctxt == nil || sys == nil || tk == nil || tkclient == nil)
		raise "fail:cannot start Debugger";
	tkclient->init();
	(top, title) := tkclient->toplevel(ctxt, "", "Debugger", Tkclient->Hide);
	action := chan[1] of string;
	tk->namechan(top, action, "debugger");
	widgets(top);

	pid, dir: string;
	fd: ref Sys->FD;
	if(argv != nil && tl argv != nil){
		pid = hd tl argv;
		command := ".nav.pid insert 0 '" + pid;
		cmd(top, command);
	}
	tkclient->startinput(top, "ptr" :: "kbd" :: nil);
	tkclient->onscreen(top, "place");
	if(pid != nil)
		action <-= "attach";
	cmd(top, "focus .nav.pid; update");

	selfpid := sys->pctl(0, nil);
	wm := top.ctxt;
	wreq := top.wreq;
	started := chan[1] of int;
	done := chan[1] of string;
	worker := 0;
	paused := 0;
	for(;;) alt {
	p := <-wm.ptr =>
		if(p != nil)
			tk->pointer(top, *p);
	key := <-wm.kbd =>
		tk->keyboard(top, key);
	s := <-wm.ctl or s = <-wreq or s = <-title =>
		if(s == "exit"){
			if(worker != 0)
				cancel(worker);
			else if(paused)
				request(fd, "start");
			return;
		}
		if(s != nil)
			tkclient->wmctl(top, s);
	err := <-done =>
		worker = 0;
		if(err == nil){
			paused = 1;
			err = "Stepped one instruction";
		}
		update(top, dir, err);
	a := <-action =>
		case a {
		"attach" =>
			if(worker != 0)
				break;
			candidate := tk->cmd(top, ".nav.pid get");
			n := int candidate;
			if(n <= 0 || string n != candidate){
				update(top, dir, "Enter a process ID");
				break;
			}
			if(n == selfpid){
				update(top, dir, "Select another process");
				break;
			}
			if(paused)
				request(fd, "start");
			fd = nil;
			paused = 0;
			dir = "/prog/" + candidate + "/";
			path := dir + "dbgctl";
			fd = sys->open(path, Sys->ORDWR);
			err := "Attached to " + candidate;
			if(fd == nil)
				err = sys->sprint("Cannot open debugger: %r");
			update(top, dir, err);
		"step" =>
			if(worker == 0 && fd != nil){
				spawn step(fd, started, done);
				worker = <-started;
				update(top, dir, "Waiting for the process");
			}
		"continue" =>
			if(worker == 0 && paused){
				err := request(fd, "start");
				if(err == nil){
					paused = 0;
					err = "Continued";
				}
				update(top, dir, err);
			}
		"refresh" =>
			update(top, dir, "");
		}
	}
}

widgets(top: ref Tk->Toplevel)
{
	cmd(top, "frame .nav; pack .nav -fill x");
	cmd(top, "entry .nav.pid -width 62; bind .nav.pid <Key-\n> {send debugger attach}");
	cmd(top, "button .nav.attach -text Attach -command {send debugger attach}");
	cmd(top, "button .nav.step -text Step -command {send debugger step}");
	cmd(top, "button .nav.run -text Continue -command {send debugger continue}");
	cmd(top, "button .nav.refresh -text Refresh -command {send debugger refresh}");
	cmd(top, "pack .nav.pid .nav.attach .nav.step .nav.run .nav.refresh -side left -padx 2 -ipady 5");
	cmd(top, "label .status -anchor w -text {Enter a process ID}; pack .status -fill x");
	cmd(top, "frame .detail; pack .detail -fill both -expand 1");
	cmd(top, "text .detail.t -width 314 -height 140 -bg white -wrap word -state disabled " +
		"-yscrollcommand {.detail.s set}");
	cmd(top, "scrollbar .detail.s -command {.detail.t yview}");
	cmd(top, "pack .detail.s -side right -fill y; pack .detail.t -fill both -expand 1");
}

update(top: ref Tk->Toplevel, dir, status: string)
{
	if(status != nil){
		command := ".status configure -text '" + status;
		cmd(top, command);
	}
	if(dir != nil){
		path := dir + "status";
		data := readfile(path);
		path = dir + "stack";
		stack := readfile(path);
		cmd(top, ".detail.t configure -state normal; .detail.t delete 1.0 end");
		command := ".detail.t insert end '" + data + "\n\n" + stack;
		cmd(top, command);
		cmd(top, ".detail.t configure -state disabled");
	}
	cmd(top, "update");
}

readfile(path: string): string
{
	fd := sys->open(path, Sys->OREAD);
	if(fd == nil)
		return sys->sprint("Cannot read %s: %r", path);
	buf := array[4096] of byte;
	n := sys->read(fd, buf, len buf);
	if(n <= 0)
		return "";
	return string buf[0:n];
}

step(fd: ref Sys->FD, started: chan of int, done: chan of string)
{
	pid := sys->pctl(0, nil);
	started <-= pid;
	err := request(fd, "step 1");
	done <-= err;
}

request(fd: ref Sys->FD, cmd: string): string
{
	bytes := array of byte cmd;
	if(sys->write(fd, bytes, len bytes) != len bytes)
		return sys->sprint("%r");
	return nil;
}

cancel(pid: int)
{
	path := "/prog/" + string pid + "/ctl";
	fd := sys->open(path, Sys->OWRITE);
	if(fd != nil)
		request(fd, "kill");
}

cmd(top: ref Tk->Toplevel, command: string)
{
	err := tk->cmd(top, command);
	if(err != nil && err[0] == '!')
		raise "fail:Debugger: " + err;
}
