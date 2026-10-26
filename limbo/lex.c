#define Extern
#include "limbo.h"
#include "y.tab.h"

enum
{
	Leof		= -1,
	Linestart	= 0,

	Mlower		= 1,
	Mupper		= 2,
	Munder		= 4,
	Malpha		= Mupper|Mlower|Munder,
	Mdigit		= 8,
	Msign		= 16,
	Mexp		= 32,
	Mhex		= 64,
	Mradix		= 128,

	HashSize	= 1024,
	MaxPath		= 4096
};

typedef	struct Keywd	Keywd;
struct	Keywd
{
	char	*name;
	int	token;
};

	File	**files;			/* files making up the module, sorted by absolute line */
	int	nfiles;
static	int	lenfiles;
static	int	lastfile;			/* index of last file looked up */

static	char	*incpath[MaxIncPath];
static	Sym	*symbols[HashSize];
static	Sym	*strings[HashSize];
static	char	map[256];
static	Biobuf	*bin;
static	Line	linestack[MaxInclude];
static	int	lineno;
static	int	linepos;
static	int	bstack;
static	int	ineof;
static	int	lasttok;
static	YYSTYPE	lastyylval;
static	char	srcdir[MaxPath];

static	Keywd	keywords[] =
{
	"adt",		Ladt,
	"alt",		Lalt,
	"array",	Larray,
	"big",		Ltid,
	"break",	Lbreak,
	"byte",		Ltid,
	"case",		Lcase,
	"chan",		Lchan,
	"con",		Lcon,
	"continue",	Lcont,
	"cyclic",	Lcyclic,
	"do",		Ldo,
	"dynamic",	Ldynamic,
	"else",		Lelse,
	"exception",	Lexcept,
	"exit",		Lexit,
	"fixed",	Lfix,
	"fn",		Lfn,
	"for",		Lfor,
	"hd",		Lhd,
	"if",		Lif,
	"implement",	Limplement,
	"import",	Limport,
	"include",	Linclude,
	"int",		Ltid,
	"len",		Llen,
	"list",		Llist,
	"load",		Lload,
	"module",	Lmodule,
	"nil",		Lnil,
	"of",		Lof,
	"or",		Lor,
	"pick",		Lpick,
	"raise",	Lraise,
	"raises",	Lraises,
	"real",		Ltid,
	"ref",		Lref,
	"return",	Lreturn,
	"self",		Lself,
	"spawn",	Lspawn,
	"string",	Ltid,
	"tagof",	Ltagof,
	"tl",		Ltl,
	"to",		Lto,
	"type",		Ltype,
	"while",	Lwhile,
	0,
};

static	Keywd	tokwords[] =
{
	"&=",	Landeq,
	"|=",	Loreq,
	"^=",	Lxoreq,
	"<<=",	Llsheq,
	">>=",	Lrsheq,
	"+=",	Laddeq,
	"-=",	Lsubeq,
	"*=",	Lmuleq,
	"/=",	Ldiveq,
	"%=",	Lmodeq,
	"**=", Lexpeq,
	":=",	Ldeclas,
	"||",	Loror,
	"&&",	Landand,
	"::",	Lcons,
	"==",	Leq,
	"!=",	Lneq,
	"<=",	Lleq,
	">=",	Lgeq,
	"<<",	Llsh,
	">>",	Lrsh,
	"<-",	Lcomm,
	"++", Linc,
	"--",	Ldec,
	"->", Lmdot,
	"=>", Llabs,
	"**", Lexp,
	"EOF",	Leof,
	"eof",	Beof,
	0,
};

void
lexinit(void)
{
	Keywd *k;
	int i;

	for(i = 0; i < 256; i++){
		if(i == '_' || i > 0xa0)
			map[i] |= Munder;
		if(i >= 'A' && i <= 'Z')
			map[i] |= Mupper;
		if(i >= 'a' && i <= 'z')
			map[i] |= Mlower;
		if(i >= 'A' && i <= 'F' || i >= 'a' && i <= 'f')
			map[i] |= Mhex;
		if(i == 'e' || i == 'E')
			map[i] |= Mexp;
		if(i == 'r' || i == 'R')
			map[i] |= Mradix;
		if(i == '-' || i == '+')
			map[i] |= Msign;
		if(i >= '0' && i <= '9')
			map[i] |= Mdigit;
	}

	memset(escmap, -1, sizeof(escmap));
	escmap['\''] = '\'';
	unescmap['\''] = '\'';
	escmap['"'] = '"';
	unescmap['"'] = '"';
	escmap['\\'] = '\\';
	unescmap['\\'] = '\\';
	escmap['a'] = '\a';
	unescmap['\a'] = 'a';
	escmap['b'] = '\b';
	unescmap['\b'] = 'b';
	escmap['f'] = '\f';
	unescmap['\f'] = 'f';
	escmap['n'] = '\n';
	unescmap['\n'] = 'n';
	escmap['r'] = '\r';
	unescmap['\r'] = 'r';
	escmap['t'] = '\t';
	unescmap['\t'] = 't';
	escmap['v'] = '\v';
	unescmap['\v'] = 'v';
	escmap['0'] = '\0';
	unescmap['\0'] = '0';

	for(k = keywords; k->name != nil; k++)
		enter(k->name, k->token);
}

