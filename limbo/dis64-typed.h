#ifndef LIMBO_DIS64_TYPED_H
#define LIMBO_DIS64_TYPED_H
#include "dis64-layout.h"
#include "dis64-code.h"

typedef struct Dis64TypedSite Dis64TypedSite;
struct Dis64TypedSite {
    unsigned int first,end,target,descriptor,resultoffset,contract,inherited;
};
typedef struct Dis64TypedExternal Dis64TypedExternal;
struct Dis64TypedExternal {
    unsigned int first,end,binding,modulemode,moduleoffset,resultoffset,contract,inherited;
    Decl *callee;
};
typedef struct Dis64TypedVariadic Dis64TypedVariadic;
struct Dis64TypedVariadic {
    unsigned int first,end,binding,modulemode,moduleoffset,resultoffset,contract,operation,count,inherited;
    Decl *callee;
    Dis64Layout frame;
};
typedef struct Dis64TypedBody Dis64TypedBody;
typedef struct Dis64TypedTable Dis64TypedTable;
struct Dis64TypedTable {unsigned int pc,offset;int op;Case *table;unsigned int altdefault;};
struct Dis64TypedBody {
	Dis64Instruction *code;
	Dis64TypedSite *sites;
	unsigned int nsites;
    Dis64TypedTable *tables;
    unsigned int ntables;
    Dis64TypedExternal *external;
    unsigned int nexternal;
    Dis64TypedVariadic *variadic;
    unsigned int nvariadic;
	unsigned char *returns;
    long *contextfields; /* Selected compiler-owned indirect field displacements. */
	Dis64Layout frame, data;
	unsigned int first, count, badpc;
	int resultkind, tail, resultpair, resulttriple, resultgeometry, sourceroles, resultaggregate;
    Type *resulttype;
	unsigned int target, targetdesc, callend, resultoffset, resultcontract;
	unsigned int resultbytes;
	char *reason;
};
/* Trusted, typechecked compiler IR only. Zero output and free after success/refusal. */
int dis64typedlower(Decl*, Decl*, Inst*, unsigned int, unsigned int, Dis64TypedBody*);
int dis64typedtail(Decl*, Decl*, Decl**, int, Inst*, unsigned int, unsigned int, Dis64TypedBody*);
int dis64typedlocal(Decl*, Decl*, Decl**, int, Inst*, unsigned int, unsigned int, Dis64TypedBody*);
int dis64typedmulti(Decl*, Decl*, Decl**, int, Inst*, unsigned int, unsigned int, Dis64TypedBody*);
int dis64typedexternal(Decl*, Decl*, Decl**, int, Inst*, unsigned int, unsigned int, Dis64TypedBody*);
int dis64typedloads(Decl*, Decl*, Decl**, int, Inst*, unsigned int, unsigned int, Dis64TypedBody*);
int dis64typedclient(Decl*, Decl*, Decl**, int, Inst*, unsigned int, unsigned int, Dis64TypedBody*);
int dis64typedcommand(Decl*, Decl*, Decl**, int, Inst*, unsigned int, unsigned int, Dis64TypedBody*);
void dis64typedfree(Dis64TypedBody*);
void dis64typedreport(Decl*, Decl**, int, Inst*, long);
#endif
