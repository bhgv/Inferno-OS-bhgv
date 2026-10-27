#include "lib9.h"
#include "isa.h"
#include "interp.h"
#include "raise.h"
#include "pool.h"

void cvtup(Rune*, String*);

#define OP(fn)	void fn(void)
#define B(r)	*((BYTE*)(R.r))
#define W(r)	*((WORD*)(R.r))
#define F(r)	*((REAL*)(R.r))
#define V(r)	*((LONG*)(R.r))
#define	S(r)	*((String**)(R.r))
#define	A(r)	*((Array**)(R.r))
#define	L(r)	*((List**)(R.r))
#define P(r)	*((WORD**)(R.r))
#define C(r)	*((Channel**)(R.r))
#define T(r)	*((void**)(R.r))


/* Validate representation and bounds before publishing an alignment-safe character. */
OP(indc)
{
	WORD index, result;
	int length, capacity, wide;
	Rune rune;
	uvlong bytes;
	uintptr address;
	String *string;

	/* Bound all decoded slots before a source read, allocation or character store. */
	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(string) ||
	   R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(index) ||
	   R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(result))
		error(exHeap);

	memmove(&index, R.m, sizeof(index));
	memmove(&string, R.s, sizeof(string));
	if(string == H || string == nil)
		error(exNilref);
	if((uintptr)string > (uintptr)-1 - offsetof(String, data))
		error(exHeap);
	memmove(&length, (uchar*)string+offsetof(String, len), sizeof(length));
	memmove(&capacity, (uchar*)string+offsetof(String, max), sizeof(capacity));
	wide = length < 0;
	if(length == (-0x7fffffff-1))
		error(exHeap);
	if(length < 0)
		length = -length;
	if(capacity < length)
		error(exHeap);
	if(index < 0 || index >= length)
		error(exBounds);
	/* Check the selected character extent before forming its native address. */
	bytes = ((uvlong)index+1)*(wide ? sizeof(Rune) : 1);
	address = (uintptr)string;
	if(address > (uintptr)-1 - offsetof(String, data))
		error(exHeap);
	address += offsetof(String, data);
	if(bytes > (uintptr)-1-address)
		error(exHeap);
	if(wide) {
		memmove(&rune, (uchar*)address+(ulong)index*sizeof(Rune), sizeof(rune));
		result = (int)rune;
	} else
		result = ((uchar*)address)[index];
	memmove(R.d, &result, sizeof(result));
}


/* Capture operands and validate the representation before ownership changes.
 * Publish replacement storage only after its contents are complete. */
OP(insc)
{
	WORD index, value;
	int length, wide, shared;
	uvlong capacity, bytes;
	uintptr address;
	Rune rune;
	String *old, *result;
	void *slot;

	/* Bound all decoded slots before a source read, allocation or character store. */
	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(value) ||
	   R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(index) ||
	   R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(old))
		error(exHeap);

	memmove(&value, R.s, sizeof(value));
	rune = value;
	memmove(&index, R.m, sizeof(index));
	slot = R.d;
	memmove(&old, slot, sizeof(old));
	if(old == nil)
		error(exNilref);
	if(old != H && ((uintptr)old <= sizeof(Heap) ||
	   (((uintptr)old-sizeof(Heap)) & (sizeof(void*)-1)) != 0 ||
	   (uintptr)old > (uintptr)-1 - offsetof(String, data)))
		error(exHeap);
	length = old == H ? 0 : old->len;
	if(length == (-0x7fffffff-1))
		error(exHeap);
	if(length < 0)
		length = -length;
	if(old != H && (old->max < length || D2H(old)->ref == 0))
		error(exHeap);
	if(index < 0 || index > length)
		error(exBounds);

	wide = rune >= Runeself || (old != H && old->len < 0);
	shared = old != H && D2H(old)->ref > 1;
	if(old != H && !shared && (old->len < 0 || !wide) && index < old->max){
		/* Reuse touches only the selected cell, including a capacity-backed append. */
		bytes = ((uvlong)index+1)*(wide ? sizeof(Rune) : 1);
		address = (uintptr)old;
		if(address > (uintptr)-1 - offsetof(String, data))
			error(exHeap);
		address += offsetof(String, data);
		if(bytes > (uintptr)-1-address)
			error(exHeap);
		if(wide)
			old->Srune[index] = rune;
		else
			old->Sascii[index] = rune;
		if(index == length)
			old->len = wide ? -(length+1) : length+1;
		return;
	}

	capacity = length;
	if(index == length)
		capacity = (uvlong)length+1+length/4;
	if(capacity > 0x7fffffff)
		error(exHeap);
	/* Replacement copies the complete old payload; validate before allocation. */
	if(old != H && length != 0){
		bytes = (uvlong)length*(old->len < 0 ? sizeof(Rune) : 1);
		address = (uintptr)old;
		if(address > (uintptr)-1 - offsetof(String, data))
			error(exHeap);
		address += offsetof(String, data);
		if(bytes > (uintptr)-1-address)
			error(exHeap);
	}
	result = wide ? newrunes((int)capacity) : newstring((int)capacity);
	if(old != H){
		if(wide && old->len >= 0)
			cvtup(result->Srune, old);
		else if(wide)
			memmove(result->Srune, old->Srune, length*sizeof(Rune));
		else
			memmove(result->Sascii, old->Sascii, length);
	}
	if(wide)
		result->Srune[index] = rune;
	else
		result->Sascii[index] = rune;
	if(index == length)
		length++;
	result->len = wide ? -length : length;
	memmove(slot, &result, sizeof(result));
	destroy(old);
}


