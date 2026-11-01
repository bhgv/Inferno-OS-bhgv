#ifndef INFERNO_HOST64_EMU_H
#define INFERNO_HOST64_EMU_H // Hosted LP64 execution context; never reuse the 32-bit ARM FPU layout.
#include "host64-fpu.h"

#ifndef USE_PTHREADS
#error hosted native64 emulator ports require the pthread current-process implementation
#endif
#define KSTACK (64 * 1024) // Native worker stack size; VM stacks have their own allocation policy.
extern Proc *getup(void);
#define up (getup()) // Resolve the current process through the platform's pthread TLS.
typedef sigjmp_buf osjmpbuf;
#define ossetjmp(buf) sigsetjmp(buf, 1) // Preserve signal masks across the native error boundary.
#endif
