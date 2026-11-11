#include "lib9.h"
#include "isa.h"
#include "interp.h"
#include "native64-scalar-boot.h"
#ifdef DIS_NATIVE64_GUI_SYS_FIXED
#if DIS_NATIVE64_GUI_SYS_FIXED != 1 || defined(DIS_NATIVE64_SYS_HEADER) || defined(DIS_NATIVE64_BOOT_SYS_IO) || defined(DIS_NATIVE64_SCALAR_BOOT)
#error native64 GUI Sys selection owns its generated declarations
#endif
#define DIS_NATIVE64_SYS_HEADER "sys-native64.h"
#define LIMBO_NATIVE64_IMPLEMENTATIONS
#include "native64-sys-fixed.h"
#endif
#ifdef DIS_NATIVE64_BOOT_SYS_IO
#ifdef DIS_NATIVE64_SYS_HEADER
#error native64 Sys startup owns its generated metadata selection
#endif
#define DIS_NATIVE64_SYS_HEADER "sys-native64.h" // Coherent generated Sys types for the explicit startup profile.
#define LIMBO_NATIVE64_TABLES // Trusted selected fixed-frame entries; variadic prefixes remain excluded.
#endif
/* Explicit native64 consumers must use one coherent generated Sys declaration set. */
#ifdef DIS_NATIVE64_SYS_HEADER
#ifndef DIS_WORD32
#error experimental native64 Sys requires DIS_WORD32=1
#endif
#include DIS_NATIVE64_SYS_HEADER
#else
#include "runt.h"
#endif
#ifndef DIS_NATIVE64_SYS_HEADER
#include "sysmod.h"
#else
#include "runtab-legacy.h"
#endif
#include "raise.h"
#include "kernel.h"
#include "native64-entry.h"
#include "native64-bind.h"
#include "native64-model.h"
#include "native64-modules.h"


static	int		utfnleng(char*, int, int*);


#ifdef DIS_NATIVE64_BOOT_SYS_IO
static const LimboNative64Entry *const native64sysio[] = {
	&limbo_native64_Sys_byte2char_entry, // Checked scalar decoding.
	&limbo_native64_Sys_char2byte_entry, // Checked scalar encoding.
	&limbo_native64_Sys_utfbytes_entry, // Checked UTF extent.
	&limbo_native64_Sys_read_entry, // Bounded native I/O.
	&limbo_native64_Sys_readn_entry, // Bounded repeated reads.
	&limbo_native64_Sys_write_entry, // Bounded native write.
	&limbo_native64_Sys_pread_entry, // Bounded positioned read.
	&limbo_native64_Sys_pwrite_entry, // Bounded positioned write.
	&limbo_native64_Sys_pctl_entry, // Process and namespace setup.
	&limbo_native64_Sys_file2chan_entry, // File-backed channel setup.
	&limbo_native64_Sys_millisec_entry, // Pointer event timestamps.
	&limbo_native64_Sys_open_entry, // Open process control files.
	&limbo_native64_Sys_fildes_entry, // Duplicate the diagnostic descriptor.
	&limbo_native64_Sys_sleep_entry, // Fixed scalar signature for the checked import group.
	&limbo_native64_Sys_dup_entry, // Two scalar descriptor arguments.
};


/* Borrow a verified export from the selected native Sys profile under VM exclusion. */
const Link*
native64sysexport(const char *name, Module **owner)
{
	const LimboNative64Entry *entry;
	Module *module;
	const Link *link;
	Type *frame;
	unsigned int i;

	if(name == nil || owner == nil)
		return nil;
	for(module=modules; module != nil; module=module->link)
		if(module->path != nil && strcmp(module->path, "$Sys") == 0)
			break;
	if(module == nil || module->prog != nil || module->origmp != H ||
	   module->ntype != sizeof(native64sysio)/sizeof(native64sysio[0]) ||
	   module->type == nil || module->ext == nil)
		return nil;
	for(i=0; i<module->ntype && strcmp(native64sysio[i]->name, name) != 0; i++)
		;
	if(i == module->ntype)
		return nil;
	entry = native64sysio[i];
	link = &module->ext[i];
	frame = module->type[i];
	if(link->name == nil || strcmp(link->name, entry->name) != 0 ||
	   link->u.runt != entry->function || (UWORD)link->sig != entry->legacy_signature ||
	   frame == nil || link->frame != frame || frame->size != entry->frame_size ||
	   frame->np != entry->map_bytes || memcmp(frame->map, entry->map, entry->map_bytes) != 0)
		return nil;
	*owner = module;
	return link;
}


/* Keep the clock pilot's exact export selection. */
const Link*
native64sysclock(Module **owner)
{
	return native64sysexport("millisec", owner);
}
#endif


#ifdef DIS_NATIVE64_GUI_SYS_FIXED
/* Only the staged Sys initializer calls this constructor while holding VM startup exclusion. */
int
sysnative64registerfixed(void)
{
	Module *module;

	for(module=modules; module != nil; module=module->link)
		if(module->path != nil && strcmp(module->path, "$Sys") == 0){
			kwerrstr("Sys module already registered");
			return 0;
		}
	return builtinmod64("$Sys", limbo_native64_Sys_specs, limbo_native64_Sys_spec_count,
		limbo_native64_Sys_implementations, limbo_native64_Sys_implementation_count) != nil;
}

/* Repeated startup accepts only the complete selected module and its already-published types. */
static int
sysnative64fixedmatches(Module *module)
{
	const LimboNative64Entry *entry;
	Link *link;
	Type *type;
	unsigned int i;

	if(module->prog != nil || module->origmp != H || module->ext == nil || module->type == nil ||
	   module->ntype != limbo_native64_Sys_implementation_count)
		return 0;
	for(i=0; i<module->ntype; i++){
		entry = limbo_native64_Sys_implementations[i];
		link = module->ext+i;
		type = module->type[i];
		if(link->name == nil || strcmp(link->name, entry->name) != 0 ||
		   link->u.runt != entry->function || (UWORD)link->sig != entry->legacy_signature ||
		   type == nil || link->frame != type || type->size != entry->frame_size ||
		   type->np != entry->map_bytes || memcmp(type->map, entry->map, entry->map_bytes) != 0)
			return 0;
	}
	return module->ext[module->ntype].name == nil;
}
#endif


