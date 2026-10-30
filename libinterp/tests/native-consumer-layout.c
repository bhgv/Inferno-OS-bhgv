/* Measure actual generated native declarations against the documented Dis64 boundary. */
#include "lib9.h"
#include "isa.h"
#include "interp.h"
#include "audited-runt.h"
#include <stddef.h>
#include <stdio.h>


/* Emit actual compiler measurements, never inferred readiness from WORD alone.
 * No arguments, allocated owners, callbacks or VM execution. Return0 after one
 * complete JSON object; the Python caller evaluates compatibility separately. */
int main(void)
{
    printf("{\"word\":%zu,\"pointer\":%zu,\"frame_links\":%zu,"
           "\"qid_actual\":%zu,\"qid_declared\":%d,"
           "\"dir_actual\":%zu,\"dir_declared\":%d,"
           "\"connection_actual\":%zu,\"connection_declared\":%d,"
           "\"sleep_frame\":%zu,\"sleep_result\":%zu,\"sleep_argument\":%zu}\n",
           sizeof(WORD),
           sizeof(void *),
           sizeof(Frame),
           sizeof(Sys_Qid),
           Sys_Qid_size,
           sizeof(Sys_Dir),
           Sys_Dir_size,
           sizeof(Sys_Connection),
           Sys_Connection_size,
           sizeof(F_Sys_sleep),
           offsetof(F_Sys_sleep, ret),
           offsetof(F_Sys_sleep, period));
    return 0;
}
