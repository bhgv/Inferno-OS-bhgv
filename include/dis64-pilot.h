/* Copyright (c) 2026 bhgv
 * EN: Allocation-free scalar Dis64 module admission; clock operands/flow live under dis64/pilot/.
 * RU: Allocation-free проверка scalar Dis64 module; clock operands/flow сгруппированы в dis64/pilot/.
 */
#ifndef DIS64_PILOT_H
#define DIS64_PILOT_H
#include "dis64-module.h"
#include "dis64-types.h"
#include "dis64-data.h"
#include "dis64-code.h"
#include "dis-string-pool.h"
#include "dis64/pilot/flow.h"
#define Dis64PilotInitializers 1024U /* Maximum initializer records for pairwise validation. */


/* EN: Accepted scalar module views borrow the caller's unchanged container bytes; no runtime Module.
 * RU: Принятые scalar views заимствуют неизменяемые container bytes; это не runtime Module. */
typedef struct Dis64Pilot Dis64Pilot;
struct Dis64Pilot
{
    Dis64View envelope; /* Borrowed accepted container. */
    Dis64ModuleInfo module; /* Linked module ordinals, limits and entry metadata. */
    Dis64TypeTable types; /* Borrowed structural descriptor table. */
    DisStringPoolView strings; /* Borrowed checked string pool, including duplicate contents. */
    Dis64CodeView code; /* Borrowed checked initial scalar instruction subset. */
    Dis64Type data; /* Scalar module-data descriptor. */
    Dis64Initializers initializers; /* Borrowed checked scalar initialization payload, if present. */
    Dis64Type frame; /* Entry frame with no managed references in this profile. */
};


/* EN: Find kind in a borrowed accepted envelope;1 found/0 missing or invalid section.
 * result changes only on success; section pointers remain bounded by the container.
 * RU: Найти kind в borrowed envelope;1 found/0 missing/invalid. result только при успехе,
 * section pointers ограничены container, allocation и ownership transfer отсутствуют. */
static int
dis64pilotsection(const Dis64View *view, unsigned int kind, Dis64Section *result)
{
    Dis64Section section; /* Current validated section descriptor. */
    unsigned int i; /* Bounded section ordinal. */

    for(i = 0; i < view->sections; i++)
    {
        if(!dis64section(view, i, &section))
        {
            return 0;
        }
        if(section.kind == kind)
        {
            *result = section;
            return 1;
        }
    }
    return 0;
}


/* EN: Require every pointer-map byte in borrowed validated type to be zero;1 scalar/0 managed.
 * This pilot does not admit GC references or claim runtime result-slot support.
 * RU: Проверить нулевой pointer map у borrowed validated type;1 scalar/0 managed.
 * Pilot не принимает GC references и не объявляет runtime result-slot support. */
static int
dis64pilotscalar(const Dis64Type *type)
{
    unsigned int i; /* Bounded pointer-map byte index. */

    for(i = 0; i < type->mapbytes; i++)
    {
        if(type->map[i] != 0)
        {
            return 0;
        }
    }
    return 1;
}


/* EN: Validate borrowed data/bytes under objectlimit/instructionlimit;1 accepted/0 refused.
 * Publish result only after envelope, descriptors, strings, initializers and scalar code agree.
 * Result views borrow input bytes; caller keeps them unchanged/alive. No VM allocation or
 * execution occurs. Imports/exports/control sections outside this profile are refused explicitly,
 * never redirected to the legacy loader or treated as a complete runtime implementation.
 * RU: Проверить borrowed data/bytes под objectlimit/instructionlimit;1 accepted/0 refused.
 * Result только после проверки всех sections; его views заимствуют живые неизменяемые bytes.
 * Без VM allocation/execution, без перехода к legacy loader для неподдерживаемых sections. */
static int
dis64pilotcheck(const unsigned char *data, unsigned int bytes,
    unsigned int objectlimit, unsigned int instructionlimit, Dis64Pilot *result)
{
    Dis64Pilot pilot; /* Private staged result until every section passes. */
    Dis64Section section; /* Current borrowed container section. */
    DisRuneView name; /* Validated borrowed module-name entry. */
    unsigned int i; /* Section-kind scan index. */

    if(result == 0 || !dis64check(data, bytes, 0, 64, 0, 0, &pilot.envelope))
    {
        return 0;
    }
    if(!dis64pilotsection(&pilot.envelope, Dis64Module, &section) ||
        !dis64modulecheck(data+section.offset, section.bytes, &pilot.module) ||
        pilot.module.profile != 1 || pilot.module.databytes > objectlimit)
    {
        return 0;
    }
    if(!dis64pilotsection(&pilot.envelope, Dis64Types, &section) ||
        !dis64typescheck(data+section.offset, section.bytes, 65536, objectlimit, &pilot.types))
    {
        return 0;
    }
    if(!dis64pilotsection(&pilot.envelope, Dis64Strings, &section) ||
        !disstringpoolcheck(data+section.offset, section.bytes, 65536, &pilot.strings) ||
        !disstringpoolget(&pilot.strings, pilot.module.name, &name))
    {
        return 0;
    }
    if(!dis64typeget(&pilot.types, pilot.module.frametype, &pilot.frame) ||
        pilot.frame.bytes < 64 || pilot.frame.align != 8 || !dis64pilotscalar(&pilot.frame))
    {
        return 0;
    }
    pilot.data.bytes = 0;
    pilot.data.align = 1;
    pilot.data.map = 0;
    pilot.data.mapbytes = 0;
    if(pilot.module.databytes != 0 &&
        (!dis64typeget(&pilot.types, pilot.module.datatype, &pilot.data) ||
        pilot.data.bytes != pilot.module.databytes || !dis64pilotscalar(&pilot.data)))
    {
        return 0;
    }
    if(dis64pilotsection(&pilot.envelope, Dis64Data, &section))
    {
        if(!dis64initializerscheck(data+section.offset, section.bytes, &pilot.data,
            Dis64PilotInitializers, &pilot.initializers))
        {
            return 0;
        }
    }
    else
    {
        if(pilot.module.databytes != 0)
        {
            return 0;
        }
        pilot.initializers.data = 0;
        pilot.initializers.bytes = 0;
        pilot.initializers.count = 0;
    }
    for(i = Dis64Imports; i <= Dis64Control; i++)
    {
        if(i == Dis64Strings)
        {
            continue;
        }
        if(dis64pilotsection(&pilot.envelope, i, &section))
        {
            return 0;
        }
    }
    if(!dis64pilotsection(&pilot.envelope, Dis64Code, &section) ||
        !dis64codecheck(data+section.offset, section.bytes, instructionlimit,
            pilot.data.bytes, pilot.frame.bytes, &pilot.code) || pilot.module.entry >= pilot.code.count)
    {
        return 0;
    }
    *result = pilot;
    return 1;
}
#endif
