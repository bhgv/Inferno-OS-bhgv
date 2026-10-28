/* Frozen ordinary reference propagation and grouped local sorting. */


/* Retain historical behavior for shallow, representable input metadata. */
void
reference_checkrefs(Decl *d)
{
	Decl *id, *m;
	long refs;

	for(; d != nil; d = d->next){
		if(d->das)
			d->refs--;
		switch(d->store){
		case Dtype:
			refs = d->refs;
			if(d->ty->kind == Tadt){
				for(id = d->ty->ids; id != nil; id = id->next){
					d->refs += id->refs;
					if(id->store != Dfn)
						continue;
					if(id->init == nil && id->link == nil && d->importid == nil)
						error(d->src.start, "function %s.%s not defined", d->sym->name, id->sym->name);
					if(superwarn && !id->refs && d->importid == nil)
						warn(d->src.start, "function %s.%s not referenced", d->sym->name, id->sym->name);
				}
			}
			if(d->ty->kind == Tmodule){
				for(id = d->ty->ids; id != nil; id = id->next){
					refs += id->refs;
					if(id->iface != nil)
						id->iface->refs += id->refs;
					if(id->store == Dtype){
						for(m = id->ty->ids; m != nil; m = m->next){
							refs += m->refs;
							if(m->iface != nil)
								m->iface->refs += m->refs;
						}
					}
				}
				d->refs = refs;
			}
			if(superwarn && !refs && d->importid == nil)
				warn(d->src.start, "%K not referenced", d);
			break;
		case Dglobal:
			if(!superwarn)
				break;
		case Dlocal:
		case Darg:
			if(!d->refs && d->sym != nil
			&& d->sym->name != nil && d->sym->name[0] != '.')
				warn(d->src.start, "%K not referenced", d);
			break;
		case Dconst:
			if(superwarn && !d->refs && d->sym != nil)
				warn(d->src.start, "%K not referenced", d);
			if(d->ty == tstring && d->init != nil)
				d->init->decl->refs += d->refs;
			break;
		case Dfn:
			if(d->init == nil && d->importid == nil)
				error(d->src.start, "%K not defined", d);
			if(superwarn && !d->refs)
				warn(d->src.start, "%K not referenced", d);
			break;
		case Dimport:
			if(superwarn && !d->refs)
				warn(d->src.start, "%K not referenced", d);
			break;
		}
		if(d->das)
			d->refs++;
	}
}


/* Retain historical behavior for shallow, representable input metadata. */
Decl*
reference_popscope(void)
{
	Decl *id;
	Type *t;

if (debug['X'])
	print("popscope\n");
	for(id = scopes[scope]; id != nil; id = id->next){
		if(id->sym != nil){
if (debug['X'])
	print("%s : %s %d\n", id->sym->name, kindname[id->ty->kind], id->init != nil ? id->init->op : 0);
			id->sym->decl = id->old;
			id->old = nil;
		}
		if(id->importid != nil)
			id->importid->refs += id->refs;
		t = id->ty;
		if(id->store == Dtype
		&& t->decl != nil
		&& t->decl->timport == id)
			t->decl->timport = id->timport;
		if(id->store == Dlocal)
			freeloc(id);
	}
	return scopes[scope--];
}


/* Retain historical behavior for shallow, representable input metadata. */
static Decl*
reference_declmerge(Decl *e, Decl *f)
{
	Decl rock, *d;
	int es, fs, v;

	d = &rock;
	while(e != nil && f != nil){
		fs = f->ty->size;
		es = e->ty->size;
		/* v = 0; */
		v = (e->link == nil) - (f->link == nil);
		if(v == 0 && (es <= IBY2WD || fs <= IBY2WD))
			v = fs - es;
		if(v == 0)
			v = e->refs - f->refs;
		if(v == 0)
			v = fs - es;
		if(v == 0)
			v = -strcmp(e->sym->name, f->sym->name);
		if(v >= 0){
			d->next = e;
			d = e;
			e = e->next;
			while(e != nil && e->nid == 0){
				d = e;
				e = e->next;
			}
		}else{
			d->next = f;
			d = f;
			f = f->next;
			while(f != nil && f->nid == 0){
				d = f;
				f = f->next;
			}
		}
		/* d = d->next; */
	}
	if(e != nil)
		d->next = e;
	else
		d->next = f;
	return rock.next;
}


/* Retain historical behavior for shallow, representable input metadata. */
static Decl*
reference_recdeclsort(Decl *d, int n)
{
	Decl *r, *dd;
	int i, m;

	if(n <= 1)
		return d;
	m = n / 2 - 1;
	dd = d;
	for(i = 0; i < m; i++){
		dd = dd->next;
		while(dd->nid == 0)
			dd = dd->next;
	}
	r = dd->next;
	while(r->nid == 0){
		dd = r;
		r = r->next;
	}
	dd->next = nil;
	return reference_declmerge(reference_recdeclsort(d, n / 2),
			reference_recdeclsort(r, (n + 1) / 2));
}


/* Retain historical behavior for shallow, representable input metadata. */
Decl*
reference_declsort(Decl *d)
{
	Decl *dd;
	int n;

	n = 0;
	for(dd = d; dd != nil; dd = dd->next)
		if(dd->nid > 0)
			n++;
	return reference_recdeclsort(d, n);
}
