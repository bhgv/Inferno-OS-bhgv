#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "disc.c"

int dontcompile; // Fixture-controlled classic translation switch.
static int calls; // Observe explicit callback execution, never attachment side effects.


/* Keep semantic failures distinguishable from compiler, signal and sanitizer failures. */
static void
check(int condition)
{
	if(!condition){
		fprintf(stderr, "static translation binding mismatch\n");
		exit(10);
	}
}


/* First registered program's independent observable implementation. */
static int
stepa(void)
{
	calls++;
	return 31;
}


/* Second program uses the same instruction count but different bytes and callback. */
static int
stepb(void)
{
	calls++;
	return 47;
}


/* Invalid registrations must neither publish a record nor mutate borrowed storage. */
static int
registrations(const uchar *bytes)
{
	DisC seed, item, saved, *argument;
	int which;

	for(which=0; which<12; which++){
		memset(&seed, 0, sizeof(seed));
		seed.image = bytes;
		seed.length = 12;
		seed.nprog = 3;
		seed.step = stepa;
		images = nil;
		check(disc_register(&seed) == 1 && images == &seed && seed.next == nil);
		check(disc_register(&seed) == 1 && images == &seed && seed.next == nil);
		item = seed;
		item.next = &seed;
		argument = &item;
		switch(which){
		case 0: argument = nil; break;
		case 1: argument = H; break;
		case 2: argument = (DisC*)((uchar*)&item+1); break;
		case 3: argument = (DisC*)((uintptr)-1 & ~((uintptr)sizeof(void*)-1)); break;
		case 4: item.image = nil; break;
		case 5: item.image = H; break;
		case 6: item.length = 0; break;
		case 7: item.length = 0x80000000UL; break;
		case 8: item.image = (uchar*)((uintptr)-1-7); item.length = 16; break;
		case 9: item.nprog = -1; break;
		case 10: item.step = nil; break;
		case 11: item.step = (int(*)(void))H; break;
		}
		memmove(&saved, &item, sizeof(saved));
		check(disc_register(argument) == 0 && images == &seed && seed.next == nil);
		check(memcmp(&item, &saved, sizeof(item)) == 0 && calls == 0);
	}
	images = nil;
	return which;
}


/* Refuse invalid attachment arguments before any registered-image payload read. */
static int
arguments(const uchar *bytes)
{
	Module module, saved, *argument;
	const uchar *image;
	ulong length;
	int which, route, result;

	for(route=0; route<2; route++)
	for(which=0; which<14; which++){
		memset(&module, 0, sizeof(module));
		module.ref = 19;
		module.nprog = 3;
		argument = &module;
		image = bytes;
		length = 12;
		dontcompile = calls = 0;
		switch(which){
		case 0: argument = nil; break;
		case 1: argument = H; break;
		case 2: argument = (Module*)((uchar*)&module+1); break;
		case 3: argument = (Module*)((uintptr)-1 & ~((uintptr)sizeof(void*)-1)); break;
		case 4: image = nil; break;
		case 5: image = H; break;
		case 6: length = 0; break;
		case 7: length = 0x80000000UL; break;
		case 8: image = (uchar*)((uintptr)-1-7); length = 16; break;
		case 9: module.jit = H; break;
		case 10: module.cstep = (int(*)(void))H; break;
		case 11: module.compiled = 1; break;
		case 12: dontcompile = 1; break;
		case 13: module.rt = DONTCOMPILE; break;
		}
		memmove(&saved, &module, sizeof(saved));
		result = 0;
		if(route == 0)
			result = disc_tryattach(argument, image, length);
		else
			disc_attach(argument, image, length);
		check(result == 0 && memcmp(&saved, &module, sizeof(module)) == 0 && calls == 0);
	}
	return 28;
}


/* Compare actual try/wrapper attachment against an independent image/mode table. */
int
main(void)
{
	static const uchar a[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
	static const uchar b[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 13};
	uchar clone[12], changed[12];
	const uchar *input;
	DisC first, second;
	Module module, wanted;
	Jit cache;
	int (*initial)(void), (*selected)(void);
	int route, mode, jit, compiled, disabled, forbidden, image, count;
	int accepted, cases, bad, invalid, attach, ok, value;

	check(sizeof(void*) == 8);
	memmove(clone, a, sizeof(clone));
	memmove(changed, a, sizeof(changed));
	changed[0] ^= 0x40;
	bad = registrations(a);
	memset(&first, 0, sizeof(first));
	first.image = a;
	first.length = 12;
	first.nprog = 3;
	first.step = stepa;
	second = first;
	second.image = b;
	second.step = stepb;
	check(disc_register(&first) && disc_register(&second));
	check(images == &second && second.next == &first && first.next == nil);
	invalid = arguments(a);
	accepted = cases = 0;
	for(route=0; route<2; route++)
	for(mode=0; mode<4; mode++)
	for(jit=0; jit<2; jit++)
	for(compiled=0; compiled<2; compiled++)
	for(disabled=0; disabled<2; disabled++)
	for(forbidden=0; forbidden<2; forbidden++)
	for(image=0; image<4; image++)
	for(count=2; count<=4; count++){
		initial = mode == 0 ? nil : mode == 1 ? stepa : mode == 2 ? stepb : (int(*)(void))H;
		input = image == 0 ? a : image == 1 ? b : image == 2 ? clone : changed;
		selected = image == 1 ? stepb : stepa;
		ok = !jit && !compiled && !disabled && !forbidden && image != 3 && count == 3 &&
			(initial == nil || initial == selected);
		memset(&module, 0, sizeof(module));
		module.ref = 17;
		module.nprog = count;
		module.cstep = initial;
		module.compiled = compiled;
		module.jit = jit ? &cache : nil;
		module.rt = forbidden ? DONTCOMPILE : MUSTCOMPILE;
		dontcompile = disabled;
		memmove(&wanted, &module, sizeof(wanted));
		if(ok)
			wanted.cstep = selected;
		calls = 0;
		attach = -1;
		if(route == 0)
			attach = disc_tryattach(&module, input, 12);
		else
			disc_attach(&module, input, 12);
		check((route != 0 || attach == ok) && memcmp(&module, &wanted, sizeof(module)) == 0 && calls == 0);
		check(images == &second && second.next == &first && first.next == nil);
		if(ok){
			value = module.cstep();
			check(value == (image == 1 ? 47 : 31) && calls == 1);
			accepted++;
		}
		cases++;
	}
	check(cases == 1536 && accepted == 12);
	printf("PASS static translation binding WORD%d: %d state/image/wrapper cases; %d accepts; %d invalid registrations; %d invalid attachments\n",
		(int)sizeof(WORD)*8, cases, accepted, bad, invalid);
	return 0;
}