int
cmap(int c)
{
	if(c<0)
		return 0;
	if(c<256)
		return map[c];
	return Mlower;
}

void
lexstart(char *in)
{
	char *p;

	ineof = 0;
	bstack = 0;
	nfiles = 0;
	lastfile = 0;
	addfile(mkfile(strdup(in), 1, 0, -1, nil, 0, -1));
	bin = bins[bstack];
	lineno = 1;
	linepos = Linestart;

	secpy(srcdir, srcdir+MaxPath, in);
	p = strrchr(srcdir, '/');
	if(p == nil)
		srcdir[0] = '\0';
	else
		p[1] = '\0';
}

static int
Getc(void)
{
	int c;

	if(ineof)
		return Beof;
	c = BGETC(bin);
	if(c == Beof)
		ineof = 1;
	linepos++;
	return c;
}

static void
unGetc(void)
{
	if(ineof)
		return;
	Bungetc(bin);
	linepos--;
}

static int
getrune(void)
{
	int c;

	if(ineof)
		return Beof;
	c = Bgetrune(bin);
	if(c == Beof)
		ineof = 1;
	linepos++;
	return c;
}

static void
ungetrune(void)
{
	if(ineof)
		return;
	Bungetrune(bin);
	linepos--;
}

void
addinclude(char *s)
{
	int i;

	for(i = 0; i < MaxIncPath; i++){
		if(incpath[i] == 0){
			incpath[i] = s;
			return;
		}
	}
	fatal("out of include path space");
}

File*
mkfile(char *name, int abs, int off, int in, char *act, int actoff, int sbl)
{
	File *f;

	f = allocmem(sizeof *f);
	f->name = name;
	f->abs = abs;
	f->off = off;
	f->in = in;
	f->act = act;
	f->actoff = actoff;
	f->sbl = sbl;
	return f;
}


/* Grow source history geometrically without overflowing int indices or byte sizes. */
int
addfile(File *f)
{
	int capacity;
	ulong limit;

	limit = (~(ulong)0)/sizeof(File*);
	if(limit > 0x7fffffffUL)
		limit = 0x7fffffffUL;
	if(nfiles < 0 || lenfiles < 0 || (ulong)nfiles >= limit)
		fatal("too many source files");
	if(nfiles >= lenfiles){
		if(nfiles < 32 && limit >= 32)
			capacity = 32;
		else if((ulong)nfiles <= limit/2)
			capacity = nfiles*2;
		else
			capacity = limit;
		files = reallocmem(files, (ulong)capacity*sizeof(File*));
		lenfiles = capacity;
	}
	files[nfiles] = f;
	return nfiles++;
}

/* Refuse truncated include paths before asking the filesystem to open them. */
static Biobuf*
openinclude(char *buf, char *dir, char *name, int slash)
{
	ulong ndir, nname;

	ndir = strlen(dir);
	nname = strlen(name);
	if(ndir >= MaxPath || nname >= MaxPath-ndir-slash){
		werrstr("include path too long");
		return nil;
	}
	seprint(buf, buf+MaxPath, "%s%s%s", dir, slash ? "/" : "", name);
	return Bopen(buf, OREAD);
}

