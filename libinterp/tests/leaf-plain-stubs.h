#ifndef TEST_LEAF_PLAIN_STUBS_H // One definition of callback identities per linked fixture.
#define TEST_LEAF_PLAIN_STUBS_H // Host tests which do not exercise object destruction.


/* Supply a stable plain-record destructor identity for unrelated native publication tests. */
void
freeheap(Heap *heap, int swept)
{
    (void)heap;
    (void)swept;
}


/* Supply a stable plain-record marker identity without replacing production GC tests. */
void
markheap(Type *type, void *object)
{
    (void)type;
    (void)object;
}

#endif // TEST_LEAF_PLAIN_STUBS_H
