implement Awmfiles;

include "sys.m";
	sys: Sys;
include "draw.m";
include "sh.m";
include "tk.m";
	tk: Tk;
include "tkclient.m";
	tkclient: Tkclient;

include "awmfiles.m";

Maxentries: con 256;
Previewbytes: con 4096;
Home: con "/usr/inferno";

init(ctxt: ref Draw->Context, nil: list of string)
{
	browse(ctxt, Home, 0);
}

choose(ctxt: ref Draw->Context, directory: string): string
{
	if(directory == nil || directory == ".")
		directory = Home;
	return browse(ctxt, directory, 1);
}

browse(ctxt: ref Draw->Context, path: string, picking: int): string
{
	sys = load Sys Sys->PATH;
	tk = load Tk Tk->PATH;
	tkclient = load Tkclient Tkclient->PATH;
	if(ctxt == nil || sys == nil || tk == nil || tkclient == nil)
		raise "fail:cannot start Files";
	tkclient->init();
	caption := "Files";
	buttons := Tkclient->Hide;
	if(picking){
		caption = "Open file";
		buttons = Tkclient->Popup;
	}
	(top, title) := tkclient->toplevel(ctxt, "", caption, buttons);
	actions := chan of string;
	# The editor can finish after this Files window closes.
	edited := chan[1] of string;
	tk->namechan(top, actions, "files");
	widgets(top, picking);
	previewed, editing: string;
	names := array[Maxentries] of string;
	modes := array[Maxentries] of int;
	count := listdir(top, path, names, modes);
	tkclient->startinput(top, "ptr"::"kbd"::nil);
	tkclient->onscreen(top, "place");
	tkclient->wmctl(top, "kbdfocus 1");
	cmd(top, "focus .entries.list; .entries.list activate 0; update");

	for(;;){
		wm := top.ctxt;
		wreq := top.wreq;
		alt{
		p := <-wm.ptr =>
			if(p != nil)
				tk->pointer(top, *p);
		c := <-wm.kbd =>
			tk->keyboard(top, c);
		s := <-wm.ctl or
		s = <-wreq or
		s = <-title =>
			if(s == "exit")
				return nil;
			if(s == "task")
				tkclient->wmctl(top, "kbdfocus 0");
			if(s != nil)
				tkclient->wmctl(top, s);
			if(s == "untask")
				tkclient->wmctl(top, "kbdfocus 1");
		result := <-edited =>
			if(editing == previewed){
				content := preview(editing);
				setpreview(top, content);
			}
			editing = nil;
			cmd(top, ".nav.edit configure -state normal");
			if(result != nil){
				message := ".status configure -text '" + result;
				cmd(top, message);
			}
			cmd(top, "update");
		action := <-actions =>
			reload := 1;
			case action {
			"close" =>
				return nil;
			"up" =>
				path = parent(path);
			"root" =>
				path = "/";
			"home" =>
				path = Home;
			"refresh" =>
				;
			"open" or "openactive" or "edit" =>
				reload = 0;
				if(action == "edit" && (picking || editing != nil))
					break;
				query := ".entries.list curselection";
				if(action == "openactive")
					query = ".entries.list index active";
				selected := tk->cmd(top, query);
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
					if(picking)
						return child;
					if(action == "edit"){
						editing = child;
						cmd(top, ".nav.edit configure -state disabled");
						spawn edit(ctxt, child, edited);
					}
					previewed = child;
					content := preview(child);
					setpreview(top, content);
					cmd(top, "update");
					break;
				}
				if(action == "edit"){
					cmd(top, ".status configure -text {Select a file to edit}; update");
					break;
				}
				path = child;
				reload = 1;
			* =>
				reload = 0;
			}
			if(reload){
				previewed = nil;
				if(!picking)
					setpreview(top, "");
				count = listdir(top, path, names, modes);
				cmd(top, ".entries.list activate 0; update");
			}
		}
	}
}

edit(ctxt: ref Draw->Context, path: string, done: chan of string)
{
	result := "";
	{
		app := load Command "/dis/wm/edit.dis";
		if(app == nil)
			result = "Cannot load Edit";
		else{
			args := "edit" :: path :: nil;
			app->init(ctxt, args);
		}
	} exception {
		"*" =>
			result = "Edit was interrupted";
	}
	done <-= result;
}

