	.text
	.p2align 2

/* Return a full LP64 unsigned product without narrowing the saved high half. */
	.globl umult
	.type umult,%function
umult:
	umulh x3, x0, x1
	mul x0, x0, x1
	str x3, [x2]
	ret
	.size umult,.-umult


/* Save both FP registers: AArch64 splits control and sticky status. */
	.globl FPsave
	.type FPsave,%function
FPsave:
	mrs x1, fpcr
	mrs x2, fpsr
	stp w1, w2, [x0]
	ret
	.size FPsave,.-FPsave


/* Restore every saved FP environment bit without applying a language-level mask. */
	.globl FPrestore
	.type FPrestore,%function
FPrestore:
	ldp w1, w2, [x0]
	msr fpcr, x1
	msr fpsr, x2
	isb
	ret
	.size FPrestore,.-FPrestore


/* Use baseline ARMv8 acquire/release exclusives; LSE instructions are not required. */
	.globl _tas
	.type _tas,%function
_tas:
	mov w2, #1
1:
	ldaxr w1, [x0]
	stlxr w3, w2, [x0]
	cbnz w3, 1b
	mov w0, w1
	ret
	.size _tas,.-_tas
	.section .note.GNU-stack,"",%progbits
