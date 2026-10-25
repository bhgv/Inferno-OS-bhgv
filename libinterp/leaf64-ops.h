/* Common decoded-operation contract for 64-bit leaf backends. */
typedef struct LeafLayout LeafLayout;

struct LeafLayout {
	int wordbytes; // VM WORD width in bytes; pointers remain native width.
	int pc, s, d, m; // Offsets of PC and operand pointers within REG.
	int arraylen, arraydata; // Array length and data pointer offsets.
	int stringlen, stringdata; // Signed string length and inline character data offsets.
	int listdata, listtail; // List payload and next-owner offsets.
	int fp, fixedtmp; // Frame pointer in REG and fixed-point scratch within a frame.
	int arraytype, typesize; // Element descriptor in Array and byte size within Type.
	int framelr, framefp; // Saved return address and caller frame pointer offsets.
	int fixedcoef; // Fixed-point coefficient offset within a frame.
	int module, linkmodule, moduleprog, modulenprog, instbytes; // Module links, program bounds and instruction stride.
	int heaptype; // Backward distance from an owner to its heap Type field.
	int linkcompiled; // Native-entry flag offset within a module link.
	int heapref, heapcolor, refbytes; // Heap field distances and reference-counter width.
	void *mutator, *nprop; // Addresses of GC mutator color and propagation counter.
	int markcolor; // Collector color used when publishing a retained owner.
	int moduletypes, modulenty, typenp; // Type table, table count and pointer-map length offsets.
	int linknlinks, linklinks, modlbytes, modlframe; // Import count, inline table, entry stride and frame-Type offset.
	int sp, ts; // Stack cursor and stack-end offsets within REG.
	int frametype, framemr, framesize; // Frame Type/module-link offsets and minimum frame size.
	int stringmax; // String capacity offset, measured in representation characters.
	int regmp, regtemp, linkprog, linkmp; // Register MP/temp and module-link program/data offsets.
	int modlpc; // Native program/runtime entry offset within an import record.
};
typedef struct LeafOp LeafOp;

/* Decoded operation parameters shared by the native leaf emitters. */
struct LeafOp {
	int kind; // Native leaf implementation selected for this opcode.
	int bytes; // Width of the stored scalar result.
	int input; // Source width, or the bounded digit count for decimal leaves.
	int alu; // Arithmetic or representation variant within a leaf kind.
	int condition; // Comparison predicate or fixed-point rounding variant.
};

enum {
	LeafModuleCall = 120, // Bytecode calls between modules with equal execution modes.
	LeafReturn = 119, // Same-module returns without mapped cleanup or extent release.
	LeafFrame = 118, // Pointer-free frames within the current stack extent.
	LeafStringSlice = 117, // Nonempty slices that reuse unique string storage.
	LeafStringAppend = 116, // Concatenation that reuses unique ASCII or Rune storage.
	LeafStringStore = 115,
	LeafDecimal = 114,
	LeafSliceCopy = 113,
	LeafMappedCopy = 112,
	LeafOwner = 111,
	LeafCaseJump = 110,
	LeafTableJump = 109,
	LeafRealPower = 108,
	LeafRealInteger = 107,
	LeafRealArithmetic = 106,
	LeafStringBranch = 105,
	LeafTypeCheck = 104,
	LeafCall = 100,
	LeafFixedCoefficient = 102,
	LeafProgramAddress = 103,
	LeafMove = 1, LeafAlu, LeafBranch, LeafJump, LeafAddress, LeafNop,
	LeafRealBranch, LeafShift, LeafDivide, LeafLength, LeafIndex, LeafNegReal,
	LeafDescriptorIndex, LeafFixedFrame, LeafRealResize, LeafIntegerReal, LeafFixedShift, LeafPower, LeafHeadCopy, LeafCopy, LeafStringIndex, LeafStringLength, LeafHead, LeafListLength,
};


