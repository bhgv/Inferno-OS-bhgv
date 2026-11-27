implement WmHelp;

include "sys.m";
	sys: Sys;
include "draw.m";
include "tk.m";
	tk: Tk;
include "tkclient.m";
	tkclient: Tkclient;
include "string.m";
	str: String;
include "awmcommands.m";

WmHelp: module
{
	init: fn(ctxt: ref Draw->Context, argv: list of string);
};

Manualbytes: con 64 * 1024;

Sections := array[] of {"1", "2", "3", "4", "5", "6", "7", "8", "9", "10"};

init(ctxt: ref Draw->Context, argv: list of string)
{
	sys = load Sys Sys->PATH;
	tk = load Tk Tk->PATH;
	tkclient = load Tkclient Tkclient->PATH;
	str = load String String->PATH;
	if(ctxt == nil || sys == nil || tk == nil || tkclient == nil || str == nil)
		raise "fail:cannot start Help";
	tkclient->init();
	(top, title) := tkclient->toplevel(ctxt, "", "Help", Tkclient->Hide);
	action := chan of string;
	tk->namechan(top, action, "manual");
	widgets(top);
	home(top);
	if(argv != nil && tl argv != nil){
		query := hd tl argv;
		if(tl tl argv != nil)
			query += " " + hd tl tl argv;
		cmd(top, ".nav.topic insert 0 '" + query);
		show(top, query);
	}
	tkclient->startinput(top, "ptr" :: "kbd" :: nil);
	tkclient->onscreen(top, "place");
	cmd(top, "focus .nav.topic; update");

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
		case a {
		"home" =>
			home(top);
		"open" =>
			query := tk->cmd(top, ".nav.topic get");
			show(top, query);
		}
	}
}

widgets(top: ref Tk->Toplevel)
{
	cmd(top, "frame .nav; pack .nav -fill x");
	cmd(top, "entry .nav.topic -width 198; bind .nav.topic <Key-\n> {send manual open}");
	cmd(top, "button .nav.open -text Open -command {send manual open}");
	cmd(top, "button .nav.home -text Home -command {send manual home}");
	cmd(top, "pack .nav.topic .nav.open .nav.home -side left -padx 3 -ipady 5");
	cmd(top, "label .status -anchor w; pack .status -fill x");
	cmd(top, "frame .book; pack .book -fill both -expand 1");
	cmd(top, "text .book.body -width 342 -height 198 -bg white -wrap word -state disabled " +
		"-yscrollcommand {.book.scroll set}");
	cmd(top, "scrollbar .book.scroll -command {.book.body yview}");
	cmd(top, "pack .book.scroll -side right -fill y; pack .book.body -fill both -expand 1");
}

home(top: ref Tk->Toplevel)
{
	cmd(top, ".nav.topic delete 0 end; .status configure -text {Inferno Manual}");
	cmd(top, ".book.body configure -state normal; .book.body delete 1.0 end");
	insert(top, "Enter a topic such as wm/man, sh or sys(2), then press Open.\n" +
		"You can open the same pages from Shell with help followed by a topic.\n\n" +
		"Commands from appl/cmd available in this build\n");
	catalog := Awmcommands + "\n";
	insert(top, catalog);
	cmd(top, ".book.body configure -state disabled; .book.body yview 1.0; focus .nav.topic; update");
}

