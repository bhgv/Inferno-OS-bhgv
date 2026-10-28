#include "limbo.h"

static char sbltname[Tend] =
{
	/* Tnone */	'n',
	/* Tadt */	'a',
	/* Tadtpick */	'a',
	/* Tarray */	'A',
	/* Tbig */	'B',
	/* Tbyte */	'b',
	/* Tchan */	'C',
	/* Treal */	'f',
	/* Tfn */	'F',
	/* Tint */	'i',
	/* Tlist */	'L',
	/* Tmodule */	'm',
	/* Tref */	'R',
	/* Tstring */	's',
	/* Ttuple */	't',
	/* Texception */	't',
	/* Tfix */	'i',
	/* Tpoly */	'P',

	/* Tainit */	'?',
	/* Talt */	'?',
	/* Tany */	'N',
	/* Tarrow */	'?',
	/* Tcase */	'?',
	/* Tcasel */	'?',
	/* Tcasec */	'?',
	/* Tdot */	'?',
	/* Terror */	'?',
	/* Tgoto */	'?',
	/* Tid */	'?',
	/* Tiface */	'?',
	/* Texcept */	'?',
	/* Tinst */	'?',
};
int	sbltadtpick = 'p';

static	Sym	*sfiles;
static	Sym	*ftail;
static	int	nsfiles;
static	int	blockid;
static	int	lastf;
static	int	lastline;

static	void	sbltype(Type*, int);
static	void	sbldecl(Decl*, int);


/* Start a new module without retaining the preceding module's private file cells. */
void
sblmod(Decl *m)
{
	Sym *s, *next;

	for(s = sfiles; s != nil; s = next){
		next = s->next;
		free(s);
	}
	Bprint(bsym, "limbo .sbl 2.1\n");
	Bprint(bsym, "%s\n", m->sym->name);

	blockid = 0;
	nsfiles = 0;
	sfiles = ftail = nil;
	lastf = 0;
	lastline = 0;
}


/* Intern borrowed file names while keeping symbol-file indices representable. */
static int
sblfile(char *name)
{
	Sym *s;
	int i;

	i = 0;
	for(s = sfiles; s != nil; s = s->next){
		if(strcmp(s->name, name) == 0)
			return i;
		if(i == 0x7fffffff-1)
			fatal("too many symbol files");
		i++;
	}
	s = allocmem(sizeof(Sym));
	s->name = name;
	s->next = nil;
	if(sfiles == nil)
		sfiles = s;
	else
		ftail->next = s;
	ftail = s;
	nsfiles = i + 1;
	return i;
}


/* Keep only the final source-path component in the symbol file. */
static char *
filename(char *s)
{
	char *t;

	t = strrchr(s, '/');
	if(t != nil)
		s = t + 1;
	t = strrchr(s, '\\');
	if(t != nil)
		s = t+1;
	t = strrchr(s, ' ');
	if(t != nil)
		s = t + 1;
	return s;
}


/* Emit unique source-file names in stable first-occurrence order. */
void
sblfiles(void)
{
	Sym *s;
	int i;

	for(i = 0; i < nfiles; i++)
		files[i]->sbl = sblfile(files[i]->name);
	Bprint(bsym, "%d\n", nsfiles);
	for(s = sfiles; s != nil; s = s->next)
		Bprint(bsym, "%s\n", filename(s->name));
}


/* Encode source deltas while retaining the previous emitted file and line. */
static char*
sblsrcconv(char *buf, char *end, Src *src)
{
	Fline fl;
	File *startf, *stopf;
	char *s;
	int startl, stopl;

	s = buf;

	fl = fline(src->start.line);
	startf = fl.file;
	startl = fl.line;
	fl = fline(src->stop.line);
	stopf = fl.file;
	stopl = fl.line;
	if(lastf != startf->sbl)
		s = seprint(s, end, "%d:", startf->sbl);
	if(lastline != startl)
		s = seprint(s, end, "%d.", startl);
	s = seprint(s, end, "%d,", src->start.pos);
	if(startf->sbl != stopf->sbl)
		s = seprint(s, end, "%d:", stopf->sbl);
	if(startl != stopl)
		s = seprint(s, end, "%d.", stopl);
	seprint(s, end, "%d ", src->stop.pos);
	lastf = stopf->sbl;
	lastline = stopl;
	return buf;
}

