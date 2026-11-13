#include "limbo.h"
#include "dis64-layout.h"

extern char *ckindname[]; // Existing primitive C spellings; layout is computed independently.

/* Private metadata-generator limits, not wire ranges or native-call admission. */
enum {
	Native64Bytes = 1024*1024, // Maximum single generated object/frame extent.
	Native64Types = 4096, // Maximum cached borrowed type identities.
	Native64Names = 8192, // Bound the private generated C identifier registry.
	Native64Depth = 128, // Bound native recursion through by-value C spellings.
};

typedef struct Native64Type Native64Type;

struct Native64Type {
	Type *type; // Borrowed verified legacy AST identity; never modified by this emitter.
	long size; // Independently computed byte extent in the explicit native64 model.
	int alignment; // Natural alignment, independent of the host compiler's long width.
	int active; // Detect invalid by-value cycles before emitting any C declarations.
	int emitted; // Order named C definitions after all embedded aggregate definitions.
	uchar *map; // Full map8 bytes, including zero scalar-only/padding bytes.
	Native64Type *next; // Private cache entry, released after this emission.
};

typedef struct Native64Name Native64Name;

struct Native64Name {
	char *name; // Owned exact generated C identifier, including macro suffixes.
	void *owner; // Borrowed Type or Decl identity permitted to repeat this declaration.
	int callable; // Source C functions keep their names when descriptor macros need disambiguation.
	Native64Name *next; // Private preflight registry, never a wire symbol table.
};

static Native64Name *native64names; // Catch flattened qualified-name and macro collisions before output.
static int native64namecount; // Enforce a bounded metadata-only symbol population.

static Native64Type *native64types; // Per-emission cache, not a compiler target selection.
static int native64count; // Bound pathological input before allocating another cache entry.

static Native64Type *native64type(Type*, int);
static void native64ctype(Type*, int);
static unsigned int native64callsequence; // Diagnostic identity across one compiler invocation.

/* Spell private tail kinds rather than serializing the runtime's C enum values. */
static char*
native64callkind(Type *type)
{
	switch(type->kind){
	case Tbyte: return "byte";
	case Tint: return "int";
	case Tfix: return "fix";
	case Tbig: return "big";
	case Treal: return "real";
	case Tstring: return "string";
	}
	return nil;
}

/* Independent fixed-prefix geometry for the first scalar/reference call-site producer.
 * Refuse by-value aggregates here instead of widening the compiler's legacy offsets. */
static int
native64callwidth(Type *type)
{
	switch(type->kind){
	case Tbyte: return 1;
	case Tint: case Tfix: return 4;
	case Tbig: case Treal: case Tstring: case Tref:
	case Tarray: case Tlist: case Tchan: case Tmodule: return 8;
	}
	return 0;
}

/* Describe actual typechecked variadic arguments after legacy idoffsets, without changing
 * descriptors, instructions, signatures, target selection or the output file. */
unsigned int
native64callplan(Decl *callee, Type *instantiated)
{
	Decl *fixed, *actual, *tail, *argument;
	uchar map[(64+2*4096*8+63)/64];
	unsigned int offset = 64, count = 0, fixedcount = 0, i, width, slot, bytes, references;
	char name[4*StrSize];
	char *reason = nil, *result;
    unsigned int id;

	if(callee == nil || callee->ty == nil || !callee->ty->varargs || instantiated == nil)
		return 0;
    if(native64callsequence == (unsigned int)-1) return 0;
    id = ++native64callsequence;
	dotprint(name, name+sizeof(name), callee, '_');
	result = callee->ty->tof->kind == Tnone ? "none" : native64callkind(callee->ty->tof);
	if(callee->ty->tof->kind == Tarray && callee->ty->tof->tof->kind == Tbyte) result = "array-byte";
	if(result == nil) reason = "result-kind";
	memset(map, 0, sizeof(map));
	actual = instantiated->ids;
	for(fixed=callee->ty->ids; fixed != nil; fixed=fixed->next){
		if(actual == nil || ++fixedcount > 4096){ reason = "fixed-arguments"; break; }
		width = native64callwidth(fixed->ty);
		if(width == 0){ reason = "fixed-aggregate"; break; }
		offset = (offset+width-1)&~(width-1);
		if(fixed->ty->kind == Tstring || fixed->ty->kind == Tref || fixed->ty->kind == Tarray ||
		   fixed->ty->kind == Tlist || fixed->ty->kind == Tchan || fixed->ty->kind == Tmodule){
			slot = offset/8; map[slot/8] |= 0x80>>(slot%8);
		}
		offset += width;
		actual = actual->next;
	}
	offset = (offset+7)&~7U;
	tail = actual;
	count = references = 0;
	if(reason == nil)
		for(argument=tail; argument != nil; argument=argument->next){
			if(++count > 4096){ reason = "tail-count"; break; }
			if(native64callkind(argument->ty) == nil){ reason = "tail-kind"; break; }
			if(argument->ty->kind == Tstring) references++;
		}
	/* The combined frame, not each half independently, must fit the bounded map. */
	if(reason == nil && offset+count*8 > sizeof(map)*64) reason = "frame-size";
	if(reason != nil){
		fprint(2, "NATIVE64_CALL {\"schema\":\"native64-call-v1\",\"id\":%ud,\"callee\":\"%s\",\"accepted\":false,\"reason\":\"%s\"}\n", id, name, reason);
		return id;
	}
	bytes = (offset+count*8+63)/64;
	for(argument=tail, i=0; argument != nil; argument=argument->next, i++)
		if(argument->ty->kind == Tstring){ slot = offset/8+i; map[slot/8] |= 0x80>>(slot%8); }
	fprint(2, "NATIVE64_CALL {\"schema\":\"native64-call-v1\",\"id\":%ud,\"callee\":\"%s\",\"accepted\":true,\"signature\":%lud,\"result\":\"%s\",\"fixed_arguments\":%ud,\"fixed_bytes\":%ud,\"frame_bytes\":%ud,\"references\":%ud,\"map\":[",
		id, name, (ulong)sign(callee), result, fixedcount, offset, offset+count*8, references);
	for(i=0; i<bytes; i++) fprint(2, "%s%ud", i == 0 ? "" : ",", map[i]);
	fprint(2, "],\"tail\":[");
	for(argument=tail, i=0; argument != nil; argument=argument->next, i++)
		fprint(2, "%s{\"kind\":\"%s\",\"legacy_offset\":%ld,\"offset\":%ud,\"bytes\":8}",
			i == 0 ? "" : ",", native64callkind(argument->ty), argument->offset, i*8);
	fprint(2, "]}\n");
    return id;
}


/* Reject ambiguous C names before printing any declaration, including descriptor macros. */
static Native64Name*
native64register(char *name, void *owner)
{
	Native64Name *entry;

	for(entry=native64names; entry != nil; entry=entry->next)
		if(strcmp(entry->name, name) == 0){
			if(entry->owner != owner)
				fatal("native64 C name collision: %s", name);
			return entry;
		}
	if(native64namecount == Native64Names)
		fatal("too many native64 C names");
	native64namecount++;
	entry = allocmem(sizeof(*entry));
	entry->name = allocmem(strlen(name)+1);
	strcpy(entry->name, name);
	entry->owner = owner;
	entry->callable = 0;
	entry->next = native64names;
	native64names = entry;
	return entry;
}


/* Select only one module's declarations while retaining its actual dependent types. */
static int
native64module(Decl *module)
{
	return module->store == Dtype && module->ty->kind == Tmodule &&
		(native64targetmodule == nil || strcmp(module->sym->name, native64targetmodule) == 0);
}


/* Preserve callable C spellings; only colliding metadata uses a reserved private name. */
static void
native64descriptorsymbol(char *name, char *suffix, char *symbol, int bytes)
{
	Native64Name *entry;

	if(snprint(symbol, bytes, "%s_%s", name, suffix) >= bytes)
		fatal("native64 descriptor name too long");
	for(entry=native64names; entry != nil; entry=entry->next)
		if(entry->callable && strcmp(entry->name, symbol) == 0){
			if(snprint(symbol, bytes, "limbo_native64_descriptor_%s_%s", name, suffix) >= bytes)
				fatal("native64 descriptor name too long");
			return;
		}
}


/* Reserve the complete descriptor spelling without conflating C names and runtime IDs. */
static void
native64reservenamed(char *name, void *owner)
{
	char symbol[4*StrSize+64];

	native64register(name, owner);
	native64descriptorsymbol(name, "size", symbol, sizeof(symbol));
	native64register(symbol, owner);
	native64descriptorsymbol(name, "map", symbol, sizeof(symbol));
	native64register(symbol, owner);
}


/* Preserve user field spellings only when they are valid non-keyword C identifiers. */
static int
native64keyword(char *name)
{
	static char *keywords[] = {
		"auto", "break", "case", "char", "const", "continue", "default", "do", "double",
		"else", "enum", "extern", "float", "for", "goto", "if", "inline", "int", "long",
		"register", "restrict", "return", "short", "signed", "sizeof", "static", "struct",
		"switch", "typedef", "union", "unsigned", "void", "volatile", "while",
		"_Bool", "_Complex", "_Imaginary", "_Alignas", "_Alignof", "_Atomic", "_Generic",
		"_Noreturn", "_Static_assert", "_Thread_local",
	};
	int i;

	for(i=0; i<nelem(keywords); i++)
		if(strcmp(name, keywords[i]) == 0)
			return 1;
	return 0;
}


