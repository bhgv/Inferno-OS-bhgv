#include "leaf-dispatch-stub.h"
#ifndef TEST_LEAF_TRANSFER_STUB_H // One preflight identity per unrelated compiler fixture.
#define TEST_LEAF_TRANSFER_STUB_H // Buffered transfer tests provide the real VM helper.
#ifndef TEST_LEAF_TRANSFER_RUNTIME // Unrelated tests never execute SEND/RECV native entries.


/* Fail explicitly if an unrelated fixture unexpectedly stages a channel transfer. */
int
cchanleaf(REG *reg, int receive, void *transaction)
{
	(void)reg;
	(void)receive;
	(void)transaction;
	abort();
	return 0;
}

#endif // TEST_LEAF_TRANSFER_RUNTIME
#endif // TEST_LEAF_TRANSFER_STUB_H
