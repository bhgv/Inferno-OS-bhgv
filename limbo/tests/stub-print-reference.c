/* Frozen recursive spelling for shallow, valid C stub layouts. */


/* Preserve the original traversal as a shallow differential oracle. */
static char*
reference_dotprint(char *buf, char *end, Decl *d, int dot)
{
	if(d->dot != nil){
		buf = reference_dotprint(buf, end, d->dot, dot);
		if(buf < end)
			*buf++ = dot;
	}
	if(d->sym == nil)
		return buf;
	return seprint(buf, end, "%s", d->sym->name);
}


/* Preserve the original traversal as a shallow differential oracle. */
static char*
reference_ctprint(char *buf, char *end, Type *t)
{
	Decl *id;
	Type *tt;
	long offset;

	if(t == nil)
		return secpy(buf, end, "void");
	switch(t->kind){
	case Tref:
		return seprint(buf, end, "%R*", t->tof);
	case Tarray:
	case Tlist:
	case Tint:
	case Tbig:
	case Tstring:
	case Treal:
	case Tbyte:
	case Tnone:
	case Tany:
	case Tchan:
	case Tmodule:
	case Tfix:
	case Tpoly:
		return seprint(buf, end, "%s", ckindname[t->kind]);
	case Tadtpick:
		return reference_ctprint(buf, end, t->decl->dot->ty);
	case Tadt:
	case Ttuple:
		if(t->decl->sym != anontupsym)
			return reference_dotprint(buf, end, t->decl, '_');
		offset = 0;
		buf = secpy(buf, end, "struct{ ");
		for(id = t->ids; id != nil; id = id->next){
			tt = id->ty;
			offset = stubalign(offset, tt->align, &buf, end);
			if(offset != id->offset)
				fatal("ctypeconv tuple bad offset");
			buf = seprint(buf, end, "%R %s; ", tt, id->sym->name);
			offset += tt->size;
		}
		offset = stubalign(offset, t->align, &buf, end);
		if(offset != t->size)
			fatal("ctypeconv tuple bad t=%T size=%ld offset=%ld", t, t->size, offset);
		return secpy(buf, end, "}");
	default:
		if(t->kind >= Tend)
			yyerror("no C equivalent for type %d", t->kind);
		else
			yyerror("no C equivalent for type %s", kindname[t->kind]);
		break;
	}
	return buf;
}
