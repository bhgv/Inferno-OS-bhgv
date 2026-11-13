#ifndef DIS64_SYS_STRING_FLOW_H
#define DIS64_SYS_STRING_FLOW_H
#include "dis64-pilot.h"

/* Private selection indices only; these integers never enter a wire signature. */
static const char *const dis64sysfixednames[] = {"werrstr","chdir","remove","millisec","sleep","dup","unmount","bind","fildes","iounit","open","create"};
static int
dis64sysfixedkind(const char *name)
{
    unsigned int i;
    if(name == 0) return -1;
    for(i=0; i<12; i++) if(strcmp(name,dis64sysfixednames[i]) == 0) return i;
    return -1;
}
static unsigned int
dis64sysfixedargs(unsigned int kind)
{
    return kind < 3 ? 1 : kind < 6 ? kind-3 : kind == 6 ? 2 : kind == 7 ? 3 : kind == 10 ? 2 : kind == 11 ? 3 : 1;
}

/* Fixed argument classes and offsets; generated native entries remain authoritative. */
static int
dis64sysfixedstring(unsigned int kind, unsigned int argument)
{
    return kind < 3 || ((kind == 6 || kind == 7) && argument < 2) || (kind >= 10 && argument == 0);
}
static unsigned int
dis64sysfixedoffset(unsigned int kind, unsigned int argument)
{
    return kind >= 10 ? (argument ? 68+4*argument : 64) : 64+(kind == 6 || kind == 7 ? 8 : 4)*argument;
}
static unsigned int
dis64sysfixedframe(unsigned int kind)
{
    return kind >= 10 ? 80 : kind == 6 || kind == 7 ? (kind == 6 ? 80 : 88) : dis64sysfixedargs(kind) ? 72 : 64;
}

static int
dis64sysfixedfdresult(unsigned int kind)
{
    return kind == 8 || kind >= 10;
}

/* Private structural ref-ADT tree, never a public Dis64 tag assignment. */
enum { Dis64SysRef=0x73, Dis64SysAdt=0x74 };
static unsigned int
dis64sysfixedsignature(unsigned int kind, unsigned char *out)
{
    unsigned int words[10], n=0, i, argument;
    words[n++]=0xface0010U; words[n++]=dis64sysfixedargs(kind);
    for(i=0; i<=dis64sysfixedargs(kind); i++){
        argument = i ? i-1 : 0;
        if((i == 0 && dis64sysfixedfdresult(kind)) || (i != 0 && kind == 9)){
            words[n++]=Dis64SysRef; words[n++]=Dis64SysAdt; words[n++]=1; words[n++]=Dis64Int;
        }else words[n++]=i && dis64sysfixedstring(kind,argument) ? 0x71U : Dis64Int;
    }
    for(i=0; i<n; i++) dis64put32(out+4*i,words[i]);
    return 4*n;
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
