#ifndef TEST_LEAF_CONS_STUB_H // One allocator identity for unrelated linked native fixtures.
#define TEST_LEAF_CONS_STUB_H // Actual scalar list construction uses the production helper.


/* Supply the pointer descriptor identity for unrelated compiler-publication fixtures. */
Type Tptr={1};


/* Satisfy allocator publication in fixtures which never execute list construction. */
List*
cons(ulong size, void *slot)
{
    (void)size;
    (void)slot;
    abort();
    return nil;
}

#endif // TEST_LEAF_CONS_STUB_H
