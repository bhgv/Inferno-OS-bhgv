/* Canonicalize an independent arithmetic result at the expression's declared width. */
static Long
narrowresult(uvlong value, Type *type)
{
	if(type == tbyte)
		return value&255;
	if(type == tint)
		return (Long)(value&0x7fffffff)-(Long)(value&0x80000000);
	return (Long)value;
}

/* Evaluate the integer subset independently, retaining the count of observable calls. */
static Long
inteval(Node *n)
{
	Long a, b;
	uvlong bits;
	int i;

	if(n->op == Oconst)
		return n->val;
	if(n->op == Oname || n->op == Ocall){
		if(n->op == Ocall)
			visits++;
		return n->val;
	}
	a = inteval(n->left);
	b = inteval(n->right);
	switch(n->op){
	case Oadd: bits = (uvlong)a+b; break;
	case Osub: bits = (uvlong)a-b; break;
	case Omul: bits = (uvlong)a*b; break;
	case Odiv: bits = b == -1 ? (uvlong)0-(uvlong)a : (uvlong)(a/b); break;
	case Omod: bits = b == -1 ? 0 : a%b; break;
	case Oand: bits = a&b; break;
	case Oor: bits = a|b; break;
	case Oxor: bits = a^b; break;
	case Olsh: bits = (uvlong)a<<b; break;
	case Orsh:
		bits = (uvlong)a>>b;
		if(a < 0 && b != 0)
			bits |= ~(uvlong)0<<(64-b);
		break;
	case Oexp:
		bits = 1;
		for(i = 0; i < b; i++)
			bits *= (uvlong)a;
		break;
	case Oeq: bits = a == b; break;
	case Oneq: bits = a != b; break;
	case Olt: bits = a < b; break;
	case Oleq: bits = a <= b; break;
	case Ogt: bits = a > b; break;
	case Ogeq: bits = a >= b; break;
	default: abort();
	}
	return narrowresult(bits, n->ty);
}

/* A typed identity must retain the value, declared type and observable evaluations. */
static void
resultcase(Type *type, int op, Long a, Long b, int leaf)
{
	Decl decl = {nil, Dlocal};
	Type *resulttype, *righttype;
	Node left, right, expression, *result;
	Long expected;
	int count;

	resulttype = isrelop[op] ? tint : type;
	righttype = op == Oexp || op == Olsh || op == Orsh ? tint : type;
	left = (Node){leaf, type, nil, nil, &decl, {3}, a, 0};
	right = (Node){Oconst, righttype, nil, nil, nil, {5}, b, 0};
	expression = (Node){op, resulttype, &left, &right, nil, {7}, 0, 0};
	visits = used = 0;
	expected = inteval(&expression);
	count = visits;
	result = efold(&expression);
	assert(result->ty == resulttype);
	visits = 0;
	assert(inteval(result) == expected && visits == count);
	checks++;
}

/* Compose big power identities with operations that must retain high result bits. */
static void
powercontext(void)
{
	Decl decl = {nil, Dlocal};
	Node variable = {Oname, tbig, nil, nil, &decl, {1}, 19, 0};
	Node exponent = {Oconst, tint, nil, nil, nil, {2}, 0, 0};
	Node power = {Oexp, tbig, &variable, &exponent, nil, {3}, 0, 0};
	Node shift = {Oconst, tint, nil, nil, nil, {4}, 40, 0};
	Node context = {Olsh, tbig, &power, &shift, nil, {5}, 0, 0};
	Node *result;

	used = 0;
	result = efold(&context);
	assert(result->op == Oconst && result->ty == tbig);
	assert(result->val == ((Long)1<<40));
	assert(result->src.start == 5);
	checks++;
}

/* Cross typed constants, variables and effectful operands with all integer identities. */
int
main(void)
{
	int ops[] = {Oadd, Osub, Omul, Odiv, Omod, Oand, Oor, Oxor,
		Olsh, Orsh, Oexp, Oeq, Oneq, Olt, Oleq, Ogt, Ogeq};
	int leaves[] = {Oname, Oconst, Ocall};
	Long values[] = {-2147483648LL, -129, -1, 0, 1, 2, 7, 127, 255, 2147483647,
		4294967296LL, -4294967297LL, (-9223372036854775807LL-1), 9223372036854775807LL};
	Type *types[] = {tbyte, tint, tbig};
	int t, op, a, b, leaf;

	sideeffect[Ocall] = 1;
	isrelop[Oeq] = isrelop[Oneq] = isrelop[Olt] = 1;
	isrelop[Oleq] = isrelop[Ogt] = isrelop[Ogeq] = 1;
	for(t = 0; t < 3; t++)for(op = 0; op < 17; op++)
	for(a = 0; a < 14; a++)for(b = 0; b < 14; b++)for(leaf = 0; leaf < 3; leaf++){
		Long lhs = narrowresult(values[a], types[t]);
		Long rhs = narrowresult(values[b], types[t]);
		int code = ops[op];
		if((code == Odiv || code == Omod) && rhs == 0)
			continue;
		if(code == Oexp && (t == 0 || values[b] < 0 || values[b] > 7))
			continue;
		if((code == Olsh || code == Orsh) && (values[b] < 0 || values[b] >= types[t]->size*8))
			continue;
		if(code == Oexp || code == Olsh || code == Orsh)
			rhs = values[b];
		resultcase(types[t], code, lhs, rhs, leaves[leaf]);
	}
	powercontext();
	printf("Compiler result types: %d value/type/effect/context cases\n", checks);
	return 0;
}