/* Retain complete qualified identifiers rather than silently truncating generated C names. */
static void
native64name(Decl *decl, char *name, int bytes)
{
	if(decl == nil || decl->sym == nil || bytes < 2)
		fatal("missing native64 declaration name");
	dotprint(name, name+bytes, decl, '_');
	if(native64keyword(name))
		fatal("native64 declaration name is a C keyword");
	if(strlen(name) >= bytes-1)
		fatal("native64 declaration name too long");
	if(strncmp(name, "limbo_native64_", 15) == 0 ||
	   strncmp(name, "LIMBO_NATIVE64_", 15) == 0 || strncmp(name, "LimboNative64", 13) == 0)
		fatal("native64 declaration name uses reserved metadata prefix");
}


/* Align bounded layouts without changing offsets or descriptor caches in the AST. */
static long
native64align(long offset, int alignment)
{
	long padding;

	if(offset < 0 || offset > Native64Bytes || alignment < 1 ||
	   alignment > 8 || (alignment & (alignment-1)) != 0)
		fatal("invalid native64 layout alignment");
	padding = (alignment-offset%alignment)%alignment;
	if(padding > Native64Bytes-offset)
		fatal("native64 layout exceeds implementation limit");
	return offset+padding;
}


/* Append a complete child extent after validating both private resource bounds. */
static long
native64end(long offset, long size)
{
	if(offset < 0 || offset > Native64Bytes || size < 0 || size > Native64Bytes-offset)
		fatal("native64 layout exceeds implementation limit");
	return offset+size;
}


/* Merge only complete marked reference slots; scalar children may have four-byte stride. */
static void
native64merge(Native64Type *parent, Native64Type *child, long offset)
{
	long bytes, bit, at;

	for(bytes=0; bytes<(child->size+63)/64; bytes++)
		for(bit=0; bit<8; bit++){
			if((child->map[bytes] & (0x80>>bit)) == 0)
				continue;
			at = native64end(offset, bytes*64+bit*8);
			if(at%8 != 0 || at > parent->size || parent->size-at < 8)
				fatal("native64 reference outside generated layout");
			parent->map[at/64] |= 0x80>>((at%64)/8);
		}
}


/* Compute immutable natural layouts and full map8, stopping at pointer identities. */
static Native64Type*
native64type(Type *type, int depth)
{
	Native64Type *layout, *child;
	Decl *field;
	long offset, bytes;
	int pointer, fields;
	char name[4*StrSize];

	if(type == nil || depth >= Native64Depth)
		fatal("native64 type missing or nested too deeply");
	for(layout=native64types; layout != nil; layout=layout->next)
		if(layout->type == type){
			if(layout->active)
				fatal("native64 by-value type cycle");
			return layout;
		}
	if(native64count == Native64Types)
		fatal("too many native64 metadata types");
	native64count++;
	layout = allocmem(sizeof(*layout));
	memset(layout, 0, sizeof(*layout));
	layout->type = type;
	layout->alignment = 1;
	layout->active = 1;
	layout->next = native64types;
	native64types = layout;
	pointer = 0;
	switch(type->kind){
	case Tnone:
		break;
	case Tbyte:
		layout->size = 1;
		break;
	case Tint:
	case Tfix:
		layout->size = layout->alignment = 4;
		break;
	case Tbig:
	case Treal:
		layout->size = layout->alignment = 8;
		break;
	case Tstring:
	case Tarray:
	case Tlist:
	case Tchan:
	case Tmodule:
	case Tref:
	case Tany:
		layout->size = layout->alignment = 8;
		pointer = 1;
		break;
	case Tadt:
	case Ttuple:
		if(type->tags != nil)
			fatal("native64 picked types are not supported");
		if(type->decl != nil && type->decl->sym != anontupsym)
			native64name(type->decl, name, sizeof(name));
		offset = 0;
		fields = 0;
		for(field=type->ids; field != nil; field=field->next){
			if(field->store != Dfield)
				continue;
			if(field->sym != nil && native64keyword(field->sym->name))
				fatal("native64 field name is a C keyword");
			if(field->sym != nil && strncmp(field->sym->name, "__limbo_native64_", 17) == 0)
				fatal("native64 field name uses reserved metadata prefix");
			child = native64type(field->ty, depth+1);
			if(child->size == 0)
				fatal("native64 zero-size field is not supported");
			offset = native64align(offset, child->alignment);
			offset = native64end(offset, child->size);
			if(child->alignment > layout->alignment)
				layout->alignment = child->alignment;
			fields++;
		}
		if(fields == 0)
			fatal("native64 empty aggregates are not supported");
		layout->size = native64align(offset, layout->alignment);
		break;
	default:
		fatal("unsupported native64 metadata type %d", type->kind);
	}
	bytes = (layout->size+63)/64;
	layout->map = allocmem(bytes != 0 ? bytes : 1);
	memset(layout->map, 0, bytes);
	if(pointer)
		layout->map[0] = 0x80;
	if(type->kind == Tadt || type->kind == Ttuple){
		offset = 0;
		for(field=type->ids; field != nil; field=field->next){
			if(field->store != Dfield)
				continue;
			child = native64type(field->ty, depth+1);
			offset = native64align(offset, child->alignment);
			native64merge(layout, child, offset);
			offset = native64end(offset, child->size);
		}
	}
	layout->active = 0;
	return layout;
}


/* Print explicit padding and readable offsets without borrowing old compiler field offsets. */
static void
native64fields(Decl *fields, long start, int depth)
{
	Native64Type *layout;
	Decl *field;
	long offset, aligned;

	offset = start;
	for(field=fields; field != nil; field=field->next){
		if(field->store != Dfield && field->store != Darg)
			continue;
		layout = native64type(field->ty, depth+1);
		aligned = native64align(offset, layout->alignment);
		if(aligned != offset)
			print("\tuchar __limbo_native64_pad%ld[%ld]; // Natural alignment padding.\n", offset, aligned-offset);
		print("\t");
		native64ctype(field->ty, depth+1);
		if(field->sym == nil)
			print(" __limbo_native64_arg%ld; // Offset %ld, %ld bytes.\n", offset, aligned, layout->size);
		else
			print(" %s; // Offset %ld, %ld bytes.\n", field->sym->name, aligned, layout->size);
		offset = native64end(aligned, layout->size);
	}
}


/* Spell native pointer/scalar and nested aggregate C types without legacy tuple pads. */
static void
native64ctype(Type *type, int depth)
{
	char name[4*StrSize];

	if(type == nil || depth >= Native64Depth)
		fatal("native64 C spelling nested too deeply");
	switch(type->kind){
	case Tref:
		native64ctype(type->tof, depth+1);
		print("*");
		break;
	case Tadt:
	case Ttuple:
		if(type->decl != nil && type->decl->sym != anontupsym){
			native64name(type->decl, name, sizeof(name));
			print("%s", name);
		}else{
			print("struct {\n");
			native64fields(type->ids, 0, depth+1);
			print("}");
		}
		break;
	default:
		if(type->kind < Tnone || type->kind >= Tend ||
		   (type->kind != Tnone && type->kind != Tbyte && type->kind != Tint &&
		    type->kind != Tfix && type->kind != Tbig && type->kind != Treal &&
		    type->kind != Tstring && type->kind != Tarray && type->kind != Tlist &&
		    type->kind != Tchan && type->kind != Tmodule && type->kind != Tany))
			fatal("unsupported native64 C type");
		print("%s", ckindname[type->kind]);
	}
}


/* Validate every C spelling before output, including referents hidden by pointer layout. */
static void
native64spellcheck(Type *type, int depth)
{
	Decl *field;
	char name[4*StrSize];

	if(type == nil || depth >= Native64Depth)
		fatal("native64 C spelling nested too deeply");
	native64type(type, 0);
	if(type->kind == Tref){
		if(type->tof != nil && (type->tof->kind == Tadt || type->tof->kind == Ttuple) &&
		   type->tof->decl != nil && type->tof->decl->sym != anontupsym){
			native64type(type->tof, 0);
			native64name(type->tof->decl, name, sizeof(name));
			return;
		}
		native64spellcheck(type->tof, depth+1);
	}
	if(type->kind == Tadt || type->kind == Ttuple)
		for(field=type->ids; field != nil; field=field->next)
			if(field->store == Dfield)
				native64spellcheck(field->ty, depth+1);
}


/* Emit embedded named aggregates before their users, independent of source declaration order. */
static void
native64definition(Type *type, int depth)
{
	Native64Type *layout;
	Decl *field;
	char name[4*StrSize];

	if(depth >= Native64Depth)
		fatal("native64 C definition nested too deeply");
	if(type->kind != Tadt && type->kind != Ttuple)
		return;
	layout = native64type(type, 0);
	if(layout->emitted)
		return;
	for(field=type->ids; field != nil; field=field->next)
		if(field->store == Dfield)
			native64definition(field->ty, depth+1);
	layout->emitted = 1;
	if(type->decl == nil || type->decl->sym == anontupsym)
		return;
	native64name(type->decl, name, sizeof(name));
	print("\n/* Natural native64 layout for %s. */\nstruct %s {\n", name, name);
	native64fields(type->ids, 0, 0);
	print("};\n");
}


