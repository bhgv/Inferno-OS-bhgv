#include "limbo.h"
#include "dis64-layout.h"

static int
layoutfail(Dis64Layout *p, char *reason)
{
	if(p->reason == nil) p->reason = reason;
	return 0;
}

static int
layoutleaf(Dis64Layout *p, unsigned int old, unsigned int at, unsigned int oldbytes, unsigned int bytes, int kind)
{
	Dis64LayoutLeaf *leaf, *next;
	unsigned int capacity;
	if(old > Dis64LayoutBytes || oldbytes > Dis64LayoutBytes-old ||
	   at > Dis64LayoutBytes || bytes > Dis64LayoutBytes-at)
		return layoutfail(p,"object-limit");
	if(p->count == Dis64LayoutLeaves) return layoutfail(p,"leaf-limit");
	if(p->count == p->capacity){
		capacity = p->capacity ? 2*p->capacity : 16;
		next = allocmem(capacity*sizeof(*next));
		if(p->count) memmove(next,p->leaves,p->count*sizeof(*next));
		free(p->leaves); p->leaves=next; p->capacity=capacity;
	}
	leaf=&p->leaves[p->count++];
	*leaf=(Dis64LayoutLeaf){old,at,oldbytes,bytes,kind};
	return 1;
}

static int layouttype(Type*, unsigned int, unsigned int, unsigned int*, unsigned int*, Dis64Layout*, Type**, int);
static int layoutorder(void*, void*);

static int
layoutroot(Dis64LayoutLeaf *l)
{
	return l->kind>=0 && l->oldbytes==4 && l->bytes==8;
}

/* Original field offsets identify source bytes; target alignment is computed afresh. */
static int
layoutfields(Decl *fields, unsigned int old, unsigned int at, unsigned int oldlimit,
	unsigned int *cursor, unsigned int *alignment, Dis64Layout *p, Type **stack, int depth)
{
	Decl *d;
	unsigned int size, a, target, previous=0;
	for(d=fields; d != nil; d=d->next){
		if(d->store != Dfield) continue;
		if(d->ty == nil || d->offset < previous || d->offset > oldlimit ||
		   d->ty->size < 0 || d->ty->size > oldlimit-d->offset)
			return layoutfail(p,"field-bounds-or-overlap");
		/* First compute into a temporary plan to learn the nested natural alignment. */
		{
			Dis64Layout child={0};
			unsigned int i;
			if(!layouttype(d->ty,0,0,&size,&a,&child,stack,depth)){
				layoutfail(p,child.reason); dis64layoutfree(&child); return 0;
			}
			target=(*cursor+a-1)&~(a-1);
			for(i=0; i<child.count; i++){
				Dis64LayoutLeaf *l=&child.leaves[i];
				if(!layoutleaf(p,old+d->offset+l->legacy,at+target+l->native,l->oldbytes,l->bytes,l->kind)){
					dis64layoutfree(&child); return 0;
				}
			}
			dis64layoutfree(&child);
		}
		if(target > Dis64LayoutBytes || size > Dis64LayoutBytes-target)
			return layoutfail(p,"object-limit");
		*cursor=target+size;
		if(a>*alignment) *alignment=a;
		previous=d->offset+d->ty->size;
	}
	return 1;
}

