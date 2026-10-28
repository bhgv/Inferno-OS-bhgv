#include "limbo.h"

/*
 * Fold integer powers with defined wraparound. Negative powers of integers
 * other than +/-1 truncate to zero; foldc diagnoses zero to a negative power.
 */
static vlong
ipow(vlong x, int n)
{
	uvlong base, result;
	uint exponent;

	if(n < 0){
		if(x == 1)
			return 1;
		if(x == -1)
			return n&1 ? -1 : 1;
		return 0;
	}
	base = x;
	exponent = n;
	result = 1;
	for(;;){
		if(exponent&1)
			result *= base;
		if((exponent >>= 1) == 0)
			break;
		base *= base;
	}
	return result;
}

/*
 * Use an unsigned exponent magnitude so INT_MIN terminates normally.
 * Invert the base first to avoid overflowing before a small reciprocal result.
 */
double
rpow(double x, int n)
{
	uint exponent;
	double result;

	exponent = n;
	if(n < 0){
		exponent = 0-exponent;
		x = 1/x;
	}
	result = 1;
	for(;;){
		if(exponent&1)
			result *= x;
		if((exponent >>= 1) == 0)
			break;
		x *= x;
	}
	return result;
}

double
fix2real(Long v, Type *f)
{
	return (double)v * scale(f);
}

int
istuple(Node *n)
{
	Decl *d;

	switch(n->op){
	case Otuple:
		return 1;
	case Oname:
		d = n->decl;
		if(d->importid != nil)
			d = d->importid;
		return d->store == Dconst && (n->ty->kind == Ttuple || n->ty->kind == Tadt);
	case Odot:
		return 0;	/* istuple(n->left); */
	}
	return 0;
}

static Node*
tuplemem(Node *n, Decl *d)
{
	Type *ty;
	Decl *ids;

	ty = n->ty;
	n = n->left;
	for(ids = ty->ids; ids != nil; ids = ids->next){
		if(ids->sym == d->sym)
			break;
		else
			n = n->right;
	}
	if(n == nil)
		fatal("tuplemem cannot cope !\n");
	return n->left;
}

int
varcom(Decl *v)
{
	Node *n, tn;

	n = v->init;
	n = fold(n);
	v->init = n;
	if(debug['v'])
		print("variable '%D' val %V\n", v, n);
	if(n == nil)
		return 1;

	tn = znode;
	tn.op = Oname;
	tn.decl = v;
	tn.src = v->src;
	tn.ty = v->ty;
	return initable(&tn, n, 0);
}

int
initable(Node *v, Node *n, int allocdep)
{
	Node *e;

	switch(n->ty->kind){
	case Tiface:
	case Tgoto:
	case Tcase:
	case Tcasel:
	case Tcasec:
	case Talt:
	case Texcept:
		return 1;
	case Tint:
	case Tbig:
	case Tbyte:
	case Treal:
	case Tstring:
	case Tfix:
		if(n->op != Oconst)
			break;
		return 1;
	case Tadt:
	case Tadtpick:
	case Ttuple:
		if(n->op == Otuple)
			n = n->left;
		else if(n->op == Ocall)
			n = n->right;
		else
			break;
		for(; n != nil; n = n->right)
			if(!initable(v, n->left, allocdep))
				return 0;
		return 1;
	case Tarray:
		if(n->op != Oarray)
			break;
		if(allocdep >= DADEPTH){
			nerror(v, "%Vs initializer has arrays nested more than %d deep", v, allocdep);
			return 0;
		}
		allocdep++;
		usedesc(mktdesc(n->ty->tof));
		if(n->left->op != Oconst){
			nerror(v, "%Vs size is not a constant", v);
			return 0;
		}
		for(e = n->right; e != nil; e = e->right)
			if(!initable(v, e->left->right, allocdep))
				return 0;
		return 1;
	case Tany:
		return 1;
	case Tref:
	case Tlist:
	case Tpoly:
	default:
		nerror(v, "can't initialize %Q", v);
		return 0;
	}
	nerror(v, "%Vs initializer, %V, is not a constant expression", v, n);
	return 0;
}

/*
 * merge together two sorted lists, yielding a sorted list
 */
static Node*
elemmerge(Node *e, Node *f)
{
	Node rock, *r;

	r = &rock;
	while(e != nil && f != nil){
		if(e->left->left->val <= f->left->left->val){
			r->right = e;
			e = e->right;
		}else{
			r->right = f;
			f = f->right;
		}
		r = r->right;
	}
	if(e != nil)
		r->right = e;
	else
		r->right = f;
	return rock.right;
}

/*
 * recursively split lists and remerge them after they are sorted
 */
static Node*
recelemsort(Node *e, int n)
{
	Node *r, *ee;
	int i, m;

	if(n <= 1)
		return e;
	m = n / 2 - 1;
	ee = e;
	for(i = 0; i < m; i++)
		ee = ee->right;
	r = ee->right;
	ee->right = nil;
	return elemmerge(recelemsort(e, n / 2),
			recelemsort(r, (n + 1) / 2));
}

/*
 * sort the elems by index; wild card is first
 */
Node*
elemsort(Node *e)
{
	Node *ee;
	int n;

	n = 0;
	for(ee = e; ee != nil; ee = ee->right){
		if(ee->left->left->op == Owild)
			ee->left->left->val = -1;
		n++;
	}
	return recelemsort(e, n);
}


/* Compare in left-to-right order without depth-proportional host recursion.
 * Constants remain terminal and pointer-identical nodes remain immediately equal.
 * Pending node pairs are borrowed; all private work storage is released on return. */
int
sametree(Node *n1, Node *n2)
{
	enum { InlineNodes = 64 }; // Pointer slots for pending right-child pairs.
	Node *inlinework[InlineNodes], **work, **next;
	ulong used, capacity;
	int same;

	work = inlinework;
	used = 0;
	capacity = InlineNodes;
	same = 1;
	for(;;){
		for(;;){
			if(n1 == n2)
				break;
			if(n1 == nil || n2 == nil || n1->op != n2->op || n1->ty != n2->ty){
				same = 0;
				goto done;
			}
			if(n1->op == Oconst){
				switch(n1->ty->kind){
				case Tbig:
				case Tbyte:
				case Tint:
					same = n1->val == n2->val;
					break;
				case Treal:
					same = n1->rval == n2->rval;
					break;
				case Tfix:
					same = n1->val == n2->val && tequal(n1->ty, n2->ty);
					break;
				case Tstring:
					same = n1->decl->sym == n2->decl->sym;
					break;
				default:
					same = 0;
					break;
				}
				if(!same)
					goto done;
				break;
			}
			if(n1->decl != n2->decl){
				same = 0;
				goto done;
			}
			if(n1->right != n2->right){
				if(used == capacity){
					if(capacity > (ulong)-1/2/sizeof(*work))
						fatal("tree comparison storage overflow");
					next = malloc(2*capacity*sizeof(*work));
					if(next == nil)
						fatal("out of memory comparing trees");
					memmove(next, work, used*sizeof(*work));
					if(work != inlinework)
						free(work);
					work = next;
					capacity *= 2;
				}
				work[used++] = n1->right;
				work[used++] = n2->right;
			}
			n1 = n1->left;
			n2 = n2->left;
		}
		if(used == 0)
			break;
		n2 = work[--used];
		n1 = work[--used];
	}
done:
	if(work != inlinework)
		free(work);
	return same;
}