/* Emit full map bytes, including zero scalar suffixes; none of these are callable signatures. */
static void
native64descriptor(char *name, Native64Type *layout)
{
	long i, bytes;
	char symbol[4*StrSize+64];

	bytes = (layout->size+63)/64;
	native64descriptorsymbol(name, "size", symbol, sizeof(symbol));
	print("#define %s %ld // Native64 byte extent.\n", symbol, layout->size);
	native64descriptorsymbol(name, "map", symbol, sizeof(symbol));
	print("#define %s {", symbol);
	for(i=0; i<bytes; i++)
		print("%s0x%2.2ux", i == 0 ? "" : ",", layout->map[i]);
	if(bytes == 0)
		print("0");
	print("} // Full map8; result addresses and frame links are not argument roots.\n");
}


/* Keep method names distinct inside one module's metadata namespace. */
static void
native64symbol(Decl *function)
{
	print("\"");
	if(function->dot != nil && function->dot->ty->kind != Tmodule)
		print("%s.", function->dot->sym->name);
	print("%s\"", function->sym->name);
}


/* Emit exact integral module constants without host-long narrowing or signed-minimum literal overflow. */
static void
native64constant(Decl *declaration)
{
	char name[4*StrSize];
	Long value;
	int big;

	if(declaration->store != Dconst ||
	   (declaration->ty->kind != Tbyte && declaration->ty->kind != Tint &&
	    declaration->ty->kind != Tfix && declaration->ty->kind != Tbig))
		return;
	native64name(declaration, name, sizeof(name));
	value = declaration->init->val;
	big = declaration->ty->kind == Tbig;
	if(declaration->ty->kind == Tbyte)
		value &= 255;
	if(value < 0)
		print("#define %s (-%lld%s-1%s) // Exact source integral constant.\n", name,
			-(value+1), big ? "LL" : "", big ? "LL" : "");
	else
		print("#define %s (%lld%s) // Exact source integral constant.\n", name, value, big ? "LL" : "");
}


/* Emit callback-free specifications and an explicitly requested complete C implementation list. */
static void
native64table(Decl *module)
{
	Decl *function;
	char name[4*StrSize], moduleid[4*StrSize];
	char frame[4*StrSize+2], symbol[4*StrSize+64];
	int count;

	native64name(module, moduleid, sizeof(moduleid));
	print("\n#ifdef LIMBO_NATIVE64_TABLES\n");
	print("static const LimboNative64Entry limbo_native64_%s_specs[] = {\n", moduleid);
	count = 0;
	for(function=module->ty->tof->ids; function != nil; function=function->next){
		if(function->ty->varargs)
			continue;
		native64name(function, name, sizeof(name)-2);
		print("\t{\n\t\t");
		native64symbol(function);
		print(", // Module-relative symbol; methods keep their ADT qualifier.\n");
		print("\t\t0, // Specification only, never a callable entry.\n");
		print("\t\t0x%luxU, // Existing Limbo hash, not a structural Dis64 signature.\n", (ulong)sign(function));
		snprint(frame, sizeof(frame), "F_%s", name);
		native64descriptorsymbol(frame, "size", symbol, sizeof(symbol));
		print("\t\t%s, // Full fixed-frame extent.\n", symbol);
		print("\t\tsizeof(limbo_native64_%s_map), // Full map8 extent.\n", name);
		print("\t\tlimbo_native64_%s_map // Borrowed immutable map storage.\n\t},\n", name);
		count++;
	}
	if(count == 0)
		print("\t{0} // ISO C requires storage even for an empty logical table.\n");
	print("};\n#define limbo_native64_%s_spec_count %d // Fixed entries; variadic prefixes are excluded.\n#endif\n", moduleid, count);
	print("\n#ifdef LIMBO_NATIVE64_IMPLEMENTATIONS\n");
	print("static const LimboNative64Entry *const limbo_native64_%s_implementations[] = {\n", moduleid);
	for(function=module->ty->tof->ids; function != nil; function=function->next){
		if(function->ty->varargs)
			continue;
		native64name(function, name, sizeof(name)-2);
		print("\t&limbo_native64_%s_entry, // Trusted C implementation; fixed frame only.\n", name);
	}
	if(count == 0)
		print("\t0 // Storage only; this module has no fixed callable implementation.\n");
	print("};\n#define limbo_native64_%s_implementation_count %d // Logical count, excluding variadic tails.\n#endif\n", moduleid, count);
}


/* Describe top-level argument fields without treating padding or mapped references as scalars. */
static void
native64fieldtable(Decl *function, char *name)
{
	Decl *argument;
	Native64Type *child;
	long offset;
	int ordinal, kind;

	print("#ifdef LIMBO_NATIVE64_FIELDS\n");
	print("static const LimboNative64Field limbo_native64_%s_fields[] = {\n", name);
	offset = 64;
	ordinal = 0;
	for(argument=function->ty->ids; argument != nil; argument=argument->next){
		child = native64type(argument->ty, 0);
		offset = native64align(offset, child->alignment);
		kind = argument->ty->kind == Tbig ? 2 : argument->ty->kind == Treal ? 3 : 0;
		print("\t{%d, %d, %ld, %ld}, // %s: existing Big=2/Real=3; zero means unsupported.\n",
			ordinal++, kind, offset, child->size, argument->sym == nil ? "anonymous" : argument->sym->name);
		offset = native64end(offset, child->size);
	}
	if(ordinal == 0)
		print("\t{0, 0, 0, 0} // ISO C storage only; logical argument count is zero.\n");
	print("};\n\nstatic const LimboNative64Fields limbo_native64_%s_field_table = {\n", name);
	print("\t&limbo_native64_%s_entry, // Exact compiled callable identity.\n", name);
	print("\tlimbo_native64_%s_fields, // Complete source argument sequence.\n", name);
	print("\t%d // Declared argument count; no variadic or nested-field admission.\n};\n#endif\n", ordinal);
}


/* Preserve the list element type in the first private managed call shapes. */
static char*
native64managedkind(Type *type)
{
	if(type->kind == Tstring)
		return "LimboNative64String";
	if(type->kind == Tlist && type->tof->kind == Tstring)
		return "LimboNative64ListString";
	return nil;
}

static void
native64managedtable(Decl *function, char *name)
{
	Decl *argument;
	char *input, *output;

	argument = function->ty->ids;
	if(argument == nil || argument->next != nil)
		return;
	input = native64managedkind(argument->ty);
	output = native64managedkind(function->ty->tof);
	if(input == nil || output == nil || strcmp(input, output) == 0)
		return;
	print("#ifdef LIMBO_NATIVE64_MANAGED\n");
	print("static const LimboNative64Managed limbo_native64_%s_managed = {\n", name);
	print("\t&limbo_native64_%s_entry, %s, %s\n};\n#endif\n", name, input, output);
}

/* Lay out fixed function arguments after Frame64; variadic tails remain explicitly unsupported. */
static void
native64frame(Decl *function, int emit)
{
	Native64Type frame, *child;
	Decl *argument;
	long offset, bytes;
	int arguments = 0;
	char name[4*StrSize], symbol[4*StrSize+64], *managedin, *managedout;

	native64name(function, name, sizeof(name)-2);
	if(function->ty->polys != nil)
		fatal("native64 polymorphic calls are not supported");
	native64spellcheck(function->ty->tof, 0);
	offset = 64;
	for(argument=function->ty->ids; argument != nil; argument=argument->next){
		if(++arguments > 4096)
			fatal("too many native64 fixed arguments");
		native64spellcheck(argument->ty, 0);
		child = native64type(argument->ty, 0);
		if(child->size == 0)
			fatal("native64 zero-size argument is not supported");
		if(argument->sym != nil && (native64keyword(argument->sym->name) ||
		   strncmp(argument->sym->name, "__limbo_native64_", 17) == 0 ||
		   strcmp(argument->sym->name, "regs") == 0 ||
		   strcmp(argument->sym->name, "ret") == 0 || strcmp(argument->sym->name, "noret") == 0 ||
		   strcmp(argument->sym->name, "temps") == 0))
			fatal("native64 argument name conflicts with frame metadata");
		offset = native64align(offset, child->alignment);
		offset = native64end(offset, child->size);
	}
	memset(&frame, 0, sizeof(frame));
	frame.size = native64align(offset, 8);
	if(!emit)
		return;
	print("\n\n/* Experimental fixed frame for %s; this does not enable a native call. */\n", name);
	print("#define LIMBO_NATIVE64_SIGNATURE_%s 0x%luxU // Compiler-computed legacy hash.\n", name, (ulong)sign(function));
	print("void %s(void*);\ntypedef struct F_%s F_%s;\n\nstruct F_%s {\n", name, name, name, name);
	print("\tFrame regs; // Native links occupy bytes 0..31.\n\t");
	if(function->ty->tof->kind == Tnone)
		print("void *noret; // Reserved result address at byte 32.\n");
	else{
		native64ctype(function->ty->tof, 0);
		print(" *ret; // Borrowed caller result address at byte 32.\n");
	}
	print("\tuchar temps[24]; // Reserved bytes 40..63; no root ownership is inferred.\n");
	native64fields(function->ty->ids, 64, 0);
	print("};\n");
	bytes = (frame.size+63)/64;
	frame.map = allocmem(bytes);
	memset(frame.map, 0, bytes);
	offset = 64;
	for(argument=function->ty->ids; argument != nil; argument=argument->next){
		child = native64type(argument->ty, 0);
		offset = native64align(offset, child->alignment);
		native64merge(&frame, child, offset);
		offset = native64end(offset, child->size);
	}
	print("#define F_%s_variadic %d // Fixed prefix only; no variadic tail ABI is defined.\n", name, function->ty->varargs != 0);
	memmove(name+2, name, strlen(name)+1);
	name[0] = 'F';
	name[1] = '_';
	native64descriptor(name, &frame);
	if(!function->ty->varargs){
		if(function->ty->polys == nil && function->ty->ids != nil && function->ty->ids->next == nil){
			managedin = native64managedkind(function->ty->ids->ty);
			managedout = native64managedkind(function->ty->tof);
			if(managedin != nil && managedout != nil && strcmp(managedin, managedout) != 0){
				print("#define LIMBO_NATIVE64_ARGUMENT_%s %s // Generated managed input kind.\n", name+2, managedin);
				print("#define LIMBO_NATIVE64_RESULT_%s %s // Generated managed result kind.\n", name+2, managedout);
			}
		}
		print("#ifdef LIMBO_NATIVE64_ENTRIES\n");
		native64descriptorsymbol(name, "map", symbol, sizeof(symbol));
		print("static const uchar limbo_native64_%s_map[] = %s; // Complete fixed-frame map8.\n", name+2, symbol);
		print("static const LimboNative64Entry limbo_native64_%s_entry = {\n", name+2);
		print("\t");
		native64symbol(function);
		print(", // Module-relative source symbol.\n");
		print("\t%s, // Actual C entry point.\n", name+2);
		print("\t0x%luxU, // Existing Limbo signature hash.\n", (ulong)sign(function));
		print("\t%ld, // Complete fixed-frame bytes.\n", frame.size);
		print("\t%ld, // Complete map8 bytes.\n", bytes);
		print("\tlimbo_native64_%s_map // Borrowed immutable map.\n};\n", name+2);
		native64fieldtable(function, name+2);
		native64managedtable(function, name+2);
		print("#endif\n");
	}else{
		print("#ifdef LIMBO_NATIVE64_PREFIXES\n");
		native64descriptorsymbol(name, "map", symbol, sizeof(symbol));
		print("static const uchar limbo_native64_%s_prefix_map[] = %s;\n", name+2, symbol);
		print("static const LimboNative64Prefix limbo_native64_%s_prefix = {\n\t{", name+2);
		native64symbol(function);
		print(", 0, 0x%luxU, %ld, %ld, limbo_native64_%s_prefix_map},\n",
			(ulong)sign(function), frame.size, bytes, name+2);
		print("\t%d // Fixed arguments only; no tail or callable implementation.\n};\n#endif\n", arguments);
	}
	free(frame.map);
}


