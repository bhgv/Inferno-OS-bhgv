#include "leaf-transfer-stub.h"
#ifndef TEST_LEAF_MAPPED_CONS_STUB_H // One descriptor validator identity per unrelated fixture.
#define TEST_LEAF_MAPPED_CONS_STUB_H // Production mapped construction defines TEST_REAL_MAPPED_CONS.
#ifndef TEST_REAL_MAPPED_CONS // Other linked tests never execute mapped-list construction.


/* Fail explicitly if an unrelated native fixture calls mapped descriptor validation. */
Type*
cconsmappedtype(Modlink *module, WORD index)
{
	(void)module;
	(void)index;
	abort();
	return nil;
}

#endif // TEST_REAL_MAPPED_CONS
#endif // TEST_LEAF_MAPPED_CONS_STUB_H
