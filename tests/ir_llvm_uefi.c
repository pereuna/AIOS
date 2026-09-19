/* Differential test: AIOS-generated code in ring3 versus host LLVM results. */
#include "../baremetal/runtime.h"
#include "../baremetal/calc.h"
#include "../.build/ir-llvm-cases.h"
static void debug(const char *s) { while (*s) __asm__ volatile("outb %0,$0xe9" :: "a"(*s++)); }
static _Noreturn void finish(unsigned code) {
    __asm__ volatile("outl %0,$0xf4" :: "a"(code));
    for (;;) __asm__ volatile("hlt");
}
_Noreturn void bm_main(void) {
    bm_init(); bm_fp_prepare();
    for (unsigned i=0;i<sizeof(llvm_cases)/sizeof(*llvm_cases);i++) {
        bm_calc_result result;
        if (bm_calc_execute(llvm_cases[i].source,NULL,&result) || result.process.rax!=llvm_cases[i].answer) {
            debug("FAIL: LLVM/ring3 disagreement: "); debug(llvm_cases[i].source); debug("\n"); finish(0x11);
        }
    }
    debug("PROCESS PASS: LLVM oracle matches AIOS SSA compiler and ring3 execution\n");
    finish(0x10);
}
