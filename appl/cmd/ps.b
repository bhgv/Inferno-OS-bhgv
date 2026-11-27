implement Ps;

include "sys.m";
include "draw.m";

FD, Dir: import Sys;
Context: import Draw;

Ps: module
{
	init:	fn(ctxt: ref Context, argv: list of string);
};

sys: Sys;
stderr: ref FD;

init(nil: ref Context, nil: list of string)
{
	sys = load Sys Sys->PATH;

	stderr = sys->fildes(2);

	sys->pctl(Sys->FORKNS, nil);
	if(sys->chdir("/prog") < 0){
		sys->fprint(stderr, "ps: can't chdir to /prog: %r\n");
		raise "fail:no /prog";
	}
	fd := sys->open(".", sys->OREAD);
	if(fd == nil) {
		sys->fprint(stderr, "ps: cannot open /prog: %r\n");
		raise "fail:no /prog";
	}

	for(;;) {
		(n, d) := sys->dirread(fd);
		if(n <= 0){
			if(n < 0) {
				sys->fprint(stderr, "ps: error reading /prog: %r\n");
				raise "fail:error on /prog";
			}
			break;
		}
		for(i := 0; i < n; i++){
			name := d[i].name;
			if(name != nil && name[0] >= '0' && name[0] <= '9'){
				pid := int name;
				ps(pid);
			}
		}
	}
}

ps(pid: int)
{
	proc := string pid+"/status";
	fd := sys->open(proc, sys->OREAD);
	if(fd == nil) {	# process must have died
		# sys->fprint(stderr, "ps: /prog/%s: %r\n", proc);
		return;
	}
	buf := array[128] of byte;
	n := sys->read(fd, buf, len buf);
	if(n > 0){
		line := string buf[0:n];
		sys->print("%s\n", line);
	}
}
