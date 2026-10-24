/* Common decoded-operation contract for 64-bit leaf backends. */
typedef struct LeafLayout LeafLayout;
struct LeafLayout {
	int wordbytes;
	int pc, s, d, m;
};
typedef struct LeafOp LeafOp;
struct LeafOp {
	int kind, bytes, input, alu, condition;
};
enum { LeafMove=1, LeafAlu, LeafBranch, LeafJump, LeafAddress, LeafNop };

/* Classify only operations whose complete VM semantics fit a helper-free leaf. */
static int
leafop(LeafOp *p, int op, int wordbytes)
{
	memset(p, 0, sizeof(*p));
	p->bytes = wordbytes;
	switch(op){
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