static int
layouttype(Type *t, unsigned int old, unsigned int at, unsigned int *size, unsigned int *alignment,
	Dis64Layout *p, Type **stack, int depth)
{
	unsigned int bytes=0, oldbytes=0, a=1, cursor=0, n,j,target;
	Type *parent;
	int i;
	if(t == nil || depth == 128) return layoutfail(p,"type-depth");
	if(t->size < 0 || t->size > Dis64LayoutBytes) return layoutfail(p,"legacy-type-size");
	switch(t->kind){
	case Tnone: break;
	case Tbyte: bytes=oldbytes=1; break;
	case Tint: case Tfix: bytes=oldbytes=a=4; break;
	case Tbig: case Treal: bytes=oldbytes=a=8; break;
	case Tstring: case Tref: case Tarray: case Tlist: case Tchan: case Tmodule: case Tany: case Tpoly:
		bytes=a=8; oldbytes=4; break;
	case Talt:
		if(t->cse==nil || t->cse->nlab<0 || t->cse->nlab>(Dis64LayoutBytes-8)/16 ||
		   (t->cse->nlab && t->cse->labs==nil) || t->size!=8+8*t->cse->nlab)
			return layoutfail(p,"alt-table-shape");
		if(!layoutleaf(p,old,at,4,4,Tint) || !layoutleaf(p,old+4,at+4,4,4,Tint)) return 0;
		for(i=0;i<t->cse->nlab;i++){
			if(!layoutleaf(p,old+8+i*8,at+8+i*16,4,8,t->cse->labs[i].isptr?Tchan:Dis64LayoutBorrowed) ||
			   !layoutleaf(p,old+12+i*8,at+16+i*16,4,8,Dis64LayoutBorrowed)) return 0;
		}
		*size=8+16*t->cse->nlab; *alignment=8; return 1;
	case Tcase: case Tcasec: case Tcasel: case Tgoto:
		if(t->cse==nil || t->cse->nlab<0 || t->cse->nlab>Dis64LayoutLeaves/4)
			return layoutfail(p,"case-table-shape");
		n=t->cse->nlab;
		oldbytes=t->kind==Tcasel ? 12+24*n : t->kind==Tgoto ? 4+4*n+(t->cse->iwild!=nil?4:0) : 8+12*n;
		if(t->size!=oldbytes) return layoutfail(p,"case-table-size");
		if(!layoutleaf(p,old,at,4,4,Tint)) return 0;
		if(t->kind==Tgoto){
			for(j=4;j<oldbytes;j+=4) if(!layoutleaf(p,old+j,at+j,4,4,Dis64LayoutCode)) return 0;
			*size=oldbytes; *alignment=4; return 1;
		}
		cursor=t->kind==Tcasel ? 8 : 4;
		for(j=0;j<n;j++){
			target=t->kind==Tcasec ? (cursor+7)&~7U : cursor;
			if(t->kind==Tcasel){
				if(!layoutleaf(p,old+8+j*24,at+target,8,8,Tbig) ||
				   !layoutleaf(p,old+16+j*24,at+target+8,8,8,Tbig) ||
				   !layoutleaf(p,old+24+j*24,at+target+16,4,4,Dis64LayoutCode)) return 0;
				cursor=target+24;
			}else if(t->kind==Tcasec){
				if(!layoutleaf(p,old+4+j*12,at+target,4,8,Tstring) ||
				   !layoutleaf(p,old+8+j*12,at+target+8,4,8,Tstring) ||
				   !layoutleaf(p,old+12+j*12,at+target+16,4,4,Dis64LayoutCode)) return 0;
				cursor=target+24;
			}else{
				if(!layoutleaf(p,old+4+j*12,at+target,4,4,Tint) ||
				   !layoutleaf(p,old+8+j*12,at+target+4,4,4,Tint) ||
				   !layoutleaf(p,old+12+j*12,at+target+8,4,4,Dis64LayoutCode)) return 0;
				cursor=target+12;
			}
		}
		if(!layoutleaf(p,old+oldbytes-4,at+cursor,4,4,Dis64LayoutCode)) return 0;
		a=t->kind==Tcase?4:8; *size=(cursor+4+a-1)&~(a-1); *alignment=a; return 1;
	case Tiface:
		/* Variable-length encoded import names need their own writer, never scalar access. */
		if(t->size && !layoutleaf(p,old,at,t->size,t->size,Dis64LayoutOpaque)) return 0;
		*size=t->size; *alignment=4; return 1;
	case Ttuple: case Tadt: case Texception: case Tadtpick:
		for(i=0;i<depth;i++) if(stack[i]==t) return layoutfail(p,"by-value-cycle");
		stack[depth++]=t;
		if(t->tags != nil) return layoutfail(p,"pick-base-needs-variant");
		if(t->kind == Tadtpick){
			if(t->decl == nil || t->decl->dot == nil || (parent=t->decl->dot->ty) == nil || parent->kind != Tadt)
				return layoutfail(p,"pick-parent");
			cursor=a=4;
			if(!layoutleaf(p,old,at,4,4,Tint) ||
			   !layoutfields(parent->ids,old,at,t->size,&cursor,&a,p,stack,depth)) return 0;
		}
		if(!layoutfields(t->ids,old,at,t->size,&cursor,&a,p,stack,depth)) return 0;
		bytes=(cursor+a-1)&~(a-1);
		if(bytes>Dis64LayoutBytes) return layoutfail(p,"object-limit");
		*size=bytes; *alignment=a; return 1;
	default: return layoutfail(p,"unsupported-type-kind");
	}
	if(t->size != oldbytes) return layoutfail(p,"legacy-scalar-width");
	if(bytes && !layoutleaf(p,old,at,oldbytes,bytes,t->kind)) return 0;
	*size=bytes; *alignment=a; return 1;
}

