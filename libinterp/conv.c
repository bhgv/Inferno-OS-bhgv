#include "lib9.h"
#include "isa.h"
#include "interp.h"
#include "mathi.h"

enum
{
	TOKI0,
	TOKI1,
	TOKI2,
	TOKI3,
	TOKSB,
	TOKFP
};
#include "tab.h"

typedef struct Addr	Addr;
struct Addr
{
	uchar	mode;
	Adr	a;
};

#pragma	varargck	type	"a"	Addr*

char*	opnam[256];
static uchar optype[256];
int	iconv(Fmt*);
int	aconv(Fmt*);

/* Render unknown address modes explicitly and preserve full-width offsets. */
int
aconv(Fmt *f)
{
	Addr *a;
	char buf[64];

	a = va_arg(f->args, Addr*);
	if(a == nil)
		return fmtstrcpy(f, "AZ");
	switch(a->mode & AMASK) {
	case AFP:	snprint(buf, sizeof(buf), "%ld(fp)", (long)a->a.ind);	break;
	case AMP:	snprint(buf, sizeof(buf), "%ld(mp)", (long)a->a.ind);	break;
	case AIMM:	snprint(buf, sizeof(buf), "$%ld", (long)a->a.imm);	break;
	case AIND|AFP:	snprint(buf, sizeof(buf), "%d(%d(fp))", a->a.i.s, a->a.i.f); break;
	case AIND|AMP:	snprint(buf, sizeof(buf), "%d(%d(mp))", a->a.i.s, a->a.i.f); break;
	default:	snprint(buf, sizeof(buf), "A%d", a->mode & AMASK); break;
	}
	return fmtstrcpy(f, buf);
}

/* Look up opcode metadata by opcode and keep malformed diagnostics printable. */
int
Dconv(Fmt *f)
{
	int j;
	Inst *i;
	Addr s, d;
	char buf[128];
	static int init;

	if(init == 0) {
		for(j = 0; keywds[j].name != nil; j++){
			opnam[keywds[j].op] = keywds[j].name;
			optype[keywds[j].op] = keywds[j].terminal;
		}

		fmtinstall('a', aconv);
		init = 1;
	}

	i = va_arg(f->args, Inst*);
	if(i == nil)
		return fmtstrcpy(f, "IZ");

	if(opnam[i->op] == nil){
		snprint(buf, sizeof(buf), "op %d", i->op);
		return fmtstrcpy(f, buf);
	}
	switch(optype[i->op]) {
	case TOKI0:
		snprint(buf, sizeof(buf), "%s", opnam[i->op]);
		break;
	case TOKI1:
		d.a = i->d;
		d.mode = UDST(i->add);
		snprint(buf, sizeof(buf), "%s\t%a", opnam[i->op], &d);
		break;
	case TOKI3:
		d.a = i->d;
		d.mode = UDST(i->add);
		s.a = i->s;
		s.mode = USRC(i->add);
		switch(i->add&ARM) {
		default:
			snprint(buf, sizeof(buf), "%s\t%a, %a", opnam[i->op], &s, &d);
			break;
		case AXIMM:
			snprint(buf, sizeof(buf), "%s\t%a, $%d, %a", opnam[i->op], &s, (short)i->reg, &d);
			break;
		case AXINF:
			snprint(buf, sizeof(buf), "%s\t%a, %d(fp), %a", opnam[i->op], &s, i->reg, &d);
			break;
		case AXINM:
			snprint(buf, sizeof(buf), "%s\t%a, %d(mp), %a", opnam[i->op], &s, i->reg, &d);
			break;
		}
		break;
	case TOKI2:
		d.a = i->d;
		d.mode = UDST(i->add);
		s.a = i->s;
		s.mode = USRC(i->add);
		snprint(buf, sizeof(buf), "%s\t%a, %a", opnam[i->op], &s, &d);
		break;
	}

	return fmtstrcpy(f, buf);
}

