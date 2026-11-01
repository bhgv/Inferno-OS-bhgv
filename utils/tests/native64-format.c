/* Appended after the actual shared parser and the existing owning-context tests. */
#include "native64-format.h"

extern int _efgfmt(Fmt*);
static int formatscenario; // Explicit growth/failure families, never production behavior.


typedef struct FormatTransaction FormatTransaction;

struct FormatTransaction
{
	F_Roots_caller *caller; // Generated caller owns the original result and pattern across construction.
	int capacity; // Deliberately small capacities must still consume/check every typed argument.
	int produced; // Target-entry observation, independent of publication.
};


/* Model only the bounded pool capacity used by the actual newstring implementation. */
ulong
poolmsize(Pool *pool, void *pointer)
{
	check(pool == heapmem);
	USED(pointer);
	return SlotBytes-O(Bhdr, u.data);
}


/* Supply the native lib9 error-text boundary without replacing any conversion code. */
void
rerrstr(char *output, uint bytes)
{
	if(bytes) snprintf(output, bytes, "%s", "fixture");
}


/* Supply a deterministic process diagnostic for the shared percent-r output path. */
char*
syserr(char *output, char *end, Prog *prog)
{
	USED(prog);
	return output+snprint(output, end-output+1, "%s", "typed error");
}


/* Check a trusted fixed String identity in the same canonical/live phases as the constructor. */
static int
formatprefix(void *state, const void *bytes, unsigned int size, int live)
{
	void *value;

	USED(state);
	if(size != sizeof(value)) return 0;
	memmove(&value, bytes, sizeof(value));
	if(value == nil) return 0;
	return !live || value == H || (D2H(value)->t == &Tstring && D2H(value)->ref > 0);
}


/* Format twice with independent cursors, then adopt an actual managed String result. */
static int
formatproduce(Native64Result *result, const Native64Arguments *arguments, void *state)
{
	FormatTransaction *transaction;
	String *pattern, *output;
	char first[129], second[129];
	int n, retry;

	transaction = state;
	transaction->produced++;
	epochs(6);
	memmove(&pattern, (uchar*)arguments->frame.value+64, sizeof(pattern));
	if(formatscenario){
		Native64Formatted refused = {0};
		int before;
		contextfreediagnostic = 1;
		before = contextattempt;
		check(!native64formatgrow(&process, arguments, pattern, 0, 512, &refused));
		check(contextattempt == before && refused.data == nil);
		if(formatscenario == 1){
			check(native64formatgrow(&process, arguments, pattern, 8, 400, &refused));
			check(refused.capacity == 400 && refused.length == 303);
			before = contextattempt;
			check(!native64formatgrow(&process, arguments, pattern, 8, 400, &refused));
			check(contextattempt == before && refused.length == 303);
			native64formattedclear(&refused); native64formattedclear(&refused);
			check(refused.data == nil && refused.capacity == 0 && refused.length == 0);
		}
		if(formatscenario == 2) contextfailure=contextattempt+1;
		if(formatscenario == 3) contextfailure=contextattempt+2;
		if(formatscenario == 4) contextfailure=contextattempt+7;
		if(formatscenario == 6 || formatscenario == 13) contextheapfail=1;
		if(formatscenario == 8 || formatscenario == 14) failpin=1;
		if(formatscenario == 11 || formatscenario >= 13)
			return native64formatarray(result, arguments, &process, pattern, 8, 512);
		return native64formatstring(result, arguments, &process, pattern, 8, formatscenario == 5 ? 256 : 512);
	}
	memset(first, 0x5a, sizeof(first)); memset(second, 0x5a, sizeof(second));
	n = native64format(&process, arguments, pattern, first, transaction->capacity);
	retry = native64format(&process, arguments, pattern, second, transaction->capacity);
	check(n == retry && memcmp(first, second, sizeof(first)) == 0);
	check(n >= 0 && n < transaction->capacity && first[transaction->capacity] == 0x5a);
	output = newstring(n);
	memmove(output->Sascii, first, n); output->Sascii[n] = 0;
	check(native64resultadopt(result, output));
	epochs(6);
	return 1;
}


/* Resolve the real caller owner after formatting and all allocations complete. */
static void*
formatresolve(void *state)
{
	FormatTransaction *transaction;

	transaction = state;
	epochs(6);
	if(formatscenario == 9 || formatscenario == 15) error("formatted resolver failure");
	return &transaction->caller->result;
}


