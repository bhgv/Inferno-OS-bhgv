#include "limbo.h"
#include <assert.h>
#include <stdio.h>

Biobuf *bout;
char debug[256];
static uchar wire[200000];
static int emitted, cursor, requests;

/* Stop an allocator loop deterministically, using actual compiler AST layouts. */
void*
reallocmem(void *p, ulong size)
{
	assert(++requests < 32 && size <= sizeof(wire));
	p = realloc(p,size);
	assert(p != nil);
	return p;
}

/* Keep fatal paths visible rather than silently accepting a partial module. */
void
fatal(char *message, ...)
{
	fprintf(stderr,"unexpected compiler failure: %s\n",message);
	abort();
}

/* Unsupported AST nodes are never part of the constructed interface global. */
void
nerror(Node *n, char *message, ...)
{
	(void)n;
	fatal(message);
}

/* Isolate type-signature generation from data serialization. */
ulong
sign(Decl *d)
{
	assert(d != nil);
	return 0x1234abcd;
}

/* Match the positive alignment used by interface data fields. */
long
align(long n, int alignment)
{
	assert(n >= 0 && alignment > 0);
	return (n+alignment-1)/alignment*alignment;
}

/* Observe all output through the real compiler writer call signatures. */
int
Bputc(Biobuf *stream, int byte)
{
	(void)stream;
	assert(emitted < sizeof(wire));
	wire[emitted++] = byte;
	return 0;
}

/* Copy each contiguous payload exactly as requested by disvar/disdatum. */
long
Bwrite(Biobuf *stream, void *p, long n)
{
	(void)stream;
	assert(n >= 0 && emitted+n <= sizeof(wire));
	if(n)
		memcpy(wire+emitted,p,n);
	emitted += n;
	return n;
}

/* File offsets are only diagnostic inputs for this in-memory compilation. */
vlong
Boffset(Biobuf *stream)
{
	(void)stream;
	return emitted;
}

/* Debug statistics remain disabled, but retain their output dependency. */
int
print(char *format, ...)
{
	(void)format;
	assert(0);
	return 0;
}

IMPLEMENTATION

/* Decode the positive lengths/offsets produced by this real-AST interface. */
static long
number(void)
{
	long v;
	int first, n;

	assert(cursor < emitted);
	first = wire[cursor++];
	n = first < 128 ? 0 : first < 192 ? 1 : 3;
	v = first & (n == 0 ? 127 : 63);
	while(n-- > 0){
		assert(cursor < emitted);
		v = v*256+wire[cursor++];
	}
	return v;
}

/* Pass a long interface identifier through production disvar and disdatum. */
static void
interface(int length, int adt)
{
	Type iface = {0}, container = {0}, parent = {0};
	Decl global = {0}, owner = {0}, id = {0}, outer = {0};
	Node init = {0};
	Sym symbol = {0}, prefix = {0};
	char *name;
	int i, size, tag;

	name = malloc(length+1);
	assert(name != nil);
	for(i = 0; i < length; i++)
		name[i] = 'a'+i%26;
	name[length] = 0;
	symbol.name = name;
	symbol.len = length;
	prefix.name = "Container";
	prefix.len = 9;
	iface.kind = Tiface;
	parent.kind = adt ? Tadt : Tmodule;
	container.ids = &id;
	outer.ty = &parent;
	outer.sym = &prefix;
	id.dot = &outer;
	id.sym = &symbol;
	owner.ty = &container;
	init.ty = &iface;
	init.decl = &owner;
	init.val = 1;
	global.store = Dglobal;
	global.init = &init;
	emitted = cursor = requests = 0;
	disvar(length+32,&global);
	assert(wire[cursor++] == DBYTE(DEFW,2) && number() == 0);
	assert(memcmp(wire+cursor,"\0\0\0\1\x12\x34\xab\xcd",8) == 0);
	cursor += 8;
	size = length+1+(adt ? 10 : 0);
	tag = wire[cursor++];
	assert((tag>>4) == DEFB && (tag&15) == 0);
	assert(number() == size && number() == 8);
	if(adt){
		assert(memcmp(wire+cursor,"Container.",10) == 0);
		cursor += 10;
	}
	assert(memcmp(wire+cursor,name,length+1) == 0);
	cursor += length+1;
	assert(wire[cursor++] == 0 && cursor == emitted);
	free(name);
}

/* Exercise module and qualified ADT interface names on both sides of growth limits. */
int
main(void)
{
	int lengths[] = {1023,1024,1025,8193,65536}, i;

	for(i = 0; i < 5; i++){
		interface(lengths[i],0);
		interface(lengths[i],1);
	}
	free(cache);
	puts("Dis interface globals: 10 real-AST serialization scenarios passed");
	return 0;
}
