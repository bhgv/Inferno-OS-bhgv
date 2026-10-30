#include "limbo.h"

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
	Native64Name *next; // Private preflight registry, never a wire symbol table.
};

static Native64Name *native64names; // Catch flattened qualified-name and macro collisions before output.
static int native64namecount; // Enforce a bounded metadata-only symbol population.

static Native64Type *native64types; // Per-emission cache, not a compiler target selection.
static int native64count; // Bound pathological input before allocating another cache entry.

static Native64Type *native64type(Type*, int);
static void native64ctype(Type*, int);


/* Reject ambiguous C names before printing any declaration, including descriptor macros. */
static void
native64register(char *name, void *owner)
{
	Native64Name *entry;

	for(entry=native64names; entry != nil; entry=entry->next)
		if(strcmp(entry->name, name) == 0){
			if(entry->owner != owner)
				fatal("native64 C name collision: %s", name);
			return;
		}
	if(native64namecount == Native64Names)
		fatal("too many native64 C names");
	native64namecount++;
	entry = allocmem(sizeof(*entry));
	entry->name = allocmem(strlen(name)+1);
	strcpy(entry->name, name);
	entry->owner = owner;
	entry->next = native64names;
	native64names = entry;
}


/* Reserve the complete descriptor spelling without conflating C names and runtime IDs. */
static void
native64reservenamed(char *name, void *owner)
{
	char symbol[2*StrSize+32];

	native64register(name, owner);
	seprint(symbol, symbol+sizeof(symbol), "%s_size", name);
	native64register(symbol, owner);
	seprint(symbol, symbol+sizeof(symbol), "%s_map", name);
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
	char name[2*StrSize];

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
	char name[2*StrSize];

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
	char name[2*StrSize];

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
	char name[2*StrSize];

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

	bytes = (layout->size+63)/64;
	print("#define %s_size %ld // Native64 byte extent.\n", name, layout->size);
	print("#define %s_map {", name);
	for(i=0; i<bytes; i++)
		print("%s0x%2.2ux", i == 0 ? "" : ",", layout->map[i]);
	if(bytes == 0)
		print("0");
	print("} // Full map8; result addresses and frame links are not argument roots.\n");
}


/* Lay out fixed function arguments after Frame64; variadic tails remain explicitly unsupported. */
static void
native64frame(Decl *function, int emit)
{
	Native64Type frame, *child;
	Decl *argument;
	long offset, bytes;
	char name[2*StrSize];

	native64name(function, name, sizeof(name)-2);
	if(function->ty->polys != nil)
		fatal("native64 polymorphic calls are not supported");
	native64spellcheck(function->ty->tof, 0);
	offset = 64;
	for(argument=function->ty->ids; argument != nil; argument=argument->next){
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
		print("#ifdef LIMBO_NATIVE64_ENTRIES\n");
		print("static const uchar limbo_native64_%s_map[] = %s_map; // Complete fixed-frame map8.\n", name+2, name);
		print("static const LimboNative64Entry limbo_native64_%s_entry = {\n", name+2);
		print("\t\"%s\", // Source symbol.\n", function->sym->name);
		print("\t%s, // Actual C entry point.\n", name+2);
		print("\t0x%luxU, // Existing Limbo signature hash.\n", (ulong)sign(function));
		print("\t%ld, // Complete fixed-frame bytes.\n", frame.size);
		print("\t%ld, // Complete map8 bytes.\n", bytes);
		print("\tlimbo_native64_%s_map // Borrowed immutable map.\n};\n#endif\n", name+2);
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
	char name[2*StrSize], frame[2*StrSize+2];

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
		if(module->store != Dtype || module->ty->kind != Tmodule)
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
	char name[2*StrSize];

	for(module=globals; module != nil; module=module->next){
		if(module->store != Dtype || module->ty->kind != Tmodule)
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
	char name[2*StrSize], symbol[2*StrSize+32];
	Decl *field;
	int previous;

	native64types = nil;
	native64count = 0;
	native64names = nil;
	native64namecount = 0;
	for(module=globals; module != nil; module=module->next){
		if(module->store != Dtype || module->ty->kind != Tmodule)
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
		if(module->store != Dtype || module->ty->kind != Tmodule)
			continue;
		for(declaration=module->ty->ids; declaration != nil; declaration=declaration->next){
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
			seprint(symbol, symbol+sizeof(symbol), "F_%s", name);
			native64reservenamed(symbol, function);
			seprint(symbol, symbol+sizeof(symbol), "F_%s_variadic", name);
			native64register(symbol, function);
		}
	}
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
	print("#if defined(LIMBO_NATIVE64_ENTRIES) && !defined(LIMBO_NATIVE64_ENTRY_TYPE)\n#define LIMBO_NATIVE64_ENTRY_TYPE // Private metadata record, never the runtime Runtab.\n");
	print("\ntypedef struct {\n\tconst char *name; // Borrowed source symbol.\n\tvoid (*function)(void*); // Actual C entry; no runtime registration is implied.\n\tUWORD legacy_signature; // Existing Limbo signature hash, not a Dis64 wire signature.\n\tunsigned int frame_size; // Full fixed-frame byte extent.\n\tunsigned int map_bytes; // Full map extent; never truncated to map[16].\n\tconst uchar *map; // Borrowed immutable full map8 storage.\n} LimboNative64Entry;\n#endif\n");
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
		if(module->store != Dtype || module->ty->kind != Tmodule)
			continue;
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
