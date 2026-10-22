#undef malloc
#undef realloc
#undef free

static struct {
	void *pointer;
	size_t size;
} allocations[4096];
static int live, fail_after = -1;

/* Track ordinary allocation failures independently of VM heap exceptions. */
void*
testmalloc(size_t size)
{
	void *pointer;
	int i;

	if(fail_after == 0)
		return nil;
	if(fail_after > 0)
		fail_after--;
	pointer = malloc(size ? size : 1);
	assert(pointer);
	for(i = 0; i < 4096; i++)
		if(allocations[i].pointer == nil) {
			allocations[i].pointer = pointer;
			allocations[i].size = size;
			live++;
			return pointer;
		}
	abort();
	return nil;
}

/* Reallocation retains ownership if a failure is injected. */
void*
testrealloc(void *old, size_t size)
{
	void *pointer;
	int i;

	if(old == nil)
		return testmalloc(size);
	for(i = 0; i < 4096; i++)
		if(allocations[i].pointer == old)
			break;
	assert(i < 4096);
	if(fail_after == 0)
		return nil;
	if(fail_after > 0)
		fail_after--;
	pointer = realloc(old, size);
	assert(pointer);
	allocations[i].pointer = pointer;
	allocations[i].size = size;
	return pointer;
}

/* Reject double frees and count every temporary owned by text conversion. */
void
testfree(void *pointer)
{
	int i;

	if(pointer == nil)
		return;
	for(i = 0; i < 4096; i++)
		if(allocations[i].pointer == pointer) {
			allocations[i].pointer = nil;
			live--;
			free(pointer);
			return;
		}
	abort();
}

/* Supply the host allocation helper used by production mpnew. */
void*
mallocz(size_t size, int zero)
{
	void *pointer;

	pointer = testmalloc(size);
	if(pointer && zero)
		memset(pointer, 0, size);
	return pointer;
}

/* Unexpected fatal paths must fail the fixture immediately. */
void
sysfatal(char *message, ...)
{
	fprintf(stderr, "unexpected mp fatal: %s\n", message);
	abort();
}

/* Formatting via Fmt is outside the direct mptoa test path. */
int
fmtstrcpy(Fmt *fmt, char *text)
{
	(void)fmt;
	(void)text;
	abort();
	return 0;
}

/* Expose allocation accounting and failure injection to the Python oracle. */
int
liveblocks(void)
{
	return live;
}

/* Limit failure injection to nonfatal ordinary text-buffer allocations. */
void
setfailure(int after)
{
	fail_after = after;
}
