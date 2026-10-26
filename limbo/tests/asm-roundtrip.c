#include "limbo.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

Biobuf *bout;
static uchar expected[512];
static char *sourcepath;
#define NAMELEN 64 // Production source-path buffer unit used by asmpath.


/* Fail independently of NDEBUG without changing the production serializer. */
static void
check(int condition)
{
	if(!condition)
		abort();
}


/* Model the installed gfltconv using the real shortest-roundtrip formatter. */
int
Bprint(Biobuf *stream, char *format, ...)
{
	char converted[256], number[32];
	va_list ap;
	long offset;
	ulong high, low;
	double value;
	int i, j, result;

	(void)stream;
	va_start(ap, format);
	if(strcmp(format, "\treal\t@mp+%ld,%g # %.8lux%.8lux\n") == 0){
		offset = va_arg(ap, long);
		value = va_arg(ap, double);
		high = va_arg(ap, ulong);
		low = va_arg(ap, ulong);
		g_fmt(number, value, 'e');
		result = printf("\treal\t@mp+%ld,%s # %08lx%08lx\n", offset, number, high, low);
	}else{
		/* Plan 9's lux/llux spellings carry the same unsigned arguments as lx/llx. */
		for(i = j = 0; format[i] != 0; i++){
			if(format[i] == 'u' && format[i+1] == 'x' && i > 0 && format[i-1] == 'l')
				continue;
			check(j+1 < sizeof(converted));
			converted[j++] = format[i];
		}
		converted[j] = 0;
		result = vprintf(converted, ap);
	}
	va_end(ap);
	return result;
}


/* Capture literal bytes without a libc interpretation of the output. */
int
Bputc(Biobuf *stream, int value)
{
	(void)stream;
	return putchar((uchar)value);
}


/* Preserve exact lengths for escaped NULs and quote sequences. */
long
Bwrite(Biobuf *stream, void *data, long size)
{
	(void)stream;
	return fwrite(data, 1, size, stdout);
}


/* The source path provider may return its owned immutable string. */
char*
srcpath(char *buffer, int size)
{
	(void)buffer;
	(void)size;
	return sourcepath;
}


/* Reject fixture branches outside numeric and string data emission. */
void
nerror(Node *node, char *format, ...)
{
	(void)node;
	(void)format;
	abort();
}


/* Interface initialization is not selected by this driver. */
ulong
sign(Decl *decl)
{
	(void)decl;
	abort();
	return 0;
}


/* Retain normal alignment for the unselected interface branch. */
long
align(long offset, int width)
{
	return (offset+width-1)/width*width;
}


/* Apply the loader's signed word widening after the binary serializer's 32-bit field. */
static void
disint(long offset, long value)
{
	vlong widened;
	int narrow;

	check(offset >= 0 && offset <= sizeof(expected)-IBY2WD);
	narrow = value;
	widened = narrow;
	memmove(expected+offset, IBY2WD == 4 ? (void*)&narrow : (void*)&widened, IBY2WD);
}


/* Big records always store eight bytes, independently of word cells. */
static void
disbig(long offset, Long value)
{
	check(offset >= 0 && offset <= sizeof(expected)-sizeof(value));
	memmove(expected+offset, &value, sizeof(value));
}


/* Preserve the binary float record's exact representation. */
static void
disreal(long offset, Real value)
{
	check(offset >= 0 && offset <= sizeof(expected)-sizeof(value));
	memmove(expected+offset, &value, sizeof(value));
}


/* Keep memory instrumentation without unavailable process-based leak inspection. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

IMPLEMENTATION


/* Run actual binary serializer arms against the same compiler nodes. */
static void
binarydata(long offset, Node *n)
{
	Case *c;
	Label *lab;
	int i;

	switch(n->ty->kind){
BINARY_ARMS
	default:
		abort();
	}
}


/* Print an entire data image so the external text decoder checks untouched padding too. */
static void
image(void)
{
	int i;

	printf("IMAGE ");
	for(i = 0; i < sizeof(expected); i++)
		printf("%02x", expected[i]);
	printf("\nEND\n");
}


/* Exercise table extrema, floating bit patterns and quoted byte strings from an independent driver. */
int
main(void)
{
	char line[4096], hex[2048];
	Node node, bounds[10];
	Type type;
	Case table;
	Label labels[5];
	Inst targets[6];
	Sym *symbol;
	uint64_t bits;
	unsigned long long inputbits;
	int count, wildcard, pattern, row, value, i, length;
	Long first;

	memset(&node, 0, sizeof(node));
	memset(&type, 0, sizeof(type));
	memset(&table, 0, sizeof(table));
	memset(bounds, 0, sizeof(bounds));
	memset(labels, 0, sizeof(labels));
	memset(targets, 0, sizeof(targets));
	node.ty = &type;
	type.cse = &table;
	table.labs = labels;
	targets[5].pc = 31;
	while(fgets(line, sizeof(line), stdin) != nil){
		printf("BEGIN\n");
		memset(expected, 0, sizeof(expected));
		if(line[0] == 't'){
			check(sscanf(line+1, "%d %d %d", &count, &wildcard, &pattern) == 3 && count >= 0 && count <= 5);
			type.kind = Tcasel;
			table.nlab = count;
			table.iwild = wildcard ? &targets[5] : nil;
			for(row = 0; row < count; row++){
				first = pattern == 0 ? -(1LL<<50) : pattern == 1 ? (1LL<<50) : pattern == 2 ? (-0x7fffffffffffffffLL-1) : 0x7fffffffffffff00LL;
				bounds[2*row].val = first+row*8;
				bounds[2*row+1].val = pattern == 3 && row == count-1 ? 0x7fffffffffffffffLL : first+row*8+3;
				labels[row].start = &bounds[2*row];
				labels[row].stop = &bounds[2*row+1];
				labels[row].inst = &targets[row];
				targets[row].pc = row+1;
			}
			binarydata(64, &node);
			asminitializer(64, &node);
			image();
		}else if(line[0] == 'r'){
			check(sscanf(line+1, "%llx", &inputbits) == 1);
			type.kind = Treal;
			bits = inputbits;
			memmove(&node.rval, &bits, sizeof(bits));
			binarydata(64, &node);
			asminitializer(64, &node);
			image();
		}else{
			check(line[0] == 'q' && sscanf(line+1, "%2047s", hex) == 1);
			length = strcmp(hex, "-") == 0 ? 0 : strlen(hex)/2;
			symbol = calloc(1, sizeof(*symbol)+length+1);
			check(symbol != nil);
			symbol->name = (char*)(symbol+1);
			symbol->len = length;
			for(i = 0; i < length; i++){
				check(sscanf(hex+2*i, "%2x", &value) == 1);
				symbol->name[i] = value;
			}
			asmstring(64, symbol);
			if(memchr(symbol->name, 0, length) == nil){
				sourcepath = symbol->name;
				asmpath();
			}
			printf("END\n");
			free(symbol);
		}
	}
	return 0;
}
