	.text
	.globl _start

/* Linux user-mode entry lets the exact target primitives execute without an Android/glibc loader. */
_start:
#if defined(__x86_64__)
	andq $-16, %rsp
	call run
	movl %eax, %edi
	movl $60, %eax
	syscall
#elif defined(__aarch64__)
	bl run
	mov x8, #93
	svc #0
#else
	.option push
	.option norelax
	lla gp, __global_pointer$
	.option pop
	call run
	li a7, 93
	ecall
#endif

#if defined(__aarch64__)
	.section .note.GNU-stack,"",%progbits
#else
	.section .note.GNU-stack,"",@progbits
#endif
