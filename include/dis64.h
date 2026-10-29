/*
 * Agreed Dis64 container envelope, not an executable-module loader.
 * Payload meanings are intentionally left to their separately versioned readers.
 * See doc/dis64-contract.md and doc/dis64-registry.json.
 */
#ifndef DIS64_H
#define DIS64_H // Shared Dis64 envelope codec guard.

/* These IDs were accepted together in FLASHVM-0004 and INFERNO-0008. */
enum {
	Dis64Major=1,
	Dis64Minor=0,
	Dis64Abi=1,
	Dis64HeaderBytes=32,
	Dis64DirectoryBytes=12,
	Dis64Required=1,
	Dis64Module=1,
	Dis64Code=2,
	Dis64Types=3,
	Dis64Data=4,
	Dis64Imports=5,
	Dis64Exports=6,
	Dis64Strings=7,
	Dis64Control=8,
	Dis64Debug=32768,
	Dis64Provenance=32769
};

static const unsigned char dis64magic[8] = {0x44, 0x69, 0x73, 0x36, 0x34, 0x0d, 0x0a, 0x1a};

typedef struct Dis64View Dis64View;
struct Dis64View {
	const unsigned char *data; // Borrowed immutable complete container bytes.
	const unsigned char *directory; // Validated directory inside data.
	unsigned int bytes; // Complete file extent, at most UINT32_MAX.
	unsigned int headerbytes; // Fixed header, extension manifest and directory.
	unsigned int extensions; // Number of mandatory extension records.
	unsigned int sections; // Number of checked directory records.
};

typedef struct Dis64Section Dis64Section;
struct Dis64Section {
	unsigned int kind; // Logical kind, including unknown optional kinds.
	unsigned int flags; // Only bit zero is currently defined.
	unsigned int offset; // Checked byte offset in the complete container.
	unsigned int bytes; // Payload extent; no C padding is serialized.
};

typedef struct Dis64Extension Dis64Extension;
struct Dis64Extension {
	const unsigned char *name; // Borrowed ASCII bytes, not NUL terminated.
	unsigned int namebytes; // Canonical name extent, 1..255.
	unsigned int major; // Exact extension major, u16.
	unsigned int minor; // Minimum compatible minor, u16.
};

typedef struct Dis64Payload Dis64Payload;
struct Dis64Payload {
	unsigned int kind; // Accepted section kind or unknown optional kind.
	unsigned int flags; // Required bit independent of numeric kind range.
	const unsigned char *data; // Borrowed payload; must not overlap output.
	unsigned int bytes; // Exact number of bytes copied by the writer.
};

typedef int (*Dis64Supports)(void*, const Dis64Extension*);


/* Decode a bounded BE u16 without alignment assumptions. */
static unsigned int
dis64u16(const unsigned char *p)
{
	return ((unsigned int)p[0]<<8)|p[1];
}


/* Decode a bounded BE u32 only after the caller has checked the full span. */
static unsigned int
dis64u32(const unsigned char *p)
{
	return ((unsigned int)p[0]<<24)|((unsigned int)p[1]<<16)|
		((unsigned int)p[2]<<8)|p[3];
}


/* Write one preflighted BE u16. */
static void
dis64put16(unsigned char *p, unsigned int value)
{
	p[0] = (value>>8)&0xff;
	p[1] = value&0xff;
}


/* Write one preflighted BE u32. */
static void
dis64put32(unsigned char *p, unsigned int value)
{
	p[0] = (value>>24)&0xff;
	p[1] = (value>>16)&0xff;
	p[2] = (value>>8)&0xff;
	p[3] = value&0xff;
}


