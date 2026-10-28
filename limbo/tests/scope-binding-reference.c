/* Frozen ordinary global binding and label-stack behavior. */


/* Preserve the original callback and lexical cleanup order on shallow input. */
static void
reference_gbind(Node *n)
{
	Decl *d, *ids;

	for(;;){
		if(n == nil)
			return;
		if(n->op != Oseq)
			break;
		reference_gbind(n->left);
		n = n->right;
	}
	switch(n->op){
	case Oas:
	case Ocondecl:
	case Odas:
	case Oexdecl:
	case Ofunc:
	case Oimport:
	case Onothing:
	case Ovardecl:
	case Ovardecli:
		break;
	case Ofielddecl:
		bindtypes(n->decl->ty);
		break;
	case Otypedecl:
		bindtypes(n->decl->ty);
		if(n->left != nil)
			reference_gbind(n->left);
		break;
	case Opickdecl:
		reference_gbind(n->left);
		d = n->right->left->decl;
		bindtypes(d->ty);
		repushids(d->ty->ids);
		reference_gbind(n->right->right);
		/* get new ids for undefined types; propagate outwards */
		ids = popids(d->ty->ids);
		if(ids != nil)
			installids(Dundef, ids);
		break;
	case Oadtdecl:
	case Omoddecl:
		bindtypes(n->ty);
		if(n->ty->polys != nil)
			repushids(n->ty->polys);
		repushids(n->ty->ids);
		reference_gbind(n->left);
		/* get new ids for undefined types; propagate outwards */
		ids = popids(n->ty->ids);
		if(ids != nil)
			installids(Dundef, ids);
		if(n->ty->polys != nil)
			popids(n->ty->polys);
		break;
	default:
		fatal("can't deal with %O in gbind", n->op);
	}
}


/* Preserve the original callback and lexical cleanup order on shallow input. */
static void
reference_pushlabel(Node *n)
{
	Sym *s;
	int i;

	if(labdep >= maxlabdep){
		maxlabdep += MaxScope;
		labstack = reallocmem(labstack, maxlabdep * sizeof *labstack);
	}
	if(n->decl != nil){
		s = n->decl->sym;
		n->decl->refs = 0;
		for(i = 0; i < labdep; i++)
			if(labstack[i]->decl != nil && labstack[i]->decl->sym == s)
				nerror(n, "label %s duplicated on line %L", s->name, labstack[i]->decl->src.start);
	}
	labstack[labdep++] = n;
}
