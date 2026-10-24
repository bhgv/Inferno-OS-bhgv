/* Common decoded-operation contract for 64-bit leaf backends. */
typedef struct LeafLayout LeafLayout;
struct LeafLayout {
	int wordbytes;
	int pc, s, d, m;
	int arraylen, arraydata;
	int stringlen, stringdata;
	int listdata, listtail;
	int fp, fixedtmp;
};
typedef struct LeafOp LeafOp;
struct LeafOp {
	int kind, bytes, input, alu, condition;
};
enum {
	LeafMove = 1, LeafAlu, LeafBranch, LeafJump, LeafAddress, LeafNop,
	LeafRealBranch, LeafShift, LeafDivide, LeafLength, LeafIndex, LeafNegReal,
	LeafFixedFrame, LeafRealResize, LeafIntegerReal, LeafFixedShift, LeafPower, LeafHeadCopy, LeafCopy, LeafStringIndex, LeafStringLength, LeafHead, LeafListLength,
};

/* Classify only operations whose complete VM semantics fit a helper-free leaf. */
static int
leafop(LeafOp *p, int op, int wordbytes)
{
	memset(p, 0, sizeof(*p));
	p->bytes = wordbytes;
	switch(op){
	case IBEQF: case IBNEF: case IBLTF: case IBLEF: case IBGTF: case IBGEF:
		p->kind = LeafRealBranch; p->bytes = 8;
		p->condition = op == IBEQF ? 0 : op == IBNEF ? 1 : op == IBLTF ? 2 :
			op == IBLEF ? 3 : op == IBGTF ? 4 : 5;
		break;
	case IMULX: p->kind = LeafFixedFrame; break;
	case IDIVX: p->kind = LeafFixedFrame; p->alu = 1; break;
	case ICVTRF: p->kind = LeafRealResize; p->input = 4; p->bytes = 8; break;
	case ICVTFR: p->kind = LeafRealResize; p->input = 8; p->bytes = 4; break;
	case ICVTWF: p->kind = LeafIntegerReal; p->input = wordbytes; p->bytes = 8; break;
	case ICVTLF: p->kind = LeafIntegerReal; p->input = 8; p->bytes = 8; break;
	case ICVTXX: p->kind = LeafFixedShift; break;
	case IEXPW: p->kind = LeafPower; break;
	case IEXPL: p->kind = LeafPower; p->bytes = 8; break;
	case IHEADB: p->bytes = 1; /* fall through */
	case IHEADW: p->kind = LeafHead; break;
	case IHEADL: case IHEADF: p->kind = LeafHead; p->bytes = 8; break;
	case ILENL: p->kind = LeafListLength; break;
	case IINDC: p->kind = LeafStringIndex; break;
	case ILENC: p->kind = LeafStringLength; break;
	case IHEADM: p->kind = LeafHeadCopy; break;
	case IMOVM: p->kind = LeafCopy; break;
	case INEGF: p->kind = LeafNegReal; p->bytes = 8; break;
	case ILENA: p->kind = LeafLength; break;
	case IINDB: case IINDW: case IINDL: case IINDF:
		/* Address-producing VM slots must be able to hold a native pointer. */
		if(wordbytes != 8) return 0;
		p->kind = LeafIndex;
		p->bytes = op == IINDB ? 1 : 8;
		break;
	case IRUNT: p->kind = LeafNop; break;
	case IMOVB: p->bytes = 1; /* fall through */
	case IMOVW: p->kind = LeafMove; break;
	case IMOVL: case IMOVF: p->kind = LeafMove; p->bytes = 8; break;
	case ICVTBW: p->kind = LeafMove; p->input = 1; break;
	case ICVTSW: p->kind = LeafMove; p->input = 2; break;
	case ICVTWB: p->kind = LeafMove; p->bytes = 1; p->input = wordbytes; break;
	case ICVTWS: p->kind = LeafMove; p->bytes = 2; p->input = wordbytes; break;
	case ILEA:
		if(wordbytes != 8) return 0;
		p->kind = LeafAddress;
		break;
	case IJMP: p->kind = LeafJump; break;
	case IADDB: p->bytes = 1; /* fall through */
	case IADDW: p->kind = LeafAlu; p->alu = 0; break;
	case ISUBB: p->bytes = 1; /* fall through */
	case ISUBW: p->kind = LeafAlu; p->alu = 1; break;
	case IMULB: p->bytes = 1; /* fall through */
	case IMULW: p->kind = LeafAlu; p->alu = 2; break;
	case IANDB: p->bytes = 1; /* fall through */
	case IANDW: p->kind = LeafAlu; p->alu = 3; break;
	case IORB: p->bytes = 1; /* fall through */
	case IORW: p->kind = LeafAlu; p->alu = 4; break;
	case IXORB: p->bytes = 1; /* fall through */
	case IXORW: p->kind = LeafAlu; p->alu = 5; break;
	case IADDL: case ISUBL: case IMULL: case IANDL: case IORL: case IXORL:
		p->kind = LeafAlu; p->bytes = 8;
		p->alu = op == IADDL ? 0 : op == ISUBL ? 1 : op == IMULL ? 2 :
			op == IANDL ? 3 : op == IORL ? 4 : 5;
		break;
	case ICVTWL: p->kind = LeafMove; p->bytes = 8; p->input = wordbytes; break;
	case ICVTLW: p->kind = LeafMove; p->input = 8; break;
	case ISHLB: p->bytes = 1; /* fall through */
	case ISHLW: p->kind = LeafShift; p->alu = 0; break;
	case ISHRB: p->bytes = 1; p->kind = LeafShift; p->alu = 1; break;
	case ILSRW: p->kind = LeafShift; p->alu = 1; break;
	case ISHRW: p->kind = LeafShift; p->alu = 2; break;
	case ISHLL: case ISHRL: case ILSRL:
		p->kind = LeafShift; p->bytes = 8;
		p->alu = op == ISHLL ? 0 : op == ILSRL ? 1 : 2;
		break;
	case IDIVB: p->bytes = 1; /* fall through */
	case IDIVW: p->kind = LeafDivide; break;
	case IMODB: p->bytes = 1; /* fall through */
	case IMODW: p->kind = LeafDivide; p->alu = 1; break;
	case IDIVL: case IMODL:
		p->kind = LeafDivide; p->bytes = 8; p->alu = op == IMODL;
		break;
	case IBEQL: case IBNEL: case IBLTL: case IBLEL: case IBGTL: case IBGEL:
		p->kind = LeafBranch; p->bytes = 8;
		p->condition = op == IBEQL ? 0 : op == IBNEL ? 1 : op == IBLTL ? 2 :
			op == IBLEL ? 3 : op == IBGTL ? 4 : 5;
		break;
	case IBEQB: p->bytes = 1; /* fall through */
	case IBEQW: p->kind = LeafBranch; p->condition = 0; break;
	case IBNEB: p->bytes = 1; /* fall through */
	case IBNEW: p->kind = LeafBranch; p->condition = 1; break;
	case IBLTB: p->bytes = 1; /* fall through */
	case IBLTW: p->kind = LeafBranch; p->condition = 2; break;
	case IBLEB: p->bytes = 1; /* fall through */
	case IBLEW: p->kind = LeafBranch; p->condition = 3; break;
	case IBGTB: p->bytes = 1; /* fall through */
	case IBGTW: p->kind = LeafBranch; p->condition = 4; break;
	case IBGEB: p->bytes = 1; /* fall through */
	case IBGEW: p->kind = LeafBranch; p->condition = 5; break;
	default: return 0;
	}
	if(p->input == 0) p->input = p->bytes;
	return 1;
}