/* Validate representation and backing capacity before slicing or allocating. */
String*
slicer(ulong start, ulong v, String *ds)
{
	String *ns;
	int l, nc;
	uvlong bytes;
	uintptr address;

	if(ds == nil)
		error(exNilref);
	if(ds != H && ((uintptr)ds <= sizeof(Heap) ||
	   (((uintptr)ds-sizeof(Heap)) & (sizeof(void*)-1)) != 0 ||
	   (uintptr)ds > (uintptr)-1-(offsetof(String, max)+sizeof(ds->max))))
		error(exHeap);
	if(ds != H && ds->len == (-0x7fffffff-1))
		error(exHeap);
	if(ds == H) {
		if(start == 0 && v == 0)
			return H;

		error(exBounds);
	}

	l = ds->len < 0 ? -ds->len : ds->len;
	if(ds->max < l || D2H(ds)->ref == 0)
		error(exHeap);
	if(v < start || v > 0x7fffffff || v > l)
		error(exBounds);
	nc = v - start;
	if(nc == 0)
		return H;
	/* Allocating slices read the selected span even when their start is zero. */
	bytes = (uvlong)v*(ds->len < 0 ? sizeof(Rune) : 1);
	address = (uintptr)ds;
	if(address > (uintptr)-1 - offsetof(String, data))
		error(exHeap);
	address += offsetof(String, data);
	if(bytes > (uintptr)-1-address)
		error(exHeap);
	if(ds->len < 0) {
		ns = newrunes(nc);
		memmove(ns->Srune, &ds->Srune[start], nc*sizeof(Rune));
	}
	else {
		ns = newstring(nc);
		memmove(ns->Sascii, &ds->Sascii[start], nc);
	}

	return ns;
}


/* Capture all operands before reusing unique storage for a nonempty slice. */
OP(slicec)
{
	WORD start, end;
	String *old, *result;
	int length, count;
	uvlong bytes;
	uintptr address;
	void *slot;

	/* Reject incomplete slots before reading geometry or changing the old owner. */
	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(start) ||
	   R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(end) ||
	   R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(old))
		error(exHeap);

	slot = R.d;
	memmove(&start, R.s, sizeof(start));
	memmove(&end, R.m, sizeof(end));
	memmove(&old, slot, sizeof(old));
	if(start < 0 || end < start || (uvlong)end > 0x7fffffff)
		error(exBounds);
	if(old == nil)
		error(exNilref);
	if(old != H){
		if((uintptr)old <= sizeof(Heap) ||
		   (((uintptr)old-sizeof(Heap)) & (sizeof(void*)-1)) != 0 ||
		   (uintptr)old > (uintptr)-1-(offsetof(String, max)+sizeof(old->max)))
			error(exHeap);
		length = old->len;
		if(length == (-0x7fffffff-1))
			error(exHeap);
		if(length < 0)
			length = -length;
		if(old->max < length || D2H(old)->ref == 0)
			error(exHeap);
		if(end > length)
			error(exBounds);
		count = end-start;
		if(count != 0 && D2H(old)->ref == 1){
			/* Prefixes do not access payload; moving slices must bound their source end. */
			if(start != 0){
				bytes = (uvlong)end*(old->len < 0 ? sizeof(Rune) : 1);
				address = (uintptr)old;
				if(address > (uintptr)-1 - offsetof(String, data))
					error(exHeap);
				address += offsetof(String, data);
				if(bytes > (uintptr)-1-address)
					error(exHeap);
			}
			if(old->len < 0){
				if(start != 0)
					memmove(old->Srune, old->Srune+start, (ulong)count*sizeof(Rune));
				old->len = -count;
			}else{
				if(start != 0)
					memmove(old->Sascii, old->Sascii+start, count);
				old->len = count;
			}
			return;
		}
	}
	result = slicer((ulong)start, (ulong)end, old);
	memmove(slot, &result, sizeof(result));
	destroy(old);
}


void
cvtup(Rune *r, String *s)
{
	uchar *bp, *ep;

	bp = (uchar*)s->Sascii;
	ep = bp + s->len;
	while(bp < ep)
		*r++ = *bp++;
}


/*
 * Concatenate strings, reusing a unique destination when requested.
 * Keep source lengths unchanged until copying finishes: s1 and s2 may alias.
 */
