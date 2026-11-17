/* Copyright (c) 2026 bhgv
 * EN: Allocation-free reachable-code and definite-initialization solver for the Sys clock pilot.
 * RU: Allocation-free reachable-code и definite-initialization solver для Sys clock pilot.
 */
#ifndef DIS64_PILOT_FLOW_H
#define DIS64_PILOT_FLOW_H
#include "operands.h"


/* EN: Validate count borrowed instructions, returning1 accepted/0 refused without execution.
 * Entry has no initialized Word cells. First validate call-boundary branch targets, then
 * require every instruction reachable and solve bounded descending predecessor intersections.
 * Stack storage is bounded by Dis64ClockInstructions; no allocation or retained code pointers.
 * RU: Проверить count borrowed instructions;1 accepted/0 refused, без execution.
 * Entry не имеет initialized Word cells. Проверить call boundaries, reachability и bounded
 * descending intersection. Stack ограничен Dis64ClockInstructions, code pointers не удерживаются. */
static int
dis64clockflow(const Dis64Instruction *code, unsigned int count)
{
    unsigned int incoming[Dis64ClockInstructions]; /* Definite facts before each instruction. */
    unsigned int outgoing[Dis64ClockInstructions]; /* Definite facts after each instruction. */
    unsigned int queue[Dis64ClockInstructions]; /* Reachability/worklist storage, reused between passes. */
    unsigned char boundary[Dis64ClockInstructions]; /* True at admitted call boundaries. */
    unsigned char reached[Dis64ClockInstructions] = {0}; /* Reachability seen set. */
    unsigned char queued[Dis64ClockInstructions] = {0}; /* Worklist membership set. */
    unsigned int i; /* Instruction scan index. */
    unsigned int p; /* Predecessor scan index. */
    unsigned int phase = 0; /* Call preparation state0/1/2. */
    unsigned int calls = 0; /* Bounded admitted IMCALL count. */
    unsigned int state; /* Current predecessor intersection plus reachability. */
    unsigned int need; /* Facts required by the current instruction. */
    unsigned int define; /* Facts established by the current instruction. */
    unsigned int target; /* Branch successor or count sentinel. */
    unsigned int fall; /* Fallthrough successor or count sentinel. */
    unsigned int at; /* Current worklist instruction. */
    unsigned int head; /* Worklist removal position. */
    unsigned int tail; /* Worklist insertion position. */
    unsigned int pending; /* Number of queued worklist items. */
    unsigned int budget; /* Finite convergence-work allowance. */
    const Dis64Instruction *in; /* Borrowed current instruction, never executed. */

    if(code == 0 || count == 0 || count > Dis64ClockInstructions)
    {
        return 0;
    }
    for(i = 0; i < count; i++)
    {
        in = code+i;
        boundary[i] = phase == 0;
        if(in->op == IMFRAME)
        {
            if(phase)
            {
                return 0;
            }
            phase = 1;
        }
        else if(in->op == ILEA)
        {
            if(phase != 1)
            {
                return 0;
            }
            phase = 2;
        }
        else if(in->op == IMCALL)
        {
            if(phase != 2 || ++calls > Dis64ClockCalls)
            {
                return 0;
            }
            phase = 0;
        }
        else if(phase && !(phase == 1 && in->op == IMOVW))
        {
            return 0;
        }
        if(!dis64clockeffect(in, &need, &define))
        {
            return 0;
        }
        incoming[i] = outgoing[i] = 255; /* Reachable plus seven top facts, monotonically removed. */
    }
    if(phase || calls == 0 || code[count-1].op != IEXIT)
    {
        return 0;
    }
    for(i = 0; i < count; i++)
    {
        if(dis64clockbranch(code[i].op))
        {
            if(!boundary[i] || code[i].destination <= 0 || code[i].destination >= count ||
                !boundary[code[i].destination])
            {
                return 0;
            }
        }
    }
    head = 0;
    tail = 1;
    queue[0] = 0;
    reached[0] = 1;
    while(head < tail)
    {
        at = queue[head++];
        in = code+at;
        target = dis64clockbranch(in->op) ? (unsigned int)in->destination : count;
        fall = in->op == IJMP || in->op == IEXIT ? count : at+1;
        if(target < count && !reached[target])
        {
            reached[target] = 1;
            queue[tail++] = target;
        }
        if(fall < count && !reached[fall])
        {
            reached[fall] = 1;
            queue[tail++] = fall;
        }
    }
    if(tail != count)
    {
        return 0;
    }
    head = 0;
    tail = 1%count;
    pending = 1;
    queue[0] = 0;
    queued[0] = 1;
    budget = 28*count+1; /* Seven disappearing bits, at most two successor edges per instruction. */
    while(pending)
    {
        if(budget == 0)
        {
            return 0;
        }
        budget--;
        at = queue[head];
        head = (head+1)%count;
        pending--;
        queued[at] = 0;
        state = at == 0 ? 16 : 0;
        for(p = 0; p < count; p++)
        {
            in = code+p;
            target = dis64clockbranch(in->op) ? (unsigned int)in->destination : count;
            fall = in->op == IJMP || in->op == IEXIT ? count : p+1;
            if(target == at || fall == at)
            {
                dis64clockjoin(&state, outgoing[p]);
            }
        }
        if(!state || !dis64clockeffect(code+at, &need, &define))
        {
            return 0;
        }
        incoming[at] = state;
        state |= define;
        if(state == outgoing[at])
        {
            continue;
        }
        outgoing[at] = state;
        in = code+at;
        target = dis64clockbranch(in->op) ? (unsigned int)in->destination : count;
        fall = in->op == IJMP || in->op == IEXIT ? count : at+1;
        if(target < count && !queued[target])
        {
            if(pending == count)
            {
                return 0;
            }
            queue[tail] = target;
            tail = (tail+1)%count;
            pending++;
            queued[target] = 1;
        }
        if(fall < count && !queued[fall])
        {
            if(pending == count)
            {
                return 0;
            }
            queue[tail] = fall;
            tail = (tail+1)%count;
            pending++;
            queued[fall] = 1;
        }
    }
    for(i = 0; i < count; i++)
    {
        if(!dis64clockeffect(code+i, &need, &define) || (need & ~incoming[i]))
        {
            return 0;
        }
    }
    return 1;
}
#endif