/* Register only the explicitly selected trusted profile; ordinary native64 declarations stay closed. */
void
sysmodinit(void)
{
#ifdef DIS_NATIVE64_GUI_SYS_FIXED
	Module *module;

	if(!limbo_native64_Sys_layout()){
		error("native64 Sys layout mismatch");
		return;
	}
	for(module=modules; module != nil; module=module->link)
		if(module->path != nil && strcmp(module->path, "$Sys") == 0){
			if(sysnative64fixedmatches(module) && sysnative64typesready()) return;
			error("incompatible native64 Sys initialization");
			return;
		}
	if(sysnative64typesready()){
		error("Sys types already initialized without the selected module");
		return;
	}
	sysinit(); /* Registers the module only after all five descriptors are staged. */
	for(module=modules; module != nil; module=module->link)
		if(module->path != nil && strcmp(module->path, "$Sys") == 0 &&
		   sysnative64fixedmatches(module) && sysnative64typesready())
			return;
	error("native64 Sys initialization incomplete");
#elif defined(DIS_NATIVE64_BOOT_SYS_IO)
	if(!limbo_native64_Sys_layout()){
		error("native64 Sys layout mismatch");
		return;
	}
	sysinit();
	if(builtinmod64("$Sys", limbo_native64_Sys_specs, limbo_native64_Sys_spec_count,
	   native64sysio, sizeof(native64sysio)/sizeof(native64sysio[0])) == nil)
		error("native64 Sys I/O registration failed");
#elif defined(DIS_NATIVE64_SYS_HEADER)
	error("experimental native64 Sys registration is not supported");
	return;
#else
	sysinit();
	builtinmod("$Sys", Sysmodtab, Sysmodlen);
#endif
}


/* A full destination consumes no bytes, even when vsnprint returns -1. */
static int
xsnprint(char *buf, int n, char *format, ...)
{
	va_list args;
	int written;

	if(n <= 0)
		return 0;
	va_start(args, format);
	written = vsnprint(buf, n, format, args);
	va_end(args);
	return written < 0 ? 0 : written;
}

typedef struct XprintString XprintString;

struct XprintString
{
	uchar *data; // Validated inline ASCII or Rune storage.
	int length; // Nonnegative count of logical characters.
	int wide; // Whether characters occupy native Rune cells.
};


/* Validate borrowed string metadata and exactly the payload the formatter uses. */
static void
xprintstring(String *string, int terminate, XprintString *view)
{
	int length, capacity;
	uintptr address, extent;
	uvlong bytes;

	if(string == nil)
		error(exNilref);
	view->data = (uchar*)"";
	view->length = view->wide = 0;
	if(string == H)
		return;
	address = (uintptr)string;
	extent = O(String, len) > O(String, max) ? O(String, len) : O(String, max);
	if(address > (uintptr)-1-extent-sizeof(length))
		error(exHeap);
	memmove(&length, (uchar*)string+O(String, len), sizeof(length));
	memmove(&capacity, (uchar*)string+O(String, max), sizeof(capacity));
	if(length == (-0x7fffffff-1))
		error(exInval);
	view->wide = length < 0;
	view->length = length < 0 ? -length : length;
	if(capacity < view->length)
		error(exHeap);
	/* An empty format reads no payload; string arguments still need a terminator. */
	if(view->length == 0 && !terminate)
		return;
	bytes = ((uvlong)view->length+terminate)*(view->wide ? sizeof(Rune) : 1);
	if(address > (uintptr)-1-O(String, data) ||
	   bytes > (uintptr)-1-(address+O(String, data)))
		error(exHeap);
	view->data = (uchar*)(address+O(String, data));
	/* The lib9 Rune formatter consumes Rune*, whereas pattern reads use byte copies. */
	if(terminate && view->wide && (uintptr)view->data%sizeof(Rune) != 0)
		error(exHeap);
}


/* Align by native coordinates and bound an argument before copying its bytes. */
static uchar*
xprintargument(void *frame, uchar *argument, int bytes, int alignment)
{
	uintptr address, padding;

	if(argument == nil || argument == H)
		error(exNilref);
	address = (uintptr)argument;
	padding = ((uintptr)frame-address)&(alignment-1);
	if(padding > (uintptr)-1-address ||
	   (uintptr)bytes > (uintptr)-1-address-padding)
		error(exBounds);
	return (uchar*)(address+padding);
}


/* Private reader categories, never serialized as Dis64 call or file identifiers. */
enum {
	XprintInt, // Four-byte integer including star widths and Unicode characters.
	XprintBig, // Eight-byte signed integer bit pattern.
	XprintReal, // Native binary64 argument.
	XprintStringArg, // Managed String identity, or H.
	XprintReference, // Managed debug reference, or H.
};

typedef struct XprintArguments XprintArguments;

struct XprintArguments
{
	void *frame; // Legacy frame-relative alignment anchor.
	uchar *cursor; // Legacy packed argument cursor.
	void (*read)(void*, int, void*, unsigned int); // Optional checked typed reader; nil retains legacy layout.
	void *context; // Borrowed typed-reader state, valid throughout formatting.
};


/* Preserve packed legacy strides while allowing a checked reader to supply exact host values. */
static void
xprinttake(XprintArguments *arguments, int kind, void *output, unsigned int bytes)
{
	int alignment;

	if(arguments->read != nil){
		arguments->read(arguments->context, kind, output, bytes);
		return;
	}
	alignment = kind == XprintReal ? sizeof(REAL) : kind == XprintBig ? IBY2LG : 1;
	arguments->cursor = xprintargument(arguments->frame, arguments->cursor, bytes, alignment);
	memmove(output, arguments->cursor, bytes);
	arguments->cursor += kind == XprintReal ? sizeof(REAL) : kind == XprintBig ? IBY2LG : IBY2WD;
}