String*
addstring(String *s1, String *s2, int append)
{
	Rune *r;
	String *ns, *source;
	uvlong bytes;
	uintptr address;
	int l, l1, l2, i;

	if(s1 == nil || s2 == nil)
		error(exNilref);
	/* Validate both headers and heap origins before any metadata read or shortcut. */
	for(i = 0; i < 2; i++){
		source = i == 0 ? s1 : s2;
		if(source == H)
			continue;
		address = (uintptr)source;
		if(address <= sizeof(Heap) ||
		   ((address-sizeof(Heap)) & (sizeof(void*)-1)) != 0 ||
		   address > (uintptr)-1-(offsetof(String, max)+sizeof(source->max)))
			error(exHeap);
	}
	if((s1 != H && s1->len == (-0x7fffffff-1)) ||
	   (s2 != H && s2->len == (-0x7fffffff-1)))
		error(exHeap);
	/* Validate both sources even when a nil operand selects a shortcut. */
	if((s1 != H && s1->max < (s1->len < 0 ? -s1->len : s1->len)) ||
	   (s2 != H && s2->max < (s2->len < 0 ? -s2->len : s2->len)))
		error(exHeap);
	if((s1 != H && D2H(s1)->ref == 0) ||
	   (s2 != H && D2H(s2)->ref == 0))
		error(exHeap);
	/* Check complete native payload spans before shortcuts, allocation or copying. */
	for(i = 0; i < 2; i++){
		source = i == 0 ? s1 : s2;
		if(source == H)
			continue;
		bytes = source->len < 0 ? (uvlong)-source->len*sizeof(Rune) : (uvlong)source->len;
		address = (uintptr)source;
		if(address > (uintptr)-1 - offsetof(String, data))
			error(exHeap);
		address += offsetof(String, data);
		if(bytes > (uintptr)-1-address)
			error(exHeap);
	}
	if(s1 == H) {
		if(s2 == H)
			return H;
		return stringdup(s2);
	}
	if(D2H(s1)->ref > 1)
		append = 0;
	if(s2 == H) {
		if(append)
			return s1;
		return stringdup(s1);
	}

	l1 = s1->len < 0 ? -s1->len : s1->len;
	l2 = s2->len < 0 ? -s2->len : s2->len;
	if(l1 > 0x7fffffff-l2)
		error(exHeap);
	l = l1+l2;
	/* Reuse must validate the extended destination span, not just its old payload. */
	if(append && l <= s1->max && !(s1->len >= 0 && s2->len < 0)){
		bytes = s1->len < 0 ? (uvlong)l*sizeof(Rune) : (uvlong)l;
		address = (uintptr)s1+offsetof(String, data);
		if(bytes > (uintptr)-1-address)
			error(exHeap);
	}
	/* Growth allowance is needed only when replacement storage must be allocated. */
	if(append && l > 0x7fffffff-l/4 &&
	   (l > s1->max || (s1->len >= 0 && s2->len < 0)))
		error(exHeap);

	if(s1->len < 0) {
		l1 = -s1->len;
		if(append && l <= s1->max)
			ns = s1;
		else {
			ns = newrunes(append? (l+l/4): l);
			memmove(ns->Srune, s1->Srune, l1*sizeof(Rune));
		}
		r = &ns->Srune[l1];
		if(s2->len < 0)
			memmove(r, s2->Srune, -s2->len*sizeof(Rune));
		else
			cvtup(r, s2);
		ns->len = -l;

		return ns;
	}

	if(s2->len < 0) {
		l2 = -s2->len;
		ns = newrunes(append? (l+l/4): l);
		ns->len = -l;
		cvtup(ns->Srune, s1);
		memmove(&ns->Srune[s1->len], s2->Srune, l2*sizeof(Rune));
		return ns;
	}

	l1 = s1->len;
	if(append && l <= s1->max)
		ns = s1;
	else {
		ns = newstring(append? (l+l/4): l);
		memmove(ns->Sascii, s1->Sascii, l1);
	}
	memmove(ns->Sascii+l1, s2->Sascii, s2->len);
	ns->len = l;

	return ns;
}


/* Capture both owners before allocation and publish before releasing the old owner. */
OP(addc)
{
	String *left, *right, *old, *result;
	void *slot;

	/* No source read or result publication precedes complete pointer-slot checks. */
	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(right) ||
	   R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(left) ||
	   R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(old))
		error(exHeap);

	slot = R.d;
	memmove(&left, R.m, sizeof(left));
	memmove(&right, R.s, sizeof(right));
	memmove(&old, slot, sizeof(old));
	result = addstring(left, right, R.m == R.d);
	if(result != old){
		memmove(slot, &result, sizeof(result));
		destroy(old);
	}
}


/* Capture source storage and reuse unique byte arrays without losing generic owners. */
OP(cvtca)
{
	String *ss;
	Array *a, *old, target;
	Heap source, previous, backing;
	Rune *r;
	uchar *bytes;
	char *p;
	uintptr address, extent;
	uvlong span;
	int l, capacity, n, size, pointers, references, reusable;

	void *slot;

	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(void*) ||
	   R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(void*))
		error(exHeap);
	slot = R.d;
	memmove(&ss, R.s, sizeof(ss));
	memmove(&old, slot, sizeof(old));
	l = 0;
	bytes = nil;
	if(ss != H){
		if(ss == nil)
			error(exNilref);
		address = (uintptr)ss;
		extent = offsetof(String, len) > offsetof(String, max) ? offsetof(String, len) : offsetof(String, max);
		if(address <= sizeof(Heap) || address > (uintptr)-1-extent-sizeof(int))
			error(exHeap);
		memmove(&source, (uchar*)ss-sizeof(Heap), sizeof(source));
		if(source.t != &Tstring)
			error(exType);
		if(source.ref == 0)
			error(exHeap);
		memmove(&l, (uchar*)ss+offsetof(String, len), sizeof(l));
		memmove(&capacity, (uchar*)ss+offsetof(String, max), sizeof(capacity));
		if(l == (-0x7fffffff-1) || capacity < (l < 0 ? -l : l))
			error(exHeap);
		if(l < 0){
			if(l < -(int)((0x7fffffff-sizeof(Heap)-sizeof(Array))/UTFmax))
				error(exHeap);
			span = (uvlong)-l*sizeof(Rune);
		}else{
			if((ulong)l > 0x7fffffff-sizeof(Heap)-sizeof(Array))
				error(exHeap);
			span = l;
		}
		/* Empty strings require no payload address or payload reads. */
		if(span != 0){
			if(address > (uintptr)-1 - offsetof(String, data) ||
			   span > (uintptr)-1-(address+offsetof(String, data)))
				error(exHeap);
			bytes = (uchar*)(address+offsetof(String, data));
		}
	}
	if(old == nil)
		error(exNilref);
	if(old != H){
		if((uintptr)old <= sizeof(Heap))
			error(exHeap);
		memmove(&previous, (uchar*)old-sizeof(Heap), sizeof(previous));
		if(previous.ref == 0)
			error(exHeap);
		/* Pointer temporaries can still own a String or ADT; only arrays are reusable. */
		if(previous.t == &Tarray && previous.ref == 1){
			if((uintptr)old > (uintptr)-1-sizeof(Array))
				error(exHeap);
			memmove(&target, old, sizeof(target));
			if(target.root == nil)
				error(exHeap);
			if(target.root != H){
				if((uintptr)target.root <= sizeof(Heap))
					error(exHeap);
				memmove(&backing, (uchar*)target.root-sizeof(Heap), sizeof(backing));
				if(backing.ref == 0)
					error(exHeap);
			}
			/* Releasing a unique old array will also release its borrowed descriptor. */
			if(target.t == nil || target.t == H)
				error(exType);
			extent = offsetof(Type, size) > offsetof(Type, np) ? offsetof(Type, size) : offsetof(Type, np);
			if(extent < offsetof(Type, ref))
				extent = offsetof(Type, ref);
			if((uintptr)target.t > (uintptr)-1-extent-sizeof(int))
				error(exType);
			memmove(&references, (uchar*)target.t+offsetof(Type, ref), sizeof(references));
			if(references <= 0)
				error(exHeap);
			memmove(&size, (uchar*)target.t+offsetof(Type, size), sizeof(size));
			memmove(&pointers, (uchar*)target.t+offsetof(Type, np), sizeof(pointers));
			reusable = l >= 0 && target.root == H && target.len >= l && (uvlong)target.len <= 0x7fffffff &&
			           (target.t == &Tbyte || (size == 1 && pointers == 0));
			if(reusable){
				if(l != 0){
					if(target.data == nil || target.data == H)
						error(exNilref);
					if((uintptr)target.data > (uintptr)-1-(uintptr)l)
						error(exHeap);
					memmove(target.data, bytes, l);
				}
				target.len = l;
				memmove((uchar*)old+offsetof(Array, len), &target.len, sizeof(target.len));
				memmove(slot, &old, sizeof(old));
				return;
			}
		}
	}
	if(l < 0){
		l = -l;
		r = (Rune*)bytes;
		n = runenlen(r, l);
		a = mem2array(nil, n);
		p = (char*)a->data;
		while(l--)
			p += runetochar(p, r++);
	}else
		a = mem2array(bytes, l);
	/* Allocation and encoding finish while the old generic owner still retains its source. */
	memmove(slot, &a, sizeof(a));
	destroy(old);
}


