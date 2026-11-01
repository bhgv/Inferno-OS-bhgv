#ifndef INFERNO_FDLIBM_NAMES_H
#define INFERNO_FDLIBM_NAMES_H // Keep private numerical cores separate from host libm strong aliases.

#define __ieee754_acos inferno_fdlibm_ieee754_acos // Private IEEE core.
#define __ieee754_acosh inferno_fdlibm_ieee754_acosh // Private IEEE core.
#define __ieee754_asin inferno_fdlibm_ieee754_asin // Private IEEE core.
#define __ieee754_atan2 inferno_fdlibm_ieee754_atan2 // Private IEEE core.
#define __ieee754_atanh inferno_fdlibm_ieee754_atanh // Private IEEE core.
#define __ieee754_cosh inferno_fdlibm_ieee754_cosh // Private IEEE core.
#define __ieee754_exp inferno_fdlibm_ieee754_exp // Private IEEE core.
#define __ieee754_fmod inferno_fdlibm_ieee754_fmod // Private IEEE core.
#define __ieee754_gamma inferno_fdlibm_ieee754_gamma // Private IEEE core.
#define __ieee754_gamma_r inferno_fdlibm_ieee754_gamma_r // Private IEEE core.
#define __ieee754_hypot inferno_fdlibm_ieee754_hypot // Private IEEE core.
#define __ieee754_j0 inferno_fdlibm_ieee754_j0 // Private IEEE core.
#define __ieee754_j1 inferno_fdlibm_ieee754_j1 // Private IEEE core.
#define __ieee754_jn inferno_fdlibm_ieee754_jn // Private IEEE core.
#define __ieee754_lgamma inferno_fdlibm_ieee754_lgamma // Private IEEE core.
#define __ieee754_lgamma_r inferno_fdlibm_ieee754_lgamma_r // Private IEEE core.
#define __ieee754_log inferno_fdlibm_ieee754_log // Private IEEE core.
#define __ieee754_log10 inferno_fdlibm_ieee754_log10 // Private IEEE core.
#define __ieee754_pow inferno_fdlibm_ieee754_pow // Private IEEE core.
#define __ieee754_rem_pio2 inferno_fdlibm_ieee754_rem_pio2 // Private IEEE core.
#define __ieee754_remainder inferno_fdlibm_ieee754_remainder // Private IEEE core.
#define __ieee754_scalb inferno_fdlibm_ieee754_scalb // Private IEEE core.
#define __ieee754_sinh inferno_fdlibm_ieee754_sinh // Private IEEE core.
#define __ieee754_sqrt inferno_fdlibm_ieee754_sqrt // Private IEEE core.
#define __ieee754_y0 inferno_fdlibm_ieee754_y0 // Private IEEE core.
#define __ieee754_y1 inferno_fdlibm_ieee754_y1 // Private IEEE core.
#define __ieee754_yn inferno_fdlibm_ieee754_yn // Private IEEE core.
#define __kernel_cos inferno_fdlibm_kernel_cos // Private fdlibm kernel.
#define __kernel_rem_pio2 inferno_fdlibm_kernel_rem_pio2 // Private fdlibm kernel.
#define __kernel_sin inferno_fdlibm_kernel_sin // Private fdlibm kernel.
#define __kernel_standard inferno_fdlibm_kernel_standard // Private fdlibm kernel.
#define __kernel_tan inferno_fdlibm_kernel_tan // Private fdlibm kernel.

#endif