/* Count uses with bounded C stack for either association and mixed trees.
 * Nodes/Decl are borrowed and never changed; Oname remains terminal even if it
 * has populated child metadata. Temporary work storage is released on success.
 * Allocation, storage-size or signed-count exhaustion is a compiler fatal. */
int
occurs(Decl *d, Node *n)
{
	enum { InlineNodes = 64 };
	Node *inlinework[InlineNodes], **work, **next;
	ulong used, capacity;
	int count;

	work = inlinework;
	used = 0;
	capacity = InlineNodes;
	count = 0;
	for(;;){
		while(n != nil){
			if(n->op == Oname){
				if(d == n->decl){
					if(count == 0x7fffffff)
						fatal("declaration use count overflow");
					count++;
				}
				break;
			}
			if(n->right != nil){
				if(used == capacity){
					if(capacity > (ulong)-1/2/sizeof(*work))
						fatal("declaration traversal storage overflow");
					next = malloc(2*capacity*sizeof(*work));
					if(next == nil)
						fatal("out of memory counting declaration uses");
					memmove(next, work, used*sizeof(*work));
					if(work != inlinework)
						free(work);
					work = next;
					capacity *= 2;
				}
				work[used++] = n->right;
			}
			n = n->left;
		}
		if(used == 0)
			break;
		n = work[--used];
	}
	if(work != inlinework)
		free(work);
	return count;
}


/* Preserve IEEE values and boolean normalization in same-tree identities. */
Node*
folds(Node *n)
{
	if(n->left->ty == treal)
		return n;
	if(hasside(n, 1))
		return n;
	switch(n->op){
	case Oeq:
	case Oleq:
	case Ogeq:
		n->val = 1;
		break;
	case Osub:
		n->val = 0;
		n->rval = 0.0;
		break;
	case Oxor:
	case Oneq:
	case Olt:
	case Ogt:
		n->val = 0;
		break;
	case Oand:
	case Oor:
		return n->left;
	case Oandand:
	case Ooror:
		if(isrelop[n->left->op])
			return n->left;
		return n;
	default:
		return n;
	}
	n->op = Oconst;
	n->left = n->right = nil;
	n->decl = nil;
	return n;
}

/*
 * constant folding for typechecked expressions,
 */
Node*
fold(Node *n)
{
	if(n == nil)
		return nil;
	if(debug['F'])
		print("fold %n\n", n);
	n = efold(n);
	if(debug['F'])
		print("folded %n\n", n);
	return n;
}

/* Count all encoded characters, including NULs inside a length-delimited constant. */
static Long
foldstringlen(Sym *symbol)
{
	char *p, *end;
	Long length;

	p = symbol->name;
	end = p+symbol->len;
	length = 0;
	while(p < end){
		length += utflen(p);
		p += strlen(p);
		if(p < end){
			length++;
			p++;
		}
	}
	return length;
}

/*
 * Fold typed expressions without reassociating inexact real/fixed arithmetic
 * or discarding operand evaluations that can raise run-time errors.
 */