/* Check nested members through their actual enclosing C type, including anonymous tuples. */
static void
native64checkfields(char *name, Decl *fields, long start, char *prefix)
{
	Native64Type *child;
	Decl *field;
	long offset, aligned;
	char unnamed[64], *member;

	offset = start;
	for(field=fields; field != nil; field=field->next){
		if(field->store != Dfield && field->store != Darg)
			continue;
		child = native64type(field->ty, 0);
		aligned = native64align(offset, child->alignment);
		if(field->sym == nil)
			snprint(unnamed, sizeof(unnamed), "__limbo_native64_arg%ld", offset);
		member = allocmem(strlen(prefix)+(field->sym != nil ? strlen(field->sym->name) : strlen(unnamed))+2);
		sprint(member, "%s%s", prefix, field->sym != nil ? field->sym->name : unnamed);
		print("\tif(offsetof(%s, %s) != %ld || sizeof(((%s*)0)->%s) != %ld)\n\t\treturn 0;\n",
			name, member, aligned, name, member, child->size);
		if(field->ty->kind == Tadt || field->ty->kind == Ttuple){
			strcat(member, ".");
			native64checkfields(name, field->ty->ids, aligned, member);
		}
		free(member);
		offset = native64end(aligned, child->size);
	}
}


/* Compare every generated aggregate and fixed frame with the consuming C compiler. */
static void
native64checkbody(Decl *globals)
{
	Native64Type *layout, *child;
	Decl *module, *declaration, *function;
	long size;
	char name[4*StrSize], frame[4*StrSize+2];

	print("\tif(!limbo_native64_model())\n\t\treturn 0;\n");
	for(layout=native64types; layout != nil; layout=layout->next){
		if((layout->type->kind != Tadt && layout->type->kind != Ttuple) ||
		   layout->type->decl == nil || layout->type->decl->sym == anontupsym)
			continue;
		native64name(layout->type->decl, name, sizeof(name));
		print("\tif(sizeof(%s) != %ld)\n\t\treturn 0;\n", name, layout->size);
		native64checkfields(name, layout->type->ids, 0, "");
		for(declaration=layout->type->ids; declaration != nil; declaration=declaration->next){
			if(declaration->store != Dfield || declaration->ty->kind != Tchan)
				continue;
			native64name(declaration, name, sizeof(name));
			child = native64type(declaration->ty->tof, 0);
			print("\tif(sizeof(%s) != %ld)\n\t\treturn 0;\n", name, child->size);
			if(child->type->kind == Tadt || child->type->kind == Ttuple)
				native64checkfields(name, child->type->ids, 0, "");
		}
	}
	for(module=globals; module != nil; module=module->next){
		if(!native64module(module))
			continue;
		for(declaration=module->ty->ids; declaration != nil; declaration=declaration->next){
			if(declaration->store != Dtype)
				continue;
			native64name(declaration, name, sizeof(name));
			child = native64type(declaration->ty, 0);
			if(declaration->ty->kind == Tchan)
				child = native64type(declaration->ty->tof, 0);
			print("\tif(sizeof(%s) != %ld)\n\t\treturn 0;\n", name, child->size);
			if(child->type->kind == Tadt || child->type->kind == Ttuple)
				native64checkfields(name, child->type->ids, 0, "");
		}
		for(function=module->ty->tof->ids; function != nil; function=function->next){
			native64name(function, name, sizeof(name)-2);
			snprint(frame, sizeof(frame), "F_%s", name);
			size = 64;
			for(declaration=function->ty->ids; declaration != nil; declaration=declaration->next){
				child = native64type(declaration->ty, 0);
				size = native64end(native64align(size, child->alignment), child->size);
			}
			print("\tif(sizeof(%s) != %ld || offsetof(%s, regs) != 0 ||\n", frame, native64align(size, 8), frame);
			print("\t   offsetof(%s, %s) != 32 || offsetof(%s, temps) != 40)\n\t\treturn 0;\n",
				frame, function->ty->tof->kind == Tnone ? "noret" : "ret", frame);
			if(function->ty->tof->kind != Tnone){
				child = native64type(function->ty->tof, 0);
				print("\tif(sizeof(*((%s*)0)->ret) != %ld)\n\t\treturn 0;\n", frame, child->size);
			}
			native64checkfields(frame, function->ty->ids, 64, "");
		}
	}
	print("\treturn 1;\n}\n");
}


/* Give each module a distinct predicate so separately generated headers can coexist. */
static void
native64checks(Decl *globals)
{
	Decl *module;
	char name[4*StrSize];

	for(module=globals; module != nil; module=module->next){
		if(!native64module(module))
			continue;
		native64name(module, name, sizeof(name));
		print("\n\n/* Validate emitted C geometry; calls and GC ownership require separate gates. */\nstatic int\nlimbo_native64_%s_layout(void)\n{\n", name);
		native64checkbody(globals);
	}
}


