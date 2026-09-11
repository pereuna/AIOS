/* Start each independent calculation with round-to-nearest, masked exceptions
 * and gradual underflow. No x87 state is required or modified. SysV ABI. */
.text
.global bm_fp_prepare
.type bm_fp_prepare,@function
bm_fp_prepare:
    ldmxcsr bm_default_mxcsr(%rip)
    ret
.size bm_fp_prepare,.-bm_fp_prepare
.section .rodata
.balign 4
bm_default_mxcsr: .long 0x1f80
.section .note.GNU-stack,"",@progbits
