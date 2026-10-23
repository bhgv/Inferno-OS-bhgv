static Node *rewrite(Node*), *simplify(Node*), *mkconst(Src*, Long);
static int inlined;
CONTROL_IMPLEMENTATION

/* Copy substituted inline bodies into bounded AST storage, just as putinline owns its copy. */
static Node*
copyinline(Node *n)
{
	Node *copy;

	if(n == nil)
		return nil;
	assert(used < 256);
	copy = &arena[used++];
	*copy = *n;
	copy->left = copyinline(n->left);
	copy->right = copyinline(n->right);
	return copy;
}

/* Bind unrelated rewriting to traversal and a controlled already-substituted inline body. */
static Node*
rewrite(Node *n)
{
	if(n == nil)
		return nil;
#ifdef HAVE_REWRITE_CONTROL
	if(n->op == Oandand || n->op == Ooror || n->op == Oif)
		return rewritecontrol(n);
#endif
	if(n->op == Ocall){
		assert(n->left->decl->store == Dfn && n->right == nil);
		inlined++;
		return simplify(copyinline(n->left->decl->init));
	}
	n->left = rewrite(n->left);
	n->right = rewrite(n->right);
	return n;
}

/* Match the production rewrite-then-fold phase boundary. */
static Node*
simplify(Node *n)
{
	return efold(rewrite(n));
}