/* Include exactly the requested name, rejecting embedded NULs and empty paths. */
void
includef(Sym *file)
{
	Biobuf *b;
	char *p, buf[MaxPath];
	int i;

	if(file->len <= 0 || file->len >= MaxPath ||
	   memchr(file->name, 0, file->len) != nil){
		yyerror("invalid include file name");
		return;
	}
	linestack[bstack].line = lineno;
	linestack[bstack].pos = linepos;
	bstack++;
	if(bstack >= MaxInclude)
		fatal("%L: include file depth too great", curline());
	p = "";
	if(file->name[0] != '/')
		p = srcdir;
	b = openinclude(buf, p, file->name, 0);
	for(i = 0; b == nil && i < MaxIncPath && incpath[i] != nil && file->name[0] != '/'; i++){
		b = openinclude(buf, incpath[i], file->name, 1);
	}
	bins[bstack] = b;
	if(bins[bstack] == nil){
		yyerror("can't include %s: %r", file->name);
		bstack--;
	}else{
		addfile(mkfile(strdup(buf), lineno+1, -lineno, lineno, nil, 0, -1));
		lineno++;
		linepos = Linestart;
	}
	bin = bins[bstack];
}

/*
 * we hit eof in the current file
 * revert to the file which included it.
 */
static void
popinclude(void)
{
	Fline fl;
	File *f;
	int oline, opos, ln;

	ineof = 0;
	bstack--;
	bin = bins[bstack];
	oline = linestack[bstack].line;
	opos = linestack[bstack].pos;
	fl = fline(oline);
	f =  fl.file;
	ln = fl.line;
	lineno++;
	linepos = opos;
	addfile(mkfile(f->name, lineno, ln-lineno, f->in, f->act, f->actoff, -1));
}

/* Clamp diagnostic locations after wide arithmetic rather than overflowing int. */
static int
sourceline(Long line)
{
	if(line > 0x7fffffff)
		return 0x7fffffff;
	if(line < (-0x7fffffff-1))
		return -0x7fffffff-1;
	return line;
}


/* Find source coordinates without overflowing the binary-search midpoint. */
Fline
fline(int absline)
{
	Fline fl;
	int l, r, m, s;

	if(absline < files[lastfile]->abs
	|| lastfile+1 < nfiles && absline >= files[lastfile+1]->abs){
		lastfile = 0;
		l = 0;
		r = nfiles - 1;
		while(l <= r){
			m = l + (r - l) / 2;
			s = files[m]->abs;
			if(s <= absline){
				l = m + 1;
				lastfile = m;
			}else
				r = m - 1;
		}
	}

	fl.file = files[lastfile];
	fl.line = sourceline((Long)absline + files[lastfile]->off);
	return fl;
}

/* Consume comments completely and reject truncated or overflowing line directives. */
static int
lexcom(void)
{
	File *f;
	char buf[StrSize], *s, *t, *act;
	int i, n, c, actline, overflow;

	i = 0;
	overflow = 0;
	while((c = Getc()) != '\n'){
		if(c == Beof)
			return -1;
		if(i < sizeof(buf)-1)
			buf[i++] = c;
		else
			overflow = 1;
	}
	buf[i] = 0;

	lineno++;
	linepos = Linestart;

	if(strncmp(buf, "line ", 5) != 0 && strncmp(buf, "line\t", 5) != 0)
		return 0;
	if(overflow){
		yyerror("line directive too long");
		return 0;
	}
	for(s = buf+5; *s == ' ' || *s == '\t'; s++)
		;
	if(!(cmap(*s) & Mdigit))
		return 0;
	n = 0;
	for(; cmap(c = *s) & Mdigit; s++){
		if(n > (0x7fffffff-(c-'0'))/10){
			yyerror("line number out of range");
			return 0;
		}
		n = n * 10 + (c - '0');
	}
	for(; *s == ' ' || *s == '\t'; s++)
		;
	if(*s != '"')
		return 0;
	s++;
	t = strchr(s, '"');
	if(t == nil || t[1] != '\0')
		return 0;
	*t = '\0';

	f = files[nfiles - 1];
	if((Long)n == (Long)f->off+lineno && strcmp(s, f->name) == 0)
		return 1;
	act = f->name;
	actline = sourceline((Long)lineno + f->off);
	if(f->act != nil){
		actline = sourceline((Long)lineno + f->off + f->actoff);
		act = f->act;
	}
	addfile(mkfile(strdup(s), lineno, n-lineno, f->in, act, sourceline((Long)actline - n), -1));

	return 1;
}

Line
curline(void)
{
	Line line;

	line.line = lineno;
	line.pos = linepos;
	return line;
}