Node*
efold(Node *n)
{
	Decl *d;
	Node *left, *right;

	if(n == nil)
		return nil;

	left = n->left;
	right = n->right;
	switch(n->op){
	case Oname:
		d = n->decl;
		if(d->importid != nil)
			d = d->importid;
		if(d->store != Dconst){
			if(d->store == Dtag){
				n->op = Oconst;
				n->ty = tint;
				n->val = d->tag;
			}
			break;
		}
		switch(n->ty->kind){
		case Tbig:
			n->op = Oconst;
			n->val = d->init->val;
			break;
		case Tbyte:
			n->op = Oconst;
			n->val = d->init->val & 0xff;
			break;
		case Tint:
		case Tfix:
			n->op = Oconst;
			n->val = d->init->val;
			break;
		case Treal:
			n->op = Oconst;
			n->rval = d->init->rval;
			break;
		case Tstring:
			n->op = Oconst;
			n->decl = d->init->decl;
			break;
		case Ttuple:
			*n = *d->init;
			break;
		case Tadt:
			*n = *d->init;
			n = rewrite(n);	/* was call */
			break;
		case Texception:
			if(!n->ty->cons)
				fatal("non-const exception type in efold");
			n->op = Oconst;
			break;
		default:
			fatal("unknown const type %T in efold", n->ty);
			break;
		}
		break;
	case Oadd:
		left = efold(left);
		right = efold(right);
		n->left = left;
		n->right = right;
		if(n->ty == tstring && right->op == Oconst){
			if(left->op == Oconst)
				n = mksconst(&n->src, stringcat(left->decl->sym, right->decl->sym));
			else if(left->op == Oadd && left->ty == tstring && left->right->op == Oconst){
				left->right = mksconst(&n->src, stringcat(left->right->decl->sym, right->decl->sym));
				n = left;
			}
		}
		break;
	case Olen:
		left = efold(left);
		n->left = left;
		if(left->ty == tstring && left->op == Oconst)
			n = mkconst(&n->src, foldstringlen(left->decl->sym));
		break;
	case Oslice:
		if(right->left->op == Onothing)
			right->left = mkconst(&right->left->src, 0);
		n->left = efold(left);
		n->right = efold(right);
		break;
	case Oinds:
		n->left = left = efold(left);
		n->right = right = efold(right);
		if(right->op == Oconst && left->op == Oconst){
			;
		}
		break;
	case Ocast:
		n->op = Ocast;
		left = efold(left);
		n->left = left;
		if(n->ty == left->ty || n->ty->kind == Tfix && tequal(n->ty, left->ty))
			return left;
		if(left->op == Oconst)
			return foldcast(n, left);
		break;
	case Odot:
	case Omdot:
		/*
		 * what about side effects from left?
		 */
		d = right->decl;
		switch(d->store){
		case Dconst:
		case Dtag:
		case Dtype:
			/*
			 * set it up as a name and let that case do the hard work
			 */
			n->op = Oname;
			n->decl = d;
			n->left = nil;
			n->right = nil;
			return efold(n);
		}
		n->left = efold(left);
		if(n->left->op == Otuple)
			n = tuplemem(n->left, d);
		else
			n->right = efold(right);
		break;
	case Otagof:
		if(n->decl != nil){
			n->op = Oconst;
			n->left = nil;
			n->right = nil;
			n->val = n->decl->tag;			
			return efold(n);
		}
		n->left = efold(left);
		break;
	case Oandand:
	case Ooror:
		n->left = left = efold(left);
		/* A discarded operand must not trigger constant-expression diagnostics. */
		if(left->op == Oconst && left->ty == tint &&
		   ((n->op == Oandand && left->val == 0) ||
		    (n->op == Ooror && left->val != 0)))
			return mkconst(&n->src, n->op == Ooror);
		n->right = efold(right);
		break;
	case Oif:
		n->left = left = efold(left);
		/* Inlining can make the condition constant before either arm is folded. */
		if(left->op == Oconst)
			return efold(left->val ? right->left : right->right);
		n->right = efold(right);
		break;
	default:
		n->left = efold(left);
		n->right = efold(right);
		break;
	}

	left = n->left;
	right = n->right;
	if(left == nil)
		return n;

	if(right == nil){
		if(left->op == Oconst){
			if(left->ty == tint || left->ty == tbyte || left->ty == tbig)
				return foldc(n);
			if(left->ty == treal)
				return foldr(n);
		}
		return n;
	}

	if(left->op == Oconst){
		switch(n->op){
		case Ooror:
			if(left->ty == tint || left->ty == tbyte || left->ty == tbig){
				if(left->val == 0){
					n = mkbin(Oneq, right, mkconst(&right->src, 0));
					n->ty = right->ty;
					n->left->ty = right->ty;
					return efold(n);
				}
				left->val = 1;
				return left;
			}
			break;
		case Oandand:
			if(left->ty == tint || left->ty == tbyte || left->ty == tbig){
				if(left->val == 0)
					return left;
				n = mkbin(Oneq, right, mkconst(&right->src, 0));
				n->ty = right->ty;
				n->left->ty = right->ty;
				return efold(n);
			}
			break;
		}
	}
	if(left->op == Oconst && right->op != Oconst
	&& opcommute[n->op]
	&& n->ty != tstring){
		n->op = opcommute[n->op];
		n->left = right;
		n->right = left;
		left = right;
		right = n->right;
	}
	if(right->op == Oconst && left->op == n->op && left->right->op == Oconst
	&& (n->op == Oadd || n->op == Omul || n->op == Oor || n->op == Oxor || n->op == Oand)
	&& (n->ty == tint || n->ty == tbyte || n->ty == tbig)){
		n->left = left->left;
		left->left = right;
		right = efold(left);
		n->right = right;
		left = n->left;
	}
	if(right->op == Oconst){
		if(n->op == Oexp && left->ty == treal){
			if(left->op == Oconst)
				return foldr(n);
			return n;
		}
		if(right->ty == tint || right->ty == tbyte || left->ty == tbig){
			if(left->op == Oconst)
				return foldc(n);
			return foldvc(n);
		}
		if(right->ty == treal && left->op == Oconst)
			return foldr(n);
	}
	if(sametree(left, right))
		return folds(n);
	return n;
}


/* Find observable effects, including traps in strict mode, in preorder.
 * Borrow all nodes unchanged; use checked heap storage for pending right edges. */
int
hasside(Node *n, int strict)
{
	enum { InlineNodes = 64 }; // Pending branches kept on the fixed host stack.
	Node *inlinework[InlineNodes], **work, **next;
	ulong used, capacity;
	int result;

	work = inlinework;
	used = 0;
	capacity = InlineNodes;
	result = 0;
	for(;;){
		while(n != nil){
			if(sideeffect[n->op] && (strict || n->op != Oadr && n->op != Oind)){
				result = 1;
				goto done;
			}
			if(n->right != nil){
				if(used == capacity){
					if(capacity > (ulong)-1/2/sizeof(*work))
						fatal("node traversal storage overflow");
					next = malloc(2*capacity*sizeof(*work));
					if(next == nil)
						fatal("out of memory traversing nodes");
					memmove(next, work, used*sizeof(*work));
					if(work != inlinework)
						free(work);
					work = next;
					capacity *= 2;
				}
				work[used++] = n->right;
			}
			n = n->left;
		}
		if(used == 0)
			break;
		n = work[--used];
	}
done:
	if(work != inlinework)
		free(work);
	return result;
}


/* Find calls or spawns without growing the host stack with AST depth.
 * Borrow all nodes unchanged; use checked heap storage for pending right edges. */
int
hascall(Node *n)
{
	enum { InlineNodes = 64 }; // Pending branches kept on the fixed host stack.
	Node *inlinework[InlineNodes], **work, **next;
	ulong used, capacity;
	int result;

	work = inlinework;
	used = 0;
	capacity = InlineNodes;
	result = 0;
	for(;;){
		while(n != nil){
			if(n->op == Ocall || n->op == Ospawn){
				result = 1;
				goto done;
			}
			if(n->right != nil){
				if(used == capacity){
					if(capacity > (ulong)-1/2/sizeof(*work))
						fatal("node traversal storage overflow");
					next = malloc(2*capacity*sizeof(*work));
					if(next == nil)
						fatal("out of memory traversing nodes");
					memmove(next, work, used*sizeof(*work));
					if(work != inlinework)
						free(work);
					work = next;
					capacity *= 2;
				}
				work[used++] = n->right;
			}
			n = n->left;
		}
		if(used == 0)
			break;
		n = work[--used];
	}
done:
	if(work != inlinework)
		free(work);
	return result;
}


/* Find consumed expressions, preserving call and nothing exceptions.
 * Borrow all nodes unchanged; use checked heap storage for pending right edges. */
int
hasasgns(Node *n)
{
	enum { InlineNodes = 64 }; // Pending branches kept on the fixed host stack.
	Node *inlinework[InlineNodes], **work, **next;
	ulong used, capacity;
	int result;

	work = inlinework;
	used = 0;
	capacity = InlineNodes;
	result = 0;
	for(;;){
		while(n != nil){
			if(n->op != Ocall && isused[n->op] && n->op != Onothing){
				result = 1;
				goto done;
			}
			if(n->right != nil){
				if(used == capacity){
					if(capacity > (ulong)-1/2/sizeof(*work))
						fatal("node traversal storage overflow");
					next = malloc(2*capacity*sizeof(*work));
					if(next == nil)
						fatal("out of memory traversing nodes");
					memmove(next, work, used*sizeof(*work));
					if(work != inlinework)
						free(work);
					work = next;
					capacity *= 2;
				}
				work[used++] = n->right;
			}
			n = n->left;
		}
		if(used == 0)
			break;
		n = work[--used];
	}
done:
	if(work != inlinework)
		free(work);
	return result;
}