int
dis64layouttype(Type *t, Dis64Layout *p)
{
	Type *stack[128];
	unsigned int i;
	if(p->leaves != nil || p->reason != nil) return layoutfail(p,"nonempty-output");
	if(!layouttype(t,0,0,&p->bytes,&p->alignment,p,stack,0)) return 0;
	p->legacybytes=t->size;
	if(p->count) qsort(p->leaves,p->count,sizeof(*p->leaves),layoutorder);
	for(i=0;i<p->count;i++){
		Dis64LayoutLeaf *l=&p->leaves[i];
		if(l->legacy>p->legacybytes || l->oldbytes>p->legacybytes-l->legacy ||
		   (i && l->legacy<p->leaves[i-1].legacy+p->leaves[i-1].oldbytes))
			return layoutfail(p,"type-leaf-overlap-or-bounds");
	}
	return 1;
}

/* Exact scalar/reference reads only. Padding, partial roots and cross-field spans refuse. */
int
dis64layoutaddress(Dis64Layout *p, unsigned int old, unsigned int width, int reference, unsigned int *result)
{
	Dis64LayoutLeaf *l;
	unsigned int i;
	if(p->reason != nil || width == 0 || old>p->legacybytes || width>p->legacybytes-old) return 0;
	for(i=0;i<p->count;i++){
		l=&p->leaves[i];
		if(old<l->legacy || old-l->legacy>=l->oldbytes) continue;
		if(reference){
			if((reference==1?!layoutroot(l):reference!=-1 || l->kind!=Dis64LayoutBorrowed) ||
			   old!=l->legacy || width!=4) return 0;
		}else if(l->kind<0 || l->bytes!=l->oldbytes || width>l->oldbytes-(old-l->legacy)) return 0;
		*result=l->native+old-l->legacy; return 1;
	}
	return 0;
}

void
dis64layoutfree(Dis64Layout *p)
{
	free(p->leaves); memset(p,0,sizeof(*p));
}

static int
layoutorder(void *a, void *b)
{
	const Dis64LayoutLeaf *x=a, *y=b;
	return x->legacy<y->legacy ? -1 : x->legacy>y->legacy;
}