/* Consume arguments even after output fills, bounding each format specification. */
static int
xprintwithargs(Prog *xp, XprintArguments *arguments, String *s1, char *buf, int n)
{
	int i;
	void *p;
	LONG bg;
	Type *t;
	double d;
	String *ss;
	ulong *ptr;
	int nc, c, isbig, isr, sip;
	char *b, *eb, *f, fmt[32];
	Rune r, zero = 0;
	XprintString pattern, text;
	ulong references;

	sip = 0;
	isr = 0;
	if(s1 == H || n <= 0)
		return 0;
	if(s1 == nil || buf == nil || buf == H)
		error(exNilref);
	if((uintptr)buf > (uintptr)-1-(uintptr)n)
		error(exBounds);
	xprintstring(s1, 0, &pattern);
	nc = pattern.length;
	isr = pattern.wide;

	b = buf;
	eb = buf+n-1;
	while(nc > 0) {
		nc--;
		if(isr){
			memmove(&r, pattern.data+(uintptr)sip*sizeof(r), sizeof(r));
			c = r;
		}else
			c = pattern.data[sip];
		sip++;
		if(c != '%') {
			if(b < eb) {
				if(c < Runeself)
					*b++ = c;
				else
					b += xsnprint(b, eb-b+1, "%C", c);
			}
			continue;
		}
		f = fmt;
		*f++ = c;
		isbig = 0;
		while(nc > 0) {
			nc--;
			if(f-fmt > sizeof(fmt)-UTFmax-1)
				error(exInval);
			if(isr){
				memmove(&r, pattern.data+(uintptr)sip*sizeof(r), sizeof(r));
				c = r;
			}else
				c = pattern.data[sip];
			sip++;
			*f++ = c;
			*f = '\0';
			switch(c) {
			default:
				continue;
			case '*':
				/* Dis integer arguments occupy four bytes, even on LP64 hosts. */
				xprinttake(arguments, XprintInt, &i, IBY2WD);
				f--;
				/* Widths need at most eleven decimal bytes and a terminator. */
				if(sizeof(fmt)-(f-fmt) < 12+UTFmax)
					error(exInval);
				f += snprint(f, sizeof(fmt)-(f-fmt), "%d", (int)i);

				continue;
			case 'b':
				f[-1] = 'l';
				*f++ = 'l';
				*f = '\0';
				isbig = 1;
				continue;
			case '%':
				if(b < eb)
					*b++ = '%';
				break;
			case 'q':
			case 's':
				xprinttake(arguments, XprintStringArg, &ss, sizeof(ss));
				xprintstring(ss, 1, &text);

				if(ss == H)
					p = "";
				else
				if(text.wide) {
					f[-1] += 'A'-'a';
					memmove(text.data+(uintptr)text.length*sizeof(zero), &zero, sizeof(zero));
					p = text.data;
				}
				else {
					text.data[text.length] = '\0';
					p = text.data;
				}
				b += xsnprint(b, eb-b+1, fmt, p);
				break;
			case 'E':
				f--;
				r = 0x00c9;	/* L'É' */
				f += runetochar(f, &r);	/* avoid clash with ether address */
				*f = '\0';
				/* fall through */
			case 'e':
			case 'f':
			case 'g':
			case 'G':
				xprinttake(arguments, XprintReal, &d, sizeof(d));
				b += xsnprint(b, eb-b+1, fmt, d);
				break;
			case 'd':
			case 'o':
			case 'x':
			case 'X':
			case 'c':
				if(isbig) {
					xprinttake(arguments, XprintBig, &bg, sizeof(bg));
					b += xsnprint(b, eb-b+1, fmt, bg);

				}
				else {
					/* Dis integer arguments occupy four bytes, even on LP64 hosts. */
					xprinttake(arguments, XprintInt, &i, IBY2WD);
					/* always a unicode character */
					if(c == 'c')
						f[-1] = 'C';
					b += xsnprint(b, eb-b+1, fmt, (int)i);

				}
				break;
			case 'r':
				if(b < eb)
					b = syserr(b, eb, xp);
				break;
/* Debugging formats - may disappear */
			case 'H':
				xprinttake(arguments, XprintReference, &ptr, sizeof(ptr));
				if(ptr == nil)
					error(exNilref);
				t = nil;
				if(ptr != H) {
					if((uintptr)ptr <= sizeof(Heap) || (uintptr)ptr%sizeof(void*) != 0)
						error(exHeap);
					references = D2H(ptr)->ref;
					t = D2H(ptr)->t;
					b += xsnprint(b, eb-b+1, "%lud.%.8lux", references, (ulong)t);
				}else
					b += xsnprint(b, eb-b+1, "%d.%.8lux", -1, (ulong)t);
				break;
			}
			break;
		}
	}
	return b - buf;
}

/* Keep the public legacy formatter on its original packed frame contract. */
int
xprint(Prog *xp, void *frame, void *values, String *format, char *buffer, int bytes)
{
	XprintArguments arguments;

	memset(&arguments, 0, sizeof(arguments));
	arguments.frame = frame;
	arguments.cursor = values;
	return xprintwithargs(xp, &arguments, format, buffer, bytes);
}

#ifdef DIS_NATIVE64_FORMATTER
#include "native64-format.h"
#ifdef DIS_NATIVE64_SYS_HEADER
#include "native64-sys-format.h"
#include "native64-sys-write.h"


/* Enter trusted native64 sprint with an explicit typed tail and late mapped-result resolver. */
int
Sys_sprint64(const Native64SysFormatRequest *request)
{
	return native64syssprint(request);
}