/* Count tree edges, including repeated DAG visits, with checked arithmetic.
 * Borrow all nodes unchanged; use checked heap storage for pending right edges. */
int
nodes(Node *n)
{
	enum { InlineNodes = 64 }; // Pending branches kept on the fixed host stack.
	Node *inlinework[InlineNodes], **work, **next;
	ulong used, capacity;
	int result;

	work = inlinework;
	used = 0;
	capacity = InlineNodes;
	result = 0;
	for(;;){
		while(n != nil){
			if(result == 0x7fffffff)
				fatal("node count overflow");
			result++;
			if(n->right != nil){
				if(used == capacity){
					if(capacity > (ulong)-1/2/sizeof(*work))
						fatal("node traversal storage overflow");
					next = malloc(2*capacity*sizeof(*work));
					if(next == nil)
						fatal("out of memory traversing nodes");
					memmove(next, work, used*sizeof(*work));
					if(work != inlinework)
						free(work);
					work = next;
					capacity *= 2;
				}
				work[used++] = n->right;
			}
			n = n->left;
		}
		if(used == 0)
			break;
		n = work[--used];
	}
	if(work != inlinework)
		free(work);
	return result;
}


/* Canonicalize a Limbo int using widened arithmetic instead of signed shifts. */
static Long
foldint32(Long value)
{
	uvlong bits;

	bits = (uvlong)value;
	return (Long)(bits & 0x7fffffff)-(Long)(bits & 0x80000000);
}

/* Leave exceptional or out-of-range real conversions for ordinary execution. */
static int
foldrealint(Real value, int kind, Long *result)
{
	if(!(value >= -9223372036854775808.0 && value < 9223372036854775808.0))
		return 0;
	/* Byte conversions use a representable Dis int before narrowing. */
	if(kind != Tbig && (value <= -2147483649.0 || value >= 2147483648.0))
		return 0;
	*result = (Long)value;
	return 1;
}

/* Match the real-to-fixed instruction's reciprocal multiply before rounding. */
static int
foldrealtofixed(Real value, Type *type, Long *result)
{
	Real converted;

	converted = value*(1.0/scale(type));
	converted = converted < 0 ? converted-.5 : converted+.5;
	return foldrealint(converted, Tint, result);
}

/* Evaluate the selected WORD32 fixed conversion with the VM's exact bit steps. */
static int
foldfixedcast(Long value, Type *from, Type *to, Long *result)
{
	int op, shift, coefficient, flags;
	Long correction, converted;
	uvlong bits, mask;

	op = fixop(ICVTXX, from, tint, to, &shift, &coefficient);
	if(op == IMOVW){
		*result = 0;
		return 1;
	}
	flags = 0;
	if(op == ICVTXX1){
		flags = shift&3;
		shift >>= 2;
	}else if(op != ICVTXX && op != ICVTXX0)
		return 0;
	if(shift < -63 || shift > 63 || (op != ICVTXX && coefficient <= 0))
		return 0;
	value = foldint32(value);
	correction = 0;
	mask = 0;
	if(flags&2){
		correction = coefficient-1;
		if(value < 0)
			correction = -correction;
	}
	if((flags&1) && ((!(flags&2) && value < 0) || ((flags&2) && value > 0))){
		if(shift >= 0)
			return 0;
		mask = ((uvlong)1<<(-shift))-1;
	}
	bits = (uvlong)value+mask;
	if(shift >= 0)
		bits <<= shift;
	else if(bits>>63)
		bits = ~((~bits)>>(-shift));
	else
		bits >>= -shift;
	bits += (uvlong)correction;
	converted = (Long)bits;
	if(op != ICVTXX)
		converted /= coefficient;
	*result = foldint32(converted);
	return 1;
}

/* Fold decimal prefixes only in the common 32-bit strtol range of all targets. */
static int
foldstringint(char *text, Long *result)
{
	uvlong value, limit;
	int negative, digit;

	while(*text == ' ' || *text == '\t' || *text == '\n' ||
	      *text == '\r' || *text == '\f' || *text == '\v')
		text++;
	negative = *text == '-';
	if(*text == '-' || *text == '+')
		text++;
	value = 0;
	limit = negative ? (uvlong)0x80000000 : (uvlong)0x7fffffff;
	while(*text >= '0' && *text <= '9'){
		digit = *text++-'0';
		if(value > (limit-digit)/10)
			return 0;
		value = value*10+digit;
	}
	*result = negative ? -(Long)value : (Long)value;
	return 1;
}

/* Fold casts only when their native intermediate conversion is representable. */
Node*
foldcast(Node *n, Node *left)
{
	Real r;
	char *buf, *e;

	switch(left->ty->kind){
	case Tint:
		left->val = foldint32(left->val);
		return foldcasti(n, left);
	case Tbyte:
		left->val &= 0xff;
		return foldcasti(n, left);
	case Tbig:
		return foldcasti(n, left);
	case Treal:
		switch(n->ty->kind){
		case Tint:
		case Tbyte:
		case Tbig:
			r = left->rval;
			r = r < 0 ? r-.5 : r+.5;
			if(!foldrealint(r, n->ty->kind, &left->val))
				return n;
			return foldcasti(n, left);
		case Tfix:
			if(!foldrealtofixed(left->rval, n->ty, &left->val))
				return n;
			break;
		case Tstring:
			buf = allocmem(NumSize);
			e = seprint(buf, buf+NumSize, "%g", left->rval);
			return mksconst(&n->src, enterstring(buf, e-buf));
		default:
			return n;
		}
		break;
	case Tfix:
		switch(n->ty->kind){
		case Tint:
		case Tbyte:
		case Tbig:
			if(n->ty->kind == Tbig){
				r = fix2real(left->val, left->ty);
				r = r < 0 ? r-.5 : r+.5;
				if(!foldrealint(r, Tbig, &left->val))
					return n;
			}else if(!foldfixedcast(left->val, left->ty, tint, &left->val))
				return n;
			return foldcasti(n, left);
		case Treal:
			left->rval = fix2real(left->val, left->ty);
			break;
		case Tfix:
			if(tequal(left->ty, n->ty))
				return left;
			if(!foldfixedcast(left->val, left->ty, n->ty, &left->val))
				return n;
			break;
		case Tstring:
			buf = allocmem(NumSize);
			e = seprint(buf, buf+NumSize, "%g", fix2real(left->val, left->ty));
			return mksconst(&n->src, enterstring(buf, e-buf));
		default:
			return n;
		}
		break;
	case Tstring:
		switch(n->ty->kind){
		case Tint:
		case Tbyte:
			if(!foldstringint(left->decl->sym->name, &left->val))
				return n;
			return foldcasti(n, left);
		case Tbig:
			left->val = strtoll(left->decl->sym->name, nil, 10);
			return foldcasti(n, left);
		case Treal:
			left->rval = strtod(left->decl->sym->name, nil);
			break;
		case Tfix:
			if(!foldrealtofixed(strtod(left->decl->sym->name, nil), n->ty, &left->val))
				return n;
			break;
		default:
			return n;
		}
		break;
	default:
		return n;
	}
	left->ty = n->ty;
	left->src = n->src;
	return left;
}