/* Capture source owners and reuse unique ASCII storage only after complete preflight. */
OP(cvtac)
{
	Array *a, view;
	String *ds, *old;
	Heap owner, previous, root;
	uchar *bytes;
	uintptr address, extent;
	int n, i, oldlen, capacity, size, pointers;

	void *slot;

	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(void*) ||
	   R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(void*))
		error(exHeap);
	slot = R.d;
	ds = H;
	memmove(&a, R.s, sizeof(a));
	memmove(&old, slot, sizeof(old));
	if(a != H){
		if(a == nil)
			error(exNilref);
		address = (uintptr)a;
		if(address <= sizeof(Heap) || address > (uintptr)-1-sizeof(Array))
			error(exHeap);
		memmove(&owner, (uchar*)a-sizeof(Heap), sizeof(owner));
		if(owner.t != &Tarray)
			error(exType);
		if(owner.ref == 0)
			error(exHeap);
		memmove(&view, a, sizeof(view));
		if(view.t != &Tbyte){
			if(view.t == nil || view.t == H)
				error(exType);
			extent = offsetof(Type, size) > offsetof(Type, np) ? offsetof(Type, size) : offsetof(Type, np);
			if((uintptr)view.t > (uintptr)-1-extent-sizeof(int))
				error(exType);
			memmove(&size, (uchar*)view.t+offsetof(Type, size), sizeof(size));
			memmove(&pointers, (uchar*)view.t+offsetof(Type, np), sizeof(pointers));
			if(size != 1 || pointers != 0)
				error(exType);
		}
		if(view.len < 0 || (uvlong)view.len > 0x7fffffff)
			error(exHeap);
		n = (int)view.len;
		if((ulong)n > 0x7fffffff-sizeof(Heap)-sizeof(String))
			error(exHeap);
		bytes = view.data;
		if(n != 0){
			if(bytes == nil || bytes == H)
				error(exNilref);
			if((uintptr)bytes > (uintptr)-1-(uintptr)n)
				error(exHeap);
		}
		if(view.root == nil)
			error(exHeap);
		if(view.root != H){
			if((uintptr)view.root <= sizeof(Heap))
				error(exHeap);
			memmove(&root, (uchar*)view.root-sizeof(Heap), sizeof(root));
			if(root.ref == 0)
				error(exHeap);
		}
		if(old == nil)
			error(exNilref);
		if(old != H){
			if((uintptr)old <= sizeof(Heap))
				error(exHeap);
			memmove(&previous, (uchar*)old-sizeof(Heap), sizeof(previous));
			if(previous.ref == 0)
				error(exHeap);
			/* Generic pointer temporaries must not be interpreted as String records. */
			if(previous.t == &Tstring && previous.ref == 1){
				extent = offsetof(String, len) > offsetof(String, max) ? offsetof(String, len) : offsetof(String, max);
				if((uintptr)old > (uintptr)-1-extent-sizeof(int))
					error(exHeap);
				memmove(&oldlen, (uchar*)old+offsetof(String, len), sizeof(oldlen));
				memmove(&capacity, (uchar*)old+offsetof(String, max), sizeof(capacity));
				if(oldlen >= 0 && capacity >= oldlen && capacity >= n){
					address = (uintptr)old;
					if(n != 0 && (address > (uintptr)-1 - offsetof(String, data) ||
					   address+offsetof(String, data) > (uintptr)-1-(uintptr)n))
						error(exHeap);
					/* An incomplete UTF-8 conversion must never partially change the old owner. */
					for(i = 0; i < n; i++)
						if(bytes[i] >= Runeself)
							break;
					if(i == n){
						if(n != 0)
							memmove((void*)(address+offsetof(String, data)), bytes, n);
						memmove((uchar*)old+offsetof(String, len), &n, sizeof(n));
						/* Restore a possibly overlapping owner slot after payload publication. */
						memmove(slot, &old, sizeof(old));
						return;
					}
				}
			}
		}
		ds = c2string((char*)bytes, n);
	}
	if(old == nil)
		error(exNilref);
	/* Nil input also releases an old generic owner: reject a dead owner first. */
	if(a == H && old != H){
		if((uintptr)old <= sizeof(Heap))
			error(exHeap);
		memmove(&previous, (uchar*)old-sizeof(Heap), sizeof(previous));
		if(previous.ref == 0)
			error(exHeap);
	}
	memmove(slot, &ds, sizeof(ds));
	destroy(old);
}