/* Enter trusted native64 aprint without reusing the legacy packed argument entry. */
int
Sys_aprint64(const Native64SysFormatRequest *request)
{
	return native64sysaprint(request);
}

/* Trusted native64 print: format before output, then publish one scalar write count. */
int
Sys_print64(const Native64SysWriteRequest *request)
{
	return native64syswrite(request, 0);
}


/* Trusted native64 fprint retains its managed FD through native output and late result resolution. */
int
Sys_fprint64(const Native64SysWriteRequest *request)
{
	return native64syswrite(request, 1);
}
#endif
#endif

typedef struct XprintCall XprintCall;

struct XprintCall
{
	Prog *prog; // Borrowed process used for error-string formatting.
	void *frame; // Borrowed frame-relative argument alignment anchor.
	void *args; // Initial packed argument cursor for each formatting attempt.
	String *format; // Borrowed format retained by the calling VM frame.
	char *buf; // Owned temporary output released when protected formatting fails.
	int size; // Current output capacity in bytes.
	int written; // Successful byte count, including embedded NUL characters.
};


/* Keep the allocated output visible to cleanup if formatting raises a VM error. */
static int
xprintcall(void *arg)
{
	XprintCall *call;

	call = arg;
	call->written = xprint(call->prog, call->frame, call->args,
		call->format, call->buf, call->size);
	return 1;
}


/* Grow without signed overflow and free temporary output before propagating errors. */
int
bigxprint(Prog *xp, void *vfp, void *vva, String *s1, char **buf, int s)
{
	XprintCall call;
	int m;

	if(s <= 0)
		error(exInval);
	if(buf == nil || buf == H)
		error(exNilref);
	if((uintptr)buf > (uintptr)-1-sizeof(*buf))
		error(exBounds);
	call.prog = xp;
	call.frame = vfp;
	call.args = vva;
	call.format = s1;
	m = s;
	for(;;){
		if(m > 0x7fffffff/2)
			error(exNomem);
		m *= 2;
		call.size = m;
		call.buf = malloc(m);
		if(call.buf == nil)
			error(exNomem);
		if(!kprotect(xprintcall, &call)){
			free(call.buf);
			krethrow();
		}
		if(call.written < m-UTFmax-2)
			break;
		free(call.buf);
	}
	memmove(buf, &call.buf, sizeof(call.buf));
	return call.written;
}

typedef struct XprintResult XprintResult;

struct XprintResult
{
	char *text; // Borrowed formatted bytes used while constructing the result.
	int length; // Exact byte count, independent of embedded NULs.
	int array; // Whether the VM result is a byte array instead of a String.
	void *value; // Newly constructed result published only after success.
};


/* Build a new VM owner without touching the caller's previous result. */
static int
buildprintresult(void *arg)
{
	XprintResult *result;

	result = arg;
	if(result->array)
		result->value = mem2array(result->text, result->length);
	else
		result->value = c2string(result->text, result->length);
	return 1;
}


/* Release dynamic formatting storage on both conversion success and VM error. */
static void*
printresult(char *text, int length, int array, int allocated)
{
	XprintResult result;
	int ok;

	result.text = text;
	result.length = length;
	result.array = array;
	result.value = H;
	ok = kprotect(buildprintresult, &result);
	if(allocated)
		free(text);
	if(!ok)
		krethrow();
	return result.value;
}


/* Publish a formatted string only after conversion, retaining old results on error. */
void
Sys_sprint(void *fp)
{
#ifdef DIS_NATIVE64_SYS_HEADER
	USED(fp);
	error("native64 Sys variadic calls are not supported");
	return;
#else
	int n;
	char buf[256], *b = buf;
	String *result, *old;
	String *format;
	Prog *prog;
	void *ret, *args;
	uintptr address;
	ulong references;

	if(fp == nil || fp == H)
		error(exNilref);
	if((uintptr)fp > (uintptr)-1-O(F_Sys_sprint, vargs))
		error(exBounds);
	/* Retain the original frame as the packed argument alignment anchor. */
	memmove(&ret, (uchar*)fp+O(F_Sys_sprint, ret), sizeof(ret));
	memmove(&format, (uchar*)fp+O(F_Sys_sprint, s), sizeof(format));
	args = (uchar*)fp+O(F_Sys_sprint, vargs);
	if(ret == nil || ret == H)
		error(exNilref);
	if((uintptr)ret > (uintptr)-1-sizeof(old))
		error(exBounds);
	memmove(&old, ret, sizeof(old));
	if(old == nil)
		error(exNilref);
	if(old != H) {
		address = (uintptr)old;
		if(address <= sizeof(Heap) || (address-sizeof(Heap))%sizeof(void*) != 0)
			error(exHeap);
		memmove(&references, (uchar*)old-sizeof(Heap)+O(Heap, ref), sizeof(references));
		if(references == 0)
			error(exHeap);
	}
	prog = currun();
	n = xprint(prog, fp, args, format, buf, sizeof(buf));
	if(n >= sizeof(buf)-UTFmax-2)
		n = bigxprint(prog, fp, args, format, &b, sizeof(buf));
	b[n] = '\0';
	result = printresult(b, n, 0, b != buf);
	memmove(ret, &result, sizeof(result));
	destroy(old);
#endif
}


