/* Test-only original recursive printer for shallow spelling comparisons. */
static char*
reference_tprint(char *buf, char *end, Type *t)
{
	Decl *id;
	Typelist *tl;

	if(t == nil)
		return buf;
	if(t->kind >= Tend)
		return seprint(buf, end, "kind %d", t->kind);
	switch(t->kind){
	case Tarrow:
		buf = seprint(buf, end, "%T->%s", t->tof, t->decl->sym->name);
		break;
	case Tdot:
		buf = seprint(buf, end, "%T.%s", t->tof, t->decl->sym->name);
		break;
	case Tid:
	case Tpoly:
		buf = seprint(buf, end, "%s", t->decl->sym->name);
		break;
	case Tinst:
		buf = reference_tprint(buf, end, t->tof);
		buf = secpy(buf ,end, "[");
		for(tl = t->u.tlist; tl != nil; tl = tl->nxt){
			buf = reference_tprint(buf, end, tl->t);
			if(tl->nxt != nil)
				buf = secpy(buf, end, ", ");
		}
		buf = secpy(buf, end, "]");
		break;
	case Tint:
	case Tbig:
	case Tstring:
	case Treal:
	case Tbyte:
	case Tany:
	case Tnone:
	case Terror:
	case Tainit:
	case Talt:
	case Tcase:
	case Tcasel:
	case Tcasec:
	case Tgoto:
	case Tiface:
	case Texception:
	case Texcept:
		buf = secpy(buf, end, kindname[t->kind]);
		break;
	case Tfix:
		buf = seprint(buf, end, "%s(%v)", kindname[t->kind], t->val);
		break;
	case Tref:
		buf = secpy(buf, end, "ref ");
		buf = reference_tprint(buf, end, t->tof);
		break;
	case Tchan:
	case Tarray:
	case Tlist:
		buf = seprint(buf, end, "%s of ", kindname[t->kind]);
		buf = reference_tprint(buf, end, t->tof);
		break;
	case Tadtpick:
		buf = seprint(buf, end, "%s.%s", t->decl->dot->sym->name, t->decl->sym->name);
		break;
	case Tadt:
		if(t->decl->dot != nil && !isimpmod(t->decl->dot->sym))
			buf = seprint(buf, end, "%s->%s", t->decl->dot->sym->name, t->decl->sym->name);
		else
			buf = seprint(buf, end, "%s", t->decl->sym->name);
		if(t->polys != nil){
			buf = secpy(buf ,end, "[");
			for(id = t->polys; id != nil; id = id->next){
				if(t->u.tmap != nil)
					buf = reference_tprint(buf, end, valtmap(id->ty, t->u.tmap));
				else
					buf = seprint(buf, end, "%s", id->sym->name);
				if(id->next != nil)
					buf = secpy(buf, end, ", ");
			}
			buf = secpy(buf, end, "]");
		}
		break;
	case Tmodule:
		buf = seprint(buf, end, "%s", t->decl->sym->name);
		break;
	case Ttuple:
		buf = secpy(buf, end, "(");
		for(id = t->ids; id != nil; id = id->next){
			buf = reference_tprint(buf, end, id->ty);
			if(id->next != nil)
				buf = secpy(buf, end, ", ");
		}
		buf = secpy(buf, end, ")");
		break;
	case Tfn:
		buf = secpy(buf, end, "fn");
		if(t->polys != nil){
			buf = secpy(buf, end, "[");
			for(id = t->polys; id != nil; id = id->next){
				buf = seprint(buf, end, "%s", id->sym->name);
				if(id->next != nil)
					buf = secpy(buf, end, ", ");
			}
			buf = secpy(buf, end, "]");
		}
		buf = secpy(buf, end, "(");
		for(id = t->ids; id != nil; id = id->next){
			if(id->sym == nil)
				buf = secpy(buf, end, "nil: ");
			else
				buf = seprint(buf, end, "%s: ", id->sym->name);
			if(id->implicit)
				buf = secpy(buf, end, "self ");
			buf = reference_tprint(buf, end, id->ty);
			if(id->next != nil)
				buf = secpy(buf, end, ", ");
		}
		if(t->varargs && t->ids != nil)
			buf = secpy(buf, end, ", *");
		else if(t->varargs)
			buf = secpy(buf, end, "*");
		if(t->tof != nil && t->tof->kind != Tnone){
			buf = secpy(buf, end, "): ");
			buf = reference_tprint(buf, end, t->tof);
			break;
		}
		buf = secpy(buf, end, ")");
		break;
	default:
		yyerror("tprint: unknown type kind %d", t->kind);
		break;
	}
	return buf;
}
