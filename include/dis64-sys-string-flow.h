#ifndef DIS64_SYS_STRING_FLOW_H
#define DIS64_SYS_STRING_FLOW_H
#include "dis64-pilot.h"

/* Private selection indices only; these integers never enter a wire signature. */
static const char *const dis64sysfixednames[] = {"werrstr","chdir","remove","millisec","sleep","dup","unmount","bind"};
static int
dis64sysfixedkind(const char *name)
{
    unsigned int i;
    if(name == 0) return -1;
    for(i=0; i<8; i++) if(strcmp(name,dis64sysfixednames[i]) == 0) return i;
    return -1;
}
static unsigned int
dis64sysfixedargs(unsigned int kind)
{
    return kind < 3 ? 1 : kind < 6 ? kind-3 : kind == 6 ? 2 : 3;
}

/* Fixed argument classes and offsets; generated native entries remain authoritative. */
static int
dis64sysfixedstring(unsigned int kind, unsigned int argument)
{
    return kind < 3 || (kind >= 6 && argument < 2);
}
static unsigned int
dis64sysfixedoffset(unsigned int kind, unsigned int argument)
{
    return 64+(kind >= 6 ? 8 : 4)*argument;
}
static unsigned int
dis64sysfixedframe(unsigned int kind)
{
    return kind >= 6 ? (kind == 6 ? 80 : 88) : dis64sysfixedargs(kind) ? 72 : 64;
}

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