int
lineconv(Fmt *f)
{
	Fline fl;
	File *file;
	Line inl, line;
	char buf[StrSize], *s;

	line = va_arg(f->args, Line);

	if(line.line < 0)
		return fmtstrcpy(f, "<noline>");
	fl = fline(line.line);
	file = fl.file;

	s = seprint(buf, buf+sizeof(buf), "%s:%d", file->name, fl.line);
	if(file->act != nil)
		s = seprint(s, buf+sizeof(buf), " [ %s:%d ]", file->act,
			sourceline((Long)file->actoff+line.line+file->off));
	if(file->in >= 0){
		inl.line = file->in;
		inl.pos = 0;
		seprint(s, buf+sizeof(buf), ": %L", inl);
	}
	return fmtstrcpy(f, buf);
}

static char*
posconv(char *s, char *e, Line line)
{
	Fline fl;

	if(line.line < 0)
		return secpy(s, e, "nopos");

	fl = fline(line.line);
	return seprint(s, e, "%s:%d.%d", fl.file->name, fl.line, line.pos);
}

int
srcconv(Fmt *f)
{
	Src src;
	char buf[StrSize], *s;

	src = va_arg(f->args, Src);
	s = posconv(buf, buf+sizeof(buf), src.start);
	s = secpy(s, buf+sizeof(buf), ",");
	posconv(s, buf+sizeof(buf), src.stop);

	return fmtstrcpy(f, buf);
}

/* Consume complete identifiers and diagnose truncation before symbols can alias. */
int
lexid(int c)
{
	Sym *sym;
	char id[StrSize*UTFmax+1], *p;
	Rune r;
	int i, t, overflow;

	p = id;
	i = 0;
	overflow = 0;
	for(;;){
		if(i < StrSize){
			if(c < Runeself)
				*p++ = c;
			else{
				r = c;
				p += runetochar(p, &r);
			}
			i++;
		}else
			overflow = 1;
		c = getrune();
		if(c == Beof
		|| !(cmap(c) & (Malpha|Mdigit))){
			ungetrune();
			break;
		}
	}
	*p = '\0';
	if(overflow)
		yyerror("identifier too long");
	sym = enter(id, Lid);
	t = sym->token;
	if(t == Lid || t == Ltid)
		yylval.tok.v.idval = sym;
	return t;
}

/* Parse integer bit patterns with defined 64-bit modular accumulation. */
Long
strtoi(char *t, int base)
{
	char *s;
	uvlong v;
	int c, neg, ck;

	neg = 0;
	if(t[0] == '-'){
		neg = 1;
		t++;
	}else if(t[0] == '+')
		t++;
	v = 0;
	for(s = t; *s; s++){
		/* Numeric tokens are bytes; do not sign-extend UTF-8 bytes on signed-char hosts. */
		c = (uchar)*s;
		ck = cmap(c);
		if(ck & Mdigit)
			c -= '0';
		else if(ck & Mlower)
			c = c - 'a' + 10;
		else if(ck & Mupper)
			c = c - 'A' + 10;
		if(c >= base){
			yyerror("digit '%c' not radix %d", *s, base);
			return -1;
		}
		v = v * base + c;
	}
	if(neg)
		return (uvlong)0-v;
	return v;
}

static int
digit(int c, int base)
{
	int cc, ck;

	/* Treat source bytes as unsigned so malformed UTF-8 cannot become a digit. */
	cc = (uchar)c;
	c = cc;
	ck = cmap(c);
	if(ck & Mdigit)
		c -= '0';
	else if(ck & Mlower)
		c = c - 'a' + 10;
	else if(ck & Mupper)
		c = c - 'A' + 10;
	else if(ck & Munder)
		{}
	else
		return -1;
	if(c >= base){
		yyerror("digit '%c' not radix %d", cc, base);
		return -1;
	}
	return c;
}

/*
 * Parse a radix fraction without building an overflowing integer numerator
 * and denominator. Backward Horner evaluation retains small fractional values.
 * Letters, including e/E, are digits in this radix syntax, not exponents.
 */
double
strtodb(char *text, int base)
{
	double number, fraction;
	char *start, *end;
	int negative, c, d;

	negative = 0;
	c = *text++;
	if(c == '-' || c == '+'){
		negative = c == '-';
		c = *text++;
	}
	number = 0;
	while((d = digit(c, base)) >= 0){
		number = number*base+d;
		c = *text++;
	}
	if(c == '.'){
		start = text;
		while(digit(*text, base) >= 0)
			text++;
		end = text;
		fraction = 0;
		while(end > start)
			fraction = (fraction+digit(*--end, base))/base;
		number += fraction;
	}
	if(negative)
		return -number;
	return number;
}

/*
 * Parse a numeric identifier, retaining the final token at EOF.
 * Consume overlong tokens completely and reject them before decoding buf.
 * format [0-9]+(r[0-9A-Za-z]+)?
 * or ([0-9]+(\.[0-9]*)?|\.[0-9]+)([eE][+-]?[0-9]+)?
 */
