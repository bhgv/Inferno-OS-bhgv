implement Awmsh;

include "sys.m";
	sys: Sys;
include "draw.m";
include "tk.m";
	tk: Tk;
include "tkclient.m";
	tkclient: Tkclient;
include "string.m";
	str: String;
include "sh.m";
include "awmcommands.m";

Awmsh: module
{
	init: fn(ctxt: ref Draw->Context, argv: list of string);
};

init(ctxt: ref Draw->Context, nil: list of string)
{
	sys = load Sys Sys->PATH;
	tk = load Tk Tk->PATH;
	tkclient = load Tkclient Tkclient->PATH;
	str = load String String->PATH;
	if(ctxt == nil || sys == nil || tk == nil || tkclient == nil || str == nil)
		raise "fail:cannot start Shell";
	if(sys->pctl(Sys->FORKNS | Sys->NEWPGRP, nil) < 0)
		raise "fail:cannot isolate Shell";
	sys->chdir("/usr/inferno");
	tkclient->init();
	(top, title) := tkclient->toplevel(ctxt, "", "Shell", Tkclient->Hide);
	action := chan of string;
	tk->namechan(top, action, "shell");
	widgets(top);
	append(top, "Inferno shell. Type help for commands.\n");
	tkclient->startinput(top, "ptr" :: "kbd" :: nil);
	tkclient->onscreen(top, "place");
	cmd(top, "focus .input.line; update");

	wm := top.ctxt;
	wreq := top.wreq;
	output := chan of string;
	done := chan[1] of string;
	running := 0;
	for(;;) alt {
	p := <-wm.ptr =>
		if(p != nil)
			tk->pointer(top, *p);
	key := <-wm.kbd =>
		tk->keyboard(top, key);
	s := <-wm.ctl or s = <-wreq or s = <-title =>
		if(s != nil)
			tkclient->wmctl(top, s);
	chunk := <-output =>
		append(top, chunk);
	finished := <-done =>
		running = 0;
		if(finished != nil)
			append(top, finished);
		cmd(top, ".input.line configure -state normal; .input.go configure -state normal; focus .input.line; update");
	a := <-action =>
		if(a != "run" || running)
			break;
		line := tk->cmd(top, ".input.line get");
		args := str->unquoted(line);
		if(args == nil)
			break;
		cmd(top, ".input.line delete 0 end");
		prompt := "; " + line + "\n";
		append(top, prompt);
		builtin := hd args;
		if(builtin == "cd"){
			path := "/usr/inferno";
			if(tl args != nil)
				path = hd tl args;
			if(sys->chdir(path) < 0){
				err := sys->sprint("cd: %r\n");
				append(top, err);
			}
			break;
		}
		if(builtin == "clear"){
			cmd(top, ".view.t configure -state normal; .view.t delete 1.0 end; .view.t configure -state disabled; update");
			break;
		}
		if(builtin == "exit")
			tkclient->wmctl(top, "exit");
		if(builtin == "help"){
			if(tl args != nil)
				spawn manual(ctxt, args);
			else{
				usage := "Built in: cd [dir], ls [dir], clear, help [topic], exit\n" +
					"Inferno commands: " + Awmcommands + "\n" +
					"Use single quotes around arguments with spaces.\n";
				append(top, usage);
			}
			break;
		}
		pipes := array[2] of ref Sys->FD;
		if(sys->pipe(pipes) < 0){
			err := sys->sprint("pipe: %r\n");
			append(top, err);
			break;
		}
		reader := pipes[0];
		writer := pipes[1];
		status := chan[1] of string;
		running = 1;
		cmd(top, ".input.line configure -state disabled; .input.go configure -state disabled; update");
		spawn execute(ctxt, args, writer, status);
		spawn display(reader, output, status, done);
		pipes = nil;
		writer = nil;
		reader = nil;
	}
}

