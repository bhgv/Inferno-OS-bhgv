#ifndef LIMBO_DIS64_LAYOUT_H
#define LIMBO_DIS64_LAYOUT_H

/* Compiler-owned typed relocation plans. These are not serialized wire records. */
enum { Dis64LayoutBytes=1024*1024, Dis64LayoutLeaves=65536 };
enum { Dis64LayoutBorrowed=-1, Dis64LayoutCode=-2, Dis64LayoutOpaque=-3 };
typedef struct Dis64LayoutLeaf Dis64LayoutLeaf;
typedef struct Dis64Layout Dis64Layout;
struct Dis64LayoutLeaf {
	unsigned int legacy, native, oldbytes, bytes;
	int kind;
};
struct Dis64Layout {
	Dis64LayoutLeaf *leaves;
	unsigned int count, capacity, legacybytes, bytes, alignment;
	char *reason;
};

/* Zero-initialize output; release it after either success or refusal. No AST mutation. */
int dis64layouttype(Type*, Dis64Layout*);
int dis64layoutstorage(Decl*, Decl*, Desc*, int, Dis64Layout*);
/* Address role: 0 scalar, 1 managed reference, -1 borrowed native address. */
int dis64layoutaddress(Dis64Layout*, unsigned int, unsigned int, int, unsigned int*);
void dis64layoutfree(Dis64Layout*);
void dis64layoutreport(Decl*, Decl**, int);
#endif
