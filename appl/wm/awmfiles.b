implement Awmfiles;

include "sys.m";
	sys: Sys;
include "draw.m";
include "tk.m";
	tk: Tk;
include "tkclient.m";
	tkclient: Tkclient;

Awmfiles: module
{
	init: fn(ctxt: ref Draw->Context, argv: list of string);
};

Maxentries: con 256;
Previewbytes: con 4096;
Home: con "/usr/inferno";

init(ctxt: ref Draw->Context, nil: list of string)
{
	sys = load Sys Sys->PATH;
	tk = load Tk Tk->PATH;
	tkclient = load Tkclient Tkclient->PATH;
	if(ctxt == nil || sys == nil || tk == nil || tkclient == nil)
		raise "fail:cannot start Files";
	tkclient->init();
	(top, title) := tkclient->toplevel(ctxt, "", "Files", Tkclient->Hide);
	actions := chan of string;
	tk->namechan(top, actions, "files");
	widgets(top);

	path := Home;
	names := array[Maxentries] of string;
	modes := array[Maxentries] of int;
	count := listdir(top, path, names, modes);
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
			if(s != nil)
				tkclient->wmctl(top, s);
		action := <-actions =>
			reload := 1;
			case action {
			"close" =>
				return;
			"up" =>
				path = parent(path);
			"root" =>
				path = "/";
			"home" =>
				path = Home;
			"refresh" =>
				;
			"open" =>
				reload = 0;
				selected := tk->cmd(top, ".entries.list curselection");
				if(selected == nil || selected[0] == '!')
					break;
				i := int selected;
				if(i < 0 || i >= count)
					break;
				name := names[i];
				mode := modes[i];
				child := path;
				if(child != "/")
					child += "/";
				child += name;
				if(!(mode & Sys->DMDIR)){
					content := preview(child);
					setpreview(top, content);
					cmd(top, "update");
					break;
				}
				path = child;
				reload = 1;
			* =>
				reload = 0;
			}
			if(reload)
				count = listdir(top, path, names, modes);
		}
	}
}

widgets(top: ref Tk->Toplevel)
{
	cmd(top, "label .path -anchor w -bg #dce8f5; pack .path -fill x -padx 6 -pady 3");
	cmd(top, "frame .nav; pack .nav -fill x -padx 6 -pady 4");
	cmd(top, "button .nav.up -text Up -command {send files up}");
	cmd(top, "button .nav.root -text Root -command {send files root}");
	cmd(top, "button .nav.home -text Home -command {send files home}");
	cmd(top, "button .nav.open -text Open -command {send files open}");
	cmd(top, "button .nav.refresh -text Refresh -command {send files refresh}");
	cmd(top, "button .nav.close -text Close -command {send files close}");
	cmd(top, "pack .nav.up .nav.root .nav.home .nav.open .nav.refresh .nav.close -side left -padx 3 -ipady 6");

	cmd(top, "frame .entries; pack .entries -fill x -padx 6");
	cmd(top, "listbox .entries.list -width 340 -height 130 -selectmode single " +
		"-yscrollcommand {.entries.scroll set}");
	cmd(top, "scrollbar .entries.scroll -command {.entries.list yview}");
	cmd(top, "pack .entries.scroll -side right -fill y");
	cmd(top, "pack .entries.list -side left -expand 1 -fill both");
	cmd(top, "bind .entries.list <Double-Button-1> {send files open}");

	cmd(top, "frame .document; pack .document -fill x -padx 6 -pady 6");
	cmd(top, "text .document.preview -width 340 -height 100 -wrap word -state disabled " +
		"-bg #ffffff -yscrollcommand {.document.scroll set}");
	cmd(top, "scrollbar .document.scroll -command {.document.preview yview}");
	cmd(top, "pack .document.scroll -side right -fill y");
	cmd(top, "pack .document.preview -side left -expand 1 -fill both");
	cmd(top, "label .status -anchor w; pack .status -fill x -padx 6 -pady 3");
}

listdir(top: ref Tk->Toplevel, path: string, names: array of string, modes: array of int): int
{
	header := ".path configure -text '" + path;
	cmd(top, header);
	cmd(top, ".entries.list delete 0 end");
	setpreview(top, "");
	fd := sys->open(path, Sys->OREAD);
	if(fd == nil){
		cmd(top, ".status configure -text {Cannot open directory}; update");
		return 0;
	}

	used := 0;
	for(;;){
		(n, rows) := sys->dirread(fd);
		if(n < 0){
			cmd(top, ".status configure -text {Cannot read directory}; update");
			return used;
		}
		if(n == 0)
			break;
		for(i := 0; i < len rows && used < Maxentries; i++){
			name := rows[i].name;
			mode := rows[i].mode;
			names[used] = name;
			modes[used] = mode;
			used++;
			if(mode & Sys->DMDIR)
				name += "/";
			# A leading single quote makes the rest of this Tk command literal.
			entry := ".entries.list insert end '" + name;
			cmd(top, entry);
		}
		if(used == Maxentries){
			cmd(top, ".status configure -text {Showing up to 256 entries}; update");
			return used;
		}
	}
	msg := ".status configure -text {" + string used + " entries}; update";
	cmd(top, msg);
	return used;
}

parent(path: string): string
{
	i := len path - 1;
	while(i > 0 && path[i] != '/')
		i--;
	if(i == 0)
		return "/";
	return path[0:i];
}

preview(path: string): string
{
	fd := sys->open(path, Sys->OREAD);
	if(fd == nil)
		return "Cannot open file";
	(ok, metadata) := sys->fstat(fd);
	if(ok < 0)
		return "Cannot stat file";
	if(metadata.length == big 0)
		return "No file content";
	buf := array[Previewbytes] of byte;
	n := sys->read(fd, buf, len buf);
	if(n <= 0)
		return "Cannot read file";
	text := string buf[0:n];
	if(metadata.length > big n)
		text += "\n[Preview truncated]";
	return text;
}

setpreview(top: ref Tk->Toplevel, text: string)
{
	cmd(top, ".document.preview configure -state normal; .document.preview delete 1.0 end");
	add := ".document.preview insert end '" + text;
	cmd(top, add);
	cmd(top, ".document.preview configure -state disabled; .document.preview yview moveto 0");
}

cmd(top: ref Tk->Toplevel, command: string)
{
	err := tk->cmd(top, command);
	if(err != nil && err[0] == '!')
		raise "fail:Files: " + err;
}
