#
# Copyright © 1996-1999 Lucent Technologies Inc.  All rights reserved.
#	Modified version of edit
#	D.B.Knudsen
# Revisions Copyright © 2000-2002 Vita Nuova Holdings Limited.  All rights reserved.
#
implement WmEdit;

include "sys.m";
	sys: Sys;

include "draw.m";

include "tk.m";
	tk: Tk;

include "tkclient.m";
	tkclient: Tkclient;

include "dialog.m";
	dialog: Dialog;

include "selectfile.m";
	selectfile: Selectfile;

include "limboedit.m";
	limboedit: Limboedit;

WmEdit: module
{
	init:	fn(ctxt: ref Draw->Context, argv: list of string);
};

ErrIco: con "error -fg red";

ed: ref Tk->Toplevel;
dirty := 0;

limbotags := array[] of {
	Limboedit->Keyword => "keyword",
	Limboedit->Comment => "comment",
	Limboedit->Literal => "literal",
	Limboedit->Number => "number",
	Limboedit->Function => "function",
	* => "",
};

document: ref Limboedit->Document;
repaint: chan of int;
repainting := 0;
choices: array of ref Limboedit->Symbol;
completing := 0;
choice_source: string;
complete_start, complete_stop, complete_cursor: int;

SEARCH,
SEARCHFOR,
REPLACE,
REPLACEWITH,
REPLACEALL,
NOSEE : con iota;

ed_config := array[] of {
	"frame .m -relief raised",
	"frame .b",
	"menubutton .m.file -text File -menu .m.file.menu",
	"menubutton .m.edit -text Edit -menu .m.edit.menu",
	"menubutton .m.search -text Search -menu .m.search.menu",
	"menubutton .m.options -text Options -menu .m.options.menu",
#	"label .m.filename",
	"menubutton .m.code -text Code -menu .m.code.menu",
	"pack .m.file .m.edit .m.search .m.options .m.code -side left",
#	"pack .m.filename -padx 10 -side left",
	"menu .m.file.menu",
	".m.file.menu add command -label New -command {send c new}",
	".m.file.menu add command -label Open... -command {send c open}",
	".m.file.menu add separator",
	".m.file.menu add command -label Save -command {send c save}",
	".m.file.menu add command -label {Save As...} -command {send c saveas}",
	".m.file.menu add separator",
	".m.file.menu add command -label {Exit} -command {send c exit}",
	"menu .m.edit.menu",
	".m.edit.menu add command -label Cut -command {send c cut}",
	".m.edit.menu add command -label Copy -command {send c copy}",
	".m.edit.menu add command -label Paste -command {send c paste}",
	"menu .m.search.menu",
	".m.search.menu add command -label {Find ...} " +
					"-command {send c searchf}",
	".m.search.menu add command -label {Replace with...} " +
					"-command {send c replacew}",
	".m.search.menu add command -label {Find Again} -command {send c search}",
	".m.search.menu add command -label {Find and Replace} " +
					"-command {send c replace}",
	".m.search.menu add command -label {Find and Replace All} " +
					"-command {send c replaceall}",
	"menu .m.options.menu",
	".m.options.menu add checkbutton -text Limbo -variable limbo -command {send c limbo}",
	".m.options.menu add command -label Indent -command {send c indent}",
	"menu .m.code.menu",
	".m.code.menu add command -label {Functions (Ctrl+G)} -command {send c functions}",
	".m.code.menu add command -label {Complete (Ctrl+T)} -command {send c complete}",
	"text .b.t  -yscrollcommand {.b.s set} -bg white",
	"bind .b.t <Button-2> {.m.edit.menu post %X %Y}",
	"bind .b.t <Key> +{send c key %s}",
	"bind .b.t <ButtonRelease-1> +{send c reindent}",
	"scrollbar .b.s -command {.b.t yview}",
	"pack .m -fill x",
	"pack .b.s -fill y -side left",
	"pack .b.t -fill both -expand 1",
	"pack .b -fill both -expand 1",
	"focus .b.t",
	"pack propagate . 0",
	".b.t tag configure keyword -fg #204bab",
	".b.t tag configure comment -fg #387141",
	".b.t tag configure literal -fg #9b3223",
	".b.t tag configure number -fg #86468c",
	".b.t tag configure function -fg #046782",
	"bind .b.t <Control-g> {send c functions}",
	"bind .b.t <Control-t> {send c complete}",
	"frame .b.code -relief groove -borderwidth 2",
	"label .b.code.title -anchor w",
	"frame .b.code.actions",
	"button .b.code.actions.choose -text Open -command {send c choose}",
	"button .b.code.actions.hide -text Hide -command {send c hidecode}",
	"pack .b.code.actions.choose .b.code.actions.hide -side left -ipadx 4 -ipady 3",
	"listbox .b.code.list -width 176 -selectmode single -yscrollcommand {.b.code.scroll set}",
	"scrollbar .b.code.scroll -command {.b.code.list yview}",
	"pack .b.code.title -side top -fill x",
	"pack .b.code.actions -side bottom -fill x",
	"pack .b.code.scroll -side right -fill y",
	"pack .b.code.list -expand 1 -fill both",
	"bind .b.code.list <Double-Button-1> {send c chooseselected}",
	"bind .b.code.list <Key-\n> {send c choose}",
	"bind .b.code.list <Key-\u001b> {send c hidecode}",
	"update",
};

