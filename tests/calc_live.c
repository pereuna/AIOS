/* Real model.bin -> LLVM SSA IR -> compiled x86 -> ring3 -> model continuation. */
#include "../neural.c"
#ifdef CALC_HOST
#include <stdio.h>
#include <stdlib.h>
#else
#include "../.build/config.h"
#endif
/* Native mode runs this SAME inference/control code; its process backend
 * transfers AIOS-compiled bytes and IR data to a small QEMU VM for actual
 * ring3 execution. */
static void live_putc(char c) {
#ifdef CALC_HOST
    putchar(c); fflush(stdout);
#else
    __asm__ volatile("outb %0,$0xe9" :: "a"(c));
#endif
}
static void live_puts(const char *s) { while (*s) live_putc(*s++); }
static void live_write(const void *text,size_t n) { const char *s=text; while (n--) live_putc(*s++); }
static void live_calc_progress(unsigned done, unsigned total) {
    if (!done) {
        live_puts("CALC MODEL: prefill ");
        char digits[16]; unsigned n=0, value=total;
        do { digits[n++]=(char)('0'+value%10); value/=10; } while (value);
        while (n) live_putc(digits[--n]);
        live_puts(" tokens\n");
    } else {
        live_puts("CALC MODEL: prefill ");
        char digits[16]; unsigned n=0, value=done;
        do { digits[n++]=(char)('0'+value%10); value/=10; } while (value);
        while (n) live_putc(digits[--n]);
        live_putc('/'); n=0; value=total;
        do { digits[n++]=(char)('0'+value%10); value/=10; } while (value);
        while (n) live_putc(digits[--n]);
        live_puts("\n");
    }
}
#define bm_putc live_putc
#define bm_puts live_puts
#define bm_write live_write
#define BM_CALC_PROGRESS live_calc_progress
#include "../baremetal/calc_model.h"
static _Noreturn void finish(unsigned n) {
#ifdef CALC_HOST
    exit(n==0x10 ? 0 : 1);
#else
    __asm__ volatile("outl %0,$0xf4" :: "a"(n));
    for (;;) __asm__ volatile("hlt");
#endif
}
static int mentions_value(const char *text,const char *value) {
    size_t n=strlen(value),length=strlen(text);
    for (size_t i=0;i+n<=length;i++) {
        if (!memcmp(text+i,value,n) && (!i || text[i-1]<'0' || text[i-1]>'9') &&
            (text[i+n]<'0' || text[i+n]>'9')) return 1;
    }
    return 0;
}
_Noreturn void bm_main(void) {
    bm_init(); bm_fp_prepare(); bm_parallel_set_limit(4);
    bm_reserve_heap(sizeof(Model)+sizeof(State)+(size_t)4096*(2*L*KD+NH+HS)*4+16*1024*1024);
    live_puts("CALC MODEL: loading model.bin\n");
    const void *data=bm_load_model(MODEL_BYTES);
#ifndef CALC_HOST
    if (bm_crc32(data,MODEL_BYTES)!=MODEL_CRC32) finish(0x11);
#endif
    Model *m=alloc(sizeof(*m)); init_model(m,data,MODEL_BYTES);
    State *s=new_state(m,4096); int *ids=alloc(MAXCTX*sizeof(int));
    /* Cache the shared system prefix once; each case still starts a fresh
     * conversation at this identical KV prefix. Useful under slow TCG. */
    int system_tokens=tokenize(m,"<|im_start|>system\n" CALC_SYSTEM_PROMPT "<|im_end|>\n",ids,MAXCTX,1);
    bm_parallel_begin();
    for (int i=0;i<system_tokens;i++) {
        forward(m,s,ids[i],0);
        if ((i+1)%64==0) live_puts("CALC MODEL: system prefill progressing\n");
    }
    bm_parallel_end();
    int prefix=s->pos;
    static const struct { const char *question; int64_t answer; int forced; const char *value; unsigned min_ops; } cases[]={
        {"/calc 12 + 30",42,1,"42",1}, {"/calc 17 - 25",-8,1,"-8",1},
        {"/calc 6 * 7",42,1,"42",1}, {"/calc -17 / 5",-3,1,"-3",1},
        {"I have 13 boxes with 9 bolts each. How many bolts are there?",117,0,"117",1},
        {"What is 1 + 100?",101,0,"101",1},
        {"Multiply the sum of 1 and 100 by 3.",303,0,"303",2},
        {"/calc (12 + 3) * (17 - 7)",150,1,"150",3},
        {"Say hello in one sentence.",0,0,NULL,0},
    };
    unsigned first=0,last=sizeof(cases)/sizeof(*cases);
#ifdef CALC_HOST
    const char *only=getenv("CALC_CASE");
    if (only) {
        char *end; long index=strtol(only,&end,10);
        if (!*only || *end || index<0 || (unsigned long)index>=last) finish(0x11);
        first=(unsigned)index; last=first+1;
    }
#endif
    for (unsigned i=first;i<last;i++) {
        s->pos=prefix; live_puts("QUESTION: "); live_puts(cases[i].question); live_putc('\n');
        bm_calc_expression expected,*target=NULL;
        if (cases[i].forced) {
            if (bm_calc_parse_expression(cases[i].question+6,&expected)==BM_CALC_CALL) target=&expected;
        }
        int n=turn_tokens(m,cases[i].question,0,ids,MAXCTX);
        calc_chat_result result;
        calc_respond(m,s,ids,n,512,cases[i].forced,target,&result);
        if (!result.complete || !result.final_answer ||
            (cases[i].value && (!result.success || !result.last.is_ir || result.last.ir.operations<cases[i].min_ops ||
                               result.last.process.rax!=(uint64_t)cases[i].answer ||
                               !mentions_value(result.final_text,cases[i].value))) ||
            (!cases[i].value && result.calls)) {
            live_puts("FAIL: real model calc\n"); finish(0x11);
        }
        live_puts("CALC MODEL CASE PASS\n");
    }
    if (last-first==1) live_puts("PROCESS PASS: selected real Q4 model LLVM IR case\n");
    else live_puts("PROCESS PASS: real Q4 model, LLVM SSA IR, arithmetic chains, natural-language routing, ring3, feedback\n");
    finish(0x10);
}
#ifdef CALC_HOST
int main(void) { bm_main(); }
#endif
