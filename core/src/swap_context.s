# this copy from libc

.att_syntax
.equ R8, 40
.equ R9, 48
.equ R12, 72
.equ R13, 80
.equ R14, 88
.equ R15, 96
.equ RDI, 104
.equ RSI, 112
.equ RBP, 120
.equ RBX, 128
.equ RDX, 136
.equ RCX, 152
.equ RSP, 160
.equ RIP, 168

.globl conet_swapcontext
.type  conet_swapcontext, @function
conet_swapcontext:
	movq	%rbx, RBX(%rdi)
	movq	%rbp, RBP(%rdi)
	movq	%r12, R12(%rdi)
	movq	%r13, R13(%rdi)
	movq	%r14, R14(%rdi)
	movq	%r15, R15(%rdi)

	movq	%rdi, RDI(%rdi)
	movq	%rsi, RSI(%rdi)
	movq	%rdx, RDX(%rdi)
	movq	%rcx, RCX(%rdi)
	movq	%r8, R8(%rdi)
	movq	%r9, R9(%rdi)

	movq	(%rsp), %rcx
	movq	%rcx, RIP(%rdi)
	leaq	8(%rsp), %rcx		
	movq	%rcx, RSP(%rdi)

	movq	RSP(%rsi), %rsp
	movq	RBX(%rsi), %rbx
	movq	RBP(%rsi), %rbp
	movq	R12(%rsi), %r12
	movq	R13(%rsi), %r13
	movq	R14(%rsi), %r14
	movq	R15(%rsi), %r15
	
	movq	RIP(%rsi), %rcx
	pushq	%rcx
	
	movq	RDI(%rsi), %rdi
	movq	RDX(%rsi), %rdx
	movq	RCX(%rsi), %rcx
	movq	R8(%rsi), %r8
	movq	R9(%rsi), %r9

	movq	RSI(%rsi), %rsi
	
	xorl	%eax, %eax
	ret

.globl conet_setcontext
.type  conet_setcontext, @function
conet_setcontext:
	movq	RSP(%rdi), %rsp
	movq	RBX(%rdi), %rbx
	movq	RBP(%rdi), %rbp
	movq	R12(%rdi), %r12
	movq	R13(%rdi), %r13
	movq	R14(%rdi), %r14
	movq	R15(%rdi), %r15
	
	movq	RIP(%rdi), %rcx
	pushq	%rcx
	
	movq	RDI(%rdi), %rdi
	movq	RDX(%rdi), %rdx
	movq	RCX(%rdi), %rcx
	movq	R8(%rdi), %r8
	movq	R9(%rdi), %r9

	movq	RSI(%rdi), %rsi
	
	xorl	%eax, %eax
	ret
