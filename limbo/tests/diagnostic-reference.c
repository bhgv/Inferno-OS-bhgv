/* Test-only recursive reference for ordinary, shallow diagnostic spelling. */
static char *reference_eprint(char*, char*, Node*);
static char *reference_eprintlist(char*, char*, Node*, char*);
static char *reference_nprint(char*, char*, Node*, int);



/* Retain the original traversal as a shallow differential oracle. */
static char*
reference_eprint(char *buf, char *end, Node *n)
{
	if(n == nil)
		return buf;
	if(n->flags & PARENS)
		buf = secpy(buf, end, "(");
	switch(n->op){
	case Obreak:
	case Ocont:
		buf = secpy(buf, end, opname[n->op]);
		if(n->decl != nil){
			buf = seprint(buf, end, " %s", n->decl->sym->name);
		}
		break;
	case Oexit:
	case Owild:
		buf = secpy(buf, end, opname[n->op]);
		break;
	case Onothing:
		break;
	case Oadr:
	case Oused:
		buf = reference_eprint(buf, end, n->left);
		break;
	case Oseq:
		buf = reference_eprintlist(buf, end, n, ", ");
		break;
	case Oname:
		if(n->decl == nil)
			buf = secpy(buf, end, "<nil>");
		else
			buf = seprint(buf, end, "%s", n->decl->sym->name);
		break;
	case Oconst:
		if(n->ty->kind == Tstring){
			buf = stringpr(buf, end, n->decl->sym);
			break;
		}
		if(n->decl != nil && n->decl->sym != nil){
			buf = seprint(buf, end, "%s", n->decl->sym->name);
			break;
		}
		switch(n->ty->kind){
		case Tint:
		case Tbyte:
			buf = seprint(buf, end, "%ld", (long)n->val);
			break;
		case Tbig:
			buf = seprint(buf, end, "%lld", n->val);
			break;
		case Treal:
			buf = seprint(buf, end, "%g", n->rval);
			break;
		case Tfix:
			buf = seprint(buf, end, "%ld(%g)", (long)n->val, n->ty->val->rval);
			break;
		default:
			buf = secpy(buf, end, opname[n->op]);
			break;
		}
		break;
	case Ocast:
		buf = seprint(buf, end, "%T ", n->ty);
		buf = reference_eprint(buf, end, n->left);
		break;
	case Otuple:
		if(n->ty != nil && n->ty->kind == Tadt)
			buf = seprint(buf, end, "%s", n->ty->decl->sym->name);
		buf = seprint(buf, end, "(");
		buf = reference_eprintlist(buf, end, n->left, ", ");
		buf = secpy(buf, end, ")");
		break;
	case Ochan:
		if(n->left){
			buf = secpy(buf, end, "chan [");
			buf = reference_eprint(buf, end, n->left);
			buf = secpy(buf, end, "] of ");
			buf = seprint(buf, end, "%T", n->ty->tof);
		}else
			buf = seprint(buf, end, "chan of %T", n->ty->tof);
		break;
	case Oarray:
		buf = secpy(buf, end, "array [");
		if(n->left != nil)
			buf = reference_eprint(buf, end, n->left);
		buf = secpy(buf, end, "] of ");
		if(n->right != nil){
			buf = secpy(buf, end, "{");
			buf = reference_eprintlist(buf, end, n->right, ", ");
			buf = secpy(buf, end, "}");
		}else{
			buf = seprint(buf, end, "%T", n->ty->tof);
		}
		break;
	case Oelem:
	case Olabel:
		if(n->left != nil){
			buf = reference_eprintlist(buf, end, n->left, " or ");
			buf = secpy(buf, end, " =>");
		}
		buf = reference_eprint(buf, end, n->right);
		break;
	case Orange:
		buf = reference_eprint(buf, end, n->left);
		buf = secpy(buf, end, " to ");
		buf = reference_eprint(buf, end, n->right);
		break;
	case Ospawn:
		buf = secpy(buf, end, "spawn ");
		buf = reference_eprint(buf, end, n->left);
		break;
	case Oraise:
		buf = secpy(buf, end, "raise ");
		buf = reference_eprint(buf, end, n->left);
		break;
	case Ocall:
		buf = reference_eprint(buf, end, n->left);
		buf = secpy(buf, end, "(");
		buf = reference_eprintlist(buf, end, n->right, ", ");
		buf = secpy(buf, end, ")");
		break;
	case Oinc:
	case Odec:
		buf = reference_eprint(buf, end, n->left);
		buf = secpy(buf, end, opname[n->op]);
		break;
	case Oindex:
	case Oindx:
	case Oinds:
		buf = reference_eprint(buf, end, n->left);
		buf = secpy(buf, end, "[");
		buf = reference_eprint(buf, end, n->right);
		buf = secpy(buf, end, "]");
		break;
	case Oslice:
		buf = reference_eprint(buf, end, n->left);
		buf = secpy(buf, end, "[");
		buf = reference_eprint(buf, end, n->right->left);
		buf = secpy(buf, end, ":");
		buf = reference_eprint(buf, end, n->right->right);
		buf = secpy(buf, end, "]");
		break;
	case Oload:
		buf = seprint(buf, end, "load %T ", n->ty);
		buf = reference_eprint(buf, end, n->left);
		break;
	case Oref:
	case Olen:
	case Ohd:
	case Otl:
	case Otagof:
		buf = secpy(buf, end, opname[n->op]);
		buf = secpy(buf, end, " ");
		buf = reference_eprint(buf, end, n->left);
		break;
	default:
		if(n->right == nil){
			buf = secpy(buf, end, opname[n->op]);
			buf = reference_eprint(buf, end, n->left);
		}else{
			buf = reference_eprint(buf, end, n->left);
			buf = secpy(buf, end, opname[n->op]);
			buf = reference_eprint(buf, end, n->right);
		}
		break;
	}
	if(n->flags & PARENS)
		buf = secpy(buf, end, ")");
	return buf;
}


/* Retain the original traversal as a shallow differential oracle. */
static char*
reference_eprintlist(char *buf, char *end, Node *elist, char *sep)
{
	if(elist == nil)
		return buf;
	for(; elist->right != nil; elist = elist->right){
		if(elist->op == Onothing)
			continue;
		if(elist->left->op == Ofnptr)
			return buf;
		buf = reference_eprint(buf, end, elist->left);
		if(elist->right->left->op != Ofnptr)
			buf = secpy(buf, end, sep);
	}
	buf = reference_eprint(buf, end, elist->left);
	return buf;
}


/* Retain the original traversal as a shallow differential oracle. */
static char*
reference_nprint(char *buf, char *end, Node *n, int indent)
{
	int i;

	if(n == nil)
		return buf;
	buf = seprint(buf, end, "\n");
	for(i = 0; i < indent; i++)
		if(buf < end-1)
			*buf++ = ' ';
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
	buf = seprint(buf, end, " %T %d %d", n->ty, n->addable, n->temps);
	indent += 2;
	buf = reference_nprint(buf, end, n->left, indent);
	buf = reference_nprint(buf, end, n->right, indent);
	return buf;
}
