#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "raise.h"
#include <setjmp.h>
#include <stdio.h>

typedef unsigned short CompareAddress;

REG R;
char exNilref[] = "nil", exHeap[] = "heap";
static jmp_buf boundary;
static char *raised;


/* Preserve the precise VM error without publishing a branch PC. */
void
error(char *message)
{
	raised = message;
	longjmp(boundary, 1);
}


/* Keep address/stack instrumentation without process-based leak inspection. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

#define OP(name) void name(void) // Extracted production branch operators.
IMPLEMENTATION


/* Model precise prefix spans without forging inaccessible host allocations. */
int
main(void)
{
	uchar *allocation, *base, leftbefore[4096], rightbefore[4096];
	String *objects[2], *left, *right, *source;
	uintptr aligned;
	uvlong begin, bytes;
	Rune rune;
	REG saved, expected;
	Inst code[3], *target = code+2;
	void (*ops[])(void) = {beqc, bnec, bltc, blec, bgtc, bgec};

	union {
		uvlong align; // Natural base alignment; individual slots vary by byte.
		uchar bytes[128]; // Operand slots and untouched surrounding canaries.
	} slots;
	uchar slotsbefore[128];
	char *why;
	int offsets[] = {0xfff8, 0xfff0, 0xffe8, 0xffe0, 0xffc0, 0xff00};
	int lengths[] = {1, 7, 128, 257};
	int side, aw, bw, offset, li, mode, op, alignment, i, n1, n2, common;
	int order, take, a, b, answer, checks = 0;

	allocation = malloc(0x40000);
	if(allocation == nil)
		abort();
	aligned = ((uintptr)allocation+0xffff)&~(uintptr)0xffff;
	base = (uchar*)aligned;
	for(side = 0; side < 2; side++)
	for(aw = 0; aw < 2; aw++)
	for(bw = 0; bw < 2; bw++)
	for(offset = 0; offset < nelem(offsets); offset++)
	for(li = 0; li < nelem(lengths); li++)
	for(mode = 0; mode < 17; mode++)
	for(op = 0; op < 6; op++)
	for(alignment = 0; alignment < 8; alignment++){
		if(getenv("COMPARE_C_WIDE") && (aw != atoi(getenv("COMPARE_C_WIDE")) ||
		   bw != atoi(getenv("COMPARE_C_WIDE"))))
			continue;
		if(getenv("COMPARE_C_MODE") && mode != atoi(getenv("COMPARE_C_MODE")))
			continue;
		objects[side] = (String*)(base+offsets[offset]);
		objects[side^1] = (String*)(base+0x800);
		memset(objects[0], 0, 4096);
		memset(objects[1], 0, 4096);
		objects[0]->len = aw ? -lengths[li] : lengths[li];
		objects[1]->len = bw ? -lengths[li] : lengths[li];
		objects[0]->max = objects[1]->max = lengths[li];
		for(i = 0; i < 257; i++){
			if(aw)
				objects[0]->Srune[i] = 0x41+i;
			else
				objects[0]->Sascii[i] = 0x41+i;
			if(bw)
				objects[1]->Srune[i] = 0x41+i;
			else
				objects[1]->Sascii[i] = 0x41+i;
		}
		left = objects[0];
		right = objects[1];
		if(mode == 1)
			right = left;
		if(mode == 2)
			left = H;
		if(mode == 3)
			right = H;
		if(mode == 4)
			left = nil;
		if(mode == 5)
			right = nil;
		if(mode == 6)
			objects[0]->max = -1;
		if(mode == 7)
			objects[1]->max = -1;
		if(mode == 8)
			objects[0]->len = (-0x7fffffff-1);
		if(mode == 9)
			objects[1]->len = (-0x7fffffff-1);
		if(mode == 10){
			if(aw)
				objects[0]->Srune[0] = 0x80000000U;
			else
				objects[0]->Sascii[0] = 0xff;
		}
		if(mode == 11){
			if(bw)
				objects[1]->Srune[0] = 0xffffffffU;
			else
				objects[1]->Sascii[0] = 0;
		}
		if(mode == 12)
			objects[0]->len = 0;
		if(mode == 13)
			objects[1]->len = 0;
		if(mode == 14 || mode == 16){
			objects[0]->len = aw ? -0x7fffffff : 0x7fffffff;
			objects[0]->max = 0x7fffffff;
			objects[1]->len = bw ? -1 : 1;
		}
		if(mode == 15){
			objects[1]->len = bw ? -0x7fffffff : 0x7fffffff;
			objects[1]->max = 0x7fffffff;
			objects[0]->len = aw ? -1 : 1;
		}
		if(mode == 16)
			right = left;
		memmove(leftbefore, objects[0], sizeof(leftbefore));
		memmove(rightbefore, objects[1], sizeof(rightbefore));
		why = left == nil || right == nil ? exNilref : nil;
		n1 = n2 = order = 0;
		if(why == nil){
			for(i = 0; i < 2; i++){
				source = i == 0 ? left : right;
				if(source != H && (source->len == (-0x7fffffff-1) ||
				   source->max < (source->len < 0 ? -source->len : source->len)))
					why = exHeap;
			}
			if(why == nil && left != right){
				n1 = left == H ? 0 : left->len < 0 ? -left->len : left->len;
				n2 = right == H ? 0 : right->len < 0 ? -right->len : right->len;
				common = n1 < n2 ? n1 : n2;
				if(common != 0)
					for(i = 0; i < 2; i++){
						source = i == 0 ? left : right;
						begin = ((uintptr)source&0xffff)+offsetof(String, data);
						bytes = (uvlong)common*(source->len < 0 ? sizeof(Rune) : 1);
						if(begin+bytes > 0xffff)
							why = exHeap;
					}
				if(why == nil){
					for(i = 0; i < common; i++){
						if(left->len < 0){
							memmove(&rune, left->Srune+i, sizeof(rune));
							a = (int)rune;
						}else
							a = (uchar)left->Sascii[i];
						if(right->len < 0){
							memmove(&rune, right->Srune+i, sizeof(rune));
							b = (int)rune;
						}else
							b = (uchar)right->Sascii[i];
						if(a != b){
							order = a < b ? -1 : 1;
							break;
						}
					}
					if(order == 0)
						order = (n1 > n2)-(n1 < n2);
				}
			}
		}
		take = why == nil && (op == 0 ? order == 0 : op == 1 ? order != 0 :
			op == 2 ? order < 0 : op == 3 ? order <= 0 : op == 4 ? order > 0 : order >= 0);
		memset(&R, 0, sizeof(R));
		memset(slots.bytes, 0xa5, sizeof(slots.bytes));
		R.s = slots.bytes+alignment;
		R.m = slots.bytes+32+7-alignment;
		R.d = take ? slots.bytes+64+alignment : (void*)1;
		R.PC = code;
		memmove(R.s, &left, sizeof(left));
		memmove(R.m, &right, sizeof(right));
		memmove(slots.bytes+64+alignment, &target, sizeof(target));
		memmove(slotsbefore, slots.bytes, sizeof(slotsbefore));
		saved = expected = R;
		if(take)
			expected.PC = target;
		if(op == 0){
			raised = nil;
			if(setjmp(boundary) == 0){
				answer = stringcmp(left, right);
				if(why != nil || (answer > 0)-(answer < 0) != order)
					abort();
			}else if(why == nil || raised != why)
				abort();
			if(memcmp(&R, &saved, sizeof(R)))
				abort();
		}
		raised = nil;
		if(setjmp(boundary) == 0){
			ops[op]();
			if(why != nil)
				abort();
		}else if(why == nil || raised != why)
			abort();
		if(memcmp(&R, &expected, sizeof(R)) || memcmp(slots.bytes, slotsbefore, sizeof(slotsbefore)) ||
		   memcmp(objects[0], leftbefore, sizeof(leftbefore)) || memcmp(objects[1], rightbefore, sizeof(rightbefore)))
			abort();
		checks++;
	}
	free(allocation);
	printf("PASS C string comparison spans: %d prefix/identity/error/target/image cases WORD%zu\n", checks, sizeof(WORD)*8);
	return 0;
}