/* Reject invalid lengths before taking the absolute representation size. */
OP(lenc)
{
	int l;
	WORD length;
	String *ss;

	/* The output must be writable before either the owner slot or its length is read. */
	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(ss) ||
	   R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(length))
		error(exHeap);

	l = 0;
	memmove(&ss, R.s, sizeof(ss));
	if(ss == nil)
		error(exNilref);
	if(ss != H) {
		if((uintptr)ss > (uintptr)-1-(offsetof(String, len)+sizeof(ss->len)))
			error(exHeap);
		memmove(&l, (uchar*)ss+offsetof(String, len), sizeof(l));
		if(l == (-0x7fffffff-1))
			error(exHeap);
		if(l < 0)
			l = -l;
	}
	length = l;
	memmove(R.d, &length, sizeof(length));
}

/* Parse through the validated UTF-8 view and store through an unaligned slot. */
OP(cvtcw)
{
	String *s;
	WORD value;

	/* Invalid destinations must fail before UTF-8 termination/cache mutation. */
	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(s) ||
	   R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(value))
		error(exHeap);

	memmove(&s, R.s, sizeof(s));
	value = strtol(string2c(s), nil, 10);
	memmove(R.d, &value, sizeof(value));
}

/* Capture the owner before parsing, including source/destination aliases. */
OP(cvtcf)
{
	String *s;
	REAL value;

	/* Invalid destinations must fail before UTF-8 termination/cache mutation. */
	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(s) ||
	   R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(value))
		error(exHeap);

	memmove(&s, R.s, sizeof(s));
	value = strtod(string2c(s), nil);
	memmove(R.d, &value, sizeof(value));
}


/* Format the complete VM word, including its sign and terminating NUL. */
OP(cvtwc)
{
	String *ds, *old;
	char digits[24];
	int length, capacity, reusable;
	WORD value;

	void *slot;
	uintptr address, extent;

	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(value) ||
	   R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(old))
		error(exHeap);
	slot = R.d;
	memmove(&value, R.s, sizeof(value));
	memmove(&old, slot, sizeof(old));
	if(old == nil)
		error(exNilref);
	extent = offsetof(String, len) > offsetof(String, max) ? offsetof(String, len) : offsetof(String, max);
	address = (uintptr)old;
	if(old != H && (address <= sizeof(Heap) || address%sizeof(void*) != 0 ||
	   address > (uintptr)-1-extent-sizeof(int)))
		error(exHeap);
	if(old != H && D2H(old)->ref == 0)
		error(exHeap);
	/* Pointer temporaries may still own an ADT/array: only strings are reusable. */
	reusable = old != H && D2H(old)->t == &Tstring;
	if(reusable && (old->len == (-0x7fffffff-1) ||
	   old->max < (old->len < 0 ? -old->len : old->len) || D2H(old)->ref == 0))
		error(exHeap);
	capacity = 0;
	if(reusable){
		if(old->len < 0 && old->max > 0x7fffffff/(int)sizeof(Rune))
			error(exHeap);
		capacity = old->len < 0 ? old->max*(int)sizeof(Rune) : old->max;
	}
	/* Stage the full text before checking exact logical character capacity. */
	length = sprint(digits, "%lld", (vlong)value);
	if(reusable && capacity >= length && D2H(old)->ref == 1)
		ds = old;
	else
		ds = newstring(24);
	/* Keep the allocation and cache while publishing byte-oriented capacity. */
	address = (uintptr)ds;
	if(address > (uintptr)-1 - offsetof(String, data) ||
	   address+offsetof(String, data) > (uintptr)-1-(uintptr)length-1)
		error(exHeap);
	if(ds == old && old->len < 0)
		ds->max = capacity;
	memmove(ds->Sascii, digits, length+1);
	ds->len = length;
	/* Restore an owner slot overlapping formatted bytes as well as new allocations. */
	memmove(slot, &ds, sizeof(ds));
	if(ds != old)
		destroy(old);
}


/*
 * Format every signed 64-bit value, including its sign and terminating NUL.
 */
