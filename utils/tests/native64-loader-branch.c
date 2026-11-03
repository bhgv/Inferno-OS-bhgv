#include "runt.h"

char exBounds[] = "bounds"; // Existing VM bounds identity used by snapshot conversion.
static Loader_Inst result; // Actual native snapshot record, never a wire image.
static Inst instruction; // Borrowed decoded branch record.
static int caseop, caseshape; // Pinpoint the opcode/geometry of any semantic failure.
static Module *selected; // Current independently owned module or nil/H refusal input.
LOADER_BRANCH


/* Require a semantic mismatch before any target could be used as executable code. */
static void
branchcheck(int condition)
{
	if(!condition){
		fprintf(stderr, "Loader branch target mismatch: opcode=%d shape=%d\n", caseop, caseshape);
		exit(10);
	}
}


/* Enumerate Dis branch identities independently of target validation arithmetic. */
static int
isbranch(int op)
{
	static int operations[] = {ICALL, IJMP, ISPAWN,
		IBEQW, IBNEW, IBLTW, IBLEW, IBGTW, IBGEW,
		IBEQB, IBNEB, IBLTB, IBLEB, IBGTB, IBGEB,
		IBEQF, IBNEF, IBLTF, IBLEF, IBGTF, IBGEF,
		IBEQC, IBNEC, IBLTC, IBLEC, IBGTC, IBGEC,
		IBEQL, IBNEL, IBLTL, IBLEL, IBGTL, IBGEL};
	int i;

	for(i=0; i<nelem(operations); i++)
		if(op == operations[i])
			return 1;
	return 0;
}


/* Retain the actual protected error frame while calling only the production converter. */
static int
convert(void *argument)
{
	USED(argument);
	brunpatch(&result, &instruction, selected);
	return 1;
}


/* Check all opcode identities, target geometry, byte preservation and full native addresses. */
int
main(void)
{
	Module module, saved;
	Inst program[4], before;
	Loader_Inst wanted;
	uintptr base;
	int op, shape, status, valid, cases, converted, refused;

	base = (uintptr)program;
	branchcheck(sizeof(void*) == 8 && base > 0xffffffffUL);
	cases = converted = refused = 0;
	for(op=0; op<MAXDIS; op++)
	for(shape=0; shape<16; shape++){
		caseop = op; caseshape = shape;
		memset(&module, 0, sizeof(module));
		memset(&instruction, 0, sizeof(instruction));
		memset(&result, 0xa5, sizeof(result));
		result.op = op;
		module.prog = program;
		module.nprog = nelem(program);
		selected = &module;
		instruction.op = op;
		instruction.d.ins = (Inst*)(base+(shape < 3 ? (shape == 2 ? 3 : shape)*sizeof(Inst) : 0));
		switch(shape){
		case 3: instruction.d.ins = program+4; break;
		case 4: instruction.d.ins = (Inst*)(base-sizeof(Inst)); break;
		case 5: instruction.d.ins = nil; break;
		case 6: instruction.d.ins = H; break;
		case 7: instruction.d.ins = (Inst*)(base+1); break;
		case 8: module.prog = nil; break;
		case 9: module.prog = H; break;
		case 10: module.nprog = 0; break;
		case 11: module.nprog = -1; break;
		case 12: module.prog = (Inst*)(base+1); break;
		case 13: module.prog = (Inst*)((uintptr)-1-7); break;
		case 14: selected = nil; break;
		case 15: selected = H; break;
		}
		memmove(&saved, &module, sizeof(saved));
		memmove(&before, &instruction, sizeof(before));
		memmove(&wanted, &result, sizeof(wanted));
		valid = !isbranch(op) || shape < 3;
		if(isbranch(op) && valid)
			wanted.dst = shape == 2 ? 3 : shape;
		strcpy(diagnostic, "before");
		status = kprotect(convert, nil);
		branchcheck(status == valid && live == 0 && worker.nerr == 0);
		branchcheck(strcmp(diagnostic, valid ? "before" : exBounds) == 0);
		branchcheck(memcmp(&wanted, &result, sizeof(wanted)) == 0);
		branchcheck(memcmp(&saved, &module, sizeof(saved)) == 0 && memcmp(&before, &instruction, sizeof(before)) == 0);
		converted += isbranch(op) && valid;
		refused += !valid;
		cases++;
	}
	printf("PASS Loader branch snapshots WORD%d: %d opcode/shape cases; %d valid native-index conversions; %d bounds refusals; pointers above4GiB\n", (int)sizeof(WORD)*8, cases, converted, refused);
	return 0;
}
