#define LeafListLimit 1024 // Maximum list nodes scanned natively before returning to C.
#define LeafStringWindow (1ULL<<34) // Covers every int-sized Rune payload and supported inline offset.

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
	int arrayroot, typeref; // Backing array owner and signed Type reference count offsets.
	void *stringtype; // Exact Tstring identity required before reusing numeric format destinations.
	void *arrayidentity; // Exact builtin Array identity required by byte-string conversion.
	int typefree, typemark; // Callback offsets within a Type descriptor.
	void *plainfree, *plainmark; // Exact standard freeheap/markheap callback identities.
	void *rawchannelentry, *mappedchannelentry; // Unpublished raw and mapped channel allocators.
	int channelmid; // Raw-width/descriptor union offset within a newly allocated Channel.
	void *channeltransferentry; // Pure buffered-transfer preflight; native code copies and commits.
	void *channelentry; // Shared scalar channel allocator selected by opcode and int capacity.
	void *constypeentry; // Pure mapped-list descriptor validation before native allocation.
	void *consentry; // Runtime list allocator; scalar capture/store remains native.
	int listtype; // Element Type field within each mapped list node.
	void *ptrtype; // Exact live builtin pointer Type used by CONSP.
};
typedef struct LeafOp LeafOp;


/* Bound complete native Array fields before testing whether their spans overlap. */
static int
leafbytearraylayout(LeafLayout *a)
{
	int offsets[4], widths[4], i, j;

	offsets[0] = a->arraylen; widths[0] = a->wordbytes;
	offsets[1] = a->arraytype; widths[1] = 8;
	offsets[2] = a->arrayroot; widths[2] = 8;
	offsets[3] = a->arraydata; widths[3] = 8;
	if(a->wordbytes != 4 && a->wordbytes != 8)
		return 0;
	for(i = 0; i < 4; i++)
		if(offsets[i] < 0 || offsets[i] > 0x7fffffff-widths[i])
			return 0;
	for(i = 0; i < 4; i++)
		for(j = i+1; j < 4; j++)
			if(offsets[i] < offsets[j]+widths[j] && offsets[j] < offsets[i]+widths[i])
				return 0;
	return 1;
}


/* Require disjoint complete native GC cells and nonoverlapping owner metadata.
 * These numeric guards do not establish mapping ownership of supplied addresses. */
static int
leafownergclayout(LeafLayout *a)
{
	unsigned long long mutator, propagation;

	if(a->heapref <= 0 || a->heapref > 255 || a->heapref%8 ||
	   a->heapcolor <= 0 || a->heapcolor > 255 || a->heapcolor%4 ||
	   a->refbytes != 8 || a->markcolor < 0 || a->markcolor > 255)
		return 0;
	if(a->heapcolor < a->heapref+4 && a->heapref < a->heapcolor+8)
		return 0;
	mutator = (unsigned long long)a->mutator;
	propagation = (unsigned long long)a->nprop;
	if(mutator == 0 || propagation == 0 || (mutator|propagation)%4 ||
	   mutator > ~0ULL-4 || propagation > ~0ULL-4 || mutator == propagation)
		return 0;
	return 1;
}


/* Validate both byte-conversion layouts before offsets or private buffers are used. */
static int
leafbyteconversionlayout(LeafLayout *a)
{
	if(a->wordbytes != 4 && a->wordbytes != 8)
		return 0;
	if(a->typeref < 0 || a->typeref > 2047 || a->typeref%4 ||
	   a->typeref == a->typesize || a->typeref == a->typenp ||
	   a->arraylen < 0 || a->arraylen > 2047 || a->arraylen%a->wordbytes ||
	   a->arraydata < 0 || a->arraydata > 2047 || a->arraydata%8 ||
	   a->arraytype < 0 || a->arraytype > 2047 || a->arraytype%8 ||
	   a->arrayroot < 0 || a->arrayroot > 2047 || a->arrayroot%8 ||
	   a->typesize < 0 || a->typesize > 2047 || a->typesize%4 ||
	   a->typenp < 0 || a->typenp > 2047 || a->typenp%4 ||
	   a->stringlen < 0 || a->stringlen > 2047 || a->stringlen%4 ||
	   a->stringmax < 0 || a->stringmax > 2047 || a->stringmax%4 ||
	   a->stringlen == a->stringmax || a->stringdata < a->stringlen+4 ||
	   a->stringdata < a->stringmax+4 || a->stringdata > 2047 ||
	   a->heapref <= 0 || a->heapref > 128 || a->heapref%8 || a->refbytes != 8 ||
	   a->heaptype <= 0 || a->heaptype > 128 || a->heaptype%8 ||
	   a->heapcolor < a->heapref || a->heapcolor < a->heaptype || a->heapcolor > 255 ||
	   a->arrayidentity == nil || a->arrayidentity == (void*)-1 ||
	   a->stringtype == nil || a->stringtype == (void*)-1)
		return 0;
	if(!leafbytearraylayout(a) || a->heapref == a->heaptype || a->arrayidentity == a->stringtype)
		return 0;
	return 1;
}


