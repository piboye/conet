#define oR8 40
#define oR9 48
#define oR12 72
#define oR13 80
#define oR14 88
#define oR15 96
#define oRDI 104
#define oRSI 112
#define oRBP 120
#define oRBX 128
#define oRDX 136
#define oRCX 152
#define oRSP 160
#define oRIP 168

.globl co_swapcontext
.type  co_swapcontext, @function
co_swapcontext:
	movq	%rbx, oRBX(%rdi)
	movq	%rbp, oRBP(%rdi)
	movq	%r12, oR12(%rdi)
	movq	%r13, oR13(%rdi)
	movq	%r14, oR14(%rdi)
	movq	%r15, oR15(%rdi)

	movq	%rdi, oRDI(%rdi)
	movq	%rsi, oRSI(%rdi)
	movq	%rdx, oRDX(%rdi)
	movq	%rcx, oRCX(%rdi)
	movq	%r8, oR8(%rdi)
	movq	%r9, oR9(%rdi)

	movq	(%rsp), %rcx
	movq	%rcx, oRIP(%rdi)
	leaq	8(%rsp), %rcx		
	movq	%rcx, oRSP(%rdi)

	movq	oRSP(%rsi), %rsp
	movq	oRBX(%rsi), %rbx
	movq	oRBP(%rsi), %rbp
	movq	oR12(%rsi), %r12
	movq	oR13(%rsi), %r13
	movq	oR14(%rsi), %r14
	movq	oR15(%rsi), %r15
	
	movq	oRIP(%rsi), %rcx
	pushq	%rcx
	
	movq	oRDI(%rsi), %rdi
	movq	oRDX(%rsi), %rdx
	movq	oRCX(%rsi), %rcx
	movq	oR8(%rsi), %r8
	movq	oR9(%rsi), %r9

	movq	oRSI(%rsi), %rsi
	
	xorl	%eax, %eax
	ret

.globl co_setcontext
.type  co_setcontext, @function
co_setcontext:
	movq	oRSP(%rdi), %rsp
	movq	oRBX(%rdi), %rbx
	movq	oRBP(%rdi), %rbp
	movq	oR12(%rdi), %r12
	movq	oR13(%rdi), %r13
	movq	oR14(%rdi), %r14
	movq	oR15(%rdi), %r15
	
	movq	oRIP(%rdi), %rcx
	pushq	%rcx
	
	movq	oRDI(%rdi), %rdi
	movq	oRDX(%rdi), %rdx
	movq	oRCX(%rdi), %rcx
	movq	oR8(%rdi), %r8
	movq	oR9(%rdi), %r9

	movq	oRSI(%rdi), %rsi
	
	xorl	%eax, %eax
	ret