#define isnilsrc(s)	((s)->start.line == 0 && (s)->stop.line == 0 && (s)->start.pos == 0 && (s)->stop.pos == 0) // No source span is attached.
#define isnilstopsrc(s)	((s)->stop.line == 0 && (s)->stop.pos == 0) // The source end still needs reconstruction.


/* Validate instruction metadata before indexing block storage or repairing spans. */
void
sblinst(Inst *inst, long ninst)
{
	Inst *in;
	char buf[StrSize];
	int *sblblocks, i, b, nextblock;
	long emitted;
	Src src;

	if(ninst < 0 || nblocks < 0 || blockid < 0 || (ulong)nblocks > 0x7fffffff/sizeof(*sblblocks))
		fatal("symbol instruction table too large");
	/* Validate every emitted block and source repair before publishing the header. */
	emitted = 0;
	for(in = inst; in != nil; in = in->next){
		if(in->op == INOOP)
			continue;
		if(emitted == ninst)
			fatal("symbol instruction count mismatch");
		emitted++;
		if(in->block < 0 || in->block >= nblocks)
			fatal("invalid symbol instruction block");
		if(in->src.start.line < 0)
			fatal("no file specified for %I", in);
		if(!isnilsrc(&in->src) && isnilstopsrc(&in->src) &&
		   in->src.start.pos == 0x7fffffff)
			fatal("symbol source position too large");
	}
	if(emitted != ninst)
		fatal("symbol instruction count mismatch");
	sblblocks = nblocks == 0 ? nil : allocmem((ulong)nblocks*sizeof(*sblblocks));
	for(i = 0; i < nblocks; i++)
		sblblocks[i] = -1;
	/* Stage all new block numbers before changing global state or output. */
	nextblock = blockid;
	for(in = inst; in != nil; in = in->next){
		if(in->op == INOOP || sblblocks[in->block] >= 0)
			continue;
		if(nextblock == 0x7fffffff)
			fatal("too many symbol blocks");
		sblblocks[in->block] = nextblock++;
	}
	blockid = nextblock;
	Bprint(bsym, "%ld\n", ninst);
	src = nosrc;
	for(in = inst; in != nil; in = in->next){
		if(in->op == INOOP)
			continue;
		b = sblblocks[in->block];
		if(isnilsrc(&in->src))
			in->src = src;
		else if(isnilstopsrc(&in->src)){	/* how does this happen ? */
			in->src.stop = in->src.start;
			in->src.stop.pos++;
		}
		Bprint(bsym, "%s%d\n", sblsrcconv(buf, buf+sizeof(buf), &in->src), b);
		src = in->src;
	}
	free(sblblocks);
}


/* Validate declaration-vector dimensions before walking or publishing its entries. */
static void
sblvector(Decl **ids, int count)
{
	int i;
	ulong bytes;

	if(count < 0 || (ulong)count > 0x7fffffff/sizeof(*ids))
		fatal("symbol declaration vector too large");
	bytes = (ulong)count*sizeof(*ids);
	if(count != 0 && (ids == nil || (uintptr)ids > (uintptr)-1-bytes))
		fatal("invalid symbol declaration vector");
	for(i = 0; i < count; i++)
		if(ids[i] == nil || ids[i]->ty == nil)
			fatal("invalid symbol declaration");
}


/* Publish nominal Type indices before emitting possibly recursive references. */
void
sblty(Decl **tys, int ntys)
{
	Decl *d;
	int i;

	sblvector(tys, ntys);
	Bprint(bsym, "%d\n", ntys);
	for(i = 0; i < ntys; i++){
		d = tys[i];
		d->ty->sbl = i;
	}
	for(i = 0; i < ntys; i++){
		d = tys[i];
		sbltype(d->ty, 1);
	}
}


