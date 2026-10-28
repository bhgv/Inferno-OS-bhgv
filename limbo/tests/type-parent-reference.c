/* Preserve the shallow recursive rule as a differential oracle. */
static int
reference_tparent0(Type *t1, Type *t2)
{
	Decl *id1, *id2;

	if(t1 == t2)
		return 1;
	if(t1 == nil || t2 == nil)
		return 0;
	if(t1->kind == Tadt && t2->kind == Tadtpick)
		t2 = t2->decl->dot->ty;
	if(t1->kind == Tadtpick && t2->kind == Tadt)
		t1 = t1->decl->dot->ty;
	if(t1->kind != t2->kind)
		return 0;
	switch(t1->kind){
	default:
		fatal("unknown type %t v %t in reference_tparent", t1, t2);
		break;
	case Terror:
	case Tstring:
	case Tnone:
	case Tint:
	case Tbig:
	case Tbyte:
	case Treal:
	case Tany:
		return 1;
	case Texception:
	case Tfix:
	case Tfn:
	case Tadt:
	case Tmodule:
	case Tpoly:
		return tcompat(t1, t2, 0);
	case Tref:
	case Tlist:
	case Tarray:
	case Tchan:
		return reference_tparent0(t1->tof, t2->tof);
	case Ttuple:
		for(id1 = t1->ids, id2 = t2->ids; id1 != nil && id2 != nil; id1 = id1->next, id2 = id2->next)
			if(!reference_tparent0(id1->ty, id2->ty))
				return 0;
		return id1 == nil && id2 == nil;
	case Tadtpick:
		return tequal(t1->decl->dot->ty, t2->decl->dot->ty);
	}
	return 0;
}


/* Preserve the shallow recursive rule as a differential oracle. */
static Type*
reference_tparent1(Type *t1, Type *t2)
{
	Type *t, *nt;
	Decl *id, *id1, *id2, *idt;

	if(t1->kind == Tadt && t2->kind == Tadtpick)
		t2 = t2->decl->dot->ty;
	if(t1->kind == Tadtpick && t2->kind == Tadt)
		t1 = t1->decl->dot->ty;
	switch(t1->kind){
	default:
		return t1;
	case Tref:
	case Tlist:
	case Tarray:
	case Tchan:
		t = reference_tparent1(t1->tof, t2->tof);
		if(t == t1->tof)
			return t1;
		return mktype(&t1->src.start, &t1->src.stop, t1->kind, t, nil);
	case Ttuple:
		nt = nil;
		id = nil;
		for(id1 = t1->ids, id2 = t2->ids; id1 != nil && id2 != nil; id1 = id1->next, id2 = id2->next){
			t = reference_tparent1(id1->ty, id2->ty);
			if(t != id1->ty){
				if(nt == nil){
					nt = mktype(&t1->src.start, &t1->src.stop, Ttuple, nil, dupdecls(t1->ids));
					for(id = nt->ids, idt = t1->ids; idt != id1; id = id->next, idt = idt->next)
						;
				}
				id->ty = t;
			}
			if(id != nil)
				id = id->next;
		}
		if(nt == nil)
			return t1;
		return nt;
	case Tadtpick:
		if(tequal(t1, t2))
			return t1;
		return t1->decl->dot->ty;
	}
}


/* Preserve the shallow recursive rule as a differential oracle. */
Type*
reference_tparent(Type *t1, Type *t2)
{
	if(reference_tparent0(t1, t2))
		return reference_tparent1(t1, t2);
	return t1;
}