widgets(top: ref Tk->Toplevel, picking: int)
{
	cmd(top, "label .path -anchor w -bg #dce8f5; pack .path -fill x -padx 6 -pady 3");
	cmd(top, "frame .nav; pack .nav -fill x -padx 6 -pady 4");
	cmd(top, "button .nav.up -text Up -takefocus 0 -command {send files up}");
	cmd(top, "button .nav.root -text Root -takefocus 0 -command {send files root}");
	cmd(top, "button .nav.home -text Home -takefocus 0 -command {send files home}");
	cmd(top, "button .nav.open -text Open -takefocus 0 -command {send files open}");
	cmd(top, "button .nav.refresh -text Refresh -takefocus 0 -command {send files refresh}");
	if(picking)
		cmd(top, "button .nav.close -text Cancel -takefocus 0 -command {send files close}");
	else
		cmd(top, "button .nav.close -text Close -takefocus 0 -command {send files close}");
	cmd(top, "pack .nav.up .nav.root .nav.home .nav.open .nav.refresh .nav.close -side left -padx 3 -ipady 6");
	if(!picking){
		cmd(top, "button .nav.edit -text Edit -takefocus 0 -command {send files edit}");
		cmd(top, "pack .nav.edit -side left -padx 3 -ipady 6");
	}

	cmd(top, "frame .entries; pack .entries -fill x -padx 6");
	cmd(top, "listbox .entries.list -width 340 -height 130 -selectmode single " +
		"-yscrollcommand {.entries.scroll set}");
	cmd(top, "scrollbar .entries.scroll -command {.entries.list yview}");
	cmd(top, "pack .entries.scroll -side right -fill y");
	cmd(top, "pack .entries.list -side left -expand 1 -fill both");
	cmd(top, "bind .entries.list <Double-Button-1> {send files open}");
	cmd(top, "bind .entries.list <Key-\n> {send files openactive}");
	cmd(top, "bind .entries.list <Key-\b> {send files up}");

	if(picking)
		cmd(top, "bind .entries.list <Key-\u001b> {send files close}");
	else{
		cmd(top, "frame .document; pack .document -fill x -padx 6 -pady 6");
		cmd(top, "text .document.preview -width 340 -height 100 -wrap word -state disabled " +
			"-takefocus 0 -bg #ffffff -yscrollcommand {.document.scroll set}");
		cmd(top, "scrollbar .document.scroll -command {.document.preview yview}");
		cmd(top, "pack .document.scroll -side right -fill y");
		cmd(top, "pack .document.preview -side left -expand 1 -fill both");
	}
	cmd(top, "label .status -anchor w; pack .status -fill x -padx 6 -pady 3");
}

listdir(top: ref Tk->Toplevel, path: string, names: array of string, modes: array of int): int
{
	header := ".path configure -text '" + path;
	cmd(top, header);
	cmd(top, ".entries.list delete 0 end");
	fd := sys->open(path, Sys->OREAD);
	if(fd == nil){
		cmd(top, ".status configure -text {Cannot open directory}; update");
		return 0;
	}

	used := 0;
	status := "";
	while(used < Maxentries){
		(n, rows) := sys->dirread(fd);
		if(n < 0){
			status = "Cannot read directory";
			break;
		}
		if(n == 0)
			break;
		for(i := 0; i < len rows && used < Maxentries; i++){
			name := rows[i].name;
			mode := rows[i].mode;
			names[used] = name;
			modes[used] = mode;
			used++;
		}
	}
	sortentries(names, modes, used);
	for(i := 0; i < used; i++){
		name := names[i];
		mode := modes[i];
		if(mode & Sys->DMDIR)
			name += "/";
		# A leading single quote makes the rest of this Tk command literal.
		entry := ".entries.list insert end '" + name;
		cmd(top, entry);
	}
	if(used == Maxentries)
		status = "Showing up to 256 entries";
	if(status == nil)
		status = string used + " entries";
	msg := ".status configure -text '" + status;
	cmd(top, msg);
	cmd(top, "update");
	return used;
}

# Keep the names and cached mode bits in the same order.
sortentries(names: array of string, modes: array of int, used: int)
{
	for(i := 1; i < used; i++){
		name := names[i];
		mode := modes[i];
		j := i;
		while(j > 0){
			previous := j - 1;
			other := names[previous];
			if(other <= name)
				break;
			othermode := modes[previous];
			names[j] = other;
			modes[j] = othermode;
			j = previous;
		}
		names[j] = name;
		modes[j] = mode;
	}
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
