/* Frozen recursive shallow oracle; production constructors and installation are shared. */


/* Preserve the original action order for differential testing. */
static int
reference_recdasdecl(Node *n, int store, int *nid)
{
	Decl *d, *old;
	int ok;

	switch(n->op){
	case Otuple:
		ok = 1;
		for(n = n->left; n != nil; n = n->right)
			ok &= reference_recdasdecl(n->left, store, nid);
		return ok;
	case Oname:
		if(n->decl == nildecl){
			*nid = -1;
			return 1;
		}
		d = mkids(&n->src, n->decl->sym, nil, nil);
		installids(store, d);
		old = d->old;
		if(old != nil
		&& old->store != Dfn
		&& old->store != Dwundef
		&& old->store != Dundef)
			warn(d->src.start,  "redeclaration of %K, previously declared as %k on line %L",
				d, old, old->src.start);
		n->decl = d;
		d->refs++;
		d->das = 1;
		if(*nid >= 0)
			(*nid)++;
		return 1;
	}
	return 0;
}


/* Preserve the original action order for differential testing. */
static int
reference_recmark(Node *n, int nid)
{
	switch(n->op){
	case Otuple:
		for(n = n->left; n != nil; n = n->right)
			nid = reference_recmark(n->left, nid);
		break;
	case Oname:
		n->decl->nid = nid;
		nid = 0;
		break;
	}
	return nid;
}


/* Preserve the original action order for differential testing. */
int
reference_dasdecl(Node *n)
{
	int store, ok, nid;

	nid = 0;
	if(scope == ScopeGlobal)
		store = Dglobal;
	else
		store = Dlocal;

	ok = reference_recdasdecl(n, store, &nid);
	if(!ok)
		nerror(n, "illegal declaration expression %V", n);
	if(ok && store == Dlocal && nid > 1)
		reference_recmark(n, nid);
	return ok;
}


/* Preserve the original action order for differential testing. */
void
reference_gdasdecl(Node *n)
{
	if(n == nil)
		return;

	if(n->op == Odas){
		reference_gdasdecl(n->right);
		reference_dasdecl(n->left);
	}else{
		reference_gdasdecl(n->left);
		reference_gdasdecl(n->right);
	}
}


/* Preserve the original action order for differential testing. */
static int
reference_tupblk0(Node *n, Decl **dd)
{
	Decl *d;
	int nid;

	switch(n->op){
	case Otuple:
		for(n = n->left; n != nil; n = n->right)
			if(!reference_tupblk0(n->left, dd))
				return 0;
		return 1;
	case Oname:
		if(n->decl == nildecl)
			return 0;
		d = *dd;
		if(d != nil && d->next != n->decl)
			return 0;
		nid = n->decl->nid;
		if(d == nil && nid == 1)
			return 0;
		if(d != nil && nid != 0)
			return 0;
		*dd = n->decl;
		return 1;
	}
	return 0;
}