/* Preflight every supported declaration before output; keep legacy target metadata immutable. */
void
native64stubs(Decl *globals)
{
	Decl *module, *declaration, *function;
	Native64Type *layout, *next;
	Native64Name *entry, *following;
	char name[4*StrSize], symbol[4*StrSize+32];
	Decl *field;
	int previous;

	if(native64targetmodule != nil){
		previous = 0;
		for(module=globals; module != nil; module=module->next)
			if(native64module(module))
				previous++;
		if(previous != 1)
			fatal("native64 selected module is missing or ambiguous: %s", native64targetmodule);
	}
	native64types = nil;
	native64count = 0;
	native64names = nil;
	native64namecount = 0;
	/* Register source functions before any metadata suffix can shadow them. */
	for(module=globals; module != nil; module=module->next){
		if(!native64module(module))
			continue;
		for(function=module->ty->tof->ids; function != nil; function=function->next){
			native64name(function, name, sizeof(name)-2);
			entry = native64register(name, function);
			entry->callable = 1;
		}
	}
	for(module=globals; module != nil; module=module->next){
		if(!native64module(module))
			continue;
		native64name(module, name, sizeof(name));
		for(declaration=module->ty->ids; declaration != nil; declaration=declaration->next){
			if(declaration->store == Dglobal)
				fatal("native64 module data is not supported");
			if(declaration->store == Dtype){
				native64name(declaration, name, sizeof(name));
				native64spellcheck(declaration->ty, 0);
				if(declaration->ty->kind == Tchan)
					native64spellcheck(declaration->ty->tof, 0);
			}
		}
		for(function=module->ty->tof->ids; function != nil; function=function->next)
			native64frame(function, 0);
	}
	previous = -1;
	while(previous != native64count){
		previous = native64count;
		for(layout=native64types; layout != nil; layout=layout->next){
			native64spellcheck(layout->type, 0);
			if(layout->type->kind != Tadt && layout->type->kind != Ttuple)
				continue;
			for(field=layout->type->ids; field != nil; field=field->next)
				if(field->store == Dfield && field->ty->kind == Tchan){
					native64name(field, name, sizeof(name));
					native64spellcheck(field->ty->tof, 0);
				}
		}
	}
	for(layout=native64types; layout != nil; layout=layout->next){
		if((layout->type->kind != Tadt && layout->type->kind != Ttuple) ||
		   layout->type->decl == nil || layout->type->decl->sym == anontupsym)
			continue;
		native64name(layout->type->decl, name, sizeof(name));
		native64register(name, layout->type);
		for(field=layout->type->ids; field != nil; field=field->next)
			if(field->store == Dfield && field->ty->kind == Tchan){
				native64name(field, name, sizeof(name));
				native64reservenamed(name, field);
			}
	}
	for(module=globals; module != nil; module=module->next){
		if(!native64module(module))
			continue;
		for(declaration=module->ty->ids; declaration != nil; declaration=declaration->next){
			if(declaration->store == Dconst &&
			   (declaration->ty->kind == Tbyte || declaration->ty->kind == Tint ||
			    declaration->ty->kind == Tfix || declaration->ty->kind == Tbig)){
				native64name(declaration, name, sizeof(name));
				native64register(name, declaration);
			}
			if(declaration->store != Dtype)
				continue;
			native64name(declaration, name, sizeof(name));
			if((declaration->ty->kind == Tadt || declaration->ty->kind == Ttuple) &&
			   declaration->ty->decl == declaration)
				native64reservenamed(name, declaration->ty);
			else
				native64reservenamed(name, declaration);
		}
		for(function=module->ty->tof->ids; function != nil; function=function->next){
			native64name(function, name, sizeof(name)-2);
			native64register(name, function);
			seprint(symbol, symbol+sizeof(symbol), "limbo_native64_%s_fields", name);
			native64register(symbol, function);
			seprint(symbol, symbol+sizeof(symbol), "limbo_native64_%s_field_table", name);
			native64register(symbol, function);
			if(function->ty->varargs){
				seprint(symbol, symbol+sizeof(symbol), "limbo_native64_%s_prefix", name);
				native64register(symbol, function);
				seprint(symbol, symbol+sizeof(symbol), "limbo_native64_%s_prefix_map", name);
				native64register(symbol, function);
			}
			seprint(symbol, symbol+sizeof(symbol), "F_%s", name);
			native64reservenamed(symbol, function);
			seprint(symbol, symbol+sizeof(symbol), "F_%s_variadic", name);
			native64register(symbol, function);
		}
	}
	print("#if defined(LIMBO_NATIVE64_FIELDS) && !defined(LIMBO_NATIVE64_ENTRIES)\n#define LIMBO_NATIVE64_ENTRIES // Typed field witnesses require exact generated entries.\n#endif\n");
	print("#if defined(LIMBO_NATIVE64_MANAGED) && !defined(LIMBO_NATIVE64_ENTRIES)\n#define LIMBO_NATIVE64_ENTRIES // Managed shape witnesses require exact generated entries.\n#endif\n");
	print("#if defined(LIMBO_NATIVE64_IMPLEMENTATIONS) && !defined(LIMBO_NATIVE64_TABLES)\n#define LIMBO_NATIVE64_TABLES // Full implementation lists require matching specifications.\n#endif\n");
	print("#if defined(LIMBO_NATIVE64_TABLES) && !defined(LIMBO_NATIVE64_ENTRIES)\n#define LIMBO_NATIVE64_ENTRIES // Table maps share opt-in entry storage.\n#endif\n");
	print("/* Experimental native64 declarations only; shared wire and runtime admission are unchanged. */\n");
	print("#include <stddef.h>\n#include <limits.h>\n#include <float.h>\n#ifndef LIMBO_NATIVE64_MODEL_HELPER\n#define LIMBO_NATIVE64_MODEL_HELPER // Emit the native-model predicate once.\n");
	print("\n/* Natural C alignment probes; these fields never enter a VM frame. */\n");
	print("\ntypedef struct {\n\tuchar prefix; // Alignment probe prefix.\n\tWORD value; // Natural word alignment.\n} LimboNative64WordAlign;\n");
	print("\ntypedef struct {\n\tuchar prefix; // Alignment probe prefix.\n\tvoid *value; // Natural ref alignment.\n} LimboNative64RefAlign;\n");
	print("\ntypedef struct {\n\tuchar prefix; // Alignment probe prefix.\n\tLONG value; // Natural big alignment.\n} LimboNative64BigAlign;\n");
	print("\ntypedef struct {\n\tuchar prefix; // Alignment probe prefix.\n\tREAL value; // Natural real alignment.\n} LimboNative64RealAlign;\n");
	print("\n\n/* Check the model before consuming this experimental metadata. */\nstatic int\nlimbo_native64_model(void)\n{\n");
	print("\tif(CHAR_BIT != 8 || FLT_RADIX != 2 || DBL_MANT_DIG != 53 || DBL_MAX_EXP != 1024 ||\n\t   sizeof(WORD) != 4 || sizeof(void*) != 8 || sizeof(LONG) != 8 || sizeof(REAL) != 8 ||\n");
	print("\t   offsetof(LimboNative64WordAlign, value) != 4 || offsetof(LimboNative64RefAlign, value) != 8 ||\n\t   offsetof(LimboNative64BigAlign, value) != 8 || offsetof(LimboNative64RealAlign, value) != 8 ||\n\t   sizeof(Frame) != 32 || offsetof(Frame, lr) != 0 || offsetof(Frame, fp) != 8 ||\n");
	print("\t   offsetof(Frame, mr) != 16 || offsetof(Frame, t) != 24)\n\t\treturn 0;\n\treturn 1;\n}\n#endif\n");
	print("#if defined(LIMBO_NATIVE64_ENTRIES) || defined(LIMBO_NATIVE64_PREFIXES)\n#include \"native64-entry.h\"\n#endif\n");
	for(layout=native64types; layout != nil; layout=layout->next){
		if((layout->type->kind == Tadt || layout->type->kind == Ttuple) &&
		   layout->type->decl != nil && layout->type->decl->sym != anontupsym){
			native64name(layout->type->decl, name, sizeof(name));
			print("\ntypedef struct %s %s;\n", name, name);
		}
	}
	for(layout=native64types; layout != nil; layout=layout->next)
		native64definition(layout->type, 0);
	for(module=globals; module != nil; module=module->next){
		if(!native64module(module))
			continue;
		for(declaration=module->ty->ids; declaration != nil; declaration=declaration->next)
			native64constant(declaration);
		for(declaration=module->ty->ids; declaration != nil; declaration=declaration->next){
			if(declaration->store != Dtype)
				continue;
			native64name(declaration, name, sizeof(name));
			layout = native64type(declaration->ty, 0);
			if(declaration->ty->kind == Tchan)
				layout = native64type(declaration->ty->tof, 0);
			if(declaration->ty->decl != declaration ||
			   (declaration->ty->kind != Tadt && declaration->ty->kind != Ttuple)){
				print("\ntypedef ");
				native64ctype(layout->type, 0);
				print(" %s;\n", name);
			}
			native64descriptor(name, layout);
		}
		for(function=module->ty->tof->ids; function != nil; function=function->next)
			native64frame(function, 1);
		native64table(module);
	}
	for(layout=native64types; layout != nil; layout=layout->next){
		if((layout->type->kind != Tadt && layout->type->kind != Ttuple) ||
		   layout->type->decl == nil || layout->type->decl->sym == anontupsym)
			continue;
		for(field=layout->type->ids; field != nil; field=field->next){
			if(field->store != Dfield || field->ty->kind != Tchan)
				continue;
			native64name(field, name, sizeof(name));
			print("\ntypedef ");
			native64ctype(field->ty->tof, 0);
			print(" %s;\n", name);
			native64descriptor(name, native64type(field->ty->tof, 0));
		}
	}
	native64checks(globals);
	for(layout=native64types; layout != nil; layout=next){
		next = layout->next;
		free(layout->map);
		free(layout);
	}
	for(entry=native64names; entry != nil; entry=following){
		following = entry->next;
		free(entry->name);
		free(entry);
	}
	native64names = nil;
	native64namecount = 0;
	native64types = nil;
	native64count = 0;
}

/* Diagnostic-only provenance. Never retain these IR pointers after this module. */
typedef struct Native64CallArg Native64CallArg;
struct Native64CallArg {
    int kind, fixed;
    long legacy;
    unsigned int native, ordinal;
};
typedef struct Native64CallPC Native64CallPC;
struct Native64CallPC {
    unsigned int id;
    Inst *frame, *result, *call;
    Decl *caller; // Persistent function identity; its frame descriptor is resolved later.
    Native64CallArg *args; // Owned primitive field witnesses copied before AST retirement.
    unsigned int arguments, nativebytes;
    char *lowerreason;
    Native64CallPC *next;
};
static Native64CallPC *native64pcfirst, *native64pclast;
static unsigned int native64pccount, native64pcarguments;

void
native64callreset(void)
{
    Native64CallPC *note, *next;
    for(note=native64pcfirst; note != nil; note=next) { next = note->next; free(note->args); free(note); }
    native64pcfirst = native64pclast = nil;
    native64pccount = native64pcarguments = 0;
}

