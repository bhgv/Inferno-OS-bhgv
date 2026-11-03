#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lib9.h"
#include "isa.h"
#include "interp.h"
#include "runt.h"
#include "raise.h"
#include "kernel.h"

int cflag; // Independent global public-compilation request.
char exNilref[] = "nil result"; // Production loaderword error identity.
static char diagnostic[80]; // Observe public errors without replacing the loader body.
static int calls, protects, callbackcalls, compilerok; // Inject only compiler/protected-call boundaries.
static Modlink *instance; // Borrowed live instance used by the compiler observation.
static Inst replacement[2]; // Synthetic successful compiler publication, never executed.
static Jit cache; // Opaque successful compiler cache, never used as machine code.


/* Require a semantic predicate rather than accepting a crash as guard detection. */
static void
check(int condition)
{
	if(!condition){
		fprintf(stderr, "loader compile mode mismatch\n");
		exit(10);
	}
}


/* Model the trusted profile's symbol identity, not its VM body; public requests must refuse it too. */
int
native64lea8step(void)
{
	callbackcalls++;
	return 1;
}


/* Keep a distinct foreign translation identity in the mode matrix. */
static int
stepb(void)
{
	callbackcalls++;
	return 0;
}


/* Preserve exact public diagnostic text; these production paths use literal strings. */
void
kwerrstr(char *message, ...)
{
	snprintf(diagnostic, sizeof(diagnostic), "%s", message);
}


/* Invalid result storage is outside this fixture and must not be counted as a control. */
void
error(char *message)
{
	fprintf(stderr, "unexpected loader VM error: %s\n", message);
	exit(11);
}


/* Observe MP selection and simulate only compiler success/refusal, not native emission. */
int
compile(Module *module, int count, Modlink *link)
{
	check(link == instance && module == instance->m && count == module->nprog);
	check(module->origmp == instance->MP);
	calls++;
	if(compilerok){
		module->prog = replacement;
		module->jit = &cache;
	}
	return compilerok;
}


/* Trace protection entry; nonlocal-error restoration has independent existing gates. */
int
kprotect(int (*function)(void*), void *argument)
{
	protects++;
	return function(argument);
}

LOADER_IMPLEMENTATION


/* Keep nil/H public owners ahead of all mode reads and compiler side effects. */
static int
owners(void)
{
	Module module, saved;
	Modlink link, savedlink;
	F_Loader_compile request;
	uchar bytes[32], wanted[32];
	WORD result;
	int which, shift, count;

	count = 0;
	for(which=0; which<4; which++)
	for(shift=0; shift<8; shift++){
		memset(&module, 0, sizeof(module));
		memset(&link, 0, sizeof(link));
		memset(&request, 0, sizeof(request));
		link.m = which == 2 ? nil : which == 3 ? H : &module;
		request.mp = which == 0 ? nil : which == 1 ? H : &link;
		request.ret = (WORD*)(bytes+shift);
		request.flag = cflag = 1;
		calls = protects = callbackcalls = 0;
		memmove(&saved, &module, sizeof(saved));
		memmove(&savedlink, &link, sizeof(savedlink));
		memset(bytes, 0xa5, sizeof(bytes));
		memmove(wanted, bytes, sizeof(wanted));
		result = -1;
		memmove(wanted+shift, &result, sizeof(result));
		Loader_compile(&request);
		check(calls == 0 && protects == 0 && callbackcalls == 0);
		check(memcmp(bytes, wanted, sizeof(bytes)) == 0 && memcmp(&module, &saved, sizeof(saved)) == 0);
		check(memcmp(&link, &savedlink, sizeof(link)) == 0);
		check(strcmp(diagnostic, which < 2 ? "nil mp" : "nil module") == 0);
		count++;
	}
	return count;
}


/* Compare actual public loader state/results with independent mode/request expectations. */
int
main(void)
{
	Module module, wanted;
	Modlink link, expectedlink;
	F_Loader_compile request;
	Inst original[2];
	uchar originalmp[8], instancemp[8], bytes[32], expected[32];
	WORD value;
	int mode, compiled, global, flag, outcome, shift, oldcache, run, rejected, rc;
	int cases, allowed, refused;

	check(sizeof(void*) == 8);
	cases = allowed = refused = 0;
	for(mode=0; mode<4; mode++)
	for(compiled=0; compiled<2; compiled++)
	for(global=0; global<2; global++)
	for(flag=0; flag<2; flag++)
	for(outcome=0; outcome<2; outcome++)
	for(oldcache=0; oldcache<2; oldcache++)
	for(shift=0; shift<8; shift++){
		memset(&module, 0, sizeof(module));
		memset(&link, 0, sizeof(link));
		memset(&request, 0, sizeof(request));
		module.ref = 17;
		module.nprog = 2;
		module.prog = original;
		module.origmp = originalmp;
		module.compiled = compiled;
		module.jit = oldcache ? &cache : nil;
		module.cstep = mode == 0 ? nil : mode == 1 ? native64lea8step : mode == 2 ? stepb : (int(*)(void))H;
		link.m = &module;
		link.prog = original;
		link.MP = instancemp;
		instance = &link;
		request.mp = &link;
		request.flag = flag;
		request.ret = (WORD*)(bytes+shift);
		cflag = global;
		compilerok = outcome;
		calls = protects = callbackcalls = 0;
		strcpy(diagnostic, "before");
		rejected = compiled || mode != 0;
		run = !rejected && (global || flag);
		rc = rejected || (run && !outcome) ? -1 : 0;
		memmove(&wanted, &module, sizeof(wanted));
		memmove(&expectedlink, &link, sizeof(expectedlink));
		if(run && outcome){
			wanted.prog = replacement;
			wanted.jit = &cache;
			expectedlink.prog = replacement;
			expectedlink.compiled = 0;
		}
		memset(bytes, 0xa5, sizeof(bytes));
		memmove(expected, bytes, sizeof(expected));
		value = rc;
		memmove(expected+shift, &value, sizeof(value));
		Loader_compile(&request);
		check(calls == run && protects == run && callbackcalls == 0);
		check(memcmp(bytes, expected, sizeof(bytes)) == 0);
		check(memcmp(&module, &wanted, sizeof(module)) == 0 && memcmp(&link, &expectedlink, sizeof(link)) == 0);
		check(strcmp(diagnostic, compiled ? "compiled module" : mode ? "static translated module" : "before") == 0);
		allowed += run;
		refused += rejected;
		cases++;
	}
	check(cases == 1024 && allowed == 96 && refused == 896);
	check(owners() == 32);
	printf("PASS public loader modes WORD%d: %d state/request/result cases; %d compiler calls; %d mode refusals\n",
		(int)sizeof(WORD)*8, cases, allowed, refused);
	printf("PASS public loader owner guards WORD%d: 32 nil/H/unaligned-result cases\n", (int)sizeof(WORD)*8);
	return 0;
}
