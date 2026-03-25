// offsets.c
#include <stddef.h>
#include "kernel.h"

#define DEFINE(name, val) \
    asm volatile(".equ " #name ", %c0" :: "i"(val))

void gen_offsets(void) {
    DEFINE(TASK_CONTEXT, offsetof(Task, context));

    DEFINE(CTX_R15, offsetof(Context, r15));
    DEFINE(CTX_R14, offsetof(Context, r14));
    DEFINE(CTX_R13, offsetof(Context, r13));
    DEFINE(CTX_R12, offsetof(Context, r12));
    DEFINE(CTX_R11, offsetof(Context, r11));
    DEFINE(CTX_R10, offsetof(Context, r10));
    DEFINE(CTX_R9,  offsetof(Context, r9));
    DEFINE(CTX_R8,  offsetof(Context, r8));
    DEFINE(CTX_RSI, offsetof(Context, rsi));
    DEFINE(CTX_RDI, offsetof(Context, rdi));
    DEFINE(CTX_RDX, offsetof(Context, rdx));
    DEFINE(CTX_RCX, offsetof(Context, rcx));
    DEFINE(CTX_RBX, offsetof(Context, rbx));
    DEFINE(CTX_RAX, offsetof(Context, rax));
    DEFINE(CTX_RBP, offsetof(Context, rbp));
    DEFINE(CTX_RSP, offsetof(Context, rsp));
    DEFINE(CTX_RFLAGS, offsetof(Context, rflags));

    DEFINE(CPU_CURRENT_TASK, offsetof(CPULocal, current_task));
    DEFINE(CPU_IDLE_TASK, offsetof(CPULocal, idle_task));
}