OP(cvtlc)
{
	String *ds, *old;
	char digits[24];
	int length, capacity, reusable;
	LONG value;

	void *slot;
	uintptr address, extent;

	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(value) ||
	   R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(old))
		error(exHeap);
	slot = R.d;
	memmove(&value, R.s, sizeof(value));
	memmove(&old, slot, sizeof(old));
	if(old == nil)
		error(exNilref);
	extent = offsetof(String, len) > offsetof(String, max) ? offsetof(String, len) : offsetof(String, max);
	address = (uintptr)old;
	if(old != H && (address <= sizeof(Heap) || address%sizeof(void*) != 0 ||
	   address > (uintptr)-1-extent-sizeof(int)))
		error(exHeap);
	if(old != H && D2H(old)->ref == 0)
		error(exHeap);
	/* Pointer temporaries may still own an ADT/array: only strings are reusable. */
	reusable = old != H && D2H(old)->t == &Tstring;
	if(reusable && (old->len == (-0x7fffffff-1) ||
	   old->max < (old->len < 0 ? -old->len : old->len) || D2H(old)->ref == 0))
		error(exHeap);
	capacity = 0;
	if(reusable){
		if(old->len < 0 && old->max > 0x7fffffff/(int)sizeof(Rune))
			error(exHeap);
		capacity = old->len < 0 ? old->max*(int)sizeof(Rune) : old->max;
	}
	/* Stage the full text before checking exact logical character capacity. */
	length = sprint(digits, "%lld", (vlong)value);
	if(reusable && capacity >= length && D2H(old)->ref == 1)
		ds = old;
	else
		ds = newstring(24);
	/* Keep the allocation and cache while publishing byte-oriented capacity. */
	address = (uintptr)ds;
	if(address > (uintptr)-1 - offsetof(String, data) ||
	   address+offsetof(String, data) > (uintptr)-1-(uintptr)length-1)
		error(exHeap);
	if(ds == old && old->len < 0)
		ds->max = capacity;
	memmove(ds->Sascii, digits, length+1);
	ds->len = length;
	/* Restore an owner slot overlapping formatted bytes as well as new allocations. */
	memmove(slot, &ds, sizeof(ds));
	if(ds != old)
		destroy(old);
}


/* Capture real bits and prepare storage before formatting or replacing the owner. */
OP(cvtfc)
{
	String *ds, *old;
	REAL value;
	uvlong bits;
	vlong integer;
	uint exponent;
	char digits[32];
	int length, capacity, reusable;

	void *slot;
	uintptr address, extent;

	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(value) ||
	   R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(old))
		error(exHeap);
	slot = R.d;
	memmove(&value, R.s, sizeof(value));
	memmove(&bits, &value, sizeof(bits));
	memmove(&old, slot, sizeof(old));
	if(old == nil)
		error(exNilref);
	extent = offsetof(String, len) > offsetof(String, max) ? offsetof(String, len) : offsetof(String, max);
	address = (uintptr)old;
	if(old != H && (address <= sizeof(Heap) || address%sizeof(void*) != 0 ||
	   address > (uintptr)-1-extent-sizeof(int)))
		error(exHeap);
	if(old != H && D2H(old)->ref == 0)
		error(exHeap);
	/* Pointer temporaries may still own an ADT/array: only strings are reusable. */
	reusable = old != H && D2H(old)->t == &Tstring;
	if(reusable && (old->len == (-0x7fffffff-1) ||
	   old->max < (old->len < 0 ? -old->len : old->len) || D2H(old)->ref == 0))
		error(exHeap);
	capacity = 0;
	if(reusable){
		if(old->len < 0 && old->max > 0x7fffffff/(int)sizeof(Rune))
			error(exHeap);
		capacity = old->len < 0 ? old->max*(int)sizeof(Rune) : old->max;
	}
	/* Exact binary powers in this range have ordinary integer %g spelling.
	 * Decode them as bits so native and C paths do not disturb FP status. */
	exponent = (bits>>52)&2047;
	if(bits == 0 || ((bits&0x000fffffffffffffULL) == 0 && exponent >= 1023 && exponent <= 1042)){
		integer = bits == 0 ? 0 : (vlong)1<<(exponent-1023);
		if(bits>>63)
			integer = -integer;
		length = sprint(digits, "%lld", integer);
		if(reusable && capacity >= length && D2H(old)->ref == 1)
			ds = old;
		else
			ds = newstring(32);
	}else{
		/* Reserve full formatter capacity before fallible allocation or FP work. */
		if(reusable && capacity >= sizeof(digits) && D2H(old)->ref == 1)
			ds = old;
		else
			ds = newstring(32);
		length = sprint(digits, "%g", value);
	}
	address = (uintptr)ds;
	if(address > (uintptr)-1 - offsetof(String, data) ||
	   address+offsetof(String, data) > (uintptr)-1-(uintptr)length-1)
		error(exHeap);
	if(ds == old && old->len < 0)
		ds->max = capacity;
	memmove(ds->Sascii, digits, length+1);
	ds->len = length;
	/* Restore an owner slot overlapping formatted bytes as well as new allocations. */
	memmove(slot, &ds, sizeof(ds));
	if(ds != old)
		destroy(old);
}


/*
 * Return a NUL-terminated UTF-8 view, caching rune conversion in the string.
 * Validate logical capacity and native spans before accessing payload storage.
 * Bound the temporary byte count before multiplying the rune count.
 */
char*
string2c(String *s)
{
	char *p, *buffer, *cache;
	int length, capacity, l, nc, i;
	uvlong bytes;
	uintptr address;
	Rune c;

	if(s == nil)
		error(exNilref);
	if(s == H)
		return "";

	/* Borrowed string views may have packed headers and rune storage. */
	address = (uintptr)s;
	if(address > (uintptr)-1 - offsetof(String, data))
		error(exHeap);
	memmove(&length, (uchar*)s+offsetof(String, len), sizeof(length));
	memmove(&capacity, (uchar*)s+offsetof(String, max), sizeof(capacity));
	address += offsetof(String, data);
	if(length >= 0) {
		if(capacity < length)
			error(exHeap);
		bytes = (uvlong)length+1;
		if(bytes > (uintptr)-1-address)
			error(exHeap);
		((char*)address)[length] = '\0';
		return (char*)address;
	}

	if(length < -(0x7fffffff-UTFmax)/UTFmax)
		error(exNomem);
	nc = -length;
	if(capacity < nc)
		error(exHeap);
	bytes = (uvlong)nc*sizeof(Rune);
	if(bytes > (uintptr)-1-address)
		error(exHeap);
	l = nc*UTFmax+UTFmax;
	memmove(&cache, (uchar*)s+offsetof(String, tmp), sizeof(cache));
	if(cache == nil || msize(cache) < l) {
		/* Retain the old cache until its replacement allocation succeeds. */
		buffer = malloc(l);
		if(buffer == nil)
			error(exNomem);
		free(cache);
		cache = buffer;
		memmove((uchar*)s+offsetof(String, tmp), &cache, sizeof(cache));
	}

	p = cache;
	for(i = 0; i < nc; i++) {
		memmove(&c, (uchar*)address+(ulong)i*sizeof(Rune), sizeof(c));
		if(c < Runeself)
			*p++ = c;
		else
			p += runetochar(p, &c);
	}
	*p = 0;
	return cache;
}