/* Narrow integer constants before changing their type or exposing their value. */
Node*
foldcasti(Node *n, Node *left)
{
	char *buf, *e;

	switch(n->ty->kind){
	case Tint:
		left->val = foldint32(left->val);
		break;
	case Tbyte:
		left->val &= 0xff;
		break;
	case Tbig:
		break;
	case Treal:
		left->rval = left->val;
		break;
	case Tfix:
		if(left->ty->kind == Tbig){
			if(!foldrealtofixed((Real)left->val, n->ty, &left->val))
				return n;
		}else if(!foldfixedcast(left->val, tint, n->ty, &left->val))
			return n;
		break;
	case Tstring:
		buf = allocmem(NumSize);
		e = seprint(buf, buf+NumSize, "%lld", left->val);
		return mksconst(&n->src, enterstring(buf, e-buf));
	default:
		return n;
	}
	left->ty = n->ty;
	left->src = n->src;
	return left;
}

/*
 * Fold a constant right operand without changing boolean normalization
 * or discarding evaluation of a left operand with side effects.
 * Preserve the expression type when its right operand has a different type.
 */
Node*
foldvc(Node *n)
{
	Node *left, *right;

	left = n->left;
	right = n->right;
	switch(n->op){
	case Oadd:
	case Osub:
	case Oor:
	case Oxor:
	case Olsh:
	case Orsh:
		if(right->val == 0)
			return left;
		break;
	case Ooror:
		if(right->val == 0){
			if(isrelop[left->op])
				return left;
		}else if(!hasside(left, 1)){
			right->val = 1;
			return right;
		}
		break;
	case Oand:
		if(right->val == 0 && !hasside(left, 1))
			return right;
		break;
	case Omul:
		if(right->val == 1)
			return left;
		if(right->val == 0 && !hasside(left, 1))
			return right;
		break;
	case Odiv:
		if(right->val == 1)
			return left;
		break;
	case Omod:
		if(right->val == 1 && !hasside(left, 1)){
			right->val = 0;
			return right;
		}
		break;
	case Oexp:
		if(right->val == 0 && !hasside(left, 1)){
			/* The exponent is int even when the power's result is big. */
			n->op = Oconst;
			n->val = 1;
			n->left = n->right = nil;
			n->decl = nil;
			return n;
		}
		if(right->val == 1)
			return left;
		break;
	case Oandand:
		if(right->val != 0){
			if(isrelop[left->op])
				return left;
			break;
		}
		if(!hasside(left, 1))
			return right;
		break;
	case Oneq:
		if(!isrelop[left->op])
			return n;
		if(right->val == 0)
			return left;
		if(right->val != 1)
			break;
		n->op = Onot;
		n->right = nil;
		break;
	case Oeq:
		if(!isrelop[left->op])
			return n;
		if(right->val == 1)
			return left;
		if(right->val != 0)
			break;
		n->op = Onot;
		n->right = nil;
		break;
	}
	return n;
}

/*
 * Fold constant integers using unsigned arithmetic for wraparound.
 * Shift masks must use the full width of Long, independently of host int.
 */
Node*
foldc(Node *n)
{
	Node *left, *right;
	Long lv, v;
	int rv, nb;

	left = n->left;
	right = n->right;
	switch(n->op){
	case Oadd:
		v = (uvlong)left->val + (uvlong)right->val;
		break;
	case Osub:
		v = (uvlong)left->val - (uvlong)right->val;
		break;
	case Omul:
		v = (uvlong)left->val * (uvlong)right->val;
		break;
	case Odiv:
		if(right->val == 0){
			nerror(n, "divide by 0 in constant expression");
			return n;
		}
		/* Avoid the host divide trap for the minimum Long divided by -1. */
		if(right->val == -1)
			v = (uvlong)0 - (uvlong)left->val;
		else
			v = left->val / right->val;
		break;
	case Omod:
		if(right->val == 0){
			nerror(n, "mod by 0 in constant expression");
			return n;
		}
		/* The remainder is zero for every numerator when the divisor is -1. */
		if(right->val == -1)
			v = 0;
		else
			v = left->val % right->val;
		break;
	case Oexp:
		if(left->val == 0 && right->val < 0){
			nerror(n, "0 to negative power in constant expression");
			return n;
		}
		v = ipow(left->val, right->val);
		break;
	case Oand:
		v = left->val & right->val;
		break;
	case Oor:
		v = left->val | right->val;
		break;
	case Oxor:
		v = left->val ^ right->val;
		break;
	case Olsh:
		lv = left->val;
		rv = right->val;
		if(rv < 0 || rv >= n->ty->size * 8){
			nwarn(n, "shift amount %d out of range", rv);
			rv = 0;
		}
		if(rv == 0){
			v = lv;
			break;
		}
		v = (uvlong)lv << rv;
		break;
	case Orsh:
		lv = left->val;
		rv = right->val;
		nb = n->ty->size * 8;
		if(rv < 0 || rv >= nb){
			nwarn(n, "shift amount %d out of range", rv);
			rv = 0;
		}
		if(rv == 0){
			v = lv;
			break;
		}
		v = lv >> rv;

		/*
		 * properly sign extend c right shifts
		 */
		if((n->ty == tint || n->ty == tbig)
		&& rv != 0
		&& ((uvlong)lv & ((uvlong)1<<(nb-1)))){
			lv = 0;
			lv = ~lv;
			v |= (uvlong)lv << (nb - rv);
		}
		break;
	case Oneg:
		v = (uvlong)0 - (uvlong)left->val;
		break;
	case Ocomp:
		v = ~left->val;
		break;
	case Oeq:
		v = left->val == right->val;
		break;
	case Oneq:
		v = left->val != right->val;
		break;
	case Ogt:
		v = left->val > right->val;
		break;
	case Ogeq:
		v = left->val >= right->val;
		break;
	case Olt:
		v = left->val < right->val;
		break;
	case Oleq:
		v = left->val <= right->val;
		break;
	case Oandand:
		v = left->val && right->val;
		break;
	case Ooror:
		v = left->val || right->val;
		break;
	case Onot:
		v = !left->val;
		break;
	default:
		return n;
	}
	if(n->ty == tint){
		v &= 0xffffffff;
		if(v & 0x80000000)
			v |= (uvlong)0xffffffff << 32;
	}else if(n->ty == tbyte)
		v &= 0xff;
	n->left = nil;
	n->right = nil;
	n->decl = nil;
	n->op = Oconst;
	n->val = v;
	return n;
}

