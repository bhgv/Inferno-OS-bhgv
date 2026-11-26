Limboedit: module {
	PATH: con "/dis/wm/limboedit.dis";

	Punct, Ident, Keyword, Comment, Literal, Number, Function: con iota;
	Token: adt {
		kind: int;
		start, stop: int;
	};
	Symbol: adt {
		name: string;
		pos: int;
	};
	Document: adt {
		source: string;
		tokens: list of ref Token;
		functions: list of ref Symbol;
	};

	keyword: fn(s: string): int;
	scan: fn(source: string): ref Document;
	complete: fn(doc: ref Document, cursor: int): (int, int, list of string);
};
