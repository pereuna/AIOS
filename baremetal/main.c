/* The UEFI application's tokenizer, transformer and interactive console. */
#include "../neural.c"
#include "../.build/config.h"
#include "process_console.h"

static void calc_progress(unsigned done, unsigned total);
#define BM_CALC_PROGRESS calc_progress
#include "calc_model.h"

static void elapsed(uint64_t ticks) {
    bm_uint(ticks/100); bm_putc('.');
    bm_putc((char)('0'+ticks/10%10)); bm_putc((char)('0'+ticks%10));
}
static void workers_status(void) {
    bm_puts("Compute: "); bm_puts(bm_parallel_mode()); bm_puts("; workers ");
    bm_uint(bm_parallel_count()); bm_puts("; requested "); bm_uint(bm_parallel_limit());
    bm_putc('\n');
    bm_puts("Matvec: "); bm_puts(bm_simd_auto() ? "auto" : "forced SSE2");
    bm_puts("; BSP "); bm_puts(bm_cpu_avx2_status());
    bm_puts("; last run "); bm_puts(bm_simd_used()); bm_putc('\n');
}
static void calc_progress(unsigned done, unsigned total) {
    if (!done) {
        bm_puts("[model prefill "); bm_uint(total); bm_puts(" tokens]\n");
    } else {
        bm_puts("[model prefill "); bm_uint(done); bm_putc('/');
        bm_uint(total); bm_puts("]\n");
    }
}
static int integer(const char *s, unsigned min, unsigned max) {
    if (!*s) return -1;
    unsigned n=0;
    while (*s) {
        if (*s<'0' || *s>'9' || n>max/10) return -1;
        n=n*10+(unsigned)(*s++-'0');
        if (n>max) return -1;
    }
    return n<min ? -1 : (int)n;
}
static void help(void) {
    bm_puts("Type a question in English and press Enter.\n"
            "/reset   clear conversation     /tokens N   answer length\n"
            "/stats   memory and context     /selftest   numerical probe\n"
            "/threads N   1..4 workers (1 = serial comparison)\n"
            "/simd auto|sse2   select matvec instructions\n"
            "/run     ring3 test             /run help   process commands\n"
            "/calc QUESTION   model -> LLVM IR -> machine code -> ring3 -> answer\n"
            "/help    show help              /quit       power off\n");
}
static void math_check(void) {
    bm_fp_prepare();
    const float expected[]={1,0,0.5403023058681397f,0.8414709848078965f,
                            316.2277660168379f,2.718281828459045f,1.4142135623730951f};
    const float actual[]={cosf(0),sinf(0),cosf(1),sinf(1),powf(100000,0.5f),expf(1),sqrtf(2)};
    bm_check_finite("math startup check",actual,sizeof(actual)/sizeof(*actual),-1,-1);
    for (unsigned i=0;i<sizeof(actual)/sizeof(*actual);i++) {
        uint32_t a,b; memcpy(&a,actual+i,4); memcpy(&b,expected+i,4);
        if ((a>b ? a-b : b-a)>2) bm_panic("math startup check failed");
    }
    bm_puts("Math: SSE2; startup check OK (2026-09-10).\n");
}
static void selftest(Model *m) {
    const int tokens[]={151644,872,198,3838};
    const int probes[]={0,1,2,17,198,216,999,1000,4096,9690,16384,19556,24576,32768,40000,49151};
    size_t before=bm_heap_available();
    State *s=new_state(m,8);
    bm_simd_reset();
    bm_parallel_begin();
    workers_status();
    bm_puts("SELFTEST BEGIN\n");
    uint64_t began=bm_ticks;
    for (unsigned pos=0;pos<4;pos++) {
        forward(m,s,tokens[pos],1);
        bm_puts("LOGITS "); bm_uint(pos); bm_putc(' '); bm_uint(greedy(s->logits));
        for (unsigned j=0;j<sizeof(probes)/sizeof(*probes);j++) {
            uint32_t bits; memcpy(&bits,s->logits+probes[j],4);
            bm_putc(' '); bm_hex(bits,8);
        }
        bm_putc('\n');
    }
    bm_parallel_end();
    bm_puts("SELFTEST time "); elapsed(bm_ticks-began); bm_puts("s\n");
    workers_status();
    free_state(s);
    if (bm_heap_available()!=before) bm_panic("selftest leaked heap memory");
    bm_puts("SELFTEST END heap restored\n");
}
_Noreturn void bm_main(void) {
    bm_init();
    bm_puts("AIOS bare metal / x86-64 UEFI\nUEFI USB -> RAM -> neural.c\n");
    math_check();
    size_t state_bytes=sizeof(State)+(size_t)BOOT_CONTEXT*(2*L*KD+NH+HS)*4+sizeof(Model);
    bm_reserve_heap(state_bytes+16*1024*1024);
    const void *data=bm_load_model(MODEL_BYTES);
    bm_puts("Model in RAM: "); bm_uint(MODEL_BYTES); bm_puts(" bytes; no more disk access.\n");
    uint64_t began=bm_ticks;
    bm_puts("Checking model CRC32... ");
    if (bm_crc32(data,MODEL_BYTES)!=MODEL_CRC32) bm_panic("model CRC32 mismatch");
    bm_puts("OK (check "); elapsed(bm_ticks-began); bm_puts("s)\n");
    Model *m=alloc(sizeof(*m)); init_model(m,data,MODEL_BYTES);
    State *s=new_state(m,BOOT_CONTEXT);
    int *ids=alloc(MAXCTX*sizeof(int));
    char line[4096];
    int limit=BOOT_TOKENS;
    bm_parallel_begin(); bm_parallel_end(); /* Discover the usable dispatch mode. */
    workers_status();
    bm_puts("Qwen2.5-Coder-1.5B-Instruct Q4; context "); bm_uint(BOOT_CONTEXT); bm_puts(" tokens.\n");
    help();
    for (;;) {
        bm_puts("YOU> ");
        int length=bm_readline(line,sizeof(line));
        if (length==-2) bm_shutdown();
        if (length<0) { bm_puts("Input too long (maximum 4095 bytes).\n"); continue; }
        if (!length) continue;
        if (!strcmp(line,"/quit")) bm_shutdown();
        if (!strcmp(line,"/reset")) { s->pos=0; bm_puts("Conversation cleared.\n"); continue; }
        if (!strcmp(line,"/help")) { help(); continue; }
        if (!strcmp(line,"/selftest")) { selftest(m); continue; }
        if (bm_process_command(line)) continue;
        if (!memcmp(line,"/simd ",6)) {
            if (!strcmp(line+6,"auto")) bm_simd_set_auto(1);
            else if (!strcmp(line+6,"sse2")) bm_simd_set_auto(0);
            else { bm_puts("Use /simd auto|sse2\n"); continue; }
            workers_status(); continue;
        }
        if (!strcmp(line,"/stats")) {
            bm_puts("Context "); bm_uint((unsigned)s->pos); bm_putc('/'); bm_uint(BOOT_CONTEXT);
            bm_puts("; free heap "); bm_uint(bm_heap_available()); bm_puts(" bytes; uptime ");
            elapsed(bm_ticks); bm_puts("s\n"); workers_status(); continue;
        }
        if (!memcmp(line,"/threads ",9)) {
            int value=integer(line+9,1,BM_MAX_THREADS);
            if (value<0) bm_puts("Use /threads 1..4\n");
            else {
                bm_parallel_set_limit((unsigned)value);
                bm_parallel_begin(); bm_parallel_end(); workers_status();
            }
            continue;
        }
        if (!memcmp(line,"/tokens ",8)) {
            int value=integer(line+8,1,MAXCTX);
            if (value<0) bm_puts("Use /tokens 1..8192\n");
            else { limit=value; bm_puts("Answer limit: "); bm_uint((unsigned)limit); bm_putc('\n'); }
            continue;
        }
        int forced=!strcmp(line,"/calc") || !memcmp(line,"/calc ",6);
        bm_calc_expression literal, *expected=NULL;
        if (forced) {
            const char *request=line+5;
            while (*request==' ' || *request=='\t') request++;
            if (!*request) { bm_puts("Use /calc A + B (also -, *, /), or /calc QUESTION\n"); continue; }
            if (bm_calc_parse_expression(request,&literal)==BM_CALC_CALL) expected=&literal;
        }
        if (line[0]=='/' && !forced) { bm_puts("Unknown command. Use /help or /run help.\n"); continue; }
        int n=turn_tokens(m,line,s->pos==0,ids,MAXCTX);
        if (n+3>s->ctx) { bm_puts("Question exceeds context. Shorten it.\n"); continue; }
        if (s->pos+n+3>s->ctx) {
            n=turn_tokens(m,line,1,ids,MAXCTX);
            if (n+3>s->ctx) { bm_puts("Question exceeds context. Shorten it.\n"); continue; }
            s->pos=0; bm_puts("[context full; starting a new conversation]\n");
        }
        calc_chat_result result;
        calc_respond(m,s,ids,n,limit,forced,expected,&result);
    }
}