/* Combine actual parameter/local declarations, retaining the compiler's shared local identities. */
int
dis64layoutstorage(Decl *args, Decl *locals, Desc *desc, int fp, Dis64Layout *p)
{
	Decl *d, *lists[2];
	Dis64Layout child;
	unsigned int group,i,j,at=fp?64:0,target,old,bit,work=0,mapbytes;
	uchar *map;
	long aliasbase;
	if(p->leaves!=nil || p->reason!=nil) return layoutfail(p,"nonempty-output");
	if(desc==nil || desc->size<0 || desc->size>Dis64LayoutBytes || desc->nmap<0 ||
	   desc->nmap>(desc->size+31)/32 || (desc->nmap && desc->map==nil)) return layoutfail(p,"descriptor-bounds");
	p->legacybytes=desc->size; p->alignment=8;
	lists[0]=args; lists[1]=locals;
	for(group=0;group<2;group++) for(d=lists[group];d!=nil;d=d->next){
		if(d->store!=Darg && d->store!=Dlocal && d->store!=Dglobal) continue;
		if(d->ty==nil || d->offset<(fp?32:0) || d->offset>desc->size ||
		   d->ty->size<0 || d->ty->size>desc->size-d->offset) return layoutfail(p,"storage-bounds");
		memset(&child,0,sizeof(child));
		if(!dis64layouttype(d->ty,&child)){
			layoutfail(p,child.reason); dis64layoutfree(&child); return 0;
		}
		/* Shared locals must have the identical leaf geometry, not merely the same map. */
		if(d->store==Dlocal && d->link!=nil){
			if(d->link->ty==nil || d->offset!=d->link->offset || d->ty->size!=d->link->ty->size){
				layoutfail(p,"shared-local-extent"); dis64layoutfree(&child); return 0;
			}
			j=0;
			for(i=0;i<p->count;i++) if(p->leaves[i].legacy>=d->offset && p->leaves[i].legacy<d->offset+d->ty->size) j++;
			if(j!=child.count){ layoutfail(p,"shared-local-leaves"); dis64layoutfree(&child); return 0; }
			aliasbase=-1;
			for(i=0;i<child.count;i++){
				Dis64LayoutLeaf *l=&child.leaves[i];
				for(j=0;j<p->count;j++){
					if(++work>1024*1024){ layoutfail(p,"alias-work-limit"); dis64layoutfree(&child); return 0; }
					if(p->leaves[j].legacy==d->offset+l->legacy) break;
				}
				if(j==p->count || p->leaves[j].kind!=l->kind || p->leaves[j].oldbytes!=l->oldbytes || p->leaves[j].bytes!=l->bytes){
					layoutfail(p,"shared-local-shape"); dis64layoutfree(&child); return 0;
				}
				if(aliasbase<0) aliasbase=(long)p->leaves[j].native-l->native;
				if(aliasbase<0 || aliasbase%child.alignment || aliasbase+l->native!=p->leaves[j].native){
					layoutfail(p,"shared-local-relocation"); dis64layoutfree(&child); return 0;
				}
			}
			dis64layoutfree(&child); continue;
		}
		target=(at+child.alignment-1)&~(child.alignment-1);
		for(i=0;i<child.count;i++){
			Dis64LayoutLeaf *l=&child.leaves[i]; old=d->offset+l->legacy;
			if(!layoutleaf(p,old,target+l->native,l->oldbytes,l->bytes,l->kind)){
				layoutfail(p,"storage-overlap"); dis64layoutfree(&child); return 0;
			}
		}
		at=target+child.bytes; dis64layoutfree(&child);
	}
	if(p->count) qsort(p->leaves,p->count,sizeof(*p->leaves),layoutorder);
	for(i=1;i<p->count;i++) if(p->leaves[i].legacy<p->leaves[i-1].legacy+p->leaves[i-1].oldbytes)
		return layoutfail(p,"storage-overlap");
	/* Every legacy root must be represented by exactly one typed reference leaf. */
	mapbytes=(desc->size+31)/32;
	map=allocmem(mapbytes?mapbytes:1); memset(map,0,mapbytes);
	for(i=0;i<p->count;i++) if(layoutroot(&p->leaves[i])){
		bit=p->leaves[i].legacy/4;
		if(p->leaves[i].legacy%4 || p->leaves[i].legacy+4>desc->size){
			free(map); return layoutfail(p,"unaligned-source-root");
		}
		map[bit/8]|=0x80>>(bit%8);
	}
	for(i=0;i<mapbytes;i++) if(map[i]!=(i<desc->nmap?desc->map[i]:0)){
		free(map); return layoutfail(p,"descriptor-root-role");
	}
	free(map);
	p->bytes=(at+7)&~7U;
	if(p->bytes>Dis64LayoutBytes) return layoutfail(p,"object-limit");
	return 1;
}

/* Opt-in migration evidence, independent of bytecode emission and admission. */
void
dis64layoutreport(Decl *globals, Decl **functions, int count)
{
	Dis64Layout p;
	Decl *d;
	unsigned int i,j,byte,map;
	int n;
	char *enabled=getenv("LIMBO_DIS64_LAYOUT");
	if(enabled==nil || strcmp(enabled,"1")!=0) return;
	for(n=-1;n<count;n++){
		d=n<0?nil:functions[n]; memset(&p,0,sizeof(p));
		dis64layoutstorage(n<0?globals:d->ty->ids,n<0?nil:d->locals,n<0?descriptors:d->desc,n>=0,&p);
		fprint(2,"DIS64_LAYOUT {\"schema\":\"inferno-layout-v1\",\"space\":\"%s\",\"pc\":%ld,\"descriptor\":%d,\"accepted\":%s",n<0?"mp":"fp",d==nil?-1L:d->pc->pc,n<0?descriptors->id:d->desc->id,p.reason==nil?"true":"false");
		if(p.reason!=nil) fprint(2,",\"reason\":\"%s\"",p.reason);
		else{
			fprint(2,",\"legacy_bytes\":%ud,\"bytes\":%ud,\"map\":[",p.legacybytes,p.bytes);
			for(byte=0;byte<(p.bytes+63)/64;byte++){
				map=0;
				for(i=0;i<p.count;i++) if(layoutroot(&p.leaves[i]) && p.leaves[i].native/64==byte)
					map|=0x80>>((p.leaves[i].native/8)%8);
				fprint(2,"%s%ud",byte?",":"",map);
			}
			fprint(2,"],\"leaves\":[");
			for(j=0;j<p.count;j++) fprint(2,"%s[%ud,%ud,%ud,%ud,%d]",j?",":"",p.leaves[j].legacy,p.leaves[j].native,p.leaves[j].oldbytes,p.leaves[j].bytes,p.leaves[j].kind);
			fprint(2,"]");
		}
		fprint(2,"}\n"); dis64layoutfree(&p);
	}
}