/* Emit function records only after validating the complete declaration vector. */
void
sblfn(Decl **fns, int nfns)
{
	Decl *f;
	int i;

	sblvector(fns, nfns);
	Bprint(bsym, "%d\n", nfns);
	for(i = 0; i < nfns; i++){
		f = fns[i];
		if(ispoly(f))
			rmfnptrs(f);
		if(f->dot != nil && f->dot->ty->kind == Tadt)
			Bprint(bsym, "%ld:%s.%s\n", f->pc->pc, f->dot->sym->name, f->sym->name);
		else
			Bprint(bsym, "%ld:%s\n", f->pc->pc, f->sym->name);
		sbldecl(f->ty->ids, Darg);
		sbldecl(f->locals, Dlocal);
		sbltype(f->ty->tof, 0);
	}
}


/* Write the visible module-global declaration section. */
void
sblvar(Decl *vars)
{
	sbldecl(vars, Dglobal);
}


/* Apply the existing visibility policy before counting or emitting declarations. */
static int
isvis(Decl *id)
{
	if(!tattr[id->ty->kind].vis
	|| id->sym == nil
	|| id->sym->name == nil		/*????*/
	|| id->sym->name[0] == '.')
		return 0;
	if(id->ty == tstring && id->init != nil && id->init->op == Oconst)
		return 0;
	if(id->src.start.line < 0 || id->src.stop.line < 0)
		return 0;
	return 1;
}

/* Symbol-output continuations preserve declaration, Type and tag ordering. */
typedef struct SblTask SblTask;
typedef struct SblPrint SblPrint;

enum
{
	SblType, // Emit one Type, resolving published nominal references first.
	SblDecls, // Count visible declarations before emitting their records.
	SblNext, // Resume a visible-declaration list after its nested Type.
	SblNewline, // Terminate a declaration after all nested output.
	SblTags, // Emit the tag count after the ADT's ordinary fields.
	SblTagNext, // Resume tag records, suppressing duplicate adjacent layouts.
	SblRestore, // Restore the caller's temporary Tany visibility.
};

struct SblTask
{
	int kind; // Continuation action selected by the symbol writer.
	int value; // Force flag, declaration store or saved visibility.
	Type *type; // Borrowed Type or previous tag layout.
	Decl *ids; // Borrowed declaration or tag cursor.
};

struct SblPrint
{
	SblTask *stack; // Private pending actions, initially backed by local storage.
	SblTask *local; // Original local buffer, never released through free.
	int depth; // Number of pending actions.
	int capacity; // Allocated action capacity within compiler byte limits.
};


/* Grow private continuations before publishing another pending action. */
static void
sblpush(SblPrint *p, SblTask task)
{
	SblTask *larger;

	if(p->depth == p->capacity){
		if((ulong)p->capacity > 0x7fffffff/(2*sizeof(*larger)))
			fatal("symbol Type too deep");
		larger = allocmem((ulong)p->capacity*2*sizeof(*larger));
		memmove(larger, p->stack, (ulong)p->depth*sizeof(*larger));
		if(p->stack != p->local)
			free(p->stack);
		p->stack = larger;
		p->capacity *= 2;
	}
	p->stack[p->depth++] = task;
}


/* Emit mutually nested Types/declarations without consuming the compiler stack.
 * Continuations retain exact legacy byte order and source-delta callback order. */
