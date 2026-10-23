/* Shared preflight for the module-data tables relocated by ARM and 386. */
typedef struct JitTable JitTable;
struct JitTable {
	ulong start, end;
	int kind;
};

/* Sort spans so overlapping tables can be rejected without quadratic scans. */
static int
jittablecmp(void *a, void *b)
{
	JitTable *left, *right;

	left = a;
	right = b;
	if(left->start < right->start)
		return -1;
	if(left->start > right->start)
		return 1;
	return 0;
}

/* Bound the full span before reading counts, default targets or row targets. */
static int
jittablespan(Module *m, Inst *i, int size, JitTable *span)
{
	ulong offset, available, header, stride, count, row, target;
	WORD *table;
	WORD n;

	if(UXDST(i->add) != DST(AMP) || i->d.ind < 0 ||
	   m->origmp == nil || m->origmp == H || m->ntype <= 0 ||
	   m->type == nil || m->type[0] == nil || m->type[0] == H ||
	   m->type[0]->size < 0)
		return 0;
	offset = i->d.ind;
	if(offset%sizeof(WORD) != 0 ||
	   (i->op == ICASEL && offset%IBY2LG != 0))
		return 0;
	header = 1;
	stride = 3;
	target = 2;
	if(i->op == ICASEL){
		header = 2;
		stride = 6;
		target = 4;
	}else if(i->op == IGOTO){
		if(offset < sizeof(WORD))
			return 0;
		offset -= sizeof(WORD);
		stride = 1;
		target = 0;
	}
	available = m->type[0]->size;
	if(offset > available || header*sizeof(WORD) > available-offset)
		return 0;
	available = (available-offset)/sizeof(WORD);
	table = (WORD*)(m->origmp+offset);
	n = table[0];
	if(n < 0)
		return 0;
	count = n;
	/* Cases require one default target after their rows; gotos do not. */
	if(i->op != IGOTO){
		if(available <= header)
			return 0;
		available--;
	}
	if(count > (available-header)/stride)
		return 0;
	for(row = 0; row < count; row++){
		n = table[header+row*stride+target];
		if(n < 0 || n >= size)
			return 0;
	}
	if(i->op != IGOTO){
		n = table[header+count*stride];
		if(n != -1 && (n < 0 || n >= size))
			return 0;
	}
	span->start = offset;
	span->end = offset+(header+count*stride+(i->op != IGOTO))*sizeof(WORD);
	span->kind = i->op;
	return 1;
}

/* Validate all raw tables before any pass can mark counts or relocate PCs. */
static int
jitvalidtables(Module *m, int size)
{
	JitTable *tables;
	int i, count, at, ok;
	Inst *instruction;

	count = 0;
	for(i = 0; i < size; i++)
		if(m->prog[i].op == ICASE || m->prog[i].op == ICASEC ||
		   m->prog[i].op == ICASEL ||
		   m->prog[i].op == IGOTO)
			count++;
	if(count == 0)
		return 1;
	if(sizeof(WORD) != IBY2WD)
		return 0;
	if(count > 0x7fffffff/sizeof(*tables))
		return 0;
	tables = malloc(count*sizeof(*tables));
	if(tables == nil)
		return 0;
	ok = 0;
	at = 0;
	for(i = 0; i < size; i++){
		instruction = &m->prog[i];
		if(instruction->op != ICASE && instruction->op != ICASEC &&
		   instruction->op != ICASEL &&
		   instruction->op != IGOTO)
			continue;
		if(!jittablespan(m, instruction, size, &tables[at++]))
			goto out;
	}
	qsort(tables, count, sizeof(*tables), jittablecmp);
	for(i = 1; i < count; i++)
		if(tables[i].start < tables[i-1].end &&
		   (tables[i].start != tables[i-1].start ||
		    tables[i].end != tables[i-1].end ||
		    tables[i].kind != tables[i-1].kind))
			goto out;
	ok = 1;
out:
	free(tables);
	return ok;
}
