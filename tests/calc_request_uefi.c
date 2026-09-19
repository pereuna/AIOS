/* Test transport only: the native model writes data arrays, not assembly. */
#include "../baremetal/runtime.h"
#include "../baremetal/process.h"
#include "../.build/calc-request.h"
static void debug_char(char c) { __asm__ volatile("outb %0,$0xe9" :: "a"(c)); }
static void debug(const char *s) { while (*s) debug_char(*s++); }
static void hex(uint64_t n,unsigned digits) {
    static const char alphabet[]="0123456789abcdef";
    while (digits--) debug_char(alphabet[(n>>(digits*4))&15]);
}
_Noreturn void bm_main(void) {
    bm_init(); bm_fp_prepare(); bm_process_result result;
    int status=bm_process_run_input(request_code,sizeof(request_code),request_input,request_input_count,&result);
    debug("CALC_RESULT ");hex((uint32_t)status,8);debug(" ");hex(result.rax,16);
    debug(" ");hex(result.steps,16);debug(" ");hex(result.fault_vector,8);debug("\n");
    debug("PROCESS PASS: compiled code and input transport to ring3 completed\n");
    __asm__ volatile("outl %0,$0xf4" :: "a"(0x10));
    for (;;) __asm__ volatile("hlt");
}