/* Publish a formatted byte array after allocation and temporary-buffer cleanup. */
void
Sys_aprint(void *fp)
{
#ifdef DIS_NATIVE64_SYS_HEADER
	USED(fp);
	error("native64 Sys variadic calls are not supported");
	return;
#else
	int n;
	char buf[256], *b = buf;
	Array *result, *old;
	String *format;
	Prog *prog;
	void *ret, *args;
	uintptr address;
	ulong references;

	if(fp == nil || fp == H)
		error(exNilref);
	if((uintptr)fp > (uintptr)-1-O(F_Sys_aprint, vargs))
		error(exBounds);
	/* Retain the original frame as the packed argument alignment anchor. */
	memmove(&ret, (uchar*)fp+O(F_Sys_aprint, ret), sizeof(ret));
	memmove(&format, (uchar*)fp+O(F_Sys_aprint, s), sizeof(format));
	args = (uchar*)fp+O(F_Sys_aprint, vargs);
	if(ret == nil || ret == H)
		error(exNilref);
	if((uintptr)ret > (uintptr)-1-sizeof(old))
		error(exBounds);
	memmove(&old, ret, sizeof(old));
	if(old == nil)
		error(exNilref);
	if(old != H) {
		address = (uintptr)old;
		if(address <= sizeof(Heap) || (address-sizeof(Heap))%sizeof(void*) != 0)
			error(exHeap);
		memmove(&references, (uchar*)old-sizeof(Heap)+O(Heap, ref), sizeof(references));
		if(references == 0)
			error(exHeap);
	}
	prog = currun();
	n = xprint(prog, fp, args, format, buf, sizeof(buf));
	if(n >= sizeof(buf)-UTFmax-2)
		n = bigxprint(prog, fp, args, format, &b, sizeof(buf));
	result = printresult(b, n, 1, b != buf);
	memmove(ret, &result, sizeof(result));
	destroy(old);
#endif
}


/* Compare unsigned byte characters with runes; nil denotes no separators. */
static int
tokdelim(int c, String *d)
{
	int length, i;
	uchar *data;
	Rune rune;

	if(d == H)
		return 0;
	/* buildtokens has already validated both metadata and the complete payload. */
	memmove(&length, (uchar*)d+offsetof(String, len), sizeof(length));
	if(length == 0)
		return 0;
	data = (uchar*)d+offsetof(String, data);
	if(length < 0) {
		for(i = 0; i < -length; i++) {
			memmove(&rune, data+(ulong)i*sizeof(rune), sizeof(rune));
			if(rune == (Rune)c)
				return 1;
		}
	} else
		for(i = 0; i < length; i++)
			if(data[i] == c)
				return 1;
	return 0;
}

typedef struct TokenizeResult TokenizeResult;

struct TokenizeResult
{
	String *source; // Input string retained by the caller throughout construction.
	String *delimiters; // Delimiter set in either string representation.
	List *words; // Partial word list owned by protected cleanup.
	List *previous; // Captured old result, validated after input metadata and before allocation.
	int count; // Number of complete words built before publication.
};


/* Keep partial words under one owner until every allocation has succeeded. */
static int
buildtokens(void *arg)
{
	TokenizeResult *result;
	String *s, *d;
	List **h, *nl;
	int c, nc, first, last, srune, i, length[2], capacity;
	String *view, *word;
	uchar *data;
	uintptr address;
	uvlong bytes;
	ulong references;
	Rune rune;

	result = arg;
	s = result->source;
	d = result->delimiters;
	if(s != H && (s == nil || d == nil))
		error(exNilref);
	/* Reject malformed views before scanning or publishing any partial word. */
	length[0] = length[1] = 0;
	for(i = 0; s != H && i < 2; i++) {
		view = i == 0 ? s : d;
		if(view == H)
			continue;
		address = (uintptr)view;
		if(address > (uintptr)-1-(offsetof(String, max)+sizeof(capacity)))
			error(exHeap);
		if(i == 0) {
			/* slicer copies an owning String and requires a valid Heap prefix. */
			if(address <= sizeof(Heap) || (address-sizeof(Heap))%sizeof(void*) != 0)
				error(exHeap);
			memmove(&references, (uchar*)view-sizeof(Heap)+offsetof(Heap, ref), sizeof(references));
			if(references == 0)
				error(exHeap);
		}
		memmove(&length[i], (uchar*)view+offsetof(String, len), sizeof(length[i]));
		memmove(&capacity, (uchar*)view+offsetof(String, max), sizeof(capacity));
		if(length[i] == (-0x7fffffff-1) || capacity < 0 ||
		   (length[i] < 0 ? -length[i] : length[i]) > capacity)
			error(exInval);
		bytes = (uvlong)(length[i] < 0 ? -length[i] : length[i])*(length[i] < 0 ? sizeof(Rune) : 1);
		if(bytes != 0 && (address > (uintptr)-1 - offsetof(String, data) ||
		   bytes > (uintptr)-1-(address+offsetof(String, data))))
			error(exHeap);
	}
	/* Input errors retain priority over malformed old result owners. */
	if(result->previous == nil)
		error(exNilref);
	if(result->previous != H) {
		address = (uintptr)result->previous;
		if(address <= sizeof(Heap) || (address-sizeof(Heap))%sizeof(void*) != 0)
			error(exHeap);
		memmove(&references, (uchar*)result->previous-sizeof(Heap)+offsetof(Heap, ref), sizeof(references));
		if(references == 0)
			error(exHeap);
	}
	h = &result->words;
	first = 0;
	srune = length[0] < 0;
	nc = srune ? -length[0] : length[0];
	if(nc == 0)
		return 1;
	data = (uchar*)s+offsetof(String, data);
	while(first < nc){
		while(first < nc){
			if(srune) {
				memmove(&rune, data+(ulong)first*sizeof(rune), sizeof(rune));
				c = rune;
			} else
				c = data[first];
			if(!tokdelim(c, d))
				break;
			first++;
		}
		last = first;
		while(last < nc){
			if(srune) {
				memmove(&rune, data+(ulong)last*sizeof(rune), sizeof(rune));
				c = rune;
			} else
				c = data[last];
			if(tokdelim(c, d))
				break;
			last++;
		}
		if(first == last)
			break;
		if(Tptr.ref <= 0 || Tptr.ref == 0x7fffffff)
			error(exHeap);
		nl = cons(sizeof(String*), h);
		nl->tail = H;
		nl->t = &Tptr;
		Tptr.ref++;
		/* slicer can raise an error after the list node has been published. */
		word = H;
		memmove(nl->data, &word, sizeof(word));
		word = slicer(first, last, s);
		memmove(nl->data, &word, sizeof(word));
		h = &nl->tail;
		first = last;
		result->count++;
	}
	return 1;
}


