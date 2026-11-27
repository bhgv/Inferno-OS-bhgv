implement Awmproc;

include "sys.m";
	sys: Sys;
include "draw.m";
include "tk.m";
	tk: Tk;
include "tkclient.m";
	tkclient: Tkclient;
include "sh.m";

Awmproc: module
{
	init: fn(ctxt: ref Draw->Context, argv: list of string);
};

init(ctxt: ref Draw->Context, nil: list of string)
{
	sys = load Sys Sys->PATH;
	tk = load Tk Tk->PATH;
	tkclient = load Tkclient Tkclient->PATH;
	if(ctxt == nil || sys == nil || tk == nil || tkclient == nil)
		raise "fail:cannot start Processes";
	tkclient->init();
	(top, title) := tkclient->toplevel(ctxt, "", "Processes", Tkclient->Hide);
	action := chan of string;
	tk->namechan(top, action, "processes");
	widgets(top);
	refresh(top);
	tkclient->startinput(top, "ptr" :: "kbd" :: nil);
	tkclient->onscreen(top, "place");
	cmd(top, "focus .rows.list; update");

	wm := top.ctxt;
	wreq := top.wreq;
	for(;;) alt {
	p := <-wm.ptr =>
		if(p != nil)
			tk->pointer(top, *p);
	key := <-wm.kbd =>
		tk->keyboard(top, key);
	s := <-wm.ctl or s = <-wreq or s = <-title =>
		if(s == "exit")
			return;
		if(s != nil)
			tkclient->wmctl(top, s);
	a := <-action =>
		if(a == "refresh")
			refresh(top);
		else if(a == "debug"){
			sel := tk->cmd(top, ".rows.list curselection");
			if(sel != nil){
				query := ".rows.list get " + sel;
				row := tk->cmd(top, query);
				pid := int row;
				if(pid > 0)
					spawn debugger(ctxt, pid);
			}
		}
	}
}

widgets(top: ref Tk->Toplevel)
{
	cmd(top, "frame .nav; pack .nav -fill x");
	cmd(top, "button .nav.refresh -text Refresh -command {send processes refresh}");
	cmd(top, "button .nav.debug -text Debug -command {send processes debug}");
	cmd(top, "pack .nav.refresh .nav.debug -side left -padx 3 -ipady 5");
	cmd(top, "label .header -anchor w -text {PID   STATE   MODULE}; pack .header -fill x");
	cmd(top, "frame .rows; pack .rows -fill both -expand 1");
	cmd(top, "listbox .rows.list -width 342 -height 174 -selectmode single -yscrollcommand {.rows.scroll set}");
	cmd(top, "scrollbar .rows.scroll -command {.rows.list yview}");
	cmd(top, "pack .rows.scroll -side right -fill y; pack .rows.list -fill both -expand 1");
	cmd(top, "bind .rows.list <Double-Button-1> {send processes debug}");
}

refresh(top: ref Tk->Toplevel)
{
	cmd(top, ".rows.list delete 0 end");
	fd := sys->open("/prog", Sys->OREAD);
	if(fd == nil){
		cmd(top, ".rows.list insert end {Cannot open /prog}; update");
		return;
	}
	for(;;){
		(n, rows) := sys->dirread(fd);
		if(n <= 0)
			break;
		for(i := 0; i < n; i++){
			name := rows[i].name;
			row := summary(name);
			if(row != nil){
				command := ".rows.list insert end '" + row;
				cmd(top, command);
			}
		}
	}
	cmd(top, ".rows.list see end; update");
}

summary(pid: string): string
{
	path := "/prog/" + pid + "/status";
	fd := sys->open(path, Sys->OREAD);
	if(fd == nil)
		return nil;
	buf := array[512] of byte;
	n := sys->read(fd, buf, len buf);
	if(n <= 0)
		return nil;
	line := string buf[0:n];
	(count, fields) := sys->tokenize(line, " ");
	if(count < 6)
		return nil;
	while(len fields > 3)
		fields = tl fields;
	state := hd fields;
	name := hd tl tl fields;
	return pid + "   " + state + "   " + name;
}

debugger(ctxt: ref Draw->Context, pid: int)
{
	app := load Command "/dis/wm/awmdebug.dis";
	if(app == nil)
		return;
	id := string pid;
	args := "debugger" :: id :: nil;
	app->init(ctxt, args);
}

cmd(top: ref Tk->Toplevel, command: string)
{
	err := tk->cmd(top, command);
	if(err != nil && err[0] == '!')
		raise "fail:Processes: " + err;
}
