/* Frozen recursive writer used only as a shallow byte-for-byte oracle. */
static void reference_sbltype(Type*, int);


/* Retain legacy declaration filtering, source callbacks and line separators. */
static void
reference_sbldecl(Decl *ids, int store)
{
	Decl *id;
	char buf[StrSize];
	int n;

	n = 0;
	for(id = ids; id != nil; id = id->next){
		if(id->store != store || !isvis(id))
			continue;
		n++;
	}
	Bprint(bsym, "%d\n", n);
	for(id = ids; id != nil; id = id->next){
		if(id->store != store || !isvis(id))
			continue;
		Bprint(bsym, "%ld:%s:%s", id->offset, id->sym->name, sblsrcconv(buf, buf+sizeof(buf), &id->src));
		reference_sbltype(id->ty, 0);
		Bprint(bsym, "\n");
	}
}


/* Retain legacy Type, function, module and adjacent-tag serialization. */
static void
reference_sbltype(Type *t, int force)
{
	Type *lastt;
	Decl *tg, *d;
	char buf[StrSize];

	if(t->kind == Tadtpick)
		t = t->decl->dot->ty;

	d = t->decl;
	if(!force && d != nil && d->ty->sbl >= 0){
		Bprint(bsym, "@%d\n", d->ty->sbl);
		return;
	}

	switch(t->kind){
	default:
		fatal("bad type %T in reference_sbltype", t);
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
		reference_sbldecl(t->ids, Darg);
		reference_sbltype(t->tof, 0);
		break;
	case Tarray:
	case Tlist:
	case Tchan:
	case Tref:
		Bprint(bsym, "%c", sbltname[t->kind]);
		if(t->kind == Tref && t->tof->kind == Tfn){
			tattr[Tany].vis = 1;
			reference_sbltype(tfnptr, 0);
			tattr[Tany].vis = 0;
		}
		else
			reference_sbltype(t->tof, 0);
		break;
	case Ttuple:
	case Texception:
		Bprint(bsym, "%c%ld.", sbltname[t->kind], t->size);
		reference_sbldecl(t->ids, Dfield);
		break;
	case Tadt:
		if(t->tags != nil)
			Bputc(bsym, sbltadtpick);
		else
			Bputc(bsym, sbltname[t->kind]);
		if(d->dot != nil && !isimpmod(d->dot->sym))
			Bprint(bsym, "%s->", d->dot->sym->name);
		Bprint(bsym, "%s %s%ld\n", d->sym->name, sblsrcconv(buf, buf+sizeof(buf), &d->src), d->ty->size);
		reference_sbldecl(t->ids, Dfield);
		if(t->tags != nil){
			Bprint(bsym, "%d\n", t->decl->tag);
			lastt = nil;
			for(tg = t->tags; tg != nil; tg = tg->next){
				Bprint(bsym, "%s:%s", tg->sym->name, sblsrcconv(buf, buf+sizeof(buf), &tg->src));
				if(lastt == tg->ty){
					Bputc(bsym, '\n');
				}else{
					Bprint(bsym, "%ld\n", tg->ty->size);
					reference_sbldecl(tg->ty->ids, Dfield);
				}
				lastt = tg->ty;
			}
		}
		break;
	case Tmodule:
		Bprint(bsym, "%c%s\n%s", sbltname[t->kind], d->sym->name, sblsrcconv(buf, buf+sizeof(buf), &d->src));
		reference_sbldecl(t->ids, Dglobal);
		break;
	}
}



/* Preserve the old section writer for valid-input differential checks. */
static void
reference_sblty(Decl **tys, int ntys)
{
	Decl *d;
	int i;

	Bprint(bsym, "%d\n", ntys);
	for(i = 0; i < ntys; i++){
		d = tys[i];
		d->ty->sbl = i;
	}
	for(i = 0; i < ntys; i++){
		d = tys[i];
		reference_sbltype(d->ty, 1);
	}
}



/* Preserve the old section writer for valid-input differential checks. */
static void
reference_sblfn(Decl **fns, int nfns)
{
	Decl *f;
	int i;

	Bprint(bsym, "%d\n", nfns);
	for(i = 0; i < nfns; i++){
		f = fns[i];
		if(ispoly(f))
			rmfnptrs(f);
		if(f->dot != nil && f->dot->ty->kind == Tadt)
			Bprint(bsym, "%ld:%s.%s\n", f->pc->pc, f->dot->sym->name, f->sym->name);
		else
			Bprint(bsym, "%ld:%s\n", f->pc->pc, f->sym->name);
		reference_sbldecl(f->ty->ids, Darg);
		reference_sbldecl(f->locals, Dlocal);
		reference_sbltype(f->ty->tof, 0);
	}
}



/* Preserve the old section writer for valid-input differential checks. */
static void
reference_sblinst(Inst *inst, long ninst)
{
	Inst *in;
	char buf[StrSize];
	int *sblblocks, i, b;
	Src src;

	Bprint(bsym, "%ld\n", ninst);
	sblblocks = allocmem(nblocks * sizeof *sblblocks);
	for(i = 0; i < nblocks; i++)
		sblblocks[i] = -1;
	src = nosrc;
	for(in = inst; in != nil; in = in->next){
		if(in->op == INOOP)
			continue;
		if(in->src.start.line < 0)
			fatal("no file specified for %I", in);
		b = sblblocks[in->block];
		if(b < 0)
			sblblocks[in->block] = b = blockid++;
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