context : ref Draw->Context;
curfile := "(New)";
snarf := "";
searchfor := "";
replacewith := "";
path := ".";

init(ctxt: ref Draw->Context, argv: list of string)
{
	wmctl: chan of string;

	sys = load Sys Sys->PATH;
	tk = load Tk Tk->PATH;
	tkclient = load Tkclient Tkclient->PATH;
	selectfile = load Selectfile Selectfile->PATH;
	dialog = load Dialog Dialog->PATH;
	limboedit = load Limboedit Limboedit->PATH;
	repaint = chan[1] of int;

	sys->pctl(Sys->NEWPGRP, nil);
	tkclient->init();
	if(selectfile != nil && selectfile->init() != nil)
		selectfile = nil;
	dialog->init();

	context = ctxt;

	(ed, wmctl) = tkclient->toplevel(context, "", "Edit", Tkclient->Appl);

	argv = tl argv;

	c := chan of string;
	tk->namechan(ed, c, "c");
	for (i := 0; i < len ed_config; i++)
		cmd(ed, ed_config[i]);

	if (argv != nil) {
		e := loadtfile(hd argv);
		if(e != nil)
			dialog->prompt(ctxt, ed.image, ErrIco, "Open file", e, 0, "Ok"::nil);
	}

	tkclient->settitle(ed, "Edit " + curfile);
	tkclient->onscreen(ed, nil);
	tkclient->startinput(ed, "ptr" :: "kbd" :: nil);
	cmd(ed, "update");

	e := cmd(ed, "variable lasterror");
	if(e != "") {
		sys->print("edit error: %s\n", e);
		return;
	}

	cmdloop: for(;;) {
		wm := ed.ctxt;
		keys := wm.kbd;
		pointer := wm.ptr;
		control := wm.ctl;
		requests := ed.wreq;
		alt {
		key := <-keys =>
			tk->keyboard(ed, key);
			schedulepaint();
		<-repaint =>
			repainting = 0;
			paint();
		m := <-pointer =>
			tk->pointer(ed, *m);
		s := <-control or
		s = <-requests or
		s = <-wmctl =>
			if(s == "exit") {
				if (check_dirty())
					break cmdloop;
				else
					break;
			}
			task_title: string;
			if (s == "task") {
				if (curfile == "(New)")
					task_title = tkclient->settitle(ed, "Edit");
				else
					task_title = tkclient->settitle(ed, "Edit " + curfile);
				cmd(ed, "update");
			}
			tkclient->wmctl(ed, s);
			if (s == "task")
				tkclient->settitle(ed, task_title);
		s := <-c =>
			if(len s > 4 && s[:4] == "key "){
				key := int s[4:];
				if(key != ('g' & 31) && key != ('t' & 31)){
					set_dirty();
					do_limbo_check(key);
				}
			}else{
				case s {
				"exit" =>	if ( check_dirty() ){ set_clean(); break cmdloop; }
				"new" =>	if ( check_dirty()) {set_clean(); do_new();}
				"open" =>	if ( check_dirty() && do_open()) set_clean();
				"save" =>	do_save(0);
				"saveas" =>	do_save(1);
				"cut" =>	do_snarf(1); set_dirty();
				"copy" =>	do_snarf(0);
				"paste" =>	do_paste(); set_dirty();
				"search" =>	do_search(SEARCH);
				"searchf" =>	do_search(SEARCHFOR);
				"replace" =>	do_replace(REPLACE);
				"replacew" =>	do_replace(REPLACEWITH);
				"replaceall" =>	do_replaceall();
				"limbo" =>	do_limbo();
				"indent" =>	do_indent();
				"reindent" =>	re_indent();
				"functions" =>	showfunctions();
				"complete" =>	showcompletions();
				"choose" =>	choose(".b.code.list index active");
				"chooseselected" => choose(".b.code.list curselection");
				"hidecode" =>	hidecode();
				}
				if(s != "functions" && s != "complete" && s != "choose" && s != "chooseselected")
					cmd(ed, "focus .b.t");
			}
			schedulepaint();
		}
		cmd(ed, "update");
		e = cmd(ed, "variable lasterror");
		if(e != "") {
			sys->print("edit error: %s\n", e);
			break cmdloop;
		}
	}
}