/* Reject invalid byte spans before scanning; retain truncated-tail behavior. */
String*
c2string(char *cs, int len)
{
	uchar *p;
	char *ecs;
	String *s;
	Rune *r, junk;
	int c, nc, isrune;

	if(len < 0)
		error(exNegsize);
	if((ulong)len > 0x7fffffff-sizeof(Heap)-sizeof(String))
		error(exHeap);
	if(len == 0)
		return newstring(0);
	if(cs == nil || cs == H)
		error(exNilref);
	/* Native callers must not wrap the end pointer before UTF scanning. */
	if((uintptr)cs > (uintptr)-1-(uintptr)len)
		error(exHeap);
	isrune = 0;
	ecs = cs+len;
	p = (uchar*)cs;
	while(len--) {
		c = *p++;
		if(c >= Runeself) {
			isrune = 1;
			break;
		}
	}

	if(isrune == 0) {
		nc = ecs - cs;
		s = newstring(nc);
		memmove(s->Sascii, cs, nc);
		return s;
	}

	p--;
	nc = p - (uchar*)cs;
	while(p < (uchar*)ecs) {
		c = *p;
		if(c < Runeself)
			p++;
		else if((uchar*)ecs-p >= UTFmax || fullrune((char*)p, (uchar*)ecs-p))
			p += chartorune(&junk, (char*)p);
		else
			break;
		nc++;
	}
	s = newrunes(nc);
	r = s->Srune;
	while(nc--)
		cs += chartorune(r++, cs);

	return s;
}


/*
 * Allocate an ASCII string with room for its terminator.
 * Validate the payload and heap headers before narrowing the allocation size.
 */
String*
newstring(int nb)
{
	Heap *h;
	String *s;

	if(nb < 0)
		error(exNegsize);
	if(Tstring.ref <= 0 || Tstring.ref == 0x7fffffff ||
	   (ulong)nb > 0x7fffffff-sizeof(Heap)-sizeof(String))
		error(exHeap);
	h = nheap(sizeof(String)+nb);
	h->t = &Tstring;
	Tstring.ref++;
	s = H2D(String*, h);
	s->tmp = nil;
	s->len = nb;
	s->max = hmsize(h) - (sizeof(String)+sizeof(Heap));
	return s;
}


/*
 * Allocate rune storage within the same heap limit as ASCII strings.
 * Preserve the historical negative-count convention without negating INT_MIN.
 */
String*
newrunes(int nr)
{
	Heap *h;
	String *s;

	if(nr == 0)
		return newstring(nr);
	if(nr == (-0x7fffffff-1))
		error(exHeap);
	if(nr < 0)
		nr = -nr;
	if(Tstring.ref <= 0 || Tstring.ref == 0x7fffffff ||
	   (ulong)nr > (0x7fffffff-sizeof(Heap)-sizeof(String))/sizeof(Rune))
		error(exHeap);
	h = nheap(sizeof(String)+nr*sizeof(Rune));
	h->t = &Tstring;
	Tstring.ref++;
	s = H2D(String*, h);
	s->tmp = nil;
	s->len = -nr;
	s->max = (hmsize(h) - (sizeof(String)+sizeof(Heap)))/sizeof(Rune);
	return s;
}


/* Reject invalid owners, lengths and capacities before allocating a duplicate. */
String*
stringdup(String *s)
{
	String *ns;
	uintptr address;
	uvlong bytes;
	int length, capacity;
	ulong references;

	if(s == nil)
		error(exNilref);
	if(s == H)
		return H;
	address = (uintptr)s;
	/* A duplicated string owns a naturally aligned Heap prefix. */
	if(address <= sizeof(Heap) ||
	   ((address-sizeof(Heap)) & (sizeof(void*)-1)) != 0 ||
	   address > (uintptr)-1-(offsetof(String, max)+sizeof(capacity)))
		error(exHeap);
	memmove(&length, (uchar*)s+offsetof(String, len), sizeof(length));
	memmove(&capacity, (uchar*)s+offsetof(String, max), sizeof(capacity));
	memmove(&references, (uchar*)s-sizeof(Heap)+offsetof(Heap, ref), sizeof(references));
	if(length == (-0x7fffffff-1) ||
	   capacity < (length < 0 ? -length : length) || references == 0)
		error(exHeap);

	/* Capture geometry before allocation; copy only the logical payload. */
	bytes = (uvlong)(length < 0 ? -length : length)*(length < 0 ? sizeof(Rune) : 1);
	if(address > (uintptr)-1 - offsetof(String, data) ||
	   bytes > (uintptr)-1-(address+offsetof(String, data)))
		error(exHeap);
	ns = length >= 0 ? newstring(length) : newrunes(-length);
	memmove((uchar*)ns+offsetof(String, data), (uchar*)s+offsetof(String, data), bytes);
	return ns;
}