int
lexnum(int c)
{
	char buf[StrSize], *base;
	enum { Int, Radix, RadixSeen, Frac, ExpSeen, ExpSignSeen, Exp, FracB } state;
	double d;
	Long v;
	int i, ck, overflow;

	i = 0;
	overflow = 0;
	buf[i++]  = c;
	state = Int;
	if(c == '.')
		state = Frac;
	base = nil;
	for(;;){
		c = Getc();
		if(c == Beof)
			goto done;

		ck = cmap(c);
		switch(state){
		case Int:
			if(ck & Mdigit)
				break;
			if(ck & Mexp){
				state = ExpSeen;
				break;
			}
			if(ck & Mradix){
				base = &buf[i];
				state = RadixSeen;
				break;
			}
			if(c == '.'){
				state = Frac;
				break;
			}
			goto done;
		case RadixSeen:
		case Radix:
			if(ck & (Mdigit|Mlower|Mupper)){
				state = Radix;
				break;
			}
			if(c == '.'){
				state = FracB;
				break;
			}
			goto done;
		case Frac:
			if(ck & Mdigit)
				break;
			if(ck & Mexp)
				state = ExpSeen;
			else
				goto done;
			break;
		case FracB:
			if(ck & (Mdigit|Mlower|Mupper))
				break;
			goto done;
		case ExpSeen:
			if(ck & Msign){
				state = ExpSignSeen;
				break;
			}
			/* fall through */
		case ExpSignSeen:
		case Exp:
			if(ck & Mdigit){
				state = Exp;
				break;
			}
			goto done;
		}
		if(i < StrSize-1)
			buf[i++] = c;
		else
			overflow = 1;
	}
done:
	buf[i] = 0;
	unGetc();
	if(overflow){
		yyerror("numeric constant too long");
		yylval.tok.v.ival = 0;
		return Lconst;
	}
	switch(state){
	default:
		yyerror("malformed numerical constant '%s'", buf);
		yylval.tok.v.ival = 0;
		return Lconst;
	case Radix:
		*base++ = '\0';
		v = strtoi(buf, 10);
		if(v < 0)
			break;
		if(v < 2 || v > 36){
			yyerror("radix '%s' must be between 2 and 36", buf);
			break;
		}
		v = strtoi(base, v);
		break;
	case Int:
		v = strtoi(buf, 10);
		break;
	case Frac:
	case Exp:
		d = strtod(buf, nil);
		yylval.tok.v.rval = d;
		return Lrconst;
	case FracB:
		*base++ = '\0';
		v = strtoi(buf, 10);
		if(v < 0)
			break;
		if(v < 2 || v > 36){
			yyerror("radix '%s' must be between 2 and 36", buf);
			break;
		}
		d = strtodb(base, v);
		yylval.tok.v.rval = d;
		return Lrconst;
	}
	yylval.tok.v.ival = v;
	return Lconst;
}

int
escchar(void)
{
	char buf[4+1];
	int c, i;

	c = getrune();
	if(c == Beof)
		return Beof;
	if(c == 'u'){
		for(i = 0; i < 4; i++){
			c = getrune();
			if(c == Beof || !(cmap(c) & (Mdigit|Mhex))){
				yyerror("malformed \\u escape sequence");
				ungetrune();
				break;
			}
			buf[i] = c;
		}
		buf[i] = 0;
		return strtoul(buf, 0, 16);
	}
	if(c < 256 && (i = escmap[c]) >= 0)
		return i;
	yyerror("unrecognized escape \\%C", c);
	return c;
}

