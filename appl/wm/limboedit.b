implement Limboedit;

include "limboedit.m";

# Reserved words from the Limbo compiler's lexer.
keywords := array[] of {
	"adt", "alt", "array", "big", "break", "byte", "case", "chan",
	"con", "continue", "cyclic", "do", "dynamic", "else", "exception",
	"exit", "fixed", "fn", "for", "hd", "if", "implement", "import",
	"include", "int", "len", "list", "load", "module", "nil", "of",
	"or", "pick", "raise", "raises", "real", "ref", "return", "self",
	"spawn", "string", "tagof", "tl", "to", "type", "while",
};

digit(c: int): int
{
	return c >= '0' && c <= '9';
}

letter(c: int): int
{
	return c == '_' || c > 16ra0 || c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z';
}

ident(c: int): int
{
	return letter(c) || digit(c);
}

keyword(s: string): int
{
	lo := 0;
	hi := len keywords;
	while(lo < hi){
		mid := (lo+hi)/2;
		if(s == keywords[mid])
			return 1;
		if(s < keywords[mid])
			hi = mid;
		else
			lo = mid+1;
	}
	return 0;
}

number(s: string, i: int): int
{
	n := len s;
	for(; i < n && digit(s[i]); i++)
		;
	if(i < n && (s[i] == 'r' || s[i] == 'R')){
		point := 0;
		for(i++; i < n; i++){
			c := s[i];
			if(c == '.' && !point)
				point = 1;
			else if(!digit(c) && !(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z'))
				break;
		}
		return i;
	}
	if(i < n && s[i] == '.')
		for(i++; i < n && digit(s[i]); i++)
			;
	if(i < n && (s[i] == 'e' || s[i] == 'E')){
		i++;
		if(i < n && (s[i] == '+' || s[i] == '-'))
			i++;
		for(; i < n && digit(s[i]); i++)
			;
	}
	return i;
}

lex(s: string): list of ref Token
{
	backward: list of ref Token;
	n := len s;
	for(i := 0; i < n; ){
		start := i;
		c := s[i++];
		kind := Punct;
		case c {
		' ' or '\t' or '\r' or '\n' or '\v' or '\f' =>
			continue;
		'#' =>
			kind = Comment;
			while(i < n && s[i] != '\n')
				i++;
		'"' or '\'' or '`' =>
			kind = Literal;
			while(i < n){
				d := s[i++];
				if(d == c)
					break;
				if(c != '`'){
					if(d == '\n'){
						i--;
						break;
					}
					if(d == '\\' && i < n && s[i] != '\n')
						i++;
				}
			}
		'0' to '9' =>
			kind = Number;
			i = number(s, start);
		'.' =>
			if(i < n && digit(s[i])){
				kind = Number;
				i = number(s, start);
			}
		* =>
			if(letter(c)){
				while(i < n && ident(s[i]))
					i++;
				kind = Ident;
				if(keyword(s[start:i]))
					kind = Keyword;
			}
		}
		backward = ref Token(kind, start, i) :: backward;
	}
	tokens: list of ref Token;
	for(; backward != nil; backward = tl backward)
		tokens = hd backward :: tokens;
	return tokens;
}

aftercomments(ts: list of ref Token): list of ref Token
{
	while(ts != nil && (hd ts).kind == Comment)
		ts = tl ts;
	return ts;
}

isat(s: string, ts: list of ref Token, word: string): int
{
	if(ts == nil)
		return 0;
	t := hd ts;
	return t.kind != Comment && t.kind != Literal && s[t.start:t.stop] == word;
}

# Return the first significant token after a balanced group.
aftergroup(s: string, ts: list of ref Token, open, close: string): list of ref Token
{
	depth := 0;
	for(; ts != nil; ts = tl ts){
		if(isat(s, ts, open))
			depth++;
		else if(isat(s, ts, close) && --depth == 0)
			return aftercomments(tl ts);
	}
	return nil;
}

# Recognize a definition header, including qualified and polymorphic names.
definition(s: string, ts: list of ref Token): (list of ref Token, list of ref Token)
{
	names: list of ref Token;
	for(;;){
		if(ts == nil || (hd ts).kind != Ident)
			return (nil, nil);
		names = hd ts :: names;
		ts = aftercomments(tl ts);
		if(isat(s, ts, "["))
			ts = aftergroup(s, ts, "[", "]");
		if(!isat(s, ts, "."))
			break;
		ts = aftercomments(tl ts);
	}
	if(!isat(s, ts, "("))
		return (nil, nil);
	ts = aftergroup(s, ts, "(", ")");
	if(isat(s, ts, ":")){
		for(ts = aftercomments(tl ts); ts != nil; ){
			if(isat(s, ts, "{") || isat(s, ts, "for") || isat(s, ts, "raises"))
				break;
			if(isat(s, ts, ";") || isat(s, ts, "=") || isat(s, ts, "}"))
				return (nil, nil);
			if(isat(s, ts, "("))
				ts = aftergroup(s, ts, "(", ")");
			else if(isat(s, ts, "["))
				ts = aftergroup(s, ts, "[", "]");
			else
				ts = aftercomments(tl ts);
		}
	}
	if(isat(s, ts, "for")){
		ts = aftercomments(tl ts);
		if(!isat(s, ts, "{"))
			return (nil, nil);
		ts = aftergroup(s, ts, "{", "}");
	}
	if(isat(s, ts, "raises")){
		ts = aftercomments(tl ts);
		if(isat(s, ts, "("))
			ts = aftergroup(s, ts, "(", ")");
		else if(ts != nil && ((hd ts).kind == Ident || isat(s, ts, "nil")))
			ts = aftercomments(tl ts);
		else
			return (nil, nil);
	}
	if(isat(s, ts, "{"))
		return (names, ts);
	return (nil, nil);
}

functions(s: string, ts: list of ref Token): list of ref Symbol
{
	found: list of ref Symbol;
	statement := 1;
	for(ts = aftercomments(ts); ts != nil; ){
		if(statement && (hd ts).kind == Ident){
			(names, body) := definition(s, ts);
			if(names != nil){
				name := "";
				for(; names != nil; names = tl names){
					t := hd names;
					if(name != "")
						name = "." + name;
					name = s[t.start:t.stop] + name;
					t.kind = Function;
				}
				found = ref Symbol(name, (hd ts).start) :: found;
				ts = aftergroup(s, body, "{", "}");
				continue;
			}
		}
		statement = isat(s, ts, ";");
		if(isat(s, ts, "{"))
			ts = aftergroup(s, ts, "{", "}");
		else if(isat(s, ts, "("))
			ts = aftergroup(s, ts, "(", ")");
		else if(isat(s, ts, "["))
			ts = aftergroup(s, ts, "[", "]");
		else
			ts = aftercomments(tl ts);
	}
	result: list of ref Symbol;
	for(; found != nil; found = tl found)
		result = hd found :: result;
	return result;
}

scan(source: string): ref Document
{
	tokens := lex(source);
	return ref Document(source, tokens, functions(source, tokens));
}

match(word, prefix: string): int
{
	return len word > len prefix && word[:len prefix] == prefix;
}

# Deduplicate while preserving the first occurrence.
candidate(words: list of string, word: string): list of string
{
	for(p := words; p != nil; p = tl p)
		if(hd p == word)
			return words;
	return word :: words;
}

complete(doc: ref Document, cursor: int): (int, int, list of string)
{
	s := doc.source;
	if(cursor <= 0 || cursor > len s)
		return (cursor, cursor, nil);
	start := cursor;
	while(start > 0 && ident(s[start-1]))
		start--;
	if(start == cursor || !letter(s[start]))
		return (cursor, cursor, nil);
	stop := cursor;
	while(stop < len s && ident(s[stop]))
		stop++;
	prefix := s[start:cursor];
	words: list of string;
	for(ts := doc.tokens; ts != nil; ts = tl ts){
		t := hd ts;
		if(t.start <= start && t.stop >= cursor &&
				t.kind != Ident && t.kind != Function && t.kind != Keyword)
			return (start, stop, nil);
		if(t.start == start || t.kind != Ident && t.kind != Function)
			continue;
		word := s[t.start:t.stop];
		if(match(word, prefix))
			words = candidate(words, word);
	}
	for(i := 0; i < len keywords; i++)
		if(match(keywords[i], prefix))
			words = candidate(words, keywords[i]);
	result: list of string;
	for(; words != nil; words = tl words)
		result = hd words :: result;
	return (start, stop, result);
}
