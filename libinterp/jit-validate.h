/* Preflight metadata that native-PC backends index while compiling. This is
 * not a verifier for arbitrary Dis memory accesses during execution. */
static int
jitvalidpc(Module *m, Inst *pc, int size)
{
	uintptr start, address, offset;
	start=(uintptr)m->prog; address=(uintptr)pc;
	if(address < start)
		return 0;
	offset=address-start;
	return offset%sizeof(Inst) == 0 && offset/sizeof(Inst) < size;
}

static int
jitvalidtable(Module *m, Inst *ip, int kind, int size, int *first, int *limit)
{
	WORD *t, count, pc;
	int bytes, header, stride, last, pcword, i, off;

	if(UXDST(ip->add) != DST(AMP) || m->origmp == H || m->origmp == nil ||
	   m->ntype <= 0 || m->type == nil || m->type[0] == nil ||
	   m->type[0]->size < 0)
		return 0;
	/* All offsets are checked before forming a pointer into module data. */
	if(ip->d.ind < 0 || (uvlong)ip->d.ind > 0x7fffffff)
		return 0;
	off=ip->d.ind; bytes=m->type[0]->size;
	if(kind == 3) {
		if(off < sizeof(WORD))
			return 0;
		off-=sizeof(WORD);
	}
	header=kind == 2 ? 2 : 1;
	stride=kind == 3 ? 1 : kind == 2 ? 6 : 3;
	last=kind == 3 ? 0 : 1;
	pcword=kind == 3 ? 0 : kind == 2 ? 4 : 2;
	if(off%sizeof(WORD) != 0 || off > bytes ||
	   (bytes-off)/sizeof(WORD) < header+last)
		return 0;
	t=(WORD*)(m->origmp+off); count=t[0];
	if(count < 0 || (uvlong)count > ((bytes-off)/sizeof(WORD)-header-last)/stride)
		return 0;
	for(i=0; i<count; i++) {
		pc=t[header+i*stride+pcword];
		if(pc < 0 || pc >= size)
			return 0;
	}
	if(last) {
		pc=t[header+count*stride];
		if(pc < 0 || pc >= size)
			return 0;
	}
	*first=off;
	*limit=off+(header+count*stride+last)*sizeof(WORD);
	return 1;
}

static int
jitkind(Inst *ip)
{
	switch(ip->op) {
	case ICASE: case ICASEC: return 1;
	case ICASEL: return 2;
	case IGOTO: return 3;
	}
	return 0;
}

static int
jitvalidprogram(Module *m, int size)
{
	Inst *ip;
	int i, j, kind, otherkind, first, limit, otherfirst, otherlimit;
	for(i=0; i<size; i++) {
		ip=&m->prog[i]; kind=jitkind(ip);
		if(kind != 0) {
			if(!jitvalidtable(m,ip,kind,size,&first,&limit))
				return 0;
			/* Exact aliases are intentional (compiler folding). Other
			 * overlaps let one table's markers/PCs corrupt another. */
			for(j=0; j<i; j++) {
				otherkind=jitkind(&m->prog[j]);
				if(otherkind == 0)
					continue;
				if(!jitvalidtable(m,&m->prog[j],otherkind,size,&otherfirst,&otherlimit))
					return 0;
				if(otherfirst < limit && first < otherlimit &&
				   (kind != otherkind || first != otherfirst || limit != otherlimit))
					return 0;
			}
		}
		switch(ip->op) {
		case IFRAME:
			if(UXSRC(ip->add) == SRC(AIMM) &&
			   (ip->s.imm < 0 || ip->s.imm >= m->ntype || m->type[ip->s.imm] == nil))
				return 0;
			break;
		case ICALL:
		case IJMP:
		case IBEQW:
		case IBNEW:
		case IBLTW:
		case IBLEW:
		case IBGTW:
		case IBGEW:
		case IBEQB:
		case IBNEB:
		case IBLTB:
		case IBLEB:
		case IBGTB:
		case IBGEB:
		case IBEQF:
		case IBNEF:
		case IBLTF:
		case IBLEF:
		case IBGTF:
		case IBGEF:
		case IBEQC:
		case IBNEC:
		case IBLTC:
		case IBLEC:
		case IBGTC:
		case IBGEC:
		case IBEQL:
		case IBNEL:
		case IBLTL:
		case IBLEL:
		case IBGTL:
		case IBGEL:
		case ISPAWN:
			if(!jitvalidpc(m,ip->d.ins,size))
				return 0;
			break;
		}
	}
	return 1;
}