/* Build a terminated UTF-8 byte buffer with checked, geometric growth. */
void
lexstring(int israw)
{
	char *str;
	int c, t, startlno;
	Rune r;
	int len, alloc;

	alloc = 32;
	len = 0;
	str = allocmem(alloc);
	str[0] = '\0';
	startlno = lineno;
	for(;;){
		c = getrune();
		if(israw){
			switch(c){
			case '`':
				yylval.tok.v.idval = enterstring(str, len);
				return;
			case '\n':
				lineno++;
				linepos = Linestart;
				break;
			case Beof:
				t = lineno;
				lineno = startlno;
				yyerror("end of file in raw string constant");
				lineno = t;
				yylval.tok.v.idval = enterstring(str, len);
				return;
			}
		}else{
			switch(c){
			case '\\':
				c = escchar();
				if(c != Beof)
					break;
				/* fall through */
			case Beof:
				yyerror("end of file in string constant");
				yylval.tok.v.idval = enterstring(str, len);
				return;
			case '\n':
				yyerror("newline in string constant");
				lineno++;
				linepos = Linestart;
				yylval.tok.v.idval = enterstring(str, len);
				return;
			case '"':
				yylval.tok.v.idval = enterstring(str, len);
				return;
			}
		}
		if(len > 0x7fffffff-UTFmax-1)
			fatal("string constant too long");
		if(len+UTFmax+1 >= alloc){
			if(alloc > 0x7fffffff/2)
				alloc = 0x7fffffff;
			else
				alloc *= 2;
			str = reallocmem(str, alloc);
		}
		r = c;
		len += runetochar(&str[len], &r);
		str[len] = '\0';
	}
}

static int
lex(void)
{
	int c;

loop:
	yylval.tok.src.start.line = lineno;
	yylval.tok.src.start.pos = linepos;
	c = getrune(); /* ehg: outside switch() to avoid bug in VisualC++5.0 */
	switch(c){
	case Beof:
		Bterm(bin);
		if(bstack == 0)
			return Leof;
		popinclude();
		break;
	case '#':
		if(lexcom() < 0){
			Bterm(bin);
			if(bstack == 0)
				return Leof;
			popinclude();
		}
		break;

	case '\n':
		lineno++;
		linepos = Linestart;
		goto loop;
	case ' ':
	case '\t':
	case '\r':
	case '\v':
	case '\f':
		goto loop;
	case '"':
		lexstring(0);
		return Lsconst;
	case '`':
		lexstring(1);
		return Lsconst;
	case '\'':
		c = getrune();
		if(c == '\\')
			c = escchar();
		if(c == Beof){
			yyerror("end of file in character constant");
			return Beof;
		}else
			yylval.tok.v.ival = c;
		c = Getc();
		if(c != '\'') {
			yyerror("missing closing '");
			unGetc();
		}
		return Lconst;
	case '(':
	case ')':
	case '[':
	case ']':
	case '{':
	case '}':
	case ',':
	case ';':
	case '~':
		return c;

	case ':':
		c = Getc();
		if(c == ':')
			return Lcons;
		if(c == '=')
			return Ldeclas;
		unGetc();
		return ':';

	case '.':
		c = Getc();
		unGetc();
		if(c != Beof && (cmap(c) & Mdigit))
			return lexnum('.');
		return '.';

	case '|':
		c = Getc();
		if(c == '=')
			return Loreq;
		if(c == '|')
			return Loror;
		unGetc();
		return '|';

	case '&':
		c = Getc();
		if(c == '=')
			return Landeq;
		if(c == '&')
			return Landand;
		unGetc();
		return '&';

	case '^':
		c = Getc();
		if(c == '=')
			return Lxoreq;
		unGetc();
		return '^';

	case '*':
		c = Getc();
		if(c == '=')
			return Lmuleq;
		if(c == '*'){
			c = Getc();
			if(c == '=')
				return Lexpeq;
			unGetc();
			return Lexp;
		}
		unGetc();
		return '*';
	case '/':
		c = Getc();
		if(c == '=')
			return Ldiveq;
		unGetc();
		return '/';
	case '%':
		c = Getc();
		if(c == '=')
			return Lmodeq;
		unGetc();
		return '%';
	case '=':
		c = Getc();
		if(c == '=')
			return Leq;
		if(c == '>')
			return Llabs;
		unGetc();
		return '=';
	case '!':
		c = Getc();
		if(c == '=')
			return Lneq;
		unGetc();
		return '!';
	case '>':
		c = Getc();
		if(c == '=')
			return Lgeq;
		if(c == '>'){
			c = Getc();
			if(c == '=')
				return Lrsheq;
			unGetc();
			return Lrsh;
		}
		unGetc();
		return '>';

	case '<':
		c = Getc();
		if(c == '=')
			return Lleq;
		if(c == '-')
			return Lcomm;
		if(c == '<'){
			c = Getc();
			if(c == '=')
				return Llsheq;
			unGetc();
			return Llsh;
		}
		unGetc();
		return '<';

	case '+':
		c = Getc();
		if(c == '=')
			return Laddeq;
		if(c == '+')
			return Linc;
		unGetc();
		return '+';

	case '-':
		c = Getc();
		if(c == '=')
			return Lsubeq;
		if(c == '-')
			return Ldec;
		if(c == '>')
			return Lmdot;
		unGetc();
		return '-';

	case '1': case '2': case '3': case '4': case '5':
	case '0': case '6': case '7': case '8': case '9':
		return lexnum(c);

	default:
		if(cmap(c) & Malpha)
			return lexid(c);
		yyerror("unknown character %c", c);
		break;
	}
	goto loop;
}

