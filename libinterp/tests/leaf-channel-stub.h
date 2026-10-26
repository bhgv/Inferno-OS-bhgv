#ifndef TEST_LEAF_CHANNEL_STUB_H // Keep a single allocator identity in unrelated publication fixtures.
#define TEST_LEAF_CHANNEL_STUB_H // Real channel regressions define TEST_LEAF_CHANNEL_RUNTIME.
#ifndef TEST_LEAF_CHANNEL_RUNTIME // Unrelated native tests never execute channel constructors.


/* Fail if an unrelated fixture unexpectedly calls the scalar channel allocator. */
Channel*
cnewcscalar(int op, int len)
{
	(void)op;
	(void)len;
	abort();
	return nil;
}


/* Reject unexpected raw allocation in unrelated native fixtures. */
Channel*
cnewcraw(int size, int len)
{
	(void)size;
	(void)len;
	abort();
	return nil;
}


/* Reject unexpected mapped allocation in unrelated native fixtures. */
Channel*
cnewcmapped(Modlink *module, WORD index, int len)
{
	(void)module;
	(void)index;
	(void)len;
	abort();
	return nil;
}

#endif // TEST_LEAF_CHANNEL_RUNTIME
#endif // TEST_LEAF_CHANNEL_STUB_H
