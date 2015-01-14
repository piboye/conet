.text
.globl jump_to_real_func
.type jump_to_real_func, @function
.align 16
jump_to_real_func:
    #pushq  -0x27(%rip) # this instruction has 7 bytes 
    #popq   %rbx
    leaq -0x27(%rip), %rbx
    movq (%rbx), %r12  #function address
    movq 0x8(%rbx), %r13 # this point
    movq 0x10(%rbx), %r14 # method function address
    jmp *%r12