int
yylex(void)
{
	int t;

	t = lex();
	yylval.tok.src.stop.line = lineno;
	yylval.tok.src.stop.pos = linepos;
	lasttok = t;
	lastyylval = yylval;
	return t;
}

static char*
toksp(int t)
{
	Keywd *k;
	static char buf[256];

	switch(t){
		case Lconst:
			snprint(buf, sizeof(buf), "%lld", lastyylval.tok.v.ival);
			return buf;
		case Lrconst:
			snprint(buf, sizeof(buf), "%f", lastyylval.tok.v.rval);
			return buf;
		case Lsconst:
			snprint(buf, sizeof(buf), "\"%s\"", lastyylval.tok.v.idval->name);
			return buf;
		case Ltid:
		case Lid:
			return lastyylval.tok.v.idval->name;
	}
	for(k = keywords; k->name != nil; k++)
		if(t == k->token)
			return k->name;
	for(k = tokwords; k->name != nil; k++)
		if(t == k->token)
			return k->name;
	if(t < 0 || t > 255)
		fatal("bad token %d in toksp()", t);
	buf[0] = t;
	buf[1] = '\0';
	return buf;
}

/* Intern owned bytes, releasing empty and duplicate buffers without reading them. */
Sym*
enterstring(char *str, int n)
{
	Sym *s;
	char *p, *e;
	ulong h;
	int c, c0;

	if(n == 0){
		free(str);
		return enter("", 0);
	}
	e = str + n;
	h = 0;
	for(p = str; p < e; p++){
		c = *(uchar*)p;
		c ^= c << 6;
		h += (c << 11) ^ (c >> 1);
		c = *(uchar*)p;
		h ^= (c << 14) + (c << 7) + (c << 4) + c;
	}

	c0 = str[0];
	h %= HashSize;
	for(s = strings[h]; s != nil; s = s->next){
		if(s->name[0] == c0 && s->len == n && memcmp(s->name, str, n) == 0){
			free(str);
			return s;
		}
	}

	s = allocmem(sizeof(Sym));
	memset(s, 0, sizeof(Sym));
	s->name = str;
	s->len = n;
	s->next = strings[h];
	strings[h] = s;
	return s;
}

int
symcmp(Sym *s, Sym *t)
{
	int n, c;

	n = s->len;
	if(n > t->len)
		n = t->len;
	c = memcmp(s->name, t->name, n);
	if(c == 0)
		return s->len - t->len;
	return c;
}

/* Bound concatenated byte lengths before adding or allocating the terminator. */
Sym*
stringcat(Sym *s, Sym *t)
{
	char *str;
	int n;

	if(s->len < 0 || t->len < 0 || s->len > 0x7ffffffe - t->len)
		fatal("string constant too long");
	n = s->len + t->len;
	str = allocmem(n+1);
	memmove(str, s->name, s->len);
	memmove(str+s->len, t->name, t->len);
	str[n] = '\0';
	return enterstring(str, n);
}

/* Hash identifier bytes as unsigned values on every host architecture. */
Sym*
enter(char *name, int token)
{
	Sym *s;
	char *p;
	ulong h;
	int c0, c, n;

	c0 = name[0];
	h = 0;
	for(p = name; c = *(uchar*)p; p++){
		c ^= c << 6;
		h += (c << 11) ^ (c >> 1);
		c = *(uchar*)p;
		h ^= (c << 14) + (c << 7) + (c << 4) + c;
	}
	n = p - name;

	h %= HashSize;
	for(s = symbols[h]; s != nil; s = s->next)
		if(s->name[0] == c0 && strcmp(s->name, name) == 0)
			return s;

	s = allocmem(sizeof(Sym));
	memset(s, 0, sizeof(Sym));
	s->hash = h;
	s->name = allocmem(n+1);
	memmove(s->name, name, n+1);
	if(token == 0)
		token = Lid;
	s->token = token;
	s->next = symbols[h];
	s->len = n;
	symbols[h] = s;
	return s;
}