void
native64callnote(unsigned int id, Inst *frame, Inst *result, Inst *call, Decl *callee, Type *actual)
{
    Native64CallPC *note;
    Decl *argument, *fixed;
    unsigned int count = 0, i, tail = 0, offset = 64, width;
    char name[4*StrSize];
    if(native64pccount == 65536) {
        fprint(2, "NATIVE64_CALL_PC {\"id\":%ud,\"module\":\"%s\",\"pc_space\":\"legacy-dis\",\"located\":false,\"reason\":\"site-limit\"}\n", id, impdecl->sym->name);
        return;
    }
    note = allocmem(sizeof(*note));
    memset(note, 0, sizeof(*note));
    note->caller = curfn;
    note->id = id; note->frame = frame; note->result = result; note->call = call; note->next = nil;
    if(native64pclast != nil) native64pclast->next = note;
    else native64pcfirst = note;
    native64pclast = note;
    native64pccount++;
    dotprint(name, name+sizeof(name), callee, '_');
    if(strcmp(name, "Sys_sprint") != 0 && strcmp(name, "Sys_aprint") != 0 &&
       strcmp(name, "Sys_print") != 0 && strcmp(name, "Sys_fprint") != 0) {
        note->lowerreason = "callee"; return;
    }
    for(argument=actual->ids; argument != nil; argument=argument->next)
        if(++count > 4098) { note->lowerreason = "argument-count"; return; }
    if(count > 1024*1024-native64pcarguments) { note->lowerreason = "argument-limit"; return; }
    note->args = allocmem(count*sizeof(*note->args));
    note->arguments = count; native64pcarguments += count;
    fixed = callee->ty->ids;
    for(argument=actual->ids, i=0; argument != nil; argument=argument->next, i++) {
        Native64CallArg *field = &note->args[i];
        field->kind = argument->ty->kind; field->legacy = argument->offset;
        field->fixed = fixed != nil;
        if(fixed != nil) {
            width = native64callwidth(fixed->ty);
            if(width == 0) { note->lowerreason = "fixed-kind"; return; }
            offset = (offset+width-1)&~(width-1);
            field->native = offset; field->ordinal = i;
            offset += width; fixed = fixed->next;
            if(fixed == nil) offset = (offset+7)&~7U;
        } else {
            if(tail == 4096 || native64callkind(argument->ty) == nil) { note->lowerreason = "tail-kind-or-count"; return; }
            field->native = offset; field->ordinal = tail++; offset += 8;
        }
    }
    note->nativebytes = offset;
}

static int
native64pcpresent(Inst **image, long count, Inst *inst)
{
    return image != nil && inst != nil && inst->pc >= 0 && inst->pc < count && image[inst->pc] == inst;
}

static char*
native64pcspace(int mode)
{
    switch(mode) {
    case Afp: return "fp";
    case Amp: return "mp";
    case Afpind: return "fp-indirect";
    case Ampind: return "mp-indirect";
    }
    return "other";
}

static void native64callstores(Native64CallPC*, int, Decl*, Inst**, long, int);

/* Report final reachable legacy coordinates only after PC/descriptor/import resolution.
 * Missing or rewritten instructions are explicit refusals, never guessed PC offsets. */
void
native64callpcs(Inst *first, long count, Decl *globals, int noexceptions)
{
    Native64CallPC *note;
    Inst **image = nil, *inst, *frame, *result, *call;
    long binding, callerpc;
    int linked, callerdesc;
    if(native64pcfirst == nil) return;
    if(count > 0 && count <= 1024*1024) {
        image = allocmem(count*sizeof(*image));
        memset(image, 0, count*sizeof(*image));
        for(inst=first; inst != nil; inst=inst->next)
            if(inst->op != INOOP && inst->pc >= 0 && inst->pc < count) image[inst->pc] = inst;
    }
    for(note=native64pcfirst; note != nil; note=note->next) {
        frame = note->frame; result = note->result; call = note->call;
        if(!native64pcpresent(image, count, frame) || !native64pcpresent(image, count, call) ||
           (result != nil && !native64pcpresent(image, count, result))) {
            fprint(2, "NATIVE64_CALL_PC {\"id\":%ud,\"module\":\"%s\",\"pc_space\":\"legacy-dis\",\"located\":false,\"reason\":\"missing-or-rewritten\"}\n", note->id, impdecl->sym->name);
            native64callstores(note, 0, globals, image, count, noexceptions);
            continue;
        }
        callerdesc = note->caller == nil || note->caller->desc == nil ? -1 : note->caller->desc->id;
        callerpc = note->caller == nil || note->caller->pc == nil ? -1 : note->caller->pc->pc;
        binding = -1;
        if(call->mm == Aoff && call->m.decl != nil && call->m.decl->iface != nil) binding = call->m.decl->iface->offset;
        linked = callerdesc >= 0 && callerpc >= 0 && callerpc <= frame->pc && frame->op == IFRAME && frame->sm == Adesc && frame->dm == Afp &&
            call->op == IMCALL && call->sm == Afp && binding >= 0 && frame->d.reg == call->s.reg &&
            result != nil && result->op == ILEA && result->dm == Afpind &&
            result->d.reg == frame->d.reg && result->d.offset == REGRET*IBY2WD &&
            frame->pc < result->pc && result->pc < call->pc;
        fprint(2, "NATIVE64_CALL_PC {\"id\":%ud,\"module\":\"%s\",\"pc_space\":\"legacy-dis\",\"located\":true,\"linked\":%s,\"caller_pc\":%ld,\"caller_descriptor\":%d,\"frame_pc\":%ld,\"frame_opcode\":%ud,\"frame_descriptor\":%ld,\"frame_slot\":%ld,\"result_pc\":%ld,\"call_pc\":%ld,\"call_opcode\":%ud,\"binding\":%ld,\"module_space\":\"%s\",\"module_offset\":%ld",
            note->id, impdecl->sym->name, linked ? "true" : "false", callerpc, callerdesc, frame->pc, frame->op, frame->s.offset, frame->d.reg,
            result == nil ? -1 : result->pc, call->pc, call->op, binding, native64pcspace(call->dm), call->d.reg);
        if(result != nil)
            fprint(2, ",\"result_space\":\"%s\",\"result_offset\":%ld,\"result_indirect\":%ld", native64pcspace(result->sm), result->s.reg, result->s.offset);
        fprint(2, "}\n");
        native64callstores(note, linked, globals, image, count, noexceptions);
    }
    free(image);
    native64callreset();
}

/* Private storage geometry only: it does not admit a serialized native caller.
 * Keep scalar words packed, align widened references and reserve the native header. */
typedef struct Native64SourceLayout Native64SourceLayout;
struct Native64SourceLayout {
    unsigned int *offsets;
    uchar *map;
    unsigned int bytes, mapbytes, legacybytes;
    Dis64Layout typed;
};

static int
native64sourcelayout(Desc *desc, Decl *arguments, Decl *locals, int fp, Native64SourceLayout *layout)
{
    unsigned int i, bit, width, root;
    Dis64LayoutLeaf *leaf;
    if(!dis64layoutstorage(arguments,locals,desc,fp,&layout->typed)) {
        dis64layoutfree(&layout->typed); return 0;
    }
    layout->legacybytes = desc->size;
    layout->offsets = allocmem((desc->size/4+1)*sizeof(*layout->offsets));
    for(i=0;i<=desc->size/4;i++) layout->offsets[i]=(unsigned int)-1;
    layout->bytes=layout->typed.bytes;
    layout->mapbytes=(layout->bytes+63)/64;
    layout->map=allocmem(layout->mapbytes?layout->mapbytes:1);
    memset(layout->map,0,layout->mapbytes);
    for(i=0;i<layout->typed.count;i++) {
        leaf=&layout->typed.leaves[i];
        root=leaf->kind>=0 && leaf->oldbytes==4 && leaf->bytes==8;
        if(root) layout->map[leaf->native/64] |= 0x80>>((leaf->native/8)%8);
        if(leaf->kind<0) continue;
        for(width=0;width+4<=leaf->oldbytes;width+=4) {
            bit=leaf->legacy+width;
            if(bit%4==0) layout->offsets[bit/4]=leaf->native+width;
        }
    }
    return 1;
}

/* Resolve only inline aggregate fields. References, padding and dynamic initializers
 * never turn into constant native reads. The AST and field offsets remain borrowed. */
static int
native64sourcefield(Type **type, Node **value, long offset)
{
    Type *t=*type;
    Node *n=*value, *item;
    Decl *field;
    int depth=0;
    while(t!=nil && (t->kind==Ttuple || t->kind==Tadt || t->kind==Texception)) {
        if(++depth==128 || t->tags!=nil || offset<0 || offset>=t->size) return 0;
        item=n==nil?nil:n->left;
        for(field=t->ids;field!=nil;field=field->next) {
            if(field->store!=Dfield) continue;
            if(field->ty==nil) return 0;
            if(offset>=field->offset && offset-field->offset<field->ty->size) break;
            if(item!=nil) item=item->right;
        }
        if(field==nil || (n!=nil && (item==nil || item->left==nil))) return 0;
        n=item==nil?nil:item->left;
        offset-=field->offset; t=field->ty;
    }
    if(t==nil || offset!=0) return 0;
    *type=t; *value=n; return 1;
}

/* Emit exact typed MP constants, including nested inline fields. Missing seeds
 * remain explicit: this is not reaching-definition analysis or a complete initializer. */
