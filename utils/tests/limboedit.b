implement LimboeditTest;

include "sys.m";
include "limboedit.m";

LimboeditTest: module {
	init: fn();
};

init()
{
	sys := load Sys Sys->PATH;
	le := load Limboedit Limboedit->PATH;
	if(le == nil)
		raise "limboedit: cannot load module";

	s := "implement Demo;\n" +
		"Demo: module { proto: fn(); };\n" +
		"initial := attempt(foo(1));\n" +
		"# fake() { } raisex\n" +
		"Δ.Map[T](\n a: fn(a: int): int, b: T): ref Demo->Type\n" +
		"for { T => eq: fn(x, y: T): int; } raises (oops) {\n" +
		" return a(1); # another() {}\n}\n" +
		"later() raises nil {\n" +
		" stringliteral := \"quoted # in\\\"side\";\n" +
		" rune := '#';\n" +
		" raw := `raw #\nof fake() { }`;\n" +
		" x := 16rFF.8 + 1.5e-2 + .25;\n" +
		" repeated := 0; repeated++;\n" +
		" repeTAIL; ret; rai;\n" +
		"}\n" +
		"ongoing() {\n";
	doc := le->scan(s);
	fs := doc.functions;
	if(len fs != 3 || (hd fs).name != "Δ.Map" || (hd tl fs).name != "later" ||
			(hd tl tl fs).name != "ongoing")
		raise "limboedit: function discovery";
	i := (hd fs).pos;
	if(s[i:i+1] != "Δ")
		raise "limboedit: Unicode caret offset";

	literals := 0;
	comments := 0;
	numbers := 0;
	for(ts := doc.tokens; ts != nil; ts = tl ts){
		t := hd ts;
		v := s[t.start:t.stop];
		case t.kind {
		Limboedit->Literal =>
			literals++;
		Limboedit->Comment =>
			comments++;
			completion := le->complete(doc, t.stop);
			if(completion.words != nil || completion.start > completion.stop)
				raise "limboedit: suggestion in a comment";
		Limboedit->Number =>
			if(v == "16rFF.8" || v == "1.5e-2" || v == ".25")
				numbers++;
		}
	}
	if(literals != 3 || comments != 2 || numbers != 3)
		raise "limboedit: lexical colour spans";

	end := find(s, "repeTAIL;");
	start := end - len "repeTAIL;";
	completion := le->complete(doc, start+4);
	words := completion.words;
	if(completion.start != start || completion.stop != start+8 || len words != 1 || hd words != "repeated")
		raise "limboedit: completion replaces the entire identifier and deduplicates";
	completion = le->complete(doc, find(s, " ret"));
	words = completion.words;
	if(len words != 1 || hd words != "return")
		raise "limboedit: reserved word completion";
	completion = le->complete(doc, find(s, "; rai"));
	words = completion.words;
	if(len words != 2 || hd words != "raise" || hd tl words != "raises")
		raise "limboedit: ambiguous completion or comment leakage";
	completion = le->complete(doc, find(s, "quoted"));
	if(completion.words != nil)
		raise "limboedit: completion inside a string";

	sys->print("LIMBOEDIT_OK\n");
}

find(s, needle: string): int
{
	n := len needle;
	for(i := 0; i+n <= len s; i++)
		if(s[i:i+n] == needle)
			return i+n;
	raise "limboedit: missing test token";
}