/*
 * left and right are const reals
 */
Node*
foldr(Node *n)
{
	Node *left, *right;
	double rv;
	Long v;

	rv = 0.;
	v = 0;

	left = n->left;
	right = n->right;
	switch(n->op){
	case Ocast:
		return n;
	case Oadd:
		rv = left->rval + right->rval;
		break;
	case Osub:
		rv = left->rval - right->rval;
		break;
	case Omul:
		rv = left->rval * right->rval;
		break;
	case Odiv:
		rv = left->rval / right->rval;
		break;
	case Oexp:
		rv = rpow(left->rval, right->val);
		break;
	case Oneg:
		rv = -left->rval;
		break;
	case Oinv:
		if(left->rval == 0.0){
			error(n->src.start, "divide by 0 in fixed point type");
			return n;
		}
		rv = 1/left->rval;
		break;
	case Oeq:
		v = left->rval == right->rval;
		break;
	case Oneq:
		v = left->rval != right->rval;
		break;
	case Ogt:
		v = left->rval > right->rval;
		break;
	case Ogeq:
		v = left->rval >= right->rval;
		break;
	case Olt:
		v = left->rval < right->rval;
		break;
	case Oleq:
		v = left->rval <= right->rval;
		break;
	default:
		return n;
	}
	n->left = nil;
	n->right = nil;

	if(isNaN(rv))
		rv = canonnan;

	n->rval = rv;
	n->val = v;

	n->op = Oconst;
	return n;
}


/* Build right-associated declaration assignments without one C frame per name. */
Node*
varinit(Decl *d, Node *e)
{
	Node *names, *name, *result;

	/* Private name links retain the recursive constructor's allocation order. */
	names = nil;
	do{
		name = mkdeclname(&e->src, d);
		name->right = names;
		names = name;
		d = d->next;
	}while(d != nil);
	result = e;
	while(names != nil){
		name = names;
		names = name->right;
		name->right = nil;
		result = mkbin(Oas, name, result);
	}
	return result;
}


/*
 * given: an Oseq list with left == next or the last child
 * make a list with the right == next
 * ie: Oseq(Oseq(a, b),c) ==> Oseq(a, Oseq(b, Oseq(c, nil))))
 */
Node*
rotater(Node *e)
{
	Node *left;

	if(e == nil)
		return e;
	if(e->op != Oseq)
		return mkunary(Oseq, e);
	e->right = mkunary(Oseq, e->right);
	while(e->left->op == Oseq){
		left = e->left;
		e->left = left->right;
		left->right = e;
		e = left;
	}
	return e;
}


/* Reverse label links iteratively, retaining any existing suffix and node metadata. */
Node*
caselist(Node *s, Node *nr)
{
	Node *next;

	do{
		next = s->right;
		s->right = nr;
		nr = s;
		s = next;
	}while(s != nil);
	return nr;
}


/*
 * e is a seq of expressions; make into cons's to build a list
 */
Node*
etolist(Node *e)
{
	Node *left, *n;

	if(e == nil)
		return nil;
	n = mknil(&e->src);
	n->src.start = n->src.stop;
	if(e->op != Oseq)
		return mkbin(Ocons, e, n);
	e->right = mkbin(Ocons, e->right, n);
	while(e->left->op == Oseq){
		e->op = Ocons;
		left = e->left;
		e->left = left->right;
		left->right = e;
		e = left;
	}
	e->op = Ocons;
	return e;
}


typedef struct NodeCopy NodeCopy;

/* Pending source edges and private output slots for preorder tree copying. */
struct NodeCopy {
	Node *source; // Borrowed subtree copied when earlier left branches finish.
	Node **destination; // Child cell belonging to an already allocated copy.
};


/* Duplicate every tree edge in the original allocation order with bounded C stack.
 * Type/Decl and other metadata remain borrowed, and source positions are optional.
 * Shared subtrees are duplicated per edge, preserving the recursive contract. */
Node*
dupn(int resrc, Src *src, Node *n)
{
	enum { InlineNodes = 64 }; // Pending right subtrees on the fixed host stack.
	NodeCopy inlinework[InlineNodes], *work, *next;
	Node *root, *nn, **destination;
	ulong used, capacity;

	root = nil;
	destination = &root;
	work = inlinework;
	used = 0;
	capacity = InlineNodes;
	for(;;){
		nn = allocmem(sizeof(*nn));
		*nn = *n;
		if(resrc)
			nn->src = *src;
		*destination = nn;
		if(n->right != nil){
			if(used == capacity){
				if(capacity > (ulong)-1/2/sizeof(*work))
					fatal("node copy storage overflow");
				next = malloc(2*capacity*sizeof(*work));
				if(next == nil)
					fatal("out of memory copying nodes");
				memmove(next, work, used*sizeof(*work));
				if(work != inlinework)
					free(work);
				work = next;
				capacity *= 2;
			}
			work[used].source = n->right;
			work[used++].destination = &nn->right;
		}
		if(n->left != nil){
			n = n->left;
			destination = &nn->left;
			continue;
		}
		if(used == 0)
			break;
		used--;
		n = work[used].source;
		destination = work[used].destination;
	}
	if(work != inlinework)
		free(work);
	return root;
}


Node*
mkn(int op, Node *left, Node *right)
{
	Node *n;

	n = allocmem(sizeof *n);
	*n = znode;
	n->op = op;
	n->left = left;
	n->right = right;
	return n;
}

Node*
mkunary(int op, Node *left)
{
	Node *n;

	n = mkn(op, left, nil);
	n->src = left->src;
	return n;
}

Node*
mkbin(int op, Node *left, Node *right)
{
	Node *n;

	n = mkn(op, left, right);
	n->src.start = left->src.start;
	n->src.stop = right->src.stop;
	return n;
}