/* Publish tuple fields through byte copies after every allocation succeeds. */
void
Sys_tokenize(void *fp)
{
	typedef struct Value Value;

	struct Value {
		WORD count; // Complete token count published after construction.
		List *words; // Old owner is replaced only after all allocations succeed.
	};
	F_Sys_tokenize f;
	TokenizeResult result;
	List *old;
	WORD count;
	uchar *ret;

	if(fp == nil || fp == H)
		error(exNilref);
	if((uintptr)fp > (uintptr)-1-sizeof(f))
		error(exHeap);
	memmove(&f, fp, sizeof(f));
	ret = (uchar*)f.ret;
	if(ret == nil || ret == H)
		error(exNilref);
	if((uintptr)ret > (uintptr)-1-sizeof(Value))
		error(exHeap);
	memmove(&old, ret+O(Value, words), sizeof(old));
	result.source = f.s;
	result.delimiters = f.delim;
	result.words = H;
	result.previous = old;
	result.count = 0;
	if(!kprotect(buildtokens, &result)){
		destroy(result.words);
		krethrow();
	}
	count = result.count;
	memmove(ret+O(Value, count), &count, sizeof(count));
	memmove(ret+O(Value, words), &result.words, sizeof(result.words));
	destroy(old);
}


/* Validate byte-array metadata and full VM lengths before native UTF routines. */
static int
sysutfarray(Array *a, uchar **data)
{
	Array view;
	Heap owner;
	int size, pointers;
	uintptr address, extent;

	if(a == H || a == nil)
		error(exBounds);
	address = (uintptr)a;
	if(address <= sizeof(Heap) || address > (uintptr)-1-sizeof(view))
		error(exBounds);
	/* Input arrays and read-only compiler descriptors may be packed. */
	memmove(&owner, (uchar*)a-sizeof(Heap), sizeof(owner));
	if(owner.t != &Tarray)
		error(exType);
	memmove(&view, a, sizeof(view));
	if(view.t != &Tbyte) {
		extent = offsetof(Type, size) > offsetof(Type, np) ? offsetof(Type, size) : offsetof(Type, np);
		if(view.t == nil || view.t == H || (uintptr)view.t > (uintptr)-1-extent-sizeof(int))
			error(exType);
		memmove(&size, (uchar*)view.t+offsetof(Type, size), sizeof(size));
		memmove(&pointers, (uchar*)view.t+offsetof(Type, np), sizeof(pointers));
		if(size != 1 || pointers != 0)
			error(exType);
	}
	if(view.len < 0 || (uvlong)view.len > 0x7fffffff ||
	   (view.len != 0 && (view.data == nil || view.data == H ||
	    (uintptr)view.data > (uintptr)-1-(uintptr)view.len)))
		error(exBounds);
	*data = view.data;
	return (int)view.len;
}


/* Bound the complete result before encoding can modify an aliased byte array. */
static uchar*
sysutfresult(void *slot, uintptr bytes)
{
	if(slot == nil || slot == H)
		error(exNilref);
	if((uintptr)slot > (uintptr)-1-bytes)
		error(exBounds);
	return slot;
}


/* Count complete UTF sequences and publish WORD without requiring alignment. */
void
Sys_utfbytes(void *fp)
{
	Array *a;
	int nbyte, length;
	WORD result;
	uchar *ret, *data;
	F_Sys_utfbytes f;

	if(fp == nil || fp == H)
		error(exNilref);
	if((uintptr)fp > (uintptr)-1-sizeof(f))
		error(exBounds);
	memmove(&f, fp, sizeof(f));
	ret = sysutfresult(f.ret, sizeof(result));
	a = f.buf;
	length = sysutfarray(a, &data);
	if(f.n < 0 || f.n > length)
		error(exBounds);

	utfnleng((char*)data, (int)f.n, &nbyte);
	result = nbyte;
	memmove(ret, &result, sizeof(result));
}


/* Decode the complete tuple before publishing into a possibly unaligned slot. */
void
Sys_byte2char(void *fp)
{
	Rune r;
	char *p;
	int n, w, valid, length;
	WORD result[3];
	uchar *ret, *data;
	Array *a;
	F_Sys_byte2char f;

	if(fp == nil || fp == H)
		error(exNilref);
	if((uintptr)fp > (uintptr)-1-sizeof(f))
		error(exBounds);
	memmove(&f, fp, sizeof(f));
	ret = sysutfresult(f.ret, sizeof(result));
	a = f.buf;
	length = sysutfarray(a, &data);
	if(f.n < 0 || f.n >= length)
		error(exBounds);
	n = (int)f.n;
	r = data[n];
	w = valid = 1;
	if(r >= Runeself){
		p = (char*)data+n;
		if(length-n >= UTFmax || fullrune(p, length-n)){
			w = chartorune(&r, p);
			valid = r != Runeerror || w != 1;
		}else{
			r = Runeerror;
			w = valid = 0;
		}
	}
	result[0] = r;
	result[1] = w;
	result[2] = valid;
	memmove(ret, result, sizeof(result));
}


/* Encode before publishing the count, preserving the full rune and offset. */
void
Sys_char2byte(void *fp)
{
	F_Sys_char2byte f;
	Array *a;
	int n, length;
	WORD c, result;
	Rune r;
	uchar *ret, *data;

	if(fp == nil || fp == H)
		error(exNilref);
	if((uintptr)fp > (uintptr)-1-sizeof(f))
		error(exBounds);
	memmove(&f, fp, sizeof(f));
	ret = sysutfresult(f.ret, sizeof(result));
	a = f.buf;
	length = sysutfarray(a, &data);
	c = f.c;
	if(f.n < 0 || f.n >= length)
		error(exBounds);
	n = (int)f.n;
	if(c < 0 || c > Runemax)
		c = Runeerror;
	result = 0;
	if(c < Runeself){
		data[n] = c;
		result = 1;
	}else{
		r = c;
		if(length-n >= UTFmax || runelen(c) <= length-n)
			result = runetochar((char*)data+n, &r);
	}
	memmove(ret, &result, sizeof(result));
}

