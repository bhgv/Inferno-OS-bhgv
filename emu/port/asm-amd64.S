	.text

/* Return the full LP64 product split into low and high unsigned-long halves. */
	.globl umult
	.type umult,@function
umult:
	movq %rdx, %rcx
	movq %rdi, %rax
	mulq %rsi
	movq %rdx, (%rcx)
	ret
	.size umult,.-umult


/* Save x87 and SSE environments without leaving FNSTENV's exception masking active. */
	.globl FPsave
	.type FPsave,@function
FPsave:
	fnstenv (%rdi)
	fldcw (%rdi)
	stmxcsr 28(%rdi)
	ret
	.size FPsave,.-FPsave


/* Restore the complete control/status pair before returning to C or JIT execution. */
	.globl FPrestore
	.type FPrestore,@function
FPrestore:
	fnclex
	fldenv (%rdi)
	ldmxcsr 28(%rdi)
	ret
	.size FPrestore,.-FPrestore


/* Exchange exactly one 32-bit lock word with full x86 memory ordering. */
	.globl _tas
	.type _tas,@function
_tas:
	movl $1, %eax
	xchgl %eax, (%rdi)
	ret
	.size _tas,.-_tas
	.section .note.GNU-stack,"",@progbits