char*
stringpr(char *buf, char *end, Sym *sym)
{
	char sb[30], *s, *p;
	int i, c, n;

	s = sym->name;
	n = sym->len;
	if(n > 10)
		n = 10;
	p = sb;
	*p++ = '"';
	for(i = 0; i < n; i++){
		c = s[i];
		switch(c){
		case '\\':
		case '"':
		case '\n':
		case '\r':
		case '\t':
		case '\b':
		case '\a':
		case '\v':
		case '\0':
			*p++ = '\\';
			*p++ = unescmap[c];
			break;
		default:
			*p++ = c;
			break;
		}
	}
	if(n != sym->len){
		*p++ = '.';
		*p++ = '.';
		*p++ = '.';
	}
	*p++ = '"';
	*p = 0;
	return secpy(buf, end, sb);
}

void
warn(Line line, char *fmt, ...)
{
	char buf[4096];
	va_list arg;

	if(errors || !dowarn)
		return;
	va_start(arg, fmt);
	vseprint(buf, buf+sizeof(buf), fmt, arg);
	va_end(arg);
	fprint(2, "%L: warning: %s\n", line, buf);
}

void
nwarn(Node *n, char *fmt, ...)
{
	char buf[4096];
	va_list arg;

	if(errors || !dowarn)
		return;
	va_start(arg, fmt);
	vseprint(buf, buf+sizeof(buf), fmt, arg);
	va_end(arg);
	fprint(2, "%L: warning: %s\n", n->src.start, buf);
}

void
error(Line line, char *fmt, ...)
{
	char buf[4096];
	va_list arg;

	errors++;
	if(errors >= maxerr){
		if(errors == maxerr)
			fprint(2, "too many errors, stopping\n");
		return;
	}
	va_start(arg, fmt);
	vseprint(buf, buf+sizeof(buf), fmt, arg);
	va_end(arg);
	fprint(2, "%L: %s\n", line, buf);
}

void
nerror(Node *n, char *fmt, ...)
{
	char buf[4096];
	va_list arg;

	errors++;
	if(errors >= maxerr){
		if(errors == maxerr)
			fprint(2, "too many errors, stopping\n");
		return;
	}
	va_start(arg, fmt);
	vseprint(buf, buf+sizeof(buf), fmt, arg);
	va_end(arg);
	fprint(2, "%L: %s\n", n->src.start, buf);
}

void
yyerror(char *fmt, ...)
{
	char buf[4096];
	va_list arg;

	errors++;
	if(errors >= maxerr){
		if(errors == maxerr)
			fprint(2, "too many errors, stopping\n");
		return;
	}
	va_start(arg, fmt);
	vseprint(buf, buf+sizeof(buf), fmt, arg);
	va_end(arg);
	if(lasttok != 0)
		fprint(2, "%L: near ` %s ` : %s\n", curline(), toksp(lasttok), buf);
	else
		fprint(2, "%L: %s\n", curline(), buf);
}

/* Keep the failure status initialized when earlier diagnostics suppress output. */
void
fatal(char *fmt, ...)
{
	char buf[4096];
	va_list arg;

	strcpy(buf, "errors");
	if(errors == 0 || isfatal){
		va_start(arg, fmt);
		vseprint(buf, buf+sizeof(buf), fmt, arg);
		va_end(arg);
		fprint(2, "fatal limbo compiler error: %s\n", buf);
	}
	if(bout != nil)
		remove(outfile);
	if(bsym != nil)
		remove(symfile);
	if(isfatal)
		abort();
	exits(buf);
}

int
gfltconv(Fmt *f)
{
	double d;
	char buf[32];

	d = va_arg(f->args, double);
	g_fmt(buf, d, 'e');
	return fmtstrcpy(f, buf);
}

/* Do not touch storage when a diagnostic has no remaining output space. */
char*
secpy(char *p, char *e, char *s)
{
	int c;

	if(p >= e)
		return p;
	for(; c = *s; s++){
		*p++ = c;
		if(p == e){
			p[-1] = '\0';
			return p;
		}
	}
	*p = '\0';
	return p;
}

char*
seprint(char *buf, char *end, char *fmt, ...)
{
	va_list arg;

	if(buf == end)
		return buf;
	va_start(arg, fmt);
	buf = vseprint(buf, end, fmt, arg);
	va_end(arg);
	return buf;
}

void*
allocmem(ulong n)
{
	void *p;

	p = malloc(n);
	if(p == nil)
		fatal("out of memory");
	return p;
}

void*
reallocmem(void *p, ulong n)
{
	if(p == nil)
		p = malloc(n);
	else
		p = realloc(p, n);
	if(p == nil)
		fatal("out of memory");
	return p;
}
