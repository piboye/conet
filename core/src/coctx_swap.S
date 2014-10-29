#define _esp 0
#define _eip 4
/* ------ */
#define _rsp 0
#define _rip 8
#define _rbx 16
#define _rdi 24
#define _rsi 32

.globl coctx_swap 
.type  coctx_swap, @function
coctx_swap:
	leaq -8(%rsp),%rsp
	pushq %rbp
	pushq %r12
	pushq %r13
	pushq %r14
	pushq %r15
	pushq %rdx
	pushq %rcx	
	pushq %r8
	pushq %r9
	leaq 80(%rsp),%rsp	

	movq %rbx,_rbx(%rdi)
	movq %rdi,_rdi(%rdi)
	movq %rsi,_rsi(%rdi)
	/* sp */
	movq (%rsp), %rcx
	movq %rcx, _rip(%rdi)
	leaq 8(%rsp), %rcx		
	movq %rcx, _rsp(%rdi)

	/* sp */
	movq _rip(%rsi), %rcx
	movq _rsp(%rsi), %rsp
	pushq %rcx

	movq _rbx(%rsi),%rbx
	movq _rdi(%rsi),%rdi
	movq _rsi(%rsi),%rsi

	leaq -80(%rsp),%rsp
	popq %r9
	popq %r8
	popq %rcx	
	popq %rdx
	popq %r15
	popq %r14
	popq %r13
	popq %r12
	popq %rbp
	leaq 8(%rsp),%rsp	

	xorl %eax, %eax
	ret
