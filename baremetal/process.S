#include "process.h"
/* No C/firmware calls under the private CR3. */
.text
.code64
.globl bm_process_enter
.type bm_process_enter,@function
bm_process_enter:
    pushq %rbx
    pushq %rbp
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    movq %rsp,bm_process_saved_rsp(%rip)
    fxsave64 bm_process_fx(%rip)
    fxrstor64 bm_process_initial_fx(%rip)
    leaq bm_process_trap_stack+4096(%rip),%rsp
    pushq $(61*8+3)
    movabsq $(BM_PROCESS_STACK+8192-16),%rax
    pushq %rax
    pushq $0x102                  /* TF=1, IF=0, IOPL=0 */
    pushq $(60*8+3)
    movabsq $BM_PROCESS_CODE,%rax
    pushq %rax
    movq bm_process_user_cr3(%rip),%rax
    movq %rax,%cr3
    xorq %rax,%rax
    xorq %rbx,%rbx
    xorq %rcx,%rcx
    xorq %rdx,%rdx
    xorq %rsi,%rsi
    xorq %rdi,%rdi
    xorq %rbp,%rbp
    xorq %r8,%r8
    xorq %r9,%r9
    xorq %r10,%r10
    xorq %r11,%r11
    xorq %r12,%r12
    xorq %r13,%r13
    xorq %r14,%r14
    xorq %r15,%r15
    iretq
.Lresume:
    fxrstor64 bm_process_fx(%rip)
    movq bm_process_saved_rsp(%rip),%rsp
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbp
    popq %rbx
    ret
.size bm_process_enter,.-bm_process_enter

/* Uniform frame: vector, error, RIP, CS, flags, RSP, SS.
 * Entry gates clear live TF; DF/AC/NT may be set by the program. */
bm_process_trap_common:
    cld
    cmpq $1,(%rsp)
    jne .Lterminal
    testb $3,24(%rsp)
    jz .Lterminal                 /* A kernel #DB is not a user step. */
    incq bm_process_steps(%rip)
    cmpq $BM_PROCESS_STEP_LIMIT,bm_process_steps(%rip)
    jae .Ltimeout
    /* IRET checks live NT, even though saved user flags are separate. */
    pushq $2
    popfq
    addq $16,%rsp
    iretq
.Ltimeout:
    movq $BM_PROCESS_TIMEOUT,(%rsp)
.Lterminal:
    movq %rax,bm_process_user_rax(%rip)
    movq (%rsp),%rax
    movq %rax,bm_process_reason(%rip)
    movq 8(%rsp),%rax
    movq %rax,bm_process_error(%rip)
    movq 16(%rsp),%rax
    movq %rax,bm_process_rip(%rip)
    movq 24(%rsp),%rax
    movq %rax,bm_process_cs(%rip)
    movq %cr2,%rax
    movq %rax,bm_process_address(%rip)
    /* 64-bit IRET pops RSP/SS even without a CPL change. Drop BOTH vector
     * and error before IRET. Use a trusted frame, not the user's flags. */
    addq $16,%rsp
    leaq .Lresume(%rip),%rax
    movq %rax,0(%rsp)
    movzwq bm_process_kernel_cs(%rip),%rax
    movq %rax,8(%rsp)
    movq $2,16(%rsp)              /* Never carry user DF/AC/NT/TF to C. */
    leaq bm_process_trap_stack+4096(%rip),%rax
    movq %rax,24(%rsp)
    movzwq bm_process_kernel_ss(%rip),%rax
    movq %rax,32(%rsp)
    pushq $2
    popfq
    movq bm_process_kernel_cr3(%rip),%rax
    movq %rax,%cr3
    iretq

.macro STUB n, error=0
.globl bm_process_exception\n
bm_process_exception\n:
.if !\error
    pushq $0
.endif
    pushq $\n
    jmp bm_process_trap_common
.endm
STUB 0
STUB 1
STUB 2
STUB 3
STUB 4
STUB 5
STUB 6
STUB 7
STUB 8,1
STUB 9
STUB 10,1
STUB 11,1
STUB 12,1
STUB 13,1
STUB 14,1
STUB 15
STUB 16
STUB 17,1
STUB 18
STUB 19
STUB 20
STUB 21,1
STUB 22
STUB 23
STUB 24
STUB 25
STUB 26
STUB 27
STUB 28
STUB 29,1
STUB 30,1
STUB 31
.globl bm_process_syscall
bm_process_syscall:
    pushq $0
    pushq $128
    jmp bm_process_trap_common
.globl bm_process_unexpected
bm_process_unexpected:
    pushq $0
    pushq $255
    jmp bm_process_trap_common

.section .data
.balign 8
.globl bm_process_exception_table
bm_process_exception_table:
.macro TABLE_ENTRY n
    .quad bm_process_exception\n
.endm
.irp n,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
    TABLE_ENTRY \n
.endr
.balign 16
bm_process_fx:
    .space 512
bm_process_initial_fx:
    .short 0x037f,0
    .space 20
    .long 0x1f80,0
    .space 480
.balign 4096
.globl bm_process_trap_stack
bm_process_trap_stack:
    .space 8192
.globl bm_process_emergency_stack
bm_process_emergency_stack:
    .space 4096
.section .note.GNU-stack,"",@progbits