/* Compare byte-safe headers and Rune prefixes; identities need no payload reads. */
int
stringcmp(String *s1, String *s2)
{
	uchar *r1, *r2;
	Rune rune;
	uchar *a1, *a2;
	int v, n, n1, n2, c1, c2, i, length1, length2, capacity1, capacity2;
	String *source;
	uvlong bytes;
	uintptr address;
	static String snil = { 0, 0, nil };

	if(s1 == nil || s2 == nil)
		error(exNilref);
	/* Both header spans precede length/capacity reads, including identities. */
	if((s1 != H && (uintptr)s1 > (uintptr)-1-(offsetof(String, max)+sizeof(s1->max))) ||
	   (s2 != H && (uintptr)s2 > (uintptr)-1-(offsetof(String, max)+sizeof(s2->max))))
		error(exHeap);
	length1 = length2 = capacity1 = capacity2 = 0;
	if(s1 != H) {
		memmove(&length1, (uchar*)s1+offsetof(String, len), sizeof(length1));
		memmove(&capacity1, (uchar*)s1+offsetof(String, max), sizeof(capacity1));
	}
	if(s2 != H) {
		memmove(&length2, (uchar*)s2+offsetof(String, len), sizeof(length2));
		memmove(&capacity2, (uchar*)s2+offsetof(String, max), sizeof(capacity2));
	}
	if(length1 == (-0x7fffffff-1) || length2 == (-0x7fffffff-1))
		error(exHeap);
	if(capacity1 < (length1 < 0 ? -length1 : length1) ||
	   capacity2 < (length2 < 0 ? -length2 : length2))
		error(exHeap);
	if(s1 == H)
		s1 = &snil;
	if(s2 == H)
		s2 = &snil;

	if(s1 == s2)
		return 0;

	v = 0;
	n1 = length1;
	if(n1 < 0) {
		n1 = -n1;
		v |= 1;
	}
	n2 = length2;
	if(n2 < 0) {
		n2 = -n2;
		v |= 2;
	}

	n = n1;
	if(n2 < n)
		n = n2;
	/* Only the common prefix is accessed; identities and empty prefixes read no data. */
	if(n != 0)
		for(i = 0; i < 2; i++){
			source = i == 0 ? s1 : s2;
			bytes = (uvlong)n*((i == 0 ? length1 : length2) < 0 ? sizeof(Rune) : 1);
			address = (uintptr)source;
			if(address > (uintptr)-1 - offsetof(String, data))
				error(exHeap);
			address += offsetof(String, data);
			if(bytes > (uintptr)-1-address)
				error(exHeap);
		}

	switch(v) {
	case 0:		/* Ascii Ascii */
		n = memcmp((char*)s1+offsetof(String, data), (char*)s2+offsetof(String, data), n);
		if(n == 0)
			n = n1 - n2;
		return n;
	case 1:		/* Rune Ascii */
		r1 = (uchar*)s1+offsetof(String, data);
		a2 = (uchar*)s2+offsetof(String, data);
		while(n > 0) {
			memmove(&rune, r1, sizeof(rune));
			r1 += sizeof(rune);
			c1 = rune;
			c2 = *a2++;
			if(c1 != c2)
				goto ne;
			n--;
		}
		break;
	case 2:		/* Ascii Rune */
		a1 = (uchar*)s1+offsetof(String, data);
		r2 = (uchar*)s2+offsetof(String, data);
		while(n > 0) {
			c1 = *a1++;
			memmove(&rune, r2, sizeof(rune));
			r2 += sizeof(rune);
			c2 = rune;
			if(c1 != c2)
				goto ne;
			n--;
		}
		break;
	case 3:		/* Rune Rune */
		r1 = (uchar*)s1+offsetof(String, data);
		r2 = (uchar*)s2+offsetof(String, data);
		while(n > 0) {
			memmove(&rune, r1, sizeof(rune));
			r1 += sizeof(rune);
			c1 = rune;
			memmove(&rune, r2, sizeof(rune));
			r2 += sizeof(rune);
			c2 = rune;
			if(c1 != c2)
				goto ne;
			n--;
		}
		break;
	}
	return n1 - n2;

ne:	if(c1 < c2)
		return -1;
	return 1;
}


/* Validate split ownership and source geometry before allocation or publication. */
String*
splitc(String **s, int expand)
{
	String *ss, *ns;
	uintptr address;
	uvlong bytes;
	int length, capacity;
	ulong references;

	/* The caller cell can be packed, but must contain a complete native owner. */
	if(s == nil || s == H || (uintptr)s > (uintptr)-1-sizeof(ss))
		error(exHeap);
	memmove(&ss, s, sizeof(ss));
	if(ss == nil)
		error(exNilref);
	length = 0;
	if(ss != H) {
		address = (uintptr)ss;
		if(address <= sizeof(Heap) ||
		   ((address-sizeof(Heap)) & (sizeof(void*)-1)) != 0 ||
		   address > (uintptr)-1-(offsetof(String, max)+sizeof(capacity)))
			error(exHeap);
		memmove(&length, (uchar*)ss+offsetof(String, len), sizeof(length));
		memmove(&capacity, (uchar*)ss+offsetof(String, max), sizeof(capacity));
		memmove(&references, (uchar*)ss-sizeof(Heap)+offsetof(Heap, ref), sizeof(references));
		if(length == (-0x7fffffff-1) || references == 0 ||
		   capacity < (length < 0 ? -length : length))
			error(exHeap);
		bytes = (uvlong)(length < 0 ? -length : length)*(length < 0 ? sizeof(Rune) : 1);
		if(address > (uintptr)-1 - offsetof(String, data) ||
		   bytes > (uintptr)-1-(address+offsetof(String, data)))
			error(exHeap);
	}
	if(expand && length > 0) {
		ns = newrunes(length);
		cvtup(ns->Srune, ss);
	} else
		ns = stringdup(ss);

	memmove(s, &ns, sizeof(ns));
	destroy(ss);
	return ns;
}