check_dirty() : int
{
	if ( dirty == 0 )
		return 1;
	if (dialog->prompt(context, ed.image, ErrIco, "Confirm",
					"File was changed.\nDiscard changes?",
					0, "Yes" :: "No" :: nil) == 0 ) {
		return 1;
	}
	return 0;
}

set_dirty()
{
	if(!dirty){
		dirty = 1;
		title := "Edit " + curfile + " (dirty)";
		tkclient->settitle(ed, title);
		cmd(ed, "update");
	}
#	We want to just remove the binding, but Inferno's tk does not
#	recognize the - in front of the command.  To make it do so would
#	require changes to utils.c and ebind.c in /tk
#	cmd(ed, "bind .b.t <Key> -{send c dirtied}");
}

set_clean()
{
	if(dirty){
		dirty = 0;
		tkclient->settitle(ed, "Edit " + curfile);
		cmd(ed, "update");
		#cmd(ed, "bind .b.t <Key> +{send c dirtied}");
	}
}

BLOCK, TEMP : con iota;
is_limbo	:= 0;
this_word := "";
in_comment	:= 0;
first_char	:= 1;
indent		: list of int;
last_kw_is_block := 0;
tab		:= "\t";
tabs		:= array[] of {
	"", "\t", "\t\t", "\t\t\t", "\t\t\t\t", "\t\t\t\t\t",
	"\t\t\t\t\t\t", "\t\t\t\t\t\t\t", "\t\t\t\t\t\t\t\t"
};

do_limbo()
{
	setlimbo(!is_limbo);
}

setlimbo(on: int)
{
	is_limbo = on;
	command := "variable limbo " + string on;
	cmd(ed, command);
	if(on)
		paint();
	else{
		cleartags();
		document = nil;
	}
}

do_limbo_check(char: int)
{
	if(!is_limbo)
		return;
	case char {
	    ' ' or '\t' =>
		if ( ! in_comment )
			look_keyword(this_word);
		this_word = "" ;
	    '\n' =>
		if ( in_comment ) {
			in_comment = 0;
		}
		else
			look_keyword(this_word);
		this_word = "" ;
		if ( last_kw_is_block )
			indent = TEMP :: indent;
		else while ( indent != nil && hd indent == TEMP )
			indent = tl indent;
		last_kw_is_block = 0;
		add_indent();
		first_char = 1;
		return;
	    '{' =>
		indent = BLOCK :: indent;
		last_kw_is_block = 0;
	    '}' =>
		if ( indent != nil )
			indent = tl indent;
		last_kw_is_block = 0;
	# If the line is just indentation plus '}', rewrite it
	# to have one less indent.
		if ( first_char ) {
			index := cmd(ed, ".b.t index insert");
			current := int index;
			command := ".b.t delete " + string current + ".0 insert";
			cmd(ed, command);
			add_indent();
			cmd(ed, ".b.t insert insert '}");
		}
#	    ';' =>
#		last_kw_is_block = 0;
#	    '\b' =>	# By the time we see this, the character has
#			# already been wiped out, probably.
#			# To know what it was we'd need a lastchar,
#			# reset for each mouse button up and \b
#	    '\u007f' =>	# Here, we have to know what used to be ahead of the
#			# insert point.
	    '#' =>
		in_comment = 1;
	    'A' to 'Z' or 'a' to 'z' or '0' to '9' or '_' =>
		if ( ! in_comment ){
			word := this_word;
			word[len word] = char;
			this_word = word;
		}
	    * =>
		if ( ! in_comment )
			look_keyword(this_word);
		this_word = "";
	}
	first_char = 0;
}