/* Build a name node and retain its borrowed declaration with a checked count. */
Node*
mkdeclname(Src *src, Decl *d)
{
	Node *n;

	n = mkn(Oname, nil, nil);
	n->src = *src;
	n->decl = d;
	n->ty = d->ty;
	d->refs = declrefsum(d->refs, 1);
	return n;
}

Node*
mknil(Src *src)
{
	return mkdeclname(src, nildecl);
}

Node*
mkname(Src *src, Sym *s)
{
	Node *n;

	n = mkn(Oname, nil, nil);
	n->src = *src;
	if(s->unbound == nil){
		s->unbound = mkdecl(src, Dunbound, nil);
		s->unbound->sym = s;
	}
	n->decl = s->unbound;
	return n;
}

Node*
mkconst(Src *src, Long v)
{
	Node *n;

	n = mkn(Oconst, nil, nil);
	n->ty = tint;
	n->val = v;
	n->src = *src;
	return n;
}

Node*
mkrconst(Src *src, Real v)
{
	Node *n;

	n = mkn(Oconst, nil, nil);
	n->ty = treal;
	n->rval = v;
	n->src = *src;
	return n;
}

Node*
mksconst(Src *src, Sym *s)
{
	Node *n;

	n = mkn(Oconst, nil, nil);
	n->ty = tstring;
	n->decl = mkdecl(src, Dconst, tstring);
	n->decl->sym = s;
	n->src = *src;
	return n;
}

int
opconv(Fmt *f)
{
	int op;
	char buf[32];

	op = va_arg(f->args, int);
	if(op < 0 || op > Oend) {
		seprint(buf, buf+sizeof(buf), "op %d", op);
		return fmtstrcpy(f, buf);
	}
	return fmtstrcpy(f, opname[op]);
}

int
etconv(Fmt *f)
{
	Node *n;
	char buf[1024];

	n = va_arg(f->args, Node*);
	if(n->ty == tany || n->ty == tnone || n->ty == terror)
		seprint(buf, buf+sizeof(buf), "%V", n);
	else
		seprint(buf, buf+sizeof(buf), "%V of type %T", n, n->ty);
	return fmtstrcpy(f, buf);
}


/* Reserve the closing quote and NUL even when expression text fills its span. */
int
expconv(Fmt *f)
{
	Node *n;
	char buf[4096], *p;

	n = va_arg(f->args, Node*);
	p = buf;
	*p = 0;
	if(f->r == 'V')
		*p++ = '\'';
	p = eprint(p, buf+sizeof(buf)-1, n);
	if(p >= buf+sizeof(buf)-1)
		p = buf+sizeof(buf)-2;
	if(f->r == 'V')
		*p++ = '\'';
	*p = 0;
	return fmtstrcpy(f, buf);
}


#define NodePrintPlan 16 // Maximum actions in one diagnostic node expansion.

/* Pending diagnostic actions retain traversal order without using the C call stack. */
typedef struct NodePrint NodePrint;

struct NodePrint {
	int kind; // Node, list, text, Type or debug-dump action.
	Node *node; // Borrowed AST node; diagnostics never mutate the tree.
	char *text; // Borrowed text or list separator.
	Type *ty; // Borrowed Type for an interleaved type conversion.
	int indent; // Nonnegative indentation for a debug dump.
};

enum {
	PrintNode, // Expand one expression into ordered output actions.
	PrintList, // Expand one sequence cell without recursive list traversal.
	PrintText, // Copy literal text through the bounded diagnostic helper.
	PrintType, // Render a Type at its original place in the output stream.
	PrintAtom, // Render one leaf expression or statement.
	PrintDump, // Render a debug node before its two children.
};


/* Append a small expansion; callers bound every node expansion to sixteen actions. */
static void
nodeprintadd(NodePrint *plan, int *count, int kind, Node *n, char *text, Type *ty)
{
	NodePrint *p;

	if(*count >= NodePrintPlan)
		fatal("diagnostic node expansion too large");
	p = &plan[(*count)++];
	p->kind = kind;
	p->node = n;
	p->text = text;
	p->ty = ty;
	p->indent = 0;
}


/* Expand a node in reading order; the driver reverses these actions onto its stack. */
static int
nodeprintplan(NodePrint *plan, NodePrint *current)
{
	Node *n;
	int count;

#define PN(node) nodeprintadd(plan, &count, PrintNode, (node), nil, nil) // Schedule an expression.
#define PL(node, sep) nodeprintadd(plan, &count, PrintList, (node), (sep), nil) // Schedule a sequence.
#define PT(text) nodeprintadd(plan, &count, PrintText, nil, (text), nil) // Schedule literal output.
#define PY(type) nodeprintadd(plan, &count, PrintType, nil, nil, (type)) // Schedule a Type conversion.

	n = current->node;
	count = 0;
	if(n == nil)
		return 0;
	if(current->kind == PrintList){
		if(n->right == nil){
			PN(n->left);
		}else if(n->op == Onothing){
			PL(n->right, current->text);
		}else if(n->left->op != Ofnptr){
			PN(n->left);
			if(n->right->left->op != Ofnptr)
				PT(current->text);
			PL(n->right, current->text);
		}
		return count;
	}
	if(n->flags & PARENS)
		PT("(");
	switch(n->op){
	case Obreak:
	case Ocont:
	case Oexit:
	case Owild:
	case Oname:
	case Oconst:
		nodeprintadd(plan, &count, PrintAtom, n, nil, nil);
		break;
	case Onothing:
		break;
	case Oadr:
	case Oused:
		PN(n->left);
		break;
	case Oseq:
		PL(n, ", ");
		break;
	case Ocast:
		PY(n->ty);
		PT(" ");
		PN(n->left);
		break;
	case Otuple:
		if(n->ty != nil && n->ty->kind == Tadt)
			PT(n->ty->decl->sym->name);
		PT("(");
		PL(n->left, ", ");
		PT(")");
		break;
	case Ochan:
		if(n->left != nil){
			PT("chan [");
			PN(n->left);
			PT("] of ");
		}else
			PT("chan of ");
		PY(n->ty->tof);
		break;
	case Oarray:
		PT("array [");
		PN(n->left);
		PT("] of ");
		if(n->right != nil){
			PT("{");
			PL(n->right, ", ");
			PT("}");
		}else
			PY(n->ty->tof);
		break;
	case Oelem:
	case Olabel:
		if(n->left != nil){
			PL(n->left, " or ");
			PT(" =>");
		}
		PN(n->right);
		break;
	case Orange:
		PN(n->left);
		PT(" to ");
		PN(n->right);
		break;
	case Ospawn:
	case Oraise:
		PT(n->op == Ospawn ? "spawn " : "raise ");
		PN(n->left);
		break;
	case Ocall:
		PN(n->left);
		PT("(");
		PL(n->right, ", ");
		PT(")");
		break;
	case Oinc:
	case Odec:
		PN(n->left);
		PT(opname[n->op]);
		break;
	case Oindex:
	case Oindx:
	case Oinds:
		PN(n->left);
		PT("[");
		PN(n->right);
		PT("]");
		break;
	case Oslice:
		PN(n->left);
		PT("[");
		PN(n->right->left);
		PT(":");
		PN(n->right->right);
		PT("]");
		break;
	case Oload:
		PT("load ");
		PY(n->ty);
		PT(" ");
		PN(n->left);
		break;
	case Oref:
	case Olen:
	case Ohd:
	case Otl:
	case Otagof:
		PT(opname[n->op]);
		PT(" ");
		PN(n->left);
		break;
	default:
		if(n->right == nil){
			PT(opname[n->op]);
			PN(n->left);
		}else{
			PN(n->left);
			PT(opname[n->op]);
			PN(n->right);
		}
		break;
	}
	if(n->flags & PARENS)
		PT(")");
	return count;
#undef PN
#undef PL
#undef PT
#undef PY
}


