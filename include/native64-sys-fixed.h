#ifndef INFERNO_NATIVE64_SYS_FIXED_H
#define INFERNO_NATIVE64_SYS_FIXED_H

/* Trusted startup-only handshake. sysinit stages five Types before invoking the
 * registry constructor; its successful return is followed by allocation-free
 * Type publication. Caller owns VM startup exclusion. No wire admission. */
int sysnative64registerfixed(void);
int sysnative64typesready(void);
#endif
