.text
.globl conet_bind_this_jump_help
.type conet_bind_this_jump_help, @function
.align 16
conet_bind_this_jump_help:
    leaq -0x27(%rip), %rax# this instruction has 7 bytes 
    movq 0x8(%rax), %r10 # this point
    movq 0x10(%rax), %r11 # method function address
    movq (%rax), %rax #function address
    jmp *%rax