look_keyword(word: string)
{
	case word {
	"do" or "else" or "for" or "if" or "while" =>
		last_kw_is_block = 1;
	* =>
		if(limboedit != nil && word != "nil" && limboedit->keyword(word))
			last_kw_is_block = 0;
	}
}

do_new()
{
	hidecode();
	cmd(ed, ".b.t delete 1.0 end");
	curfile = "(New)";
	tkclient->settitle(ed, "Edit " + curfile);
}

filename(): string
{
	image := ed.image;
	if(selectfile != nil)
		return selectfile->filename(context, image, "", nil, path);
	file := dialog->getstring(context, image, "Open file (path)");
	if(len file > 0 && file[0] != '/' && path != "")
		file = path + "/" + file;
	return file;
}

do_open(): int
{
	for(;;) {
		fname := filename();
		if(fname == "")
			break;
		e := loadtfile(fname);
		if(e == nil) {
			basepath(fname);
			return 1;
		}

		options := list of {
			"Cancel",
			"Open another file"
		};

		if(dialog->prompt(context, ed.image, ErrIco, "Open file", e, 0,  options) == 0)
			break;
	}
	return 0;
}

basepath(file: string)
{
	for(i := len file-1; i >= 0; i--)
		if(file[i] == '/') {
			path = file[0:i];
			break;
		}
}

do_save(prompt: int)
{
	fname := curfile;

	contents := tk->cmd(ed, ".b.t get 1.0 end");
	for(;;) {
		if(prompt || curfile == "(New)") {
			fname = dialog->getstring(context, ed.image, "File");
			if ( len fname > 0 && fname[0] != '/' && path != "" )
				fname = path + "/" + fname;
		}

		if(savetfile(fname, contents)) {
			set_clean();
			break;
		}

		options := list of {
			"Cancel",
			"Try another file"
		};

		msg := sys->sprint("Trying to write file \"%s\"\n%r", fname);
		if(dialog->prompt(context, ed.image, ErrIco, "Save file", msg, 0, options) == 0)
			break;

		prompt = 1;
	}
}

do_snarf(del: int)
{
	range := cmd(ed, ".b.t tag nextrange sel 1.0");
	if(range == "" || (len range > 0 && range[0] == '!'))
		return;
	snarf = tk->cmd(ed, ".b.t get " + range);
	if(del)
		cmd(ed, ".b.t delete " + range);
	tkclient->snarfput(snarf);
}

do_paste()
{
	snarf = tkclient->snarfget();
	if(snarf == "")
		return;
	cmd(ed, ".b.t insert insert '" + snarf);
}

do_search(prompt: int) : int
{
	if(prompt == SEARCHFOR)
		searchfor = dialog->getstring(context, ed.image, "Search For");
	if(searchfor == "")
		return 0;
	cmd(ed, "cursor -bitmap cursor.wait");
	pattern := tk->quote(searchfor);
	command := ".b.t search -- " + pattern + " insert+1c";
	ix := cmd(ed, command);
	if(ix != "" && len ix > 1 && ix[0] != '!') {
		cmd(ed, ".b.t tag remove sel 0.0 end");
		cmd(ed, ".b.t mark set anchor " + ix);
		cmd(ed, ".b.t mark set insert " + ix);
		length := string(len searchfor);
		command = ".b.t tag add sel " + ix + " " + ix + "+" + length + "c";
		cmd(ed, command);
		if ( prompt != NOSEE )
			cmd(ed, ".b.t see " + ix);
		cmd(ed, "cursor -default");
		return 1;
	}
	cmd(ed, "cursor -default");
	return 0;
}