/* Preserve leaf spelling and numeric conversion independently of traversal storage. */
static char*
nodeprintatom(char *buf, char *end, Node *n)
{
	switch(n->op){
	case Obreak:
	case Ocont:
		buf = secpy(buf, end, opname[n->op]);
		if(n->decl != nil)
			buf = seprint(buf, end, " %s", n->decl->sym->name);
		return buf;
	case Oexit:
	case Owild:
		return secpy(buf, end, opname[n->op]);
	case Oname:
		if(n->decl == nil)
			return secpy(buf, end, "<nil>");
		return seprint(buf, end, "%s", n->decl->sym->name);
	case Oconst:
		if(n->ty->kind == Tstring)
			return stringpr(buf, end, n->decl->sym);
		if(n->decl != nil && n->decl->sym != nil)
			return seprint(buf, end, "%s", n->decl->sym->name);
		switch(n->ty->kind){
		case Tint:
	case Tbyte:
			return seprint(buf, end, "%ld", (long)n->val);
		case Tbig:
			return seprint(buf, end, "%lld", n->val);
		case Treal:
			return seprint(buf, end, "%g", n->rval);
		case Tfix:
			return seprint(buf, end, "%ld(%g)", (long)n->val, n->ty->val->rval);
		default:
			return secpy(buf, end, opname[n->op]);
		}
	}
	return buf;
}


/* Render one debug header, bounding indentation before visiting any descendants. */
static char*
nodeprintdump(char *buf, char *end, Node *n, int indent)
{
	int i;

	buf = seprint(buf, end, "\n");
	for(i = 0; i < indent && buf < end-1; i++)
		*buf++ = ' ';
	*buf = 0;
	switch(n->op){
	case Oname:
		if(n->decl == nil)
			buf = secpy(buf, end, "name <nil>");
		else
			buf = seprint(buf, end, "name %s", n->decl->sym->name);
		break;
	case Oconst:
		if(n->decl != nil && n->decl->sym != nil)
			buf = seprint(buf, end, "const %s", n->decl->sym->name);
		else
			buf = seprint(buf, end, "%O", n->op);
		if(n->ty == tint || n->ty == tbyte || n->ty == tbig)
			buf = seprint(buf, end, " (%ld)", (long)n->val);
		break;
	default:
		buf = seprint(buf, end, "%O", n->op);
		break;
	}
	return seprint(buf, end, " %T %d %d", n->ty, n->addable, n->temps);
}


/* Stop when output is full and use checked heap storage for deferred siblings. */
static char*
nodeprintwork(char *buf, char *end, Node *n, int kind, char *sep, int indent)
{
	NodePrint inlinework[64], plan[NodePrintPlan], current, *work, *grown;
	int used, capacity, count, i, next;

	if(buf >= end || n == nil)
		return buf;
	*buf = 0;
	work = inlinework;
	capacity = nelem(inlinework);
	used = 0;
	nodeprintadd(work, &used, kind, n, sep, nil);
	work[0].indent = indent;
	while(used != 0 && buf < end-1){
		current = work[--used];
		count = 0;
		switch(current.kind){
		case PrintText:
			buf = secpy(buf, end, current.text);
			break;
		case PrintType:
			buf = seprint(buf, end, "%T", current.ty);
			break;
		case PrintAtom:
			buf = nodeprintatom(buf, end, current.node);
			break;
		case PrintDump:
			if(current.node == nil)
				break;
			buf = nodeprintdump(buf, end, current.node, current.indent);
			next = current.indent > 0x7fffffff-2 ? 0x7fffffff : current.indent+2;
			nodeprintadd(plan, &count, PrintDump, current.node->left, nil, nil);
			nodeprintadd(plan, &count, PrintDump, current.node->right, nil, nil);
			plan[0].indent = plan[1].indent = next;
			break;
		default:
			count = nodeprintplan(plan, &current);
			break;
		}
		/* secpy historically returns end on truncation; expose the live NUL cursor. */
		if(buf >= end-1){
			buf = end-1;
			*buf = 0;
			break;
		}
		if(count > capacity-used){
			if(capacity > 0x7fffffff/2/sizeof(*work))
				fatal("diagnostic expression too complex");
			capacity *= 2;
			grown = allocmem(capacity*sizeof(*work));
			memmove(grown, work, used*sizeof(*work));
			if(work != inlinework)
				free(work);
			work = grown;
		}
		for(i = count; i != 0; i--)
			work[used++] = plan[i-1];
	}
	if(work != inlinework)
		free(work);
	return buf;
}


/* Print a complete expression without recursion through its AST edges. */
char*
eprint(char *buf, char *end, Node *n)
{
	return nodeprintwork(buf, end, n, PrintNode, nil, 0);
}


/* Print sequence cells with their historical separators and function-pointer stop rule. */
char*
eprintlist(char *buf, char *end, Node *elist, char *sep)
{
	return nodeprintwork(buf, end, elist, PrintList, sep, 0);
}


/* Convert a debug tree through the bounded nonrecursive printer. */
int
nodeconv(Fmt *f)
{
	Node *n;
	char buf[4096];

	n = va_arg(f->args, Node*);
	buf[0] = 0;
	nprint(buf, buf+sizeof(buf), n, 0);
	return fmtstrcpy(f, buf);
}


/* Print debug headers and descendants using the same bounded action stack. */
char*
nprint(char *buf, char *end, Node *n, int indent)
{
	return nodeprintwork(buf, end, n, PrintDump, nil, indent);
}
