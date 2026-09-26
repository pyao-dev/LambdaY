    .text

    .global interrupt_default_entry
    .type interrupt_default_entry, @function
interrupt_default_entry:
    cli
1:
    hlt
    jmp 1b
    .size interrupt_default_entry, .-interrupt_default_entry

    .global interrupt_keyboard_entry
    .type interrupt_keyboard_entry, @function
interrupt_keyboard_entry:
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rbp
    pushq %rdi
    pushq %rsi
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    movq %rsp, %r12
    andq $-16, %rsp
    cld
    call keyboard_handle_irq
    movq %r12, %rsp

    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rsi
    popq %rdi
    popq %rbp
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax
    iretq
    .size interrupt_keyboard_entry, .-interrupt_keyboard_entry

    .section .note.GNU-stack,"",@progbits
