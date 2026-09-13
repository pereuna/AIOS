/* The UEFI application's tokenizer, transformer and interactive console. */
#include "../neural.c"
#include "../.build/config.h"
#include "process_console.h"

static void elapsed(uint64_t ticks) {
    bm_uint(ticks/100); bm_putc('.');
    bm_putc((char)('0'+ticks/10%10)); bm_putc((char)('0'+ticks%10));
}
static void rate(unsigned tokens, uint64_t ticks) {
    if (!ticks) { bm_puts("n/a"); return; }
    uint64_t hundredths=(uint64_t)tokens*10000/ticks;
    elapsed(hundredths);
}
static void workers_status(void) {
    bm_puts("Compute: "); bm_puts(bm_parallel_mode()); bm_puts("; workers ");
    bm_uint(bm_parallel_count()); bm_puts("; requested "); bm_uint(bm_parallel_limit());
    bm_putc('\n');
    bm_puts("Matvec: "); bm_puts(bm_simd_auto() ? "auto" : "forced SSE2");
    bm_puts("; BSP "); bm_puts(bm_cpu_avx2_status());
    bm_puts("; last run "); bm_puts(bm_simd_used()); bm_putc('\n');
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
            "/asm SOURCE   compile and run asm1 (use ';' for newlines)\n"
            "/help    show help              /quit       power off\n"
            "\n"
            "asm1 commands (space-separated operands; no commas):\n"
            "  li rd N       load constant N (uint32 0..4294967295)\n"
            "  mov rd rs     copy register rs to rd\n"
            "  add rd rs     rd = rd + rs\n"
            "  sub rd rs     rd = rd - rs\n"
            "  mul rd rs     rd = rd * rs\n"
            "  udiv rd rs    unsigned rd = rd / rs\n"
            "  umod rd rs    unsigned rd = rd % rs\n"
            "  and rd rs     bitwise AND into rd\n"
            "  or rd rs      bitwise OR into rd\n"
            "  xor rd rs     bitwise XOR into rd\n"
            "  shl rd rs     shift rd left by (rs & 31)\n"
            "  shr rd rs     shift rd right by (rs & 31)\n"
            "  eq rd rs      rd = 1 if old rd == rs, otherwise 0\n"
            "  lt rd rs      rd = 1 if old rd < rs (unsigned), otherwise 0\n"
            "  ld rd I       load memory cell I (0..255) into rd\n"
            "  st I rs       store rs into memory cell I (0..255)\n"
            "  input A B ...  initialize memory cells 0, 1, ... (max 64)\n"
            "  label lK      define label l0..l31\n"
            "  jmp lK        unconditional jump\n"
            "  jz rN lK      jump if register rN is zero\n"
            "  exit rN       return register rN (required)\n"
            "Registers are r0..r9; arithmetic wraps modulo 2^32.\n"
            "Optional source markers: asm1 and end; end must be last.\n");
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
static void respond(Model *m, State *s, int *ids, int n, int limit) {
    bm_simd_reset();
    bm_parallel_begin();
    workers_status();
    uint64_t start=bm_ticks;
    for (int i=0;i<n;i++) forward(m,s,ids[i],i==n-1);
    uint64_t ready=bm_ticks;
    unsigned count=0;
    const char *stop="token limit reached";
    bm_puts("AI> ");
    while (count<(unsigned)limit) {
        if (s->pos+2>=s->ctx) { stop="context limit reached"; break; }
        int token=greedy(s->logits);
        count++;
        if (model_token_end(token)) { stop="complete"; break; }
        if (!model_token_text(token)) { stop="invalid control token"; break; }
        Word w=m->words[token];
        int invalid=0;
        for (unsigned i=0;i<w.n;i++) if (!w.p[i]) invalid=1;
        if (invalid) { stop="invalid control token"; break; }
        bm_write(w.p,w.n);
        forward(m,s,token,1);
    }
    /* Close the assistant turn even when generation reaches a limit. */
    forward(m,s,MODEL_EOS,0); forward(m,s,(int)m->byte_id['\n'],0);
    bm_parallel_end();
    uint64_t end=bm_ticks;
    bm_puts("\n[prompt "); bm_uint((unsigned)n); bm_puts(" tokens, "); elapsed(ready-start);
    bm_puts("s; output "); bm_uint(count); bm_puts(" tokens, "); elapsed(end-ready);
    bm_puts("s; "); rate(count,end-ready); bm_puts(" tok/s; ");
    bm_puts(bm_parallel_mode()); bm_putc(' '); bm_uint(bm_parallel_count()); bm_puts(" workers");
    bm_puts("; "); bm_puts(bm_simd_used());
    bm_puts("; "); bm_puts(stop); bm_puts("]\n");
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
        if (line[0]=='/') { bm_puts("Unknown command. Use /help or /run help.\n"); continue; }
        int n=turn_tokens(m,line,s->pos==0,ids,MAXCTX);
        if (n+3>s->ctx) { bm_puts("Question exceeds context. Shorten it.\n"); continue; }
        if (s->pos+n+3>s->ctx) {
            n=turn_tokens(m,line,1,ids,MAXCTX);
            if (n+3>s->ctx) { bm_puts("Question exceeds context. Shorten it.\n"); continue; }
            s->pos=0; bm_puts("[context full; starting a new conversation]\n");
        }
        respond(m,s,ids,n,limit);
    }
}
