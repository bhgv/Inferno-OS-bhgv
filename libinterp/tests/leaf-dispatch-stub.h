#ifndef TEST_LEAF_DISPATCH_STUB_H // One complete-dispatch identity for unrelated publication fixtures.
#define TEST_LEAF_DISPATCH_STUB_H // Dedicated dispatch tests link the real runtime executor.
#ifndef TEST_LEAF_DISPATCH_RUNTIME // Other regressions invoke guarded fast entries only.


/* Stop an unrelated fixture from silently substituting for real VM dispatch. */
int
cjitexec(REG *reg, int op, int (*fast)(REG*))
{
	(void)reg;
	(void)op;
	(void)fast;
	abort();
	return 0;
}

#endif // TEST_LEAF_DISPATCH_RUNTIME
#endif // TEST_LEAF_DISPATCH_STUB_H
