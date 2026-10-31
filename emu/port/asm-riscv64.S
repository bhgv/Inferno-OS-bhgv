	.text
	.p2align 2

/* The RV64M product yields native low/high unsigned-long halves. */
	.globl umult
	.type umult,@function
umult:
	mulhu t0, a0, a1
	mul a0, a0, a1
	sd t0, 0(a2)
	ret
	.size umult,.-umult


/* FCSR contains both FRM and FFLAGS, independently of ordinary C argument registers. */
	.globl FPsave
	.type FPsave,@function
FPsave:
	frcsr t0
	sw t0, 0(a0)
	ret
	.size FPsave,.-FPsave


/* Restore the exact saved rounding/status environment before resuming the VM. */
	.globl FPrestore
	.type FPrestore,@function
FPrestore:
	lw t0, 0(a0)
	fscsr t0
	ret
	.size FPrestore,.-FPrestore


/* AQ/RL ordering publishes only a 32-bit Lock.val and preserves adjacent fields. */
	.globl _tas
	.type _tas,@function
_tas:
	li t0, 1
	amoswap.w.aqrl a0, t0, (a0)
	ret
	.size _tas,.-_tas
	.section .note.GNU-stack,"",@progbits
