#ifndef LIMBO_DIS64_TYPED_H
#define LIMBO_DIS64_TYPED_H
#include "dis64-layout.h"
#include "dis64-code.h"

typedef struct Dis64TypedBody Dis64TypedBody;
struct Dis64TypedBody {
	Dis64Instruction *code;
	Dis64Layout frame, data;
	unsigned int first, count, badpc;
	char *reason;
};
/* Trusted, typechecked compiler IR only. Zero output and free after success/refusal. */
int dis64typedlower(Decl*, Decl*, Inst*, unsigned int, unsigned int, Dis64TypedBody*);
void dis64typedfree(Dis64TypedBody*);
void dis64typedreport(Decl*, Decl**, int, Inst*, long);
#endif
