#ifndef DIS64_SYS_STRING_FLOW_H
#define DIS64_SYS_STRING_FLOW_H
#include "dis64-pilot.h"

/* Virtual scalar proof cells are independent of the real mapped String at FP64.
 * Keep the returned Word last, so the trusted full-pointer binder remains generic. */
static long
dis64sysstringcell(long offset, int native)
{
    static const long virtuals[] = {64,68,76,72};
    static const long natives[] = {80,84,88,104};
    unsigned int i;
    for(i=0; i<4; i++) if(offset == (native ? virtuals[i] : natives[i]))
        return native ? natives[i] : virtuals[i];
    return -1;
}

static int
dis64sysstringword(Dis64Instruction *in, int native)
{
    long offset;
    if(!dis64clockwordop(in->op) && !dis64clockbranch(in->op)) return 0;
    if(USRC(in->mode) == AFP){
        offset = dis64sysstringcell(in->source,native); if(offset < 0) return 0; in->source = offset;
    }
    if((in->mode&ARM) == AXINF){
        offset = dis64sysstringcell(in->middle,native); if(offset < 0) return 0; in->middle = offset;
    }
    if(dis64clockwordop(in->op)){
        if(UDST(in->mode) != AFP) return 0;
        offset = dis64sysstringcell(in->destination,native); if(offset < 0) return 0; in->destination = offset;
    }
    return 1;
}
#endif