static void
sblprint(Type *type, Decl *ids, int value, int kind)
{
	SblTask local[64], task, next;
	SblPrint p;
	Type *t;
	Decl *d, *id;
	char buf[StrSize];
	int n;

	p.stack = p.local = local;
	p.depth = 0;
	p.capacity = nelem(local);
	task.kind = kind;
	task.value = value;
	task.type = type;
	task.ids = ids;
	for(;;){
		switch(task.kind){
		case SblNewline:
			Bputc(bsym, '\n');
			break;
		case SblRestore:
			tattr[Tany].vis = task.value;
			break;
		case SblDecls:
			n = 0;
			for(id = task.ids; id != nil; id = id->next){
				if(id->store != task.value || !isvis(id))
					continue;
				if(n == 0x7fffffff)
					fatal("too many symbol declarations");
				n++;
			}
			Bprint(bsym, "%d\n", n);
			task.kind = SblNext;
			continue;
		case SblNext:
			id = task.ids;
			while(id != nil && (id->store != task.value || !isvis(id)))
				id = id->next;
			if(id == nil)
				break;
			Bprint(bsym, "%ld:%s:%s", id->offset, id->sym->name,
				sblsrcconv(buf, buf+sizeof(buf), &id->src));
			next = task;
			next.ids = id->next;
			sblpush(&p, next);
			next.kind = SblNewline;
			sblpush(&p, next);
			task.kind = SblType;
			task.type = id->ty;
			task.value = 0;
			continue;
		case SblTags:
			Bprint(bsym, "%d\n", task.type->decl->tag);
			task.ids = task.type->tags;
			task.type = nil;
			task.kind = SblTagNext;
			continue;
		case SblTagNext:
			id = task.ids;
			if(id == nil)
				break;
			Bprint(bsym, "%s:%s", id->sym->name,
				sblsrcconv(buf, buf+sizeof(buf), &id->src));
			next = task;
			next.ids = id->next;
			next.type = id->ty;
			if(task.type == id->ty){
				Bputc(bsym, '\n');
				task = next;
				continue;
			}
			Bprint(bsym, "%ld\n", id->ty->size);
			sblpush(&p, next);
			task.kind = SblDecls;
			task.ids = id->ty->ids;
			task.value = Dfield;
			continue;
		case SblType:
			t = task.type;
			if(t->kind == Tadtpick)
				t = t->decl->dot->ty;
			d = t->decl;
			if(!task.value && d != nil && d->ty->sbl >= 0){
				Bprint(bsym, "@%d\n", d->ty->sbl);
				break;
			}
			switch(t->kind){
			default:
				fatal("bad type %T in sbltype", t);
				break;
			case Tnone:
			case Tany:
			case Tint:
			case Tbig:
			case Tbyte:
			case Treal:
			case Tstring:
			case Tfix:
			case Tpoly:
				Bprint(bsym, "%c", sbltname[t->kind]);
				break;
			case Tfn:
				Bprint(bsym, "%c", sbltname[t->kind]);
				next = task;
				next.type = t->tof;
				next.value = 0;
				sblpush(&p, next);
				task.kind = SblDecls;
				task.ids = t->ids;
				task.value = Darg;
				continue;
			case Tarray:
			case Tlist:
			case Tchan:
			case Tref:
				Bprint(bsym, "%c", sbltname[t->kind]);
				if(t->kind == Tref && t->tof->kind == Tfn){
					next = task;
					next.kind = SblRestore;
					next.value = tattr[Tany].vis;
					sblpush(&p, next);
					tattr[Tany].vis = 1;
					task.type = tfnptr;
				}else
					task.type = t->tof;
				task.value = 0;
				continue;
			case Ttuple:
			case Texception:
				Bprint(bsym, "%c%ld.", sbltname[t->kind], t->size);
				task.kind = SblDecls;
				task.ids = t->ids;
				task.value = Dfield;
				continue;
			case Tadt:
				Bputc(bsym, t->tags != nil ? sbltadtpick : sbltname[t->kind]);
				if(d->dot != nil && !isimpmod(d->dot->sym))
					Bprint(bsym, "%s->", d->dot->sym->name);
				Bprint(bsym, "%s %s%ld\n", d->sym->name,
					sblsrcconv(buf, buf+sizeof(buf), &d->src), d->ty->size);
				if(t->tags != nil){
					next = task;
					next.kind = SblTags;
					next.type = t;
					sblpush(&p, next);
				}
				task.kind = SblDecls;
				task.ids = t->ids;
				task.value = Dfield;
				continue;
			case Tmodule:
				Bprint(bsym, "%c%s\n%s", sbltname[t->kind], d->sym->name,
					sblsrcconv(buf, buf+sizeof(buf), &d->src));
				task.kind = SblDecls;
				task.ids = t->ids;
				task.value = Dglobal;
				continue;
			}
			break;
		}
		if(p.depth == 0)
			break;
		task = p.stack[--p.depth];
	}
	if(p.stack != p.local)
		free(p.stack);
}


/* Emit a declaration section through the same bounded symbol traversal. */
static void
sbldecl(Decl *ids, int store)
{
	sblprint(nil, ids, store, SblDecls);
}


/* Emit a Type without recursive C frames for nested fields or return Types. */
static void
sbltype(Type *t, int force)
{
	sblprint(t, nil, force, SblType);
}