/* Exercise actual parser/lib9, canonical slots, Type mismatches and bounded output through owning calls. */
static void
formatcase(int phase, int state, int mode, int capacity)
{
	char *patterntext = "%d|%*bd|%.1f|%s|%c|%d|%x|%%";
	char *expected = "-23|   -2|1.5|ok|λ|-256|ff|%";
	char growexpected[512]; // Independent exact-length reference, including embedded NUL.
	uchar map[] = {0, 0x80};
	LimboNative64Entry fixed = {"format", nil, 0, 72, 2, map};
	Native64TailSlot slots[8];
	uvlong values[8];
	Native64ArgumentSource source;
	FormatTransaction transaction;
	String *pattern, *text;
	Type *caller_type;
	void *prefix;
	double real;
	int i, status, baseline, expectedbytes;

	if(formatscenario) patterntext = formatscenario == 10 ? "%300d%c%s" : "%300d|%s";
	contextstart(phase, state);
	caller_type = native64frametype(&limbo_native64_Roots_caller_entry);
	check(caller_type != nil);
	memset(&transaction, 0, sizeof(transaction));
	transaction.caller = (F_Roots_caller*)stackframes(caller_type, 1);
	transaction.capacity = capacity;
	if(mode == 6){
		pattern = newrunes(strlen(patterntext));
		for(i=0; patterntext[i]; i++) pattern->Srune[i] = patterntext[i];
		pattern->Srune[i] = 0;
	}else{
		pattern = newstring(strlen(patterntext)); memmove(pattern->Sascii, patterntext, pattern->len+1);
	}
	transaction.caller->tail = (Array*)pattern;
	if(mode == 7 || formatscenario == 12 || formatscenario == 16){
		text = newrunes(2); text->Srune[0]=0x3bb; text->Srune[1]=0x3b1; text->Srune[2]=0;
	}else{
		text = newstring(2); memmove(text->Sascii, "ok", 3);
	}
	transaction.caller->result = (Array*)text;
	prefix = pattern;
	for(i=0; i<8; i++){
		slots[i].kind=Native64TailInt; slots[i].offset=i*8; slots[i].bytes=8; slots[i].type=nil;
	}
	values[0]=(uvlong)(vlong)-23; values[1]=5;
	slots[2].kind=Native64TailBig; values[2]=(uvlong)(vlong)-2;
	slots[3].kind=Native64TailReal; real=1.5; memmove(&values[3], &real, 8);
	slots[4].kind=Native64TailRef; slots[4].type=&Tstring; values[4]=(uvlong)(uintptr)text;
	values[5]=0x3bb; slots[6].kind=Native64TailFix; values[6]=(uvlong)(vlong)-256;
	slots[7].kind=Native64TailByte; values[7]=255;
	expectedbytes = strlen(expected);
	if(mode == 5){ values[4]=(uvlong)(uintptr)H; expected="-23|   -2|1.5||λ|-256|ff|%"; expectedbytes=strlen(expected); }
	if(mode == 7){ expected="-23|   -2|1.5|λα|λ|-256|ff|%"; expectedbytes=strlen(expected); }
	if(mode == 8){ values[5]=0; expected="-23|   -2|1.5|ok|\0|-256|ff|%"; expectedbytes=sizeof("-23|   -2|1.5|ok|\0|-256|ff|%")-1; }
	if(mode == 1){ slots[0].kind=Native64TailBig; }
	if(mode == 2){ slots[4].type=&Tarray; values[4]=(uvlong)(uintptr)H; }
	if(mode == 3){ slots[7].kind=Native64TailReal; }
	memset(&source, 0, sizeof(source));
	source.fixed=&fixed; source.prefix=&prefix; source.prefix_bytes=8;
	source.slots=slots; source.count=mode == 4 ? 7 : 8;
	source.values=values; source.value_bytes=source.count*8;
	source.prefix_check=formatprefix; source.reference_check=contextreference;
	if(formatscenario){
		slots[1].kind=Native64TailRef; slots[1].type=&Tstring; values[1]=(uvlong)(uintptr)text;
		source.count=2;
		if(formatscenario == 7){ slots[1].type=&Tarray; values[1]=(uvlong)(uintptr)H; }
		expected=growexpected;
		expectedbytes=snprint(growexpected, sizeof(growexpected), "%300d|%s", -23, "ok");
		if(formatscenario == 12 || formatscenario == 16){
			expectedbytes=snprint(growexpected, sizeof(growexpected), "%300d|", -23);
			memmove(growexpected+expectedbytes, "λα", 4); expectedbytes+=4;
		}
		if(formatscenario == 10){
			slots[1].kind=Native64TailInt; slots[1].type=nil; values[1]=0;
			slots[2].kind=Native64TailRef; slots[2].type=&Tstring; values[2]=(uvlong)(uintptr)text;
			source.count=3;
			expectedbytes=snprint(growexpected, sizeof(growexpected), "%300d", -23);
			growexpected[expectedbytes++]=0;
			memmove(growexpected+expectedbytes, "ok", 2); expectedbytes+=2;
		}
		source.value_bytes=source.count*8;
	}
	baseline = contextlive;
	allocationcollections = 1;
	status = native64argumentscall(&source, formatproduce, formatresolve, &transaction);
	contextfreediagnostic = 0;
	allocationcollections = contextheapfail = failpin = contextfailure = 0;
	if(formatscenario) check(cleanupobservations > 0);
	check(status == (formatscenario ? formatscenario == 1 || formatscenario == 10 || formatscenario == 11 || formatscenario == 12 || formatscenario == 16 : mode == 0 || mode >= 5) && transaction.produced == 1 && errdepth == 0);
	check(contextlive == baseline && freed[4] == 1);
	if(formatscenario ? formatscenario == 1 || formatscenario == 10 || formatscenario == 11 || formatscenario == 12 || formatscenario == 16 : mode == 0 || mode >= 5){
		String *output;
		if(formatscenario) check(strcmp(up->env->errstr, "initial argument error") == 0);
		output = (String*)transaction.caller->result;
		if(formatscenario == 11 || formatscenario == 16){
			Array *bytes;
			bytes = (Array*)output;
			check(bytes->len == expectedbytes && memcmp(bytes->data, expected, expectedbytes) == 0);
		}else if(formatscenario == 12){
			check(output->len == -303 && output->Srune[300] == '|' && output->Srune[301] == 0x3bb && output->Srune[302] == 0x3b1);
		}else if(capacity == 128 || formatscenario) check(output->len == expectedbytes && memcmp(output->Sascii, expected, output->len) == 0);
		check(freed[3] == 1 && !freed[5]);
	}else{
		check(transaction.caller->result == (Array*)text && text->len == 2 && D2H(text)->ref == 1);
		if(formatscenario){
			char *failure;
			failure = (formatscenario == 6 || formatscenario == 13) ? exHeap : formatscenario == 7 ? exInval :
				(formatscenario == 8 || formatscenario == 14) ? "ptradd malloc" : (formatscenario == 9 || formatscenario == 15) ? "formatted resolver failure" : exNomem;
			check(strcmp(up->env->errstr, failure) == 0);
		}else
			check(strcmp(up->env->errstr, mode == 4 ? exBounds : exInval) == 0);
	}
	epochs(6);
	freeptrs(transaction.caller, caller_type); initmem(caller_type, transaction.caller);
	process.R.EX = process.R.SP = nil;
	epochs(6); freetype(caller_type); clearrootcache();
	check(contextlive == 0 && Tstring.ref == 1 && Tarray.ref == 1 && Tbyte.ref == 1);
	heapmonitor = nil;
}


/* Keep existing ownership controls and add mixed typed formats across every collector/process state. */
int
main(int argc, char **argv)
{
	int phase, state, mode, capacity, cases;

	native64_arguments_main(argc, argv);
	fmtinstall('f', _efgfmt);
	cases = 0;
	for(phase=0; phase<3; phase++)
		for(state=Palt; state<=Pbroken; state++)
			for(mode=0; mode<9; mode++)
				for(capacity=1; capacity<=128; capacity*=2){
					formatcase(phase, state, mode, capacity);
					cases++;
				}
	for(formatscenario=1; formatscenario<=16; formatscenario++)
		for(phase=0; phase<3; phase++)
			for(state=Palt; state<=Pbroken; state++)
				formatcase(phase, state, 0, 128);
	formatscenario = 0;
	printf("PASS bounded typed growth: 384 allocation/limit/heap/pin/resolver/UTF/String/array/embedded-NUL transactions\n");
	printf("PASS actual typed formatter: %d mixed-format/Type/bounds transactions, actual lib9/heap/collector/results\n", cases);
	return 0;
}