typedef struct BuiltinModule BuiltinModule;
struct BuiltinModule
{
	char *name;
	Runtab *table;
	int count;
	Module *module;
};


/* Keep every frame descriptor reachable by cleanup while export names can fail. */
static int
buildbuiltin(void *arg)
{
	BuiltinModule *c;
	Module *m;
	Runtab *r;
	Type *t;
	int i;

	c = arg;
	c->module = m = newmod(c->name);
	m->ntype = c->count;
	if(c->count != 0){
		m->type = mallocz(c->count*sizeof(Type*), 1);
		if(m->type == nil){
			kwerrstr(exNomem);
			return 0;
		}
	}
	m->ext = mallocz((c->count+1)*sizeof(Link), 1);
	if(m->ext == nil){
		kwerrstr(exNomem);
		return 0;
	}
	for(i = 0; i < c->count; i++){
		r = &c->table[i];
		t = dtype(freeheap, r->size, r->map, r->np);
		if(t == nil){
			kwerrstr("bad builtin frame or out of memory");
			return 0;
		}
		m->type[i] = t;
		runtime(m, &m->ext[i], r->name, r->sig, r->fn, t);
	}
	return 1;
}


/* Validate table bounds, build under one error owner, then publish exactly once. */
Module*
builtinmod(char *name, void *vr, int rlen)
{
	BuiltinModule c;
	Runtab *r;
	Module *m;
	int n, limit;

	limit = 0x7fffffff/sizeof(Link)-1;
	if(name == nil || vr == nil || rlen < 0 || rlen > limit)
		goto badtable;
	r = vr;
	for(n = 0; r[n].name != nil; n++){
		if(n >= limit || (rlen != 0 && n >= rlen) ||
		   r[n].fn == nil || r[n].size < 0 ||
		   r[n].np < 0 || r[n].np > sizeof(r[n].map))
			goto badtable;
	}
	if(rlen != 0 && n != rlen)
		goto badtable;
	c.name = name;
	c.table = r;
	c.count = n;
	c.module = nil;
	if(kprotect(buildbuiltin, &c)){
		m = c.module;
		m->link = modules;
		modules = m;
		return m;
	}
	if(c.module != nil){
		destroylinks(c.module);
		freemod(c.module);
	}
	return nil;
badtable:
	kwerrstr("bad builtin table");
	return nil;
}


typedef struct Native64Builtin Native64Builtin;

struct Native64Builtin
{
	char *name; // Borrowed module name for the protected construction interval.
	const LimboNative64Entry *const *entries; // Validated immutable implementation records.
	unsigned int count; // Number of selected fixed-frame exports.
	Module *module; // Partial owner, retained until publication or complete cleanup.
};


/* Retain every completed descriptor while later export-name allocations can fail. */
static int
buildbuiltin64(void *arg)
{
	Native64Builtin *build;
	const LimboNative64Entry *entry;
	Module *module;
	Type *type;
	unsigned int i;
	int signature;

	build = arg;
	build->module = module = newmod(build->name);
	module->ntype = build->count;
	if(build->count != 0){
		module->type = mallocz(build->count*sizeof(Type*), 1);
		if(module->type == nil){
			kwerrstr(exNomem);
			return 0;
		}
	}
	module->ext = mallocz((build->count+1)*sizeof(Link), 1);
	if(module->ext == nil){
		kwerrstr(exNomem);
		return 0;
	}
	for(i=0; i<build->count; i++){
		entry = build->entries[i];
		type = dtype(freeheap, entry->frame_size, (uchar*)entry->map, entry->map_bytes);
		if(type == nil){
			kwerrstr("bad native64 builtin frame or out of memory");
			return 0;
		}
		module->type[i] = type;
		/* Link.sig stores the same 32 bits even when the consistency hash has bit31 set. */
		memmove(&signature, &entry->legacy_signature, sizeof(signature));
		runtime(module, &module->ext[i], (char*)entry->name, signature, entry->function, type);
	}
	return 1;
}


typedef struct Native64BuiltinSet Native64BuiltinSet;

struct Native64BuiltinSet
{
	const Native64ModuleSpec *specs; // Borrowed immutable complete construction requests.
	unsigned int count; // Number of independently owned modules to stage.
	Module **staged; // Private zeroed module-owner vector, never published during allocation.
	unsigned int completed; // Completed module owners in staged request order.
	Native64Builtin current; // Partial module owner outside the protected jump frame.
};


/* Check the entire set before allocating, including duplicate module names and aggregate resource bounds. */
static int
native64builtinsetcheck(const Native64ModuleSpec *specs, unsigned int count, Module **output)
{
	unsigned int i, j, declarations, selected;

	if(!native64model() || count > Native64BindEntries ||
	   (count != 0 && (specs == nil || specs == H || output == nil || output == H ||
		(uintptr)specs > (uintptr)-1-count*sizeof(*specs) ||
		(uintptr)output > (uintptr)-1-count*sizeof(*output))))
		return 0;
	declarations = selected = 0;
	for(i=0; i<count; i++){
		if(output[i] != nil || !native64bindname(specs[i].name) ||
		   specs[i].count > Native64BindEntries-declarations ||
		   specs[i].selected > Native64BindEntries-selected ||
		   !native64bindcheck(specs[i].specs, specs[i].count, specs[i].entries, specs[i].selected))
			return 0;
		for(j=0; j<i; j++)
			if(native64bindsame(specs[j].name, specs[i].name))
				return 0;
		declarations += specs[i].count;
		selected += specs[i].selected;
	}
	return 1;
}