do_replace(prompt : int)
{
	range := "";
	if ( prompt == REPLACEWITH ) {
		replacewith = dialog->getstring(context, ed.image, "Replacement String");

		range = cmd(ed, ".b.t tag nextrange sel 1.0");
		if(range == "" || (len range > 0 && range[0] == '!'))
			return;			# nothing currently selected
	}
	if ( range != "" ) {		# there's something selected
		cmd(ed, ".b.t mark set insert sel.first");
	}
	else {				# have to find a string
		if ( searchfor == "" ) {	# no search string!
			if ( do_search(SEARCHFOR) == 0 )
				return;
		}
		else if ( do_search(SEARCH) == 0 )
			return;
	}
	cmd(ed, ".b.t delete sel.first sel.last");
	replacement := tk->quote(replacewith);
	command := ".b.t insert insert " + replacement;
	cmd(ed, command);
}

do_replaceall()
{
	cur := cmd(ed, ".b.t index insert");
	if ( cur == "" || cur[0] == '!' )
		return;
	dirt := 0;
	if ( searchfor == "" )		# no search string
		searchfor = dialog->getstring(context, ed.image, "Search For");
	if ( searchfor == "" )		# still no search string
		return;
	srch := tk->quote(searchfor);
	repl := tk->quote(replacewith);
	for ( ix := "1.0"; len ix > 0 && ix[0] != '!'; ) {
		command := ".b.t search -- " + srch + " " + ix + " end";
		ix = cmd(ed, command);
		if ( ix == "" || len ix <= 1 || ix[0] == '!')
			break;
		length := string(len searchfor);
		command = ".b.t delete " + ix + " " + ix + "+" + length + "c";
		cmd(ed, command);
		if ( replacewith != "" ) {
			command = ".b.t insert " + ix + " " + repl;
			cmd(ed, command);
			length = string(len replacewith);
			command = ".b.t index " + ix + "+" + length + "c";
			ix = cmd(ed, command);
		}
		dirt++;
	}
	cmd(ed, ".b.t mark set insert " + cur);
	if ( dirt > 0 )
		set_dirty();
}
	

loadtfile(path: string): string
{
	if ( path != nil && path[0] == '/' )
		basepath(path);
	fd := sys->open(path, sys->OREAD);
	if(fd == nil)
		return "Can't open "+path+", the error was:\n"+sys->sprint("%r");
	(ok, d) := sys->fstat(fd);
	if(ok < 0)
		return "Can't stat "+path+", the error was:\n"+sys->sprint("%r");
	if(d.mode & Sys->DMDIR)
		return path+" is a directory";

	hidecode();
	cmd(ed, "cursor -bitmap cursor.wait");
	cmd(ed, ".b.t delete 1.0 end");
	document = nil;
	BLEN: con 8192;
	buf := array[BLEN+Sys->UTFmax] of byte;
	inset := 0;
	for(;;) {
		n := sys->read(fd, buf[inset:], BLEN);
		if(n <= 0)
			break;
		n += inset;
		nutf := sys->utfbytes(buf, n);
		s := string buf[0:nutf];
		# move any partial rune to beginning of buffer
		inset = n-nutf;
		buf[0:] = buf[nutf:n];
		cmd(ed, ".b.t insert end '" + s);
	}
	curfile = path;
	suffix := "";
	if(len path >= 2)
		suffix = path[len path-2:];
	limbo := 0;
	if(suffix == ".b" || suffix == ".m")
		limbo = 1;
	setlimbo(limbo);
	cmd(ed, ".b.t mark set insert 1.0");
	re_indent();
	tkclient->settitle(ed, "Edit " + curfile);
	cmd(ed, "cursor -default");
	cmd(ed, "update");
	return "";
}

savetfile(path: string, contents: string): int
{
	buf := array of byte contents;
	n := len buf;

	fd := sys->create(path, sys->OWRITE, 8r664);
	if(fd == nil)
		return 0;
	i := sys->write(fd, buf, n);
	if(i != n) {
		sys->print("savetfile only wrote %d of %d: %r\n", i, n);
		return 0;
	}
	curfile = path;
#	cmd(ed, ".m.filename configure -text '" + curfile);
	tkclient->settitle(ed, "Edit " + curfile);

	return 1;
}