static void
native64sourceseed(Decl *globals, long offset, int kind)
{
    Decl *d;
    Node *n;
    Type *type;
    Sym *text;
    uvlong bits;
    ulong real[2];
    unsigned int i;
    for(d=globals; d != nil; d=d->next) {
        if(d->store != Dglobal || d->ty == nil || offset<d->offset || offset-d->offset>=d->ty->size) continue;
        n=d->init; type=d->ty;
        if(!native64sourcefield(&type,&n,offset-d->offset)) continue;
        if(n == nil || (n->op==Oconst && n->ty!=nil && n->ty->kind==Tany)) {
            if(kind == Tstring && (type->kind == Tstring || type->kind == Tany)) {
                fprint(2, "{\"kind\":\"nil-string\"}"); return;
            }
            continue;
        }
        if(n->op != Oconst || n->ty == nil || n->ty->kind != type->kind) continue;
        if(kind == Tstring && type->kind == Tstring && n->decl != nil && n->decl->sym != nil) {
            text = n->decl->sym;
            if(text->len < 0 || text->len > 1024*1024) break;
            fprint(2, "{\"kind\":\"string\",\"bytes\":[");
            for(i=0; i<text->len; i++) fprint(2, "%s%ud", i == 0 ? "" : ",", (uchar)text->name[i]);
            fprint(2, "]}"); return;
        }
        bits = n->val;
        if(kind == Tint && (type->kind == Tint || type->kind == Tfix)) bits &= 0xffffffffULL;
        else if(kind == Tbig && type->kind == Tbig) { /* Retain canonical signed64 bits. */ }
        else if(kind == Treal && type->kind == Treal) {
            dtocanon(n->rval, real); bits = ((uvlong)real[0]<<32) | real[1];
        } else continue;
        fprint(2, "{\"kind\":\"%s\",\"high\":%ud,\"low\":%ud}",
            kind == Tint ? "word" : kind == Tbig ? "long" : "real", (uint)(bits>>32), (uint)bits);
        return;
    }
    fprint(2, "null");
}

/* A local definition is accepted only in a straight direct-write suffix.
 * Calls, address escape, indirect writes, unknown opcodes and hidden entries refuse. */
static Inst*
native64fpdefinition(Native64CallPC *note, long offset, unsigned int bytes, int kind,
    Inst **image, long count, int noexceptions, Inst *projected, Inst **aggregate)
{
    Inst *in, *definition = nil;
    long pc, j, first;
    unsigned int width, input, bit;
    int supported;
    Type *copytype;
    Dis64Layout copy;
    unsigned int translated, leaf;
    *aggregate=nil;
    if(!noexceptions || image == nil || note->caller == nil || note->caller->pc == nil) return nil;
    first = note->caller->pc->pc;
    for(pc=note->frame->pc-1; pc>=first; pc--) {
        in = image[pc];
        if(in == nil || in->branch != nil || in->dm != Afp) return nil;
        if(in->op==IMOVMP){
            if(in->mm!=Adesc || in->m.decl==nil || in->m.decl->desc==nil ||
               (copytype=in->m.decl->ty)==nil || in->m.decl->desc->size!=copytype->size ||
               in->sm!=Amp || in->s.reg<0 || in->s.reg>Dis64LayoutBytes ||
               offset<in->d.reg || offset-in->d.reg>copytype->size ||
               bytes>copytype->size-(offset-in->d.reg)) return nil;
            memset(&copy,0,sizeof(copy));
            if(!dis64layouttype(copytype,&copy)){dis64layoutfree(&copy); return nil;}
            supported=dis64layoutaddress(&copy,offset-in->d.reg,bytes,kind==Tstring,&translated);
            for(leaf=0;leaf<copy.count;leaf++) if(copy.leaves[leaf].legacy==offset-in->d.reg) break;
            if(!supported || leaf==copy.count || copy.leaves[leaf].oldbytes!=bytes ||
               copy.leaves[leaf].kind!=kind){dis64layoutfree(&copy); return nil;}
            dis64layoutfree(&copy);
            *projected=*in; projected->mm=Anone;
            memset(&projected->m,0,sizeof(projected->m));
            projected->op=kind==Tstring?IMOVP:kind==Tint?IMOVW:kind==Tbig?IMOVL:kind==Treal?IMOVF:0;
            if(projected->op==0) return nil;
            projected->s.reg=in->s.reg+offset-in->d.reg; projected->d.reg=offset;
            *aggregate=in; definition=projected; break;
        }
        if(in->mm!=Anone) return nil;
        switch(in->op) {
        case IMOVW: width = 4; break;
        case IMOVP: width = 4; break;
        case IMOVL: case IMOVF: case ICVTWL: case ICVTWF: case ICVTLF: width = 8; break;
        default: return nil;
        }
        if(in->d.reg < offset+bytes && offset < in->d.reg+width) {
            supported = kind == Tint ? in->op == IMOVW : kind == Tstring ? in->op == IMOVP :
                kind == Tbig ? in->op == IMOVL || in->op == ICVTWL :
                kind == Treal ? in->op == IMOVF || in->op == ICVTWF || in->op == ICVTLF : 0;
            if(!supported || in->d.reg != offset || width != bytes ||
               (in->sm != Aimm && in->sm != Amp) ||
               (in->sm == Aimm && in->op != IMOVW && in->op != ICVTWL && in->op != ICVTWF)) return nil;
            definition = in; break;
        }
    }
    if(definition == nil) return nil;
    for(j=0; j<count; j++) {
        in = image[j]; if(in == nil) continue;
        if(in->op == IGOTO || in->op == ICASE || in->op == ICASEC || in->op == ICASEL ||
           in->op == IALT || in->op == INBALT || (in->op == IJMP && in->branch == nil)) return nil;
        if(in->branch != nil && in->branch->pc > definition->pc && in->branch->pc <= note->frame->pc) return nil;
    }
    if(definition->sm == Amp) {
        input = definition->op == IMOVP || definition->op == IMOVW || definition->op == ICVTWL || definition->op == ICVTWF ? 4 : 8;
        if(descriptors == nil || definition->s.reg < 0 || definition->s.reg%4 ||
           descriptors->size < input || definition->s.reg > descriptors->size-input) return nil;
        for(j=0; j<input/4; j++) {
            bit = definition->s.reg/4+j;
            if(!!(bit/8 < descriptors->nmap && (descriptors->map[bit/8]&(0x80>>(bit%8)))) != (definition->op == IMOVP)) return nil;
        }
    }
    return definition;
}

/* Lower proven direct argument writes and private FP/MP source locations.
 * Records remain internal compiler plans, not an executable image. */