/* Preserve each completed owner while a later module's descriptors/names can still fail. */
static int
buildbuiltinset64(void *argument)
{
	Native64BuiltinSet *build;
	const Native64ModuleSpec *spec;
	unsigned int i;

	build = argument;
	for(i=0; i<build->count; i++){
		spec = build->specs+i;
		build->current.name = spec->name;
		build->current.entries = spec->entries;
		build->current.count = spec->selected;
		if(!buildbuiltin64(&build->current))
			return 0;
		build->staged[i] = build->current.module;
		build->current.module = nil;
		build->completed++;
	}
	return 1;
}


/* Detach unpublished owners before teardown; cleanup must not throw or reenter registration. */
static void
native64builtinsetclear(Native64BuiltinSet *build)
{
	Module *module;
	char saved[ERRMAX];

	kgerrstr(saved, sizeof(saved));
	module = build->current.module;
	build->current.module = nil;
	if(module != nil){
		destroylinks(module);
		freemod(module);
	}
	while(build->completed != 0){
		build->completed--;
		module = build->staged[build->completed];
		build->staged[build->completed] = nil;
		destroylinks(module);
		freemod(module);
	}
	kwerrstr("%s", saved);
}


/* Publish one complete chain only after every constructor returns; no callback or allocation occurs during commit. */
static int
native64builtinsetregister(const Native64ModuleSpec *specs, unsigned int count, Module **output, Module **staged)
{
	Native64BuiltinSet build;
	Module *head;
	unsigned int i;

	memset(&build, 0, sizeof(build));
	build.specs = specs;
	build.count = count;
	build.staged = staged;
	if(!kprotect(buildbuiltinset64, &build)){
		native64builtinsetclear(&build);
		return 0;
	}
	head = modules;
	for(i=0; i<count; i++){
		staged[i]->link = head;
		head = staged[i];
	}
	modules = head;
	for(i=0; i<count; i++)
		output[i] = staged[i];
	return 1;
}


/* Atomically register a trusted native64 set; caller serializes registration and supplies disjoint nil output cells. */
int
builtinmods64(const Native64ModuleSpec *specs, unsigned int count, Module **output)
{
	Module **staged;
	char saved[ERRMAX];
	int status;

	if(!native64builtinsetcheck(specs, count, output)){
		kwerrstr("bad native64 builtin ABI or table");
		return 0;
	}
	if(count == 0)
		return 1;
	staged = mallocz(count*sizeof(*staged), 1);
	if(staged == nil){
		kwerrstr(exNomem);
		return 0;
	}
	status = native64builtinsetregister(specs, count, output, staged);
	kgerrstr(saved, sizeof(saved));
	free(staged);
	kwerrstr("%s", saved);
	return status;
}


/* Keep the single-module ABI and allocation sequence while sharing the complete staged-construction engine. */
Module*
builtinmod64(char *name, const LimboNative64Entry *specs, unsigned int count,
	const LimboNative64Entry *const *entries, unsigned int selected)
{
	Native64ModuleSpec request;
	Module *output, *staged;

	request.name = name;
	request.specs = specs;
	request.count = count;
	request.entries = entries;
	request.selected = selected;
	output = staged = nil;
	if(!native64builtinsetcheck(&request, 1, &output)){
		kwerrstr("bad native64 builtin ABI or table");
		return nil;
	}
	if(!native64builtinsetregister(&request, 1, &output, &staged))
		return nil;
	return output;
}


/* Prepare the string before publishing through a potentially unaligned return slot. */
void
retnstr(char *s, int n, void *d)
{
	String *s1, *old;

	s1 = H;
	if(n != 0)
		s1 = c2string(s, n);
	memmove(&old, d, sizeof(old));
	memmove(d, &s1, sizeof(s1));
	destroy(old);
}


/* Retain the original owner until conversion and byte-addressed slot publication succeed. */
void
retstr(char *s, void *d)
{
	String *s1, *old;
	uvlong length;

	s1 = H;
	if(s != nil){
		length = strlen(s);
		if(length > 0x7fffffff)
			error(exHeap);
		s1 = c2string(s, (int)length);
	}
	memmove(&old, d, sizeof(old));
	memmove(d, &s1, sizeof(s1));
	destroy(old);
}


/* Reserve both descriptors and bound the total heap size before allocating bytes. */
Array*
mem2array(void *va, int n)
{
	Heap *h;
	Array *a;

	if(n < 0)
		n = 0;
	if(Tarray.ref <= 0 || Tarray.ref == 0x7fffffff ||
	   Tbyte.ref <= 0 || Tbyte.ref == 0x7fffffff ||
	   (ulong)n > 0x7fffffff-sizeof(Heap)-sizeof(Array))
		error(exHeap);
	h = nheap(sizeof(Array)+n);
	h->t = &Tarray;
	h->t->ref++;
	a = H2D(Array*, h);
	a->t = &Tbyte;
	Tbyte.ref++;
	a->len = n;
	a->root = H;
	a->data = (uchar*)a+sizeof(Array);
	if(va != 0)
		memmove(a->data, va, n);

	return a;
}


/* Empty prefixes need no data pointer; compare remaining bytes within the span. */
static int
utfnleng(char *s, int nb, int *ngood)
{
	int c;
	long n;
	Rune rune;
	char *es, *starts;

	if(nb == 0){
		if(ngood)
			*ngood = 0;
		return 0;
	}
	starts = s;
	es = s+nb;
	for(n = 0; s < es; n++) {
		c = *(uchar*)s;
		if(c < Runeself)
			s++;
		else {
			if(es-s >= UTFmax || fullrune(s, es-s))
				s += chartorune(&rune, s);
			else
				break;
		}
	}
	if(ngood)
		*ngood = s-starts;
	return n;
}