/* Validate module lookup, descriptor callbacks and record lifetime offsets for zero reuse. */
static int
leafrecordlayout(LeafLayout *a)
{
	int offsets[5], widths[5], i, j;

	if((a->wordbytes != 4 && a->wordbytes != 8) ||
	   a->module < 0 || a->module > 2047 || a->module%8 ||
	   a->linkmodule < 0 || a->linkmodule > 2047 || a->linkmodule%8 ||
	   a->moduletypes < 0 || a->moduletypes > 2047 || a->moduletypes%8 ||
	   a->modulenty < 0 || a->modulenty > 2047 || a->modulenty%4 ||
	   a->heapref <= 0 || a->heapref > 128 || a->heapref%8 ||
	   a->heaptype <= 0 || a->heaptype > 128 || a->heaptype%8 || a->heaptype == a->heapref ||
	   a->heapcolor < a->heapref || a->heapcolor < a->heaptype || a->heapcolor > 255 || a->refbytes != 8 ||
	   a->plainfree == nil || a->plainmark == nil || a->plainfree == (void*)-1 || a->plainmark == (void*)-1)
		return 0;
	offsets[0]=a->typeref; widths[0]=4;
	offsets[1]=a->typesize; widths[1]=4;
	offsets[2]=a->typenp; widths[2]=4;
	offsets[3]=a->typefree; widths[3]=8;
	offsets[4]=a->typemark; widths[4]=8;
	for(i=0; i<5; i++){
		if(offsets[i] < 0 || offsets[i] > 2047 || offsets[i]%widths[i])
			return 0;
		for(j=0; j<i; j++)
			if(offsets[i] < offsets[j]+widths[j] && offsets[j] < offsets[i]+widths[i])
				return 0;
	}
	return 1;
}


/* Require complete disjoint array fields in addition to live module/type metadata. */
static int
leafarraycreatelayout(LeafLayout *a)
{
	if(!leafrecordlayout(a) || !leafbytearraylayout(a) ||
	   a->arrayidentity == nil || a->arrayidentity == (void*)-1 ||
	   a->arraylen < 0 || a->arraylen > 2047 || a->arraylen%a->wordbytes ||
	   a->arraytype < 0 || a->arraytype > 2047 || a->arraytype%8 ||
	   a->arrayroot < 0 || a->arrayroot > 2047 || a->arrayroot%8 ||
	   a->arraydata < 0 || a->arraydata > 2047 || a->arraydata%8)
		return 0;
	return 1;
}

/* Validate the allocator identity and native payload offset before emitting a runtime call. */
static int
leafscalarconslayout(LeafLayout *a, int bytes)
{
	if((a->wordbytes != 4 && a->wordbytes != 8) ||
	   (bytes != 1 && bytes != 4 && bytes != 8) ||
	   a->listdata < 0 || a->listdata > 2047 || a->listdata%bytes ||
	   a->consentry == nil || a->consentry == (void*)-1)
		return 0;
	return 1;
}


/* Require disjoint list metadata and int Type fields for bounded pointer-free mapped construction. */
static int
leafmappedconslayout(LeafLayout *a)
{
	if(!leafscalarconslayout(a, 8) || a->constypeentry == nil || a->constypeentry == (void*)-1 ||
	   a->module < 0 || a->module > 2047 || a->module%8 ||
	   a->listtail < 0 || a->listtype < 0 || a->listtype > 2047 || a->listtail > 2047 ||
	   a->listtail%8 || a->listtype%8 || a->listtail == a->listtype ||
	   a->listtail+8 > a->listdata || a->listtype+8 > a->listdata ||
	   a->typeref < 0 || a->typeref > 2047 || a->typeref%4 ||
	   a->typesize < 0 || a->typesize > 2047 || a->typesize%4 ||
	   a->typenp < 0 || a->typenp > 2047 || a->typenp%4 ||
	   a->typeref == a->typesize || a->typeref == a->typenp || a->typesize == a->typenp)
		return 0;
	return 1;
}


/* Validate live GC cells, descriptor references and disjoint pointer-list fields. */
static int
leafpointerconslayout(LeafLayout *a)
{
	unsigned long long cells[3];
	int i, j, fields[3];

	if(!leafscalarconslayout(a, 8) || a->refbytes != 8 ||
	   a->heapref <= 0 || a->heapref > 128 || a->heapref%8 ||
	   a->heapcolor < a->heapref+4 || a->heapcolor > 255 || a->heapcolor%4 ||
	   a->typeref < 0 || a->typeref > 2047 || a->typeref%4 ||
	   a->ptrtype == nil || (unsigned long long)a->ptrtype%8 ||
	   (unsigned long long)a->ptrtype > ~0ULL-(a->typeref+4) ||
	   a->markcolor < 0 || a->markcolor > 255)
		return 0;
	fields[0]=a->listtail; fields[1]=a->listtype; fields[2]=a->listdata;
	for(i=0;i<3;i++)
		if(fields[i] < 0 || fields[i] > 2047 || fields[i]%8 ||
		   fields[i] == fields[(i+1)%3] || fields[i] == fields[(i+2)%3])
			return 0;
	cells[0]=(unsigned long long)a->mutator; cells[1]=(unsigned long long)a->nprop;
	cells[2]=(unsigned long long)a->ptrtype+a->typeref;
	for(i=0;i<3;i++)
		if(cells[i] == 0 || cells[i]%4 || cells[i] > ~0ULL-4)
			return 0;
	for(i=0;i<3;i++)for(j=0;j<i;j++)
		if(cells[i] == cells[j])return 0;
	return 1;
}

