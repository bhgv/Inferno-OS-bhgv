#include	"dat.h"
#include	"fns.h"
//#include <lib9.h>

#include <stdio.h>
#include <time.h>
#include <string.h>

#include <unwind.h>
#include <dlfcn.h>



#ifdef ANDROID
#include <android/log.h>

#define  LOG_TAG    "inferno CT"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGW(...)  __android_log_print(ANDROID_LOG_WARN,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)
#else
#define  LOGI(...)  printf(__VA_ARGS__)
#define  LOGW(...)  
#define  LOGE(...)  
#endif

//static FILE *fp_trace;
 

typedef struct BacktraceState
{
    void** current; // Next output slot in the bounded borrowed frame array.
    void** end; // Exclusive end of caller-provided storage.
} BacktraceState;


/* Only the ARM32 EHABI helper consumes this private 32-bit register layout. */
#if defined(__arm__)
typedef struct
{
    uintptr_t r[16]; // ARM32 r0-r15 for the legacy EHABI seed only.
} core_regs;


typedef struct
{
    uintptr_t demand_save_flags; // Private ARM32 unwinder state.
    core_regs   core; // ARM32 context, never instantiated for a native64 target.
} phase2_vrs;
#endif



extern char calltracelog_path[];
extern void *sig_ctx;

#if defined(__arm__)
extern _Unwind_Reason_Code __gnu_Unwind_Backtrace(_Unwind_Trace_Fn trace, void *trace_argument, phase2_vrs *entry_vrs);
#endif


static _Unwind_Reason_Code unwindCallback(struct _Unwind_Context* context, void* arg)
{
    BacktraceState* state = (BacktraceState*)(arg);
    uintptr_t pc = _Unwind_GetIP(context);
    if (pc) {
        if (state->current == state->end) {
            return _URC_END_OF_STACK;
        } else {
            *state->current++ = (void*)(pc);
        }
    }
    return _URC_NO_REASON;
}


char** dumpBacktrace(void** buffer, size_t count)
{
	size_t idx;
	int len;
	char **sym_buf = (char**)malloc( count * sizeof(char**) );
	
	for (idx = 0; idx < count; ++idx) {
        const void* addr = buffer[idx];
        const char* symbol = "";
		char *sym_rslt;

        Dl_info info;
        if (dladdr(addr, &info) && info.dli_sname) {
            symbol = info.dli_sname;
        }
		len = strlen(symbol) + 1 + 25;
		sym_rslt = malloc(len);
		
		snprint(sym_rslt, len-1, "# %d: %#p %s", idx-2, addr, symbol);
		sym_buf[idx] = sym_rslt;
    }
	return sym_buf;
}


/* Native64 uses the public current-stack unwinder, never an ARM32 signal-frame cast. */
size_t
captureBacktrace(void **buffer, size_t max, void *arg)
{

    BacktraceState state;

	if(buffer == nil || max == 0 || max > (~(uintptr)0-(uintptr)buffer)/sizeof(*buffer))
		return 0;
	state.current = buffer;
	state.end = buffer+max;
#if defined(__arm__)
	if(arg != nil){
	    ucontext_t* signal_context = (ucontext_t*) arg;

	    phase2_vrs pre_signal_state = {};
	//	pre_signal_state.demand_save_flags = 0;
	    pre_signal_state.core = *(const core_regs*)(&(signal_context->uc_mcontext.arm_r0));

	    __gnu_Unwind_Backtrace(unwindCallback, &state, &pre_signal_state);
	}else
#else
	(void)arg;
#endif
	{
		_Unwind_Backtrace(unwindCallback, &state);
	}

    return state.current - buffer;
}


void do_backtrace(char *err_str)
{
	size_t i, max = 30;
	void* buffer[max];
	char **sym_buf;

	FILE *f = fopen(calltracelog_path, "a");

	max = captureBacktrace(buffer, max, sig_ctx );
	
	sym_buf = dumpBacktrace(buffer, max);

	sig_ctx = nil;
	
	if(f != nil ){
		fprintf(f, "\n%s\n-----\n", err_str ? err_str : "ERROR: ??");

		for(i=2; i<max; i++){
			if(sym_buf[i])
				fprintf(f, "%s\n", sym_buf[i]);
		}
		fflush(f);
		fclose(f);
	}

	for(i=0; i<max; i++){
		if(sym_buf[i]){
//			LOGE("%s", sym_buf[i]);
			free(sym_buf[i]);
		}
	}
	free(sym_buf);
}



#if 0
#include <execinfo.h>

void **backtrace_buf[32];


void do_backtrace(){
	int i, cnt = backtrace(backtrace_buf, 30);
	char **symbs;
	int f = open(calltracelog_path, O_WRONLY);
	if(f){
		backtrace_symbols_fd(backtrace_buf, cnt, f);
		close(f);
	}

	symbs = backtrace_symbols(backtrace_buf, cnt);
	for(i=0; i<cnt; i++){
		LOGE("%s", symbs[i]);
	}
	free(symbs);
}
#endif


/*
void
__attribute__ ((constructor))
trace_begin (void)
{
//LOGI("trace_begin");
// Trace output uses the configured app storage root.
}
*/

/* 
void
__attribute__ ((destructor))
trace_end (void)
{
LOGI("trace_end");
// if(fp_trace != NULL) {
// fclose(fp_trace);
// }
}
*/
 
void
__cyg_profile_func_enter (void *func,  void *caller)
{
//LOGI("trace_foo_ent %p, %p", func, caller);
// if(fp_trace != NULL) {
// fprintf(fp_trace, "e %p %p %lu\n", func, caller, time(NULL) );
// }
}
 
void
__cyg_profile_func_exit (void *func, void *caller)
{
//LOGI("trace_foo_exit %p, %p", func, caller);
// if(fp_trace != NULL) {
// fprintf(fp_trace, "x %p %p %lu\n", func, caller, time(NULL));
// }
}