manual(ctxt: ref Draw->Context, args: list of string)
{
	app := load Command "/dis/wm/help.dis";
	if(app != nil)
		app->init(ctxt, args);
}

widgets(top: ref Tk->Toplevel)
{
	cmd(top, "frame .view; pack .view -fill both -expand 1");
	cmd(top, "text .view.t -width 348 -height 154 -bg white -state disabled -wrap word -yscrollcommand {.view.s set}");
	cmd(top, "scrollbar .view.s -command {.view.t yview}");
	cmd(top, "pack .view.s -side right -fill y; pack .view.t -fill both -expand 1");
	cmd(top, "frame .input; pack .input -fill x");
	cmd(top, "label .input.prompt -text {;}");
	cmd(top, "entry .input.line -width 270");
	cmd(top, "button .input.go -text Run -command {send shell run}");
	cmd(top, "pack .input.prompt -side left -padx 4");
	cmd(top, "pack .input.go -side right -padx 3 -ipady 5");
	cmd(top, "pack .input.line -side left -expand 1 -fill x");
	cmd(top, "bind .input.line <Key-\n> {send shell run}");
}

execute(ctxt: ref Draw->Context, args: list of string, writer: ref Sys->FD, done: chan of string)
{
	# Only this command receives the console pipe and the null input FD.
	if(sys->pctl(Sys->FORKFD | Sys->FORKNS, nil) < 0){
		done <-= "Cannot isolate command\n";
		return;
	}
	number := writer.fd;
	if(sys->dup(number, 1) < 0 || sys->dup(number, 2) < 0){
		done <-= "Cannot redirect command output\n";
		return;
	}
	writer = nil;
	input := sys->open("/dev/null", Sys->OREAD);
	if(input == nil){
		done <-= "Cannot open command input\n";
		return;
	}
	number = input.fd;
	if(sys->dup(number, 0) < 0){
		done <-= "Cannot redirect command input\n";
		return;
	}
	input = nil;
	result := "";
	{
		result = run(ctxt, args);
	} exception {
	"*" =>
		result = "Command failed\n";
	}
	done <-= result;
}

run(ctxt: ref Draw->Context, args: list of string): string
{
	name := hd args;
	if(name == "ls"){
		path := ".";
		if(tl args != nil)
			path = hd tl args;
		return listdir(path);
	}
	path := name;
	if(len path < 4 || path[len path-4:] != ".dis")
		path += ".dis";
	if(path[0] != '/')
		path = "/dis/" + path;
	app := load Command path;
	if(app == nil)
		return sys->sprint("%s: %r\n", name);
	app->init(ctxt, args);
	return nil;
}

listdir(path: string): string
{
	fd := sys->open(path, Sys->OREAD);
	if(fd == nil)
		return sys->sprint("ls: %r\n");
	out := sys->fildes(1);
	for(;;){
		(n, rows) := sys->dirread(fd);
		if(n < 0)
			return sys->sprint("ls: %r\n");
		if(n == 0)
			break;
		for(i := 0; i < n; i++){
			name := rows[i].name;
			sys->fprint(out, "%s\n", name);
		}
	}
	return nil;
}

display(reader: ref Sys->FD, output, status, done: chan of string)
{
	buf := array[4096] of byte;
	for(;;){
		n := sys->read(reader, buf, len buf);
		if(n <= 0)
			break;
		chunk := string buf[0:n];
		output <-= chunk;
	}
	s: string;
	alt {
	s = <-status =>;
	* =>;
	}
	done <-= s;
}

append(top: ref Tk->Toplevel, text: string)
{
	cmd(top, ".view.t configure -state normal");
	command := ".view.t insert end '" + text;
	cmd(top, command);
	cmd(top, ".view.t configure -state disabled; .view.t see end; update");
}

cmd(top: ref Tk->Toplevel, command: string)
{
	err := tk->cmd(top, command);
	if(err != nil && err[0] == '!')
		raise "fail:Shell: " + err;
}
