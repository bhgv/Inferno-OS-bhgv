#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int32_t WORD;
#define nil NULL
#define H ((void*)-1)
enum { EHorizontal = 1, EVertical, ESeparator, EText };
enum { IRFF = 14, IRRew, IRUp, IRDn, IRSelect };
typedef struct Prefab_Element { int kind; void *tag; } Prefab_Element;
typedef struct List List;
typedef struct PElement PElement;
struct List { List *tail; unsigned char data[sizeof(PElement*)]; };
struct PElement { struct { int ref; } heap; Prefab_Element e; int pkind, nkids; List *first; };
typedef struct PCompound { void *display; } PCompound;
typedef struct Result { int t0, t1; Prefab_Element *t2; } Result;
typedef struct F_Compound_select { Result *ret; PCompound *comp; PElement *elem; int i; void *c; } F_Compound_select;
#define D2H(p) (&(p)->heap)
static int live, calls, failat, locked, highlighted, receives, checks;
static PCompound compound;

/* Preserve allocation checking without unsupported LeakSanitizer enumeration. */
int __lsan_is_turned_off(void) { return 1; }

/* Fault one native allocation and retain ownership accounting for vectors/values. */
static void*
selectionmalloc(size_t n)
{
	void *p;
	if(++calls == failat)
		return nil;
	assert(n > 0 && n < 1000000);
	p = malloc(n);
	assert(p != nil);
	live++;
	return p;
}

/* realloc failure must leave the old vector alive for production cleanup. */
static void*
selectionrealloc(void *p, size_t n)
{
	void *q;
	if(++calls == failat)
		return nil;
	assert(n > 0 && n < 1000000);
	q = realloc(p, n);
	assert(q != nil);
	if(p == nil)
		live++;
	return q;
}

/* Track cleanup of empty, nested and partially collected vectors. */
static void
selectionfree(void *p)
{
	if(p != nil)
		live--;
	free(p);
}

/* Boundary stubs validate display lock order and observable highlight state. */
static PCompound* checkcompound(PCompound *p) { assert(p == &compound); return p; }
static PElement* lookupelement(PElement *p) { return p; }
static int lockdisplay(void *d) { assert(d == compound.display && locked++ == 0); return 1; }
static void unlockdisplay(void *d) { assert(d == compound.display && --locked == 0); }
static void flushimage(void *d, int visible) { assert(d == compound.display && visible == 1 && locked == 1); }
static void destroy(void *p) { assert(p == H); }

/* Resolve ordinary selection by index without borrowing any traversal results. */
static PElement*
element(PElement *p, int index, int *actual)
{
	List *l = p->first;
	int i;
	for(i = 0; i < index && l != H; i++)l = l->tail;
	if(actual != nil)*actual = i;
	return l == H ? H : *(PElement**)l->data;
}

/* All fixture inputs have a valid initial selection index. */
static int wrapelement(PElement *p, int index, int n) { (void)p; (void)n; assert(index == 0); return index; }

/* Highlights may change only while the caller holds the display lock. */
static void
dohighlight(PCompound *pc, PElement *list, PElement *pe, int on)
{
	assert(pc == &compound && list != nil && pe != H && locked == 1);
	highlighted += on ? 1 : -1;
	assert(highlighted >= 0 && highlighted <= 1);
}

/* Keep untagged selection on the same highlight path as tagged selection. */
static void highlight(PCompound *pc, PElement *list, int index, int on) { dohighlight(pc, list, element(list, index, nil), on); }

/* Deliver one select event after setup, with the display unlocked while waiting. */
static void crecv(void *channel, WORD *value) { (void)channel; assert(locked == 0); receives++; *value = IRSelect; }

#define malloc selectionmalloc
#define realloc selectionrealloc
#define free selectionfree
IMPLEMENTATION
#undef malloc
#undef realloc
#undef free

/* Build native fixture lists independently of the allocation failure schedule. */
static void
children(PElement *parent, PElement *elements, List *cells, int n)
{
	parent->nkids = n;
	parent->first = n ? cells : H;
	for(int i = 0; i < n; i++){
		cells[i].tail = i+1 < n ? &cells[i+1] : H;
		memcpy(cells[i].data, &elements, sizeof elements);
		elements++;
	}
}

/* Compare a nested traversal with the explicit expected selection order. */
static void
traversal(void)
{
	PElement root, nodes[31], nested[21];
	List cells[31], subcells[21];
	PElement **found, *expected[52];
	int n, count, total, i, outer, inner, position;

	for(outer = 0; outer <= 31; outer++)
		for(inner = 0; inner <= 21; inner++){
			memset(&root, 0, sizeof root);
			memset(nodes, 0, sizeof nodes);
			memset(nested, 0, sizeof nested);
			for(i = 0; i < outer; i++){
				nodes[i].pkind = EText;
				nodes[i].e.tag = i%3 ? (void*)1 : H;
			}
			for(i = 0; i < inner; i++){
				nested[i].pkind = EText;
				nested[i].e.tag = i%4 ? (void*)1 : H;
			}
			position = outer/2;
			if(outer){
				nodes[position].pkind = EVertical;
				nodes[position].e.tag = H;
				children(&nodes[position], nested, subcells, inner);
			}
			children(&root, nodes, cells, outer);
			count = 0;
			for(i = 0; i < outer; i++){
				if(i == position){
					for(int j = 0; j < inner; j++)
						if(j%4)expected[count++] = &nested[j];
				}else if(i%3)
					expected[count++] = &nodes[i];
			}
			calls = 0; failat = 0;
			found = tags(&root, &n);
			assert(n == count);
			for(i = 0; i < n; i++)assert(found[i] == expected[i]);
			total = calls;
			selectionfree(found);
			assert(live == 0); checks++;
			for(i = 1; i <= total; i++){
				calls = 0; failat = i;
				found = tags(&root, &n);
				assert(found == nil && n < 0 && live == 0);
				checks++;
			}
		}
	failat = 0;
}

/* Exercise failed selection setup and successful return in both selection modes. */
static void
selection(void)
{
	PElement root, child;
	List cell;
	Result result;
	F_Compound_select frame;
	int tagged, fail, total;

	memset(&root, 0, sizeof root); memset(&child, 0, sizeof child);
	root.pkind = EVertical; child.pkind = child.e.kind = EText; child.e.tag = (void*)1;
	children(&root, &child, &cell, 1);
	memset(&frame, 0, sizeof frame);
	frame.ret = &result; frame.comp = &compound; frame.elem = &root;
	for(tagged = 0; tagged <= 1; tagged++){
		total = tagged ? 2 : 1;
		for(fail = 0; fail <= total; fail++){
			calls = receives = highlighted = 0; failat = fail;
			child.heap.ref = 1;
			result.t2 = H;
			doselect(&frame, tagged);
			assert(locked == 0 && highlighted == 0 && live == 0);
			if(fail){
				assert(result.t0 == 9999 && result.t2 == H && receives == 0);
				assert(child.heap.ref == 1);
			}else{
				assert(result.t0 == IRSelect && result.t1 == 0 && result.t2 == &child.e);
				assert(child.heap.ref == 2 && receives == 1);
			}
			checks++;
		}
	}
	failat = 0;
}

/* Run the production traversal and event-setup paths without a live UI server. */
int
main(void)
{
	traversal();
	selection();
	printf("prefab selection: %d checks passed\n", checks);
	return 0;
}
