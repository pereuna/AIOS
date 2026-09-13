/* Load all FAT32 parts and execute the real model under firmware. */
#include "../neural.c"
#include "../.build/config.h"

static void debug(const char *s) {
    while (*s) __asm__ volatile("outb %0,$0xe9" :: "a"(*s++));
}
static void debug_number(unsigned n) {
    char digits[16]; unsigned used=0;
    do { digits[used++]=(char)('0'+n%10); n/=10; } while (n);
    while (used) __asm__ volatile("outb %0,$0xe9" :: "a"(digits[--used]));
}
_Noreturn void bm_main(void) {
    bm_init(); bm_fp_prepare();
    debug("QWEN MODEL: loading model.000\n");
    bm_reserve_heap(sizeof(Model)+sizeof(State)+(size_t)64*(2*L*KD+NH+HS)*4+16*1024*1024);
    const void *data=bm_load_model(MODEL_BYTES);
    if (bm_crc32(data,MODEL_BYTES)!=MODEL_CRC32) bm_panic("model CRC32 mismatch");
    debug("QWEN MODEL: CRC32 passed\n");
    Model *m=alloc(sizeof(*m)); init_model(m,data,MODEL_BYTES);
    State *s=new_state(m,64);
    const int tokens[]={151644,872,198,3838};
    bm_parallel_begin();
    for (unsigned i=0;i<4;i++) {
        forward(m,s,tokens[i],1);
        debug("QWEN GREEDY "); debug_number((unsigned)greedy(s->logits)); debug("\n");
    }
    bm_parallel_end();
    free_state(s);
    debug("PROCESS PASS: Qwen2.5-Coder FAT32 loading, CRC32 and inference\n");
    __asm__ volatile("outl %0,$0xf4" :: "a"(0x10));
    for (;;) __asm__ volatile("hlt");
}