/* Validate a namespace-qualified manifest name and its u16 version fields. */
static int
dis64extension(const Dis64Extension *extension)
{
	unsigned int i, c;

	if(extension == 0 || extension->name == 0 || extension->namebytes == 0 ||
	   extension->namebytes > 255 || extension->major > 65535 || extension->minor > 65535)
		return 0;
	for(i=0; i<extension->namebytes; i++){
		c = extension->name[i];
		if(c >= 'a' && c <= 'z')
			continue;
		if(i == 0 || !((c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-'))
			return 0;
	}
	return 1;
}


/* Compare ASCII names lexicographically, including unequal prefix lengths. */
static int
dis64nameorder(const Dis64Extension *left, const Dis64Extension *right)
{
	unsigned int i, count;

	count = left->namebytes < right->namebytes ? left->namebytes : right->namebytes;
	for(i=0; i<count; i++)
		if(left->name[i] != right->name[i])
			return left->name[i] < right->name[i] ? -1 : 1;
	return left->namebytes < right->namebytes ? -1 : left->namebytes > right->namebytes;
}


/* Apply common flags, singleton and envelope-minimum rules, not payload decoding. */
static int
dis64sectionrule(unsigned int kind, unsigned int flags, unsigned int bytes, unsigned int *seen)
{
	unsigned int bit;

	if(kind > 65535 || (flags&~Dis64Required) != 0)
		return 0;
	bit = 0;
	if(kind >= Dis64Module && kind <= Dis64Control){
		if(flags != Dis64Required ||
		   ((kind == Dis64Module || kind == Dis64Code || kind == Dis64Types) && bytes == 0) ||
		   (kind >= Dis64Imports && bytes < 4))
			return 0;
		bit = 1U<<(kind-1);
	}else if(kind == Dis64Debug || kind == Dis64Provenance){
		if(flags != 0)
			return 0;
		/* Their internal versions are not specified yet; semantic readers decide. */
		bit = 1U<<(kind == Dis64Debug ? 8 : 9);
	}else if(flags != 0)
		return 0;
	if(bit != 0 && (*seen&bit) != 0)
		return 0;
	*seen |= bit;
	return 1;
}


/* Validate the envelope without allocation or ABI-dependent heap construction.
 * Limits are explicit resource policy; callback confirms every mandatory feature.
 * Success proves only the envelope, never the executable meaning of payloads. */
static int
dis64check(const unsigned char *data, unsigned int bytes,
	unsigned int extensionlimit, unsigned int sectionlimit,
	Dis64Supports supports, void *context, Dis64View *result)
{
	Dis64View view;
	Dis64Extension current, previous;
	const unsigned char *row;
	unsigned int i, at, count, cursor, seen, kind, flags, offset, length;

	if(data == 0 || result == 0 || bytes < Dis64HeaderBytes || (unsigned int)-1 != 0xffffffffU)
		return 0;
	for(i=0; i<8; i++)
		if(data[i] != dis64magic[i])
			return 0;
	if(dis64u16(data+8) != Dis64Major || dis64u16(data+10) != Dis64Minor ||
	   dis64u32(data+16) != Dis64Abi || dis64u32(data+20) != 0)
		return 0;
	view.headerbytes = dis64u32(data+12);
	view.extensions = dis64u32(data+24);
	view.sections = dis64u32(data+28);
	if(view.headerbytes < Dis64HeaderBytes || view.headerbytes > bytes ||
	   view.extensions > extensionlimit || view.sections > sectionlimit ||
	   view.extensions > (view.headerbytes-Dis64HeaderBytes)/9)
		return 0;
	at = Dis64HeaderBytes;
	previous.name = 0;
	previous.namebytes = 0;
	for(i=0; i<view.extensions; i++){
		if(view.headerbytes-at < 8)
			return 0;
		current.namebytes = dis64u16(data+at);
		current.major = dis64u16(data+at+2);
		current.minor = dis64u16(data+at+4);
		if(dis64u16(data+at+6) != 0)
			return 0;
		at += 8;
		if(current.namebytes > view.headerbytes-at)
			return 0;
		current.name = data+at;
		if(!dis64extension(&current) || (i != 0 && dis64nameorder(&previous, &current) >= 0))
			return 0;
		if(supports == 0 || !supports(context, &current))
			return 0;
		previous = current;
		at += current.namebytes;
	}
	count = view.sections;
	if(count > (view.headerbytes-at)/Dis64DirectoryBytes ||
	   count*Dis64DirectoryBytes != view.headerbytes-at)
		return 0;
	view.directory = data+at;
	cursor = view.headerbytes;
	seen = 0;
	for(i=0; i<count; i++){
		row = view.directory+i*Dis64DirectoryBytes;
		kind = dis64u16(row);
		flags = dis64u16(row+2);
		offset = dis64u32(row+4);
		length = dis64u32(row+8);
		if(offset != cursor || length > bytes-cursor || !dis64sectionrule(kind, flags, length, &seen))
			return 0;
		cursor += length;
	}
	if(cursor != bytes || (seen&7) != 7)
		return 0;
	view.data = data;
	view.bytes = bytes;
	*result = view;
	return 1;
}


/* Return one directory entry from an immutable previously checked envelope. */
static int
dis64section(const Dis64View *view, unsigned int index, Dis64Section *section)
{
	const unsigned char *row;

	if(view == 0 || section == 0 || index >= view->sections || view->directory == 0)
		return 0;
	row = view->directory+index*Dis64DirectoryBytes;
	section->kind = dis64u16(row);
	section->flags = dis64u16(row+2);
	section->offset = dis64u32(row+4);
	section->bytes = dis64u32(row+8);
	return 1;
}


/* Emit a canonical envelope from non-overlapping borrowed names and payloads.
 * Preflight all input and capacities before changing output or published length.
 * Writer does not claim support for extensions; the executing reader must do so. */
static int
dis64write(unsigned char *output, unsigned int capacity,
	const Dis64Extension *extensions, unsigned int ne,
	const Dis64Payload *sections, unsigned int ns, unsigned int *written)
{
	unsigned int i, j, at, header, total, seen, cursor;
	const Dis64Extension *extension;
	const Dis64Payload *section;

	if(output == 0 || written == 0 || (unsigned int)-1 != 0xffffffffU ||
	   capacity < Dis64HeaderBytes || (ne != 0 && extensions == 0) ||
	   (ns != 0 && sections == 0) || ns > (capacity-Dis64HeaderBytes)/Dis64DirectoryBytes ||
	   ne > (capacity-Dis64HeaderBytes)/9)
		return 0;
	header = Dis64HeaderBytes+ns*Dis64DirectoryBytes;
	for(i=0; i<ne; i++){
		extension = &extensions[i];
		if(!dis64extension(extension) ||
		   (i != 0 && dis64nameorder(&extensions[i-1], extension) >= 0) ||
		   extension->namebytes+8 > capacity-header)
			return 0;
		header += extension->namebytes+8;
	}
	total = header;
	seen = 0;
	for(i=0; i<ns; i++){
		section = &sections[i];
		if((section->bytes != 0 && section->data == 0) || section->bytes > capacity-total ||
		   !dis64sectionrule(section->kind, section->flags, section->bytes, &seen))
			return 0;
		total += section->bytes;
	}
	if((seen&7) != 7)
		return 0;
	for(i=0; i<8; i++)
		output[i] = dis64magic[i];
	dis64put16(output+8, Dis64Major);
	dis64put16(output+10, Dis64Minor);
	dis64put32(output+12, header);
	dis64put32(output+16, Dis64Abi);
	dis64put32(output+20, 0);
	dis64put32(output+24, ne);
	dis64put32(output+28, ns);
	at = Dis64HeaderBytes;
	for(i=0; i<ne; i++){
		extension = &extensions[i];
		dis64put16(output+at, extension->namebytes);
		dis64put16(output+at+2, extension->major);
		dis64put16(output+at+4, extension->minor);
		dis64put16(output+at+6, 0);
		at += 8;
		for(j=0; j<extension->namebytes; j++)
			output[at++] = extension->name[j];
	}
	cursor = header;
	for(i=0; i<ns; i++){
		section = &sections[i];
		dis64put16(output+at, section->kind);
		dis64put16(output+at+2, section->flags);
		dis64put32(output+at+4, cursor);
		dis64put32(output+at+8, section->bytes);
		at += Dis64DirectoryBytes;
		for(j=0; j<section->bytes; j++)
			output[cursor++] = section->data[j];
	}
	*written = total;
	return 1;
}
#endif
