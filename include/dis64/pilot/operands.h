/* Copyright (c) 2026 bhgv
 * EN: Operand admission and Word-definition effects for the bounded Sys clock pilot.
 * RU: Проверка operands и Word-definition effects для ограниченного Sys clock pilot.
 */
#ifndef DIS64_PILOT_OPERANDS_H
#define DIS64_PILOT_OPERANDS_H
#include "../../dis64-code.h"
#define Dis64ClockWords 64U /* Maximum scalar steps, including saved return values. */
#define Dis64ClockCalls 32U /* Maximum admitted Sys calls. */
#define Dis64ClockLocals 3U /* Persistent Word cells at frame offsets64,68,76. */
#define Dis64ClockInstructions (6U*Dis64ClockCalls+2U+Dis64ClockWords) /* Bound all worklist arrays. */


/* EN: Map a borrowed frame offset to local0..2, or -1; no memory access or ownership.
 * RU: Сопоставить offset с local0..2 либо -1; без доступа к памяти и ownership. */
static int
dis64clocklocal(long offset)
{
    return offset == 64 ? 0 : offset == 68 ? 1 : offset == 76 ? 2 : -1;
}


/* EN: Return1 for an admitted direct Word opcode,0 otherwise; no control transfer.
 * RU: Вернуть1 для допустимого Word opcode,0 иначе; без control transfer. */
static int
dis64clockwordop(unsigned int op)
{
    switch(op)
    {
    case IMOVW:
    case IADDW:
    case ISUBW:
    case IMULW:
    case IDIVW:
    case IMODW:
    case IANDW:
    case IORW:
    case IXORW:
    case ISHLW:
    case ISHRW:
        return 1;
    default:
        return 0;
    }
}


/* EN: Return1 for the pilot's admitted jump/comparison opcodes,0 otherwise; scalar input only.
 * RU: Вернуть1 для разрешённого jump/comparison opcode,0 иначе; только scalar input. */
static int
dis64clockbranch(unsigned int op)
{
    return op == IJMP || op == IBEQW || op == IBNEW || op == IBLTW ||
        op == IBLEW || op == IBGTW || op == IBGEW;
}


/* EN: Return initialization fact bit for frame offset,0 when unsupported. Bits0..2 are locals,
 * bit3 is Word result72, bits5..7 describe the checked call-projection cells96,112,120.
 * RU: Вернуть initialization bit по offset,0 при отказе. Bits0..2 locals, bit3 result72,
 * bits5..7 — проверенные call-projection cells96,112,120. Никаких runtime memory reads. */
static unsigned int
dis64clockbit(long offset)
{
    int local = dis64clocklocal(offset);
    return local >= 0 ? 1U<<local : offset == 72 ? 8U : offset == 96 ? 32U :
        offset == 112 ? 64U : offset == 120 ? 128U : 0;
}


/* EN: Join predecessor state into borrowed target; first predecessor copies its facts, later
 * predecessors intersect them. Bit4 marks reachability and mask239 excludes that marker.
 * RU: Объединить state с borrowed target: первый predecessor копируется, затем intersection.
 * Bit4 обозначает reachability, mask239 исключает marker из Word facts. */
static void
dis64clockjoin(unsigned int *target, unsigned int state)
{
    *target = *target ? 16U|((*target & state)&239U) : state;
}


/* EN: Recompute need/define masks from one borrowed checked instruction;1 accepted/0 refused.
 * Both outputs are initialized before validation and may contain a prefix on refusal.
 * No caller-supplied facts, native memory access, allocation or execution occurs.
 * RU: Вычислить need/define по borrowed instruction;1 accepted/0 refused. Outputs обнуляются,
 * при отказе могут содержать prefix. Без caller facts, allocation, memory access и execution. */
static int
dis64clockeffect(const Dis64Instruction *in, unsigned int *need, unsigned int *define)
{
    unsigned int sm = USRC(in->mode); /* Encoded source addressing mode. */
    unsigned int mm = in->mode&ARM; /* Encoded middle addressing mode. */
    unsigned int bit; /* Validated frame-cell fact bit. */

    *need = *define = 0;
    if(dis64clockwordop(in->op) || (dis64clockbranch(in->op) && in->op != IJMP))
    {
        if(sm == AFP)
        {
            bit = dis64clockbit(in->source);
            if(!bit)
            {
                return 0;
            }
            *need |= bit;
        }
        else if(sm != AIMM)
        {
            return 0;
        }
        if(in->op != IMOVW)
        {
            if(mm == AXINF || mm == AXNON)
            {
                bit = dis64clockbit(mm == AXNON ? in->destination : in->middle);
                if(!bit)
                {
                    return 0;
                }
                *need |= bit;
            }
            else if(mm != AXIMM)
            {
                return 0;
            }
        }
        if(!dis64clockbranch(in->op) && UDST(in->mode) == AFP)
        {
            *define = dis64clockbit(in->destination);
            if(!*define)
            {
                return 0;
            }
        }
    }
    else if(in->op == ISLICEA)
    {
        if(UDST(in->mode) != AFP || in->destination != 112)
        {
            return 0;
        }
        *need = 64;
        *define = 64;
        if(sm == AFP)
        {
            bit = dis64clockbit(in->source);
            if(!bit || (bit&~15U))
            {
                return 0;
            }
            *need |= bit;
        }
        else if(sm != AIMM)
        {
            return 0;
        }
        if(mm == AXINF)
        {
            bit = dis64clockbit(in->middle);
            if(!bit || (bit&~15U))
            {
                return 0;
            }
            *need |= bit;
        }
        else if(mm != AXIMM)
        {
            return 0;
        }
    }
    else if(in->op == IMCALL)
    {
        *define = in->middle == -1 ? 32 : 8;
    }
    else if(in->op != ILOAD && in->op != IMFRAME && in->op != ILEA && in->op != IEXIT && in->op != IJMP)
    {
        return 0;
    }
    return 1;
}
#endif
