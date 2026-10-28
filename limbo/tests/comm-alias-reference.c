/* Frozen shallow oracle for communication callbacks and alias order. */


/* Retain the historical traversal and callback sequence for shallow input. */
static Node*
reference_rewritecomm(Node *n, Node *comm, Node *tmp, Node *slot)
{
	Node *adr;
	Inst *p;

	if(n == nil)
		return nil;
	adr = nil;
	if(n == comm){
		if(comm->op == Osnd && sumark(n->right)->addable < Rcant)
			adr = n->right;
		else{
			adr = talloc(tmp, n->ty, nil);
			tmp->src = n->src;
			if(comm->op == Osnd){
				ecom(&n->right->src, tmp, n->right);
				tfreenow();
			}
			else
				trelease(tmp);
		}
	}
	if(n->right == comm && n->op == Oas && comm->op == Orcv
	&& sumark(n->left)->addable < Rcant && (n->left->op != Oname || n->left->decl != nildecl))
		adr = n->left;
	if(adr != nil){
		p = genrawop(&comm->left->src, ILEA, adr, nil, slot);
		p->m.offset = adr->ty->size;	/* for optimizer */
		if(comm->op == Osnd)
			p->m.reg = 1;	/* for optimizer */
		return adr;
	}
	n->left = reference_rewritecomm(n->left, comm, tmp, slot);
	n->right = reference_rewritecomm(n->right, comm, tmp, slot);
	return n;
}


/* Retain the historical traversal and callback sequence for shallow input. */
static int
reference_tupaliased(Node *n, Node *e)
{
	for(;;){
		if(e == nil)
			return 0;
		if(e->op == Oname && e->decl == n->decl)
			return 1;
		if(reference_tupaliased(n, e->left))
			return 1;
		e = e->right;
	}
}


/* Retain the historical traversal and callback sequence for shallow input. */
static int
reference_tupsaliased(Node *n, Node *e)
{
	for(;;){
		if(n == nil)
			return 0;
		if(n->op == Oname && reference_tupaliased(n, e))
			return 1;
		if(reference_tupsaliased(n->left, e))
			return 1;
		n = n->right;
	}
}