static void
native64callstores(Native64CallPC *note, int linked, Decl *globals, Inst **image, long nimage, int noexceptions)
{
    typedef struct Store Store;
    struct Store { Inst *origin; unsigned int field, op, cell; };
    typedef struct Cell Cell;
    struct Cell { int mode, kind; long legacy; unsigned int oldbytes, bytes, native, origin; Inst *setup, *aggregate; Inst projected; };
    Cell *cells = nil;
    unsigned int ncells = 0, j, oldbytes, readbytes, readkind, root, bit, mapbytes, bankbytes, origin;
    uchar bankmap[(64+4098*8+63)/64];
    Desc *descriptor;
    Native64SourceLayout layouts[2] = {{0}, {0}}, *layout;
    unsigned int space;
    Store *stores = nil;
    uchar *seen = nil;
    Native64CallArg *field;
    Inst *in;
    unsigned int count = 0, i, low, high, middle, op;
    char *reason = note->lowerreason, *kind, *source;
    long value;
    if(reason == nil && !linked) reason = "unlinked";
    if(reason == nil && note->args == nil) reason = "missing-fields";
    if(reason == nil) {
        stores = allocmem(note->arguments*sizeof(*stores));
        cells = allocmem(note->arguments*sizeof(*cells));
        memset(bankmap, 0, sizeof(bankmap));
        seen = allocmem(note->arguments); memset(seen, 0, note->arguments);
        for(in=note->frame->next; in != note->result; in=in->next) {
            if(in == nil) { reason = "instruction-range"; break; }
            if(in->op == INOOP) continue;
            if(count == note->arguments || in->branch != nil || in->mm != Anone ||
               in->dm != Afpind || in->d.reg != note->frame->d.reg ||
               (in->sm != Afp && in->sm != Amp && in->sm != Aimm)) { reason = "argument-address"; break; }
            low = 0; high = note->arguments;
            while(low < high) {
                middle = low+(high-low)/2;
                if(note->args[middle].legacy < in->d.offset) low = middle+1;
                else high = middle;
            }
            if(low == note->arguments || note->args[low].legacy != in->d.offset || seen[low]) { reason = "argument-field"; break; }
            field = &note->args[low]; op = 0;
            switch(field->kind) {
            case Tint: case Tfix:
                if(!field->fixed && in->op == IMOVW) op = ICVTWL;
                break;
            case Tbig:
                if(in->op == ICVTWL || (in->op == IMOVL && in->sm != Aimm)) op = in->op;
                break;
            case Treal:
                if(in->op == ICVTWF || (in->sm != Aimm && (in->op == IMOVF || in->op == ICVTLF))) op = in->op;
                break;
            case Tstring: case Tref: case Tarray: case Tlist: case Tchan: case Tmodule:
                if(in->op == IMOVP && in->sm != Aimm && (field->fixed || field->kind == Tstring)) op = IMOVP;
                break;
            }
            if(op == 0) { reason = "argument-opcode"; break; }
            seen[low] = 1;
            stores[count].origin = in; stores[count].field = low; stores[count].op = op; count++;
        }
        if(reason == nil && count != note->arguments) reason = "incomplete-arguments";
    }
    /* One native source cell per identical typed legacy read. No affine widening:
     * references grow4->8, WORD reads stay4 and LONG/REAL reads remain8. */
    if(reason == nil) for(i=0; i<count; i++) {
        in = stores[i].origin; stores[i].cell = (unsigned int)-1;
        if(in->sm == Aimm) continue;
        root = stores[i].op == IMOVP;
        oldbytes = root || stores[i].op == ICVTWL || stores[i].op == ICVTWF ? 4 : 8;
        readbytes = root ? 8 : oldbytes;
        readkind = root ? (note->args[stores[i].field].kind == Tstring ? Tstring : Tref) :
            stores[i].op == IMOVF ? Treal : oldbytes == 8 ? Tbig : Tint;
        descriptor = in->sm == Afp ? note->caller->desc : descriptors;
        value = in->s.reg;
        if(descriptor == nil || descriptor->size < 0 || value < (in->sm == Afp ? STemp : 0) || value%4 != 0 ||
           descriptor->size < oldbytes || value > descriptor->size-oldbytes) { reason = "source-bounds"; break; }
        for(j=0; j<oldbytes/4; j++) {
            bit = value/4+j;
            if(!!(bit/8 < descriptor->nmap && (descriptor->map[bit/8] & (0x80>>(bit%8)))) != root) {
                reason = "source-root-role"; break;
            }
        }
        if(reason != nil) break;
        space = in->sm == Afp ? 0 : 1; layout = &layouts[space];
        if(layout->offsets == nil && !native64sourcelayout(descriptor,
           space==0?note->caller->ty->ids:globals,space==0?note->caller->locals:nil,space==0,layout)) {
            reason = "source-layout"; break;
        }
        if(!dis64layoutaddress(&layout->typed,value,oldbytes,root,&origin)) {
            reason = "source-typed-span"; break;
        }
        for(j=0; j<ncells; j++) if(cells[j].mode == in->sm) {
            if(value < cells[j].legacy+cells[j].oldbytes && cells[j].legacy < value+oldbytes) {
                if(value != cells[j].legacy || oldbytes != cells[j].oldbytes || readkind != cells[j].kind) reason = "source-overlap-or-kind";
                break;
            }
        }
        if(reason != nil) break;
        if(j == ncells) {
            cells[j].mode = in->sm; cells[j].kind = readkind; cells[j].legacy = value;
            cells[j].oldbytes = oldbytes; cells[j].bytes = readbytes; cells[j].native = 64+j*8;
            cells[j].origin = origin;
            if(root) bankmap[cells[j].native/64] |= 0x80>>((cells[j].native/8)%8);
            ncells++;
        }
        stores[i].cell = j;
    }
    if(reason == nil) for(i=0; i<ncells; i++) {
        cells[i].aggregate=nil;
        cells[i].setup = cells[i].mode == Afp ? native64fpdefinition(note, cells[i].legacy, cells[i].oldbytes, cells[i].kind, image, nimage, noexceptions,&cells[i].projected,&cells[i].aggregate) : nil;
        if(cells[i].setup != nil && cells[i].setup->sm == Amp) {
            in=cells[i].setup;
            root=in->op==IMOVP;
            oldbytes=root || in->op==IMOVW || in->op==ICVTWL || in->op==ICVTWF ? 4 : 8;
            if((layouts[1].offsets==nil && !native64sourcelayout(descriptors,globals,nil,0,&layouts[1])) ||
               !dis64layoutaddress(&layouts[1].typed,in->s.reg,oldbytes,root,&origin)) cells[i].setup=nil;
        }
    }
    bankbytes = 64+ncells*8; mapbytes = (bankbytes+63)/64;
    if(reason != nil) {
        fprint(2, "NATIVE64_CALL_LOWER {\"id\":%ud,\"module\":\"%s\",\"lowered\":false,\"reason\":\"%s\"}\n", note->id, impdecl->sym->name, reason);
    } else {
        fprint(2, "NATIVE64_CALL_LOWER {\"id\":%ud,\"module\":\"%s\",\"lowered\":true,\"source_space\":\"legacy-operands\",\"frame_bytes\":%ud,\"stores\":[", note->id, impdecl->sym->name, note->nativebytes);
        for(i=0; i<count; i++) {
            in = stores[i].origin; field = &note->args[stores[i].field];
            switch(field->kind) {
            case Tint: kind = "int"; break;
            case Tfix: kind = "fix"; break;
            case Tbig: kind = "big"; break;
            case Treal: kind = "real"; break;
            case Tstring: kind = "string"; break;
            default: kind = "reference"; break;
            }
            source = in->sm == Aimm ? "immediate" : native64pcspace(in->sm);
            value = in->sm == Aimm ? in->s.offset : in->s.reg;
            fprint(2, "%s{\"legacy_pc\":%ld,\"opcode\":%ud,\"fixed\":%s,\"ordinal\":%ud,\"kind\":\"%s\",\"source_mode\":\"%s\",\"source_value\":%ld,\"destination\":%ud,\"bytes\":8,\"source_cell\":%ld,\"source_offset\":%ud}",
                i == 0 ? "" : ",", in->pc, stores[i].op, field->fixed ? "true" : "false", field->ordinal, kind, source, value, field->native, stores[i].cell == (unsigned int)-1 ? -1L : (long)stores[i].cell, stores[i].cell == (unsigned int)-1 ? 0 : cells[stores[i].cell].native);
        }
        fprint(2, "],\"bank\":{\"evaluation\":\"per-read\",\"bytes\":%ud,\"map\":[", bankbytes);
        for(i=0; i<mapbytes; i++) fprint(2, "%s%ud", i == 0 ? "" : ",", bankmap[i]);
        fprint(2, "],\"cells\":[");
        for(i=0; i<ncells; i++) {
            kind = cells[i].kind == Tstring ? "string" : cells[i].kind == Tref ? "reference" :
                cells[i].kind == Tint ? "word" : cells[i].kind == Tbig ? "long" : "real";
            fprint(2, "%s{\"source_mode\":\"%s\",\"legacy_offset\":%ld,\"legacy_bytes\":%ud,\"native_offset\":%ud,\"native_bytes\":%ud,\"class\":\"%s\",\"origin_offset\":%ud,\"initial\":",
                i == 0 ? "" : ",", native64pcspace(cells[i].mode), cells[i].legacy, cells[i].oldbytes, cells[i].native, cells[i].bytes, kind, cells[i].origin);
            if(cells[i].mode == Amp) native64sourceseed(globals, cells[i].legacy, cells[i].kind);
            else fprint(2, "null");
            fprint(2, ",\"setup\":");
            in = cells[i].setup;
            if(in == nil) fprint(2, "null");
            else {
                fprint(2, "{\"legacy_pc\":%ld,\"opcode\":%ud,\"source_mode\":\"%s\",\"source_value\":%ld,\"source_offset\":%ud,\"source_bytes\":%ud,\"input\":",
                    in->pc, in->op, in->sm == Aimm ? "immediate" : "mp", in->sm == Aimm ? in->s.offset : in->s.reg,
                    in->sm == Aimm ? 0 : layouts[1].offsets[in->s.reg/4],
                    in->op == IMOVP ? 8 : in->op == IMOVW || in->op == ICVTWL || in->op == ICVTWF ? 4 : 8);
                if(in->sm == Aimm) fprint(2, "{\"kind\":\"word\",\"high\":0,\"low\":%ud}", (uint)in->s.offset);
                else native64sourceseed(globals, in->s.reg, in->op == IMOVP ? Tstring : in->op == IMOVW || in->op == ICVTWL || in->op == ICVTWF ? Tint : in->op == IMOVF ? Treal : Tbig);
                fprint(2, "}");
            }
            fprint(2, ",\"aggregate\":");
            in=cells[i].aggregate;
            if(cells[i].setup==nil || in==nil) fprint(2,"null");
            else fprint(2,"{\"opcode\":%ud,\"descriptor\":%ld,\"source\":%ld,\"destination\":%ld}",in->op,in->m.offset,in->s.reg,in->d.reg);
            fprint(2, "}");
        }
        fprint(2, "],\"preparation\":[");
        memset(seen, 0, ncells);
        for(j=0, high=0; j<ncells; j++) {
            low = ncells;
            for(i=0; i<ncells; i++) if(!seen[i] && cells[i].setup != nil &&
               (low == ncells || cells[i].setup->pc < cells[low].setup->pc)) low = i;
            if(low == ncells) break;
            seen[low] = 1; fprint(2, "%s%ud", high++ == 0 ? "" : ",", low);
        }
        fprint(2, "],\"sources\":[");
        for(space=0; space<2; space++) {
            layout = &layouts[space];
            fprint(2, "%s{\"space\":\"%s\",\"policy\":\"typed-word4-ref8\",\"legacy_bytes\":%ud,\"bytes\":%ud,\"map\":[",
                space == 0 ? "" : ",", space == 0 ? "fp" : "mp", layout->legacybytes, layout->bytes);
            for(j=0; j<layout->mapbytes; j++) fprint(2, "%s%ud", j == 0 ? "" : ",", layout->map[j]);
            fprint(2, "],\"leaves\":[");
            for(j=0;j<layout->typed.count;j++) {
                Dis64LayoutLeaf *leaf=&layout->typed.leaves[j];
                fprint(2,"%s[%ud,%ud,%ud,%ud,%d]",j?",":"",leaf->legacy,leaf->native,leaf->oldbytes,leaf->bytes,leaf->kind);
            }
            fprint(2, "]}");
        }
        fprint(2, "]}}\n");
    }
    for(space=0; space<2; space++) { free(layouts[space].offsets); free(layouts[space].map); dis64layoutfree(&layouts[space].typed); }
    free(cells); free(seen); free(stores);
}
