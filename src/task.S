#include "offsets.inc"

.section .text

.global save_context
.type save_context, @function
save_context:
    pushfq # save rflags
    push %rax # save rax
    push %rdx # save rdx

    mov %gs:CPU_CURRENT_TASK, %rax   # rax = Task*

    pop %rdx # get rdx
    mov %rdx, TASK_CONTEXT+CTX_RDX(%rax)

    pop %rdx # get rax
    mov %rdx, TASK_CONTEXT+CTX_RAX(%rax)

    pop %rdx # get rflags
    mov %rdx, TASK_CONTEXT+CTX_RFLAGS(%rax)

    # save remaining
    mov %r15, TASK_CONTEXT+CTX_R15(%rax)
    mov %r14, TASK_CONTEXT+CTX_R14(%rax)
    mov %r13, TASK_CONTEXT+CTX_R13(%rax)
    mov %r12, TASK_CONTEXT+CTX_R12(%rax)
    mov %r11, TASK_CONTEXT+CTX_R11(%rax)
    mov %r10, TASK_CONTEXT+CTX_R10(%rax)
    mov %r9,  TASK_CONTEXT+CTX_R9(%rax)
    mov %r8,  TASK_CONTEXT+CTX_R8(%rax)
    mov %rsi, TASK_CONTEXT+CTX_RSI(%rax)
    mov %rdi, TASK_CONTEXT+CTX_RDI(%rax)
    mov %rcx, TASK_CONTEXT+CTX_RCX(%rax)
    mov %rbx, TASK_CONTEXT+CTX_RBX(%rax)
    mov %rbp, TASK_CONTEXT+CTX_RBP(%rax)
    mov %rsp, TASK_CONTEXT+CTX_RSP(%rax)

    ret

.global restore_context
.type restore_context, @function
restore_context:
    mov TASK_CONTEXT+CTX_R15(%rdi), %r15
    mov TASK_CONTEXT+CTX_R14(%rdi), %r14
    mov TASK_CONTEXT+CTX_R13(%rdi), %r13
    mov TASK_CONTEXT+CTX_R12(%rdi), %r12
    mov TASK_CONTEXT+CTX_R11(%rdi), %r11
    mov TASK_CONTEXT+CTX_R10(%rdi), %r10
    mov TASK_CONTEXT+CTX_R9(%rdi), %r9
    mov TASK_CONTEXT+CTX_R8(%rdi), %r8
    mov TASK_CONTEXT+CTX_RSI(%rdi), %rsi
    mov TASK_CONTEXT+CTX_RDX(%rdi), %rdx
    mov TASK_CONTEXT+CTX_RCX(%rdi), %rcx
    mov TASK_CONTEXT+CTX_RBX(%rdi), %rbx
    mov TASK_CONTEXT+CTX_RAX(%rdi), %rax
    mov TASK_CONTEXT+CTX_RBP(%rdi), %rbp

    # rflags
    push TASK_CONTEXT+CTX_RFLAGS(%rdi)
    popfq

    mov TASK_CONTEXT+CTX_RSP(%rdi), %rsp

    mov TASK_CONTEXT+CTX_RDI(%rdi), %rdi

    ret

.global task_trampoline
task_trampoline:
    pop %rax # save entry
    pop %rdi # save task arg
    pop %rdx # get task_exit
    push %rdx # push return to task_exit
    jmp *%rax # jump to entry

.global task_run
task_run:
    mov %rdi, %rsp
    ret