/* Classify helper-free native paths that explicitly decline to the VM when needed. */
static int
leafop(LeafOp *p, int op, int wordbytes)
{
	memset(p, 0, sizeof(*p));
	p->bytes = wordbytes;
	switch(op){
	case IBEQC: case IBNEC: case IBLTC: case IBLEC: case IBGTC: case IBGEC:
		p->kind = LeafStringBranch;
		p->condition = op == IBEQC ? 0 : op == IBNEC ? 1 : op == IBLTC ? 2 :
			op == IBLEC ? 3 : op == IBGTC ? 4 : 5;
		break;
	case IEXPF: p->kind = LeafRealPower; p->bytes = 8; break;
	case ICVTFW: case ICVTFL: case ICVTFX:
		p->kind = LeafRealInteger; p->bytes = op == ICVTFL ? 8 : wordbytes;
		p->alu = op == ICVTFX; break;
	case ICVTXF:
		p->kind = LeafRealArithmetic; p->bytes = 8; p->alu = 4; break;
	case IADDF: case ISUBF: case IMULF: case IDIVF:
		p->kind = LeafRealArithmetic; p->bytes = 8;
		p->alu = op == IADDF ? 0 : op == ISUBF ? 1 : op == IMULF ? 2 : 3;
		break;
	case ISLICEC: p->kind = LeafStringSlice; break;
	case IADDC: p->kind = LeafStringAppend; break;
	case IINSC: p->kind = LeafStringStore; break;
	case ICVTCW: p->kind = LeafDecimal; break;
	case ICVTCL: p->kind = LeafDecimal; p->bytes = 8; break;
	case ICVTCF: p->kind = LeafDecimal; p->bytes = 8; p->alu = 1; break;
	case ISLICELA: p->kind = LeafSliceCopy; break;
	case IMOVMP: p->kind = LeafMappedCopy; break;
	case IHEADMP: p->kind = LeafMappedCopy; p->alu = 1; break;
	case IMOVP: p->kind = LeafOwner; break;
	case ISELF: p->kind = LeafOwner; p->alu = 1; break;
	case IHEADP: p->kind = LeafOwner; p->alu = 2; break;
	case ITAIL: p->kind = LeafOwner; p->alu = 3; break;
	case ITCMP: p->kind = LeafTypeCheck; break;
	case IBEQF: case IBNEF: case IBLTF: case IBLEF: case IBGTF: case IBGEF:
		p->kind = LeafRealBranch; p->bytes = 8;
		p->condition = op == IBEQF ? 0 : op == IBNEF ? 1 : op == IBLTF ? 2 :
			op == IBLEF ? 3 : op == IBGTF ? 4 : 5;
		break;
	case IINDX:
		p->kind = LeafDescriptorIndex; break;
	case IMULX1: p->kind = LeafFixedCoefficient; p->condition = 1; break;
	case IDIVX1: p->kind = LeafFixedCoefficient; p->condition = 1; p->alu = 1; break;
	case ICVTXX1: p->kind = LeafFixedCoefficient; p->condition = 1; p->alu = 2; break;
	case IMULX0: p->kind = LeafFixedCoefficient; break;
	case IDIVX0: p->kind = LeafFixedCoefficient; p->alu = 1; break;
	case ICVTXX0: p->kind = LeafFixedCoefficient; p->alu = 2; break;
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
		/* Match the C interpreter: validate a native address, then store WORD. */
		p->kind = LeafIndex;
		p->bytes = op == IINDB ? 1 : op == IINDW ? wordbytes : 8;
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
		p->kind = LeafAddress;
		break;
	case IMOVPC: p->kind = LeafProgramAddress; p->bytes = 8; break;
	case IGOTO: p->kind = LeafTableJump; p->bytes = 8; break;
	case ICASE: p->kind = LeafCaseJump; break;
	case ICASEL: p->kind = LeafCaseJump; p->bytes = 8; p->alu = 1; break;
	case IMCALL: p->kind = LeafModuleCall; break;
	case IRET: p->kind = LeafReturn; break;
	case IMFRAME: p->kind = LeafFrame; p->alu = 1; break;
	case IFRAME: p->kind = LeafFrame; break;
	case ICALL: p->kind = LeafCall; break;
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
	if(p->kind == LeafDecimal)
		p->input = p->alu ? 15 : p->bytes == 4 ? 9 : 18; // Digits bounded for exact parsing on every supported host.
	if(p->input == 0) p->input = p->bytes;
	return 1;
}
