/* Append after actual owning formatter and port-error/collector services. */
#define LIMBO_NATIVE64_PREFIXES
#include "sys-native64.h"
#include "native64-sys-format.h"

typedef struct SysFormatTest SysFormatTest;

struct SysFormatTest
{
	F_Roots_caller *caller; // Real mapped caller result/pattern owners.
	int mode, resolves; // Explicit failure family and late resolver observations.
};


/* Collect before returning the live logical owner; cancellation/error must preserve its prior value. */
static void*
sysformatresolve(void *context)
{
	SysFormatTest *test;

	test = context;
	test->resolves++;
	epochs(6);
	if(test->mode == 7){ kwerrstr("%s", "Sys format cancelled"); return nil; }
	if(test->mode == 8) error("Sys format resolver error");
	return &test->caller->result;
}


/* Check generated Sys gateways through actual String/array results, managed roots and all refusal stages. */
static void
sysformatcase(int phase, int state, int array, int mode)
{
	Native64SysFormatRequest request;
	Native64TailSlot slots[3];
	uvlong values[3];
	SysFormatTest test;
	Type *caller_type;
	String *pattern, *text;
	void *old, *heldold, *result;
	int i, baseline, before, status, success, resolves;
	char *expected;

	contextstart(phase, state);
	caller_type = native64frametype(&limbo_native64_Roots_caller_entry);
	check(caller_type != nil);
	memset(&test, 0, sizeof(test));
	test.caller = (F_Roots_caller*)stackframes(caller_type, 1);
	test.mode = mode;
	pattern = newstring(9); memmove(pattern->Sascii, "%d|%s|%c", 9); pattern->len=8;
	test.caller->tail = (Array*)pattern;
	text = newstring(2); memmove(text->Sascii, "ok", 3);
	ptradd(D2H(text));
	if((array ^ (mode == 6)) || mode == 14 || mode == 21){
		Array *bytes;
		bytes = H2D(Array*, heaparray(mode == 21 ? &Tword : &Tbyte, 1)); bytes->data[0]=0x71;
		old = bytes;
	}else{
		String *string;
		string = newstring(1); string->Sascii[0]='q'; string->Sascii[1]=0;
		old = string;
	}
	heldold=nil;
	if(mode == 22){ heldold=old; ptradd(D2H(heldold)); old=H; }
	test.caller->result = old;
	for(i=0; i<3; i++){
		slots[i].kind=Native64TailInt; slots[i].offset=i*8; slots[i].bytes=8; slots[i].type=nil;
	}
	values[0]=(uvlong)(vlong)-1;
	slots[1].kind=Native64TailRef; slots[1].type=&Tstring; values[1]=(uvlong)(uintptr)text;
	values[2]=0x3bb;
	memset(&request, 0, sizeof(request));
	request.prog=&process; request.pattern=pattern;
	request.slots=slots; request.count=3; request.values=values; request.value_bytes=sizeof(values);
	request.resolve=sysformatresolve; request.argument=&test; request.initial=8; request.limit=256;
	if(mode == 1){ request.count=2; request.value_bytes=16; }
	if(mode == 2) slots[0].kind=Native64TailBig;
	if(mode == 3) values[0]=0xffffffffULL;
	if(mode == 4) request.pattern=H;
	if(mode == 5) request.pattern=nil;
	if(mode == 9) contextheapfail=1;
	if(mode == 10) request.limit=8;
	if(mode == 11) request.resolve=nil;
	if(mode == 12) contextfailure=contextattempt+1;
	if(mode == 13) request.initial=0;
	if(mode == 14) request.pattern=old;
	if(mode == 15) values[1]=(uvlong)(uintptr)H;
	if(mode == 16) request.prog=nil;
	if(mode == 17) request.value_bytes=16;
	if(mode == 18) values[1]=0;
	if(mode == 19) request.count=Native64BindEntries+1;
	if(mode == 20) request.slots=nil;
	baseline = contextlive;
	before = contextattempt;
	allocationcollections=1;
	status = array ? Sys_aprint64(&request) : Sys_sprint64(&request);
	allocationcollections=contextheapfail=contextfailure=0;
	success = mode == 0 || mode == 4 || mode == 15 || mode == 22;
	resolves = success || mode == 6 || mode == 7 || mode == 8 || mode == 21;
	check(status == success && test.resolves == resolves && errdepth == 0 && contextlive == baseline);
	check(D2H(text)->ref == 1 && !freed[2] && !freed[3]);
	if(mode == 3 || mode == 5 || mode == 11 || mode == 13 || mode == 14 || (mode >= 16 && mode <= 20)) check(contextattempt == before);
	if(success){
		result = test.caller->result;
		check(result != old && freed[4] == (mode == 22 ? 0 : 1) && !freed[6]);
		if(array){
			Array *bytes;
			bytes = result;
			expected = mode == 15 ? "-1||λ" : "-1|ok|λ";
			check(bytes->t == &Tbyte && bytes->len == (mode == 4 ? 0 : strlen(expected)));
			if(mode != 4) check(memcmp(bytes->data, expected, bytes->len) == 0);
		}else{
			String *string;
			string = result;
			check(string->len == (mode == 4 ? 0 : mode == 15 ? -5 : -7));
			if(mode != 4) check(string->Srune[-string->len-1] == 0x3bb);
		}
	}else{
		check(test.caller->result == old && !freed[4] && D2H(old)->ref == 1);
		if(mode == 3 || mode == 5 || mode == 11 || mode == 13 || mode == 14 || (mode >= 16 && mode <= 20))
			check(strcmp(up->env->errstr, "initial argument error") == 0);
		if(mode == 1) check(strcmp(up->env->errstr, exBounds) == 0);
		if(mode == 2) check(strcmp(up->env->errstr, exInval) == 0);
		if(mode == 6 || mode == 7 || mode == 8 || mode == 21) check(freed[6] == 1);
		if(mode == 6 || mode == 21) check(strcmp(up->env->errstr, exType) == 0);
		if(mode == 7) check(strcmp(up->env->errstr, "Sys format cancelled") == 0);
		if(mode == 8) check(strcmp(up->env->errstr, "Sys format resolver error") == 0);
		if(mode == 9) check(strcmp(up->env->errstr, exHeap) == 0);
		if(mode == 10 || mode == 12) check(strcmp(up->env->errstr, exNomem) == 0);
	}
	if(heldold != nil){ ptrdel(D2H(heldold)); destroy(heldold); }
	ptrdel(D2H(text)); destroy(text);
	freeptrs(test.caller, caller_type); initmem(caller_type, test.caller);
	process.R.EX=process.R.SP=nil;
	epochs(6); freetype(caller_type); clearrootcache();
	check(contextlive == 0 && Tstring.ref == 1 && Tarray.ref == 1 && Tbyte.ref == 1 && Tword.ref == 1 && errdepth == 0);
	heapmonitor=nil;
}


/* Retain all previous controls and run both exact generated Sys return contracts across GC/process states. */
int
main(int argc, char **argv)
{
	int phase, state, array, mode, cases;

	native64_formatter_main(argc, argv);
	cases=0;
	for(phase=0; phase<3; phase++)
		for(state=Palt; state<=Pbroken; state++)
			for(array=0; array<2; array++)
				for(mode=0; mode<23; mode++){
					sysformatcase(phase, state, array, mode);
					cases++;
				}
	printf("PASS generated native64 Sys format gateways: %d String/array/identity/GC/error transactions\n", cases);
	return 0;
}
