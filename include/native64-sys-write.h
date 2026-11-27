#ifndef INFERNO_NATIVE64_SYS_WRITE_H
#define INFERNO_NATIVE64_SYS_WRITE_H // Trusted synchronous output gateways; no file variadic admission.

extern Type *TFD; // Owning port's persistent actual Sys_FD descriptor from sysinit.

typedef struct Native64SysWritePrefix Native64SysWritePrefix;

struct Native64SysWritePrefix
{
	void *fd; // Actual managed Sys_FD identity, or H for descriptor -1.
	String *pattern; // Caller-rooted copied format argument.
};

typedef struct Native64SysWriteCall Native64SysWriteCall;

struct Native64SysWriteCall
{
	Native64SysWriteRequest request; // Stable synchronous control snapshot outside the protected jump frame.
	Native64SysWritePrefix prefix; // Borrowed construction input; copied into the managed argument frame.
	Native64Arguments arguments; // Owned/pinned complete typed frame through formatting and native output.
	Native64Formatted output; // Native bytes retained through the single write attempt.
	int fprint; // Whether the generated fixed prefix includes a Sys_FD.
	int released; // Exactly one successful VM release still needs its matching acquire.
};


/* Check canonical prefix values before exact live managed identities; persistent TFD remains port-owned. */
static int
native64syswriteprefix(void *context, const void *bytes, unsigned int count, int live)
{
	Native64SysWriteCall *call;
	Native64SysWritePrefix prefix;

	call = context;
	if(!call->fprint)
		return native64sysformatprefix(nil, bytes, count, live);
	if(count != sizeof(prefix))
		return 0;
	memmove(&prefix, bytes, sizeof(prefix));
	if(prefix.fd == nil || prefix.pattern == nil)
		return 0;
	return !live || (native64sysformatreference(nil, prefix.fd, TFD) &&
		native64sysformatreference(nil, prefix.pattern, &Tstring));
}


/* Resolve without caching across release/write/acquire; callbacks owe live scalar-cell provenance. */
static void*
native64syswritecell(Native64SysWriteCall *call)
{
	void *cell;

	cell = call->request.format.resolve(call->request.format.argument);
	if(cell == H || (cell != nil && (uintptr)cell > (uintptr)-1-sizeof(WORD)))
		error(exBounds);
	return cell;
}


/* Preserve the write/unwind diagnostic across the nonthrowing scheduler reacquisition boundary. */
static void
native64syswriteacquire(Native64SysWriteCall *call)
{
	char saved[ERRMAX];

	if(!call->released)
		return;
	kgerrstr(saved, sizeof(saved));
	acquire();
	call->released = 0;
	kwerrstr("%s", saved);
}


/* Finish validation/formatting before output; issue one native write and resolve again after reacquiring the VM. */
static int
native64syswriteperform(void *argument)
{
	Native64SysWriteCall *call;
	String *pattern;
	Sys_FD *handle;
	void *cell;
	WORD descriptor, result;
	long written;
	unsigned int offset;

	call = argument;
	offset = call->fprint ? offsetof(F_Sys_fprint, s) : offsetof(F_Sys_print, s);
	memmove(&pattern, (uchar*)call->arguments.frame.value+offset, sizeof(pattern));
	descriptor = 1;
	if(call->fprint){
		memmove(&handle, (uchar*)call->arguments.frame.value+offsetof(F_Sys_fprint, fd), sizeof(handle));
		descriptor = -1;
		if(handle != H)
			memmove(&descriptor, (uchar*)handle+offsetof(Sys_FD, fd), sizeof(descriptor));
	}
	if(!native64formatgrow(call->request.format.prog, &call->arguments, pattern,
	   call->request.format.initial, call->request.format.limit, &call->output))
		return 0;
	if(native64syswritecell(call) == nil)
		return 0;
	release();
	call->released = 1;
	written = kwrite((int)descriptor, call->output.data, call->output.length);
	native64syswriteacquire(call);
	if(written < -1 || written > call->output.length)
		error(exBounds);
	cell = native64syswritecell(call);
	if(cell == nil)
		return 0;
	result = (WORD)written;
	memmove(cell, &result, sizeof(result));
	return 1;
}


/* Own arguments/native bytes outside protection and pair every release even when a native write raises. */
static int
native64syswrite(const Native64SysWriteRequest *request, int fprint)
{
	uchar printmap[] = F_Sys_print_map;
	uchar fprintmap[] = F_Sys_fprint_map;
	const LimboNative64Entry *fixed;
	Native64ArgumentSource source;
	Native64SysWriteCall call;
	int status;

	if(!native64model() || request == nil || request->format.prog == nil ||
	   request->format.prog == H ||
	   request->format.resolve == nil || request->format.initial <= UTFmax+2 ||
	   request->format.limit < request->format.initial ||
	   sizeof(F_Sys_print) != 72 || offsetof(F_Sys_print, s) != 64 ||
	   sizeof(F_Sys_fprint) != 80 || offsetof(F_Sys_fprint, fd) != 64 || offsetof(F_Sys_fprint, s) != 72 ||
	   sizeof(Native64SysWritePrefix) != 16 || offsetof(Native64SysWritePrefix, pattern) != 8 ||
	   sizeof(printmap) != 2 || printmap[0] != 0 || printmap[1] != 0x80 ||
	   sizeof(fprintmap) != 2 || fprintmap[0] != 0 || fprintmap[1] != 0xc0 ||
	   (fprint && (TFD == nil || TFD == H || TFD->ref <= 0)))
		return 0;
	memset(&call, 0, sizeof(call));
	call.request = *request;
	call.fprint = fprint;
	call.prefix.fd = request->fd;
	call.prefix.pattern = request->format.pattern;
	native64argumentsinit(&call.arguments);
	fixed = native64tailprefix(fprint ? &limbo_native64_Sys_fprint_prefix : &limbo_native64_Sys_print_prefix,
		fprint ? "fprint" : "print", fprint ? LIMBO_NATIVE64_SIGNATURE_Sys_fprint : LIMBO_NATIVE64_SIGNATURE_Sys_print,
		fprint ? 2 : 1, fprint ? sizeof(F_Sys_fprint) : sizeof(F_Sys_print), fprint ? fprintmap : printmap, 2);
	if(fixed == nil)
		return 0;
	memset(&source, 0, sizeof(source));
	source.fixed = fixed;
	source.prefix = fprint ? (void*)&call.prefix : (void*)&call.prefix.pattern;
	source.prefix_bytes = fprint ? sizeof(call.prefix) : sizeof(call.prefix.pattern);
	source.slots = call.request.format.slots;
	source.count = call.request.format.count;
	source.values = call.request.format.values;
	source.value_bytes = call.request.format.value_bytes;
	source.prefix_check = native64syswriteprefix;
	source.reference_check = native64sysformatreference;
	source.argument = &call;
	if(!native64argumentsbuild(&call.arguments, &source))
		return 0;
	status = kprotect(native64syswriteperform, &call);
	native64syswriteacquire(&call);
	native64formattedclear(&call.output);
	native64argumentsclear(&call.arguments);
	return status;
}
#endif