/* Record construction modes keep operand selection distinct from scalar initialization. */
enum {
	LeafRecordZero = 0, // NEWZ resolves a local descriptor and clears scalar bytes.
	LeafRecordImportedZero = 1, // MNEWZ resolves a descriptor from the source module.
	LeafRecordRaw = 2, // NEW leaves scalar bytes unspecified without accessing them.
};


/* Decoded operation parameters shared by the native leaf emitters. */
struct LeafOp {
	int kind; // Native leaf implementation selected for this opcode.
	int bytes; // Width of the stored scalar result.
	int input; // Source/allocation width, or the bounded digit count for decimal leaves.
	int alu; // Arithmetic or representation variant within a leaf kind.
	int condition; // Comparison predicate, rounding variant or string-case selector.
};

enum {
	LeafTransfer = 132, // Copy pointer-free buffered SEND/RECV after pure runtime preflight.
	LeafMappedCons = 131, // Resolve live descriptors and copy pointer-free mapped payloads.
	LeafRawCons = 128, // Allocate and copy bounded raw list payloads before publication.
	LeafChannel = 130, // Construct typed channels after native argument and empty-owner checks.
	LeafPointerCons = 129, // Retain pointer heads and mark owners after successful allocation.
	LeafScalarCons = 127, // Capture scalar list heads natively around the shared allocator.
	LeafArrayCreate = 126, // Reuse unique owning pointer-free arrays for NEWA/NEWAZ.
	LeafRecordObject = 125, // Reuse unique pointer-free standard records for NEW/NEWZ/MNEWZ.
	LeafStringBytes = 124, // Export bounded byte-backed strings into reusable byte arrays.
	LeafByteString = 123, // Convert bounded ASCII arrays into reusable unique strings.
	LeafFormatInteger = 122, // Format integers or selected real values into reusable strings.
	LeafArraySlice = 121, // Narrow unique views or pointer-free roots without allocating.
	LeafModuleCall = 120, // Bytecode calls between modules with equal execution modes.
	LeafReturn = 119, // Returns without mapped cleanup, extent release or module destruction.
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


/* Classify guarded native and helper-assisted paths that explicitly decline to the VM when needed. */
static int
leafop(LeafOp *p, int op, int wordbytes)
{
	memset(p, 0, sizeof(*p));
	p->bytes = wordbytes;
	switch(op){
	case ISEND: p->kind = LeafTransfer; p->alu = 0; break;
	case IRECV: p->kind = LeafTransfer; p->alu = 1; break;
	case INEWCM: case INEWCMP:
		p->kind = LeafChannel; p->alu = op; break;
	case INEWCB: case INEWCW: case INEWCL: case INEWCF: case INEWCP:
		p->kind = LeafChannel; p->alu = op; break;
	case ICONSMP: p->kind = LeafMappedCons; break;
	case ICONSM: p->kind = LeafRawCons; break;
	case ICONSP: p->kind = LeafPointerCons; p->bytes = 8; p->input = 8; break;
	case ICONSB: p->kind = LeafScalarCons; p->bytes = 1; p->input = 4; break;
	case ICONSW: p->kind = LeafScalarCons; p->input = wordbytes; break;
	case ICONSL: case ICONSF: p->kind = LeafScalarCons; p->bytes = 8; p->input = 8; break;
	case INEWA: p->kind = LeafArrayCreate; break;
	case INEWAZ: p->kind = LeafArrayCreate; p->alu = 1; break;
	case INEW: p->kind = LeafRecordObject; p->alu = LeafRecordRaw; break;
	case INEWZ: p->kind = LeafRecordObject; p->alu = LeafRecordZero; break;
	case IMNEWZ: p->kind = LeafRecordObject; p->alu = LeafRecordImportedZero; break;
	case ICVTAC: p->kind = LeafByteString; break;
	case ICVTCA: p->kind = LeafStringBytes; break;
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
	case ISLICEA: p->kind = LeafArraySlice; break;
	case IADDC: p->kind = LeafStringAppend; break;
	case IINSC: p->kind = LeafStringStore; break;
	case ICVTFC: p->kind = LeafFormatInteger; p->bytes = 8; p->alu = 1; break;
	case ICVTWC: p->kind = LeafFormatInteger; break;
	case ICVTLC: p->kind = LeafFormatInteger; p->bytes = 8; break;
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
	case ICASEC:
		p->kind = LeafCaseJump;
		p->bytes = 8;
		p->condition = 1;
		break;
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
	if(p->kind == LeafDecimal){
		/* Unsigned accumulation fits; integer leaves check the final signed limit. */
		p->input = p->alu ? 15 : p->bytes == 4 ? 10 : 19;
	}
	if(p->input == 0) p->input = p->bytes;
	return 1;
}