schedulepaint()
{
	if(is_limbo && limboedit != nil && !repainting){
		repainting = 1;
		spawn paintlater(repaint);
	}
}

paintlater(c: chan of int)
{
	sys->sleep(120);
	c <-= 1;
}

cleartags()
{
	for(i := 0; i < len limbotags; i++){
		tag := limbotags[i];
		if(tag != ""){
			command := ".b.t tag remove " + tag + " 1.0 end";
			cmd(ed, command);
		}
	}
}

# Read text directly so a leading exclamation mark stays part of the buffer.
readsource(): string
{
	return tk->cmd(ed, ".b.t get 1.0 end");
}

analyse(source: string): ref Limboedit->Document
{
	if(limboedit == nil)
		return nil;
	cached := document;
	if(cached != nil && cached.source == source)
		return cached;
	d := limboedit->scan(source);
	if(is_limbo){
		document = d;
		cleartags();
		highlight(d);
	}
	return d;
}

paint()
{
	if(is_limbo && limboedit != nil){
		source := readsource();
		analyse(source);
	}
}

highlight(d: ref Limboedit->Document)
{
	ranges := array[len limbotags] of string;
	line := 1;
	column := 0;
	pos := 0;
	s := d.source;
	for(ts := d.tokens; ts != nil; ts = tl ts){
		t := hd ts;
		kind := t.kind;
		tag := limbotags[kind];
		if(tag == "")
			continue;
		begin := t.start;
		end := t.stop;
		for(; pos < begin; pos++){
			if(s[pos] == '\n'){
				line++;
				column = 0;
			}else
				column++;
		}
		start := string line + "." + string column;
		for(; pos < end; pos++){
			if(s[pos] == '\n'){
				line++;
				column = 0;
			}else
				column++;
		}
		range := ranges[kind];
		range += " " + start + " " + string line + "." + string column;
		if(len range >= 2048){
			command := ".b.t tag add " + tag + range;
			cmd(ed, command);
			range = "";
		}
		ranges[kind] = range;
	}
	for(i := 0; i < len ranges; i++){
		range := ranges[i];
		if(range != ""){
			tag := limbotags[i];
			command := ".b.t tag add " + tag + range;
			cmd(ed, command);
		}
	}
}

hidecode()
{
	cmd(ed, "pack forget .b.code");
	choices = nil;
	choice_source = nil;
	cmd(ed, "focus .b.t");
}

showchoices(title, action: string, items: list of ref Limboedit->Symbol)
{
	choices = array[len items] of ref Limboedit->Symbol;
	label := tk->quote(title);
	command := ".b.code.title configure -text " + label;
	cmd(ed, command);
	cmd(ed, ".b.code.list delete 0 end");
	state := "disabled";
	if(items != nil)
		state = "normal";
	command = ".b.code.actions.choose configure -text " + action + " -state " + state;
	cmd(ed, command);
	for(i := 0; items != nil; items = tl items){
		symbol := hd items;
		choices[i] = symbol;
		i++;
		name := symbol.name;
		label = tk->quote(name);
		command = ".b.code.list insert end " + label;
		cmd(ed, command);
	}
	if(len choices > 0)
		cmd(ed, ".b.code.list activate 0");
	cmd(ed, "pack .b.code -side right -fill y -before .b.t");
	cmd(ed, "focus .b.code.list");
}

showfunctions()
{
	completing = 0;
	source := readsource();
	d := analyse(source);
	if(d == nil){
		showchoices("Limbo tools unavailable", "Jump", nil);
		return;
	}
	choice_source = d.source;
	symbols := d.functions;
	if(symbols == nil)
		showchoices("No functions", "Jump", nil);
	else
		showchoices("Functions", "Jump", symbols);
}

