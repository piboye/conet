.text
.globl conet_bind_this_jump_help
.type conet_bind_this_jump_help, @function
.align 16
conet_bind_this_jump_help:
    leaq -0x27(%rip), %rbx # this instruction has 7 bytes 
    movq (%rbx), %r12  #function address
    movq 0x8(%rbx), %r13 # this point
    movq 0x10(%rbx), %r14 # method function address
    jmp *%r12