show(top: ref Tk->Toplevel, query: string)
{
	words := str->unquoted(query);
	if(words == nil){
		home(top);
		return;
	}
	topic := hd words;
	section := "";
	if(len words == 2){
		section = topic;
		topic = hd tl words;
	}else if(len words == 1 && len topic > 2 && topic[len topic-1] == ')'){
		for(i := 1; i < len topic-1; i++)
			if(topic[i] == '('){
				section = topic[i+1:len topic-1];
				topic = topic[:i];
				break;
			}
	}else if(len words > 2){
		cmd(top, ".status configure -text {Enter a topic or topic(section)}; update");
		return;
	}
	path := "";
	for(i := 0; i < len Sections; i++){
		s := Sections[i];
		if(section != nil && s != section)
			continue;
		path = lookup(s, topic);
		if(path != nil){
			section = s;
			break;
		}
	}
	page: string;
	if(path != nil)
		page = readmanual(path);
	if(page == nil){
		status := ".status configure -text 'No manual page for " + topic;
		cmd(top, status);
		cmd(top, "update");
		return;
	}
	status := ".status configure -text '" + topic + "(" + section + ")";
	cmd(top, status);
	cmd(top, ".book.body configure -state normal; .book.body delete 1.0 end");
	(nil, lines) := sys->tokenize(page, "\n");
	for(; lines != nil; lines = tl lines){
		line := hd lines + "\n";
		text := manualtext(line);
		if(text != nil)
			insert(top, text);
	}
	cmd(top, ".book.body configure -state disabled; .book.body yview 1.0; update");
}

lookup(section, topic: string): string
{
	dir := "/man/" + section + "/";
	indexpath := dir + "INDEX";
	index := readmanual(indexpath);
	(nil, lines) := sys->tokenize(index, "\n");
	for(; lines != nil; lines = tl lines){
		row := hd lines;
		(n, fields) := sys->tokenize(row, " \t\r");
		if(n != 2 || hd fields != topic)
			continue;
		file := hd tl fields;
		if(filename(file))
			return dir + file;
	}
	if(filename(topic)){
		path := dir + topic;
		(ok, entry) := sys->stat(path);
		if(ok >= 0 && !(entry.mode & Sys->DMDIR))
			return path;
	}
	return nil;
}

readmanual(path: string): string
{
	fd := sys->open(path, Sys->OREAD);
	if(fd == nil)
		return nil;
	buffer := array[Manualbytes] of byte;
	used := 0;
	while(used < len buffer){
		rest := buffer[used:];
		n := sys->read(fd, rest, len rest);
		if(n <= 0)
			break;
		used += n;
	}
	return string buffer[0:used];
}

filename(file: string): int
{
	if(file == nil || file == "." || file == "..")
		return 0;
	for(i := 0; i < len file; i++){
		c := file[i];
		if(!(c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || c >= '0' && c <= '9' ||
		     c == '.' || c == '-' || c == '_'))
			return 0;
	}
	return 1;
}

# Draw the text of the shipped manual. Formatting requests are never executed.
manualtext(line: string): string
{
	prefix := "";
	if(line[0] == '.'){
		i := 1;
		while(i < len line && line[i] != ' ' && line[i] != '\t' && line[i] != '\n')
			i++;
		macro := line[1:i];
		case macro {
		"PP" or "LP" or "P" or "TP" or "br" or "sp" =>
			return "\n";
		"SH" or "SS" =>
			prefix = "\n";
		"B" or "I" or "BR" or "BI" or "IB" or "IR" or "RB" or "RI" or "IP" =>
			;
		* =>
			return nil;
		}
		while(i < len line && (line[i] == ' ' || line[i] == '\t'))
			i++;
		line = line[i:];
	}
	return literal(line, prefix);
}

literal(line, text: string): string
{
	for(i := 0; i < len line; i++){
		c := line[i];
		if(c == '\\' && i+1 < len line){
			i++;
			c = line[i];
			case c {
			'f' =>
				if(i+1 < len line)
					i++;
				continue;
			'&' or '|' or '^' =>
				continue;
			'e' =>
				c = '\\';
			'"' =>
				return text + "\n";
			'(' =>
				if(i+2 < len line){
					pair := line[i+1:i+3];
					i += 2;
					if(pair == "bu")
						c = 16r2022;
					else if(pair == "em" || pair == "en" || pair == "hy")
						c = '-';
					else
						continue;
				}
			}
		}
		text[len text] = c;
	}
	return text;
}

insert(top: ref Tk->Toplevel, text: string)
{
	command := ".book.body insert end '" + text;
	cmd(top, command);
}

cmd(top: ref Tk->Toplevel, command: string)
{
	err := tk->cmd(top, command);
	if(err != nil && err[0] == '!')
		raise "fail:Help: " + err;
}
