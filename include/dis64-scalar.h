/* Agreed scalar vocabulary for signatures and initializers, not ISA opcode IDs. */
#ifndef DIS64_SCALAR_H
#define DIS64_SCALAR_H // Shared scalar-kind vocabulary guard.

#include "dis64.h"

/* FLASHVM payload response and INFERNO-0009/0011 accept this separate namespace. */
enum {
	Dis64Void=0,
	Dis64Int=1,
	Dis64Big=2,
	Dis64Real=3,
	Dis64Bool=4
};


/* Void is result-only and has no initializer width. Unknown kinds return zero. */
static unsigned int
dis64scalarwidth(unsigned int kind)
{
	switch(kind){
	case Dis64Int: case Dis64Bool: return 4;
	case Dis64Big: case Dis64Real: return 8;
	}
	return 0;
}
#endif