showcompletions()
{
	source := readsource();
	d := analyse(source);
	completing = 1;
	if(d == nil){
		showchoices("Limbo tools unavailable", "Insert", nil);
		return;
	}
	choice_source = d.source;
	complete_cursor = len tk->cmd(ed, ".b.t get 1.0 insert");
	completion := limboedit->complete(d, complete_cursor);
	complete_start = completion.start;
	complete_stop = completion.stop;
	words := completion.words;
	if(words == nil){
		showchoices("No completions", "Insert", nil);
		return;
	}
	tail := tl words;
	if(tail == nil){
		word := hd words;
		insertcompletion(word);
		return;
	}
	items: list of ref Limboedit->Symbol;
	for(; words != nil; words = tl words){
		word := hd words;
		symbol := ref Limboedit->Symbol(word, 0);
		items = symbol :: items;
	}
	ordered: list of ref Limboedit->Symbol;
	for(; items != nil; items = tl items){
		symbol := hd items;
		ordered = symbol :: ordered;
	}
	showchoices("Completions", "Insert", ordered);
}

choose(query: string)
{
	if(len choices == 0)
		return;
	selected := cmd(ed, query);
	if(selected == "" || selected[0] == '!')
		return;
	i := int selected;
	if(i < 0 || i >= len choices)
		return;
	chosen := choices[i];
	source := readsource();
	if(completing){
		if(source != choice_source || len tk->cmd(ed, ".b.t get 1.0 insert") != complete_cursor){
			hidecode();
			return;
		}
		word := chosen.name;
		insertcompletion(word);
		return;
	}
	if(source != choice_source){
		d := analyse(source);
		name := chosen.name;
		chosen = nil;
		for(fs := d.functions; fs != nil; fs = tl fs){
			symbol := hd fs;
			if(symbol.name == name){
				chosen = symbol;
				break;
			}
		}
		if(chosen == nil){
			showfunctions();
			return;
		}
	}
	pos := chosen.pos;
	movecaret(pos);
}

textindex(pos: int): string
{
	return "1.0+" + string pos + "c";
}

movecaret(pos: int)
{
	index := textindex(pos);
	cmd(ed, ".b.t tag remove sel 1.0 end");
	cmd(ed, ".b.t mark set insert " + index);
	cmd(ed, ".b.t see " + index);
	hidecode();
	re_indent();
}

insertcompletion(word: string)
{
	start := textindex(complete_start);
	stop := textindex(complete_stop);
	command := ".b.t delete " + start + " " + stop;
	cmd(ed, command);
	quoted := tk->quote(word);
	command = ".b.t insert " + start + " " + quoted;
	cmd(ed, command);
	pos := complete_start + len word;
	movecaret(pos);
	set_dirty();
}

do_indent()
{
	tab = dialog->getstring(context, ed.image, "single indent");
	for ( i := 1; i <= 8; i++ ) {
		s := "";
		for ( j := i; j > 0; j-- )
			s += tab;
		padding := collapse(s);
		tabs[i] = padding;
	}	
}

collapse(s: string): string
{
	prefix := "";
	while(len s >= 8 && s[0:8] == "        "){
		prefix += "\t";
		s = s[8:];
	}
	return prefix + s;
}

add_indent()
{
	amount := len indent;
	for(; amount >= 8; amount -= 8){
		padding := tabs[8];
		command := ".b.t insert insert '" + padding;
		cmd(ed, command);
	}
	padding := tabs[amount];
	command := ".b.t insert insert '" + padding;
	cmd(ed, command);
}
#
#	We should also look at the previous line, maybe.
#	And the line after.  That may be too much.
#
#	This is also the logical place to check if we are in a keyword,
#	reinitialize this_word (which presents problems if we are in the
#	middle of a word, etc.)  Also check if we are in a comment or not.  
#
re_indent()
{
	init := tk->cmd(ed, ".b.t get {insert linestart} insert");
	l := len init;
	for ( i := 8; i > 0; i-- ) {
		padding := tabs[i];
		lt := len padding;
		if ( l >= lt && init[:lt] == padding )
			break;
	}
	for ( indent = nil; len indent < i; indent = 0 :: indent) ;
	
	in_comment = 0;		# Are we in a comment?
	padding := tabs[i];
	for ( i = len padding; i < l; i++ )
		if ( init[i] == '#' ) {
			in_comment = 1;
			break;
		}
}

cmd(win: ref Tk->Toplevel, s: string): string
{
#	sys->print("%s\n", s);
	r := tk->cmd(win, s);
	if (r != nil && r[0] == '!') {
		sys->print("wm/edit: error executing '%s': %s\n", s, r);
	}
	return r;
}
