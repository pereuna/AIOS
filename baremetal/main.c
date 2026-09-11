/* The UEFI application's tokenizer, transformer and interactive console. */
#include "../neural.c"
#include "../.build/config.h"

static void elapsed(uint64_t ticks) {
    bm_uint(ticks/100); bm_putc('.');
    bm_putc((char)('0'+ticks/10%10)); bm_putc((char)('0'+ticks%10));
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
    const int tokens[]={1,9690,198,19556};
    const int probes[]={0,1,2,17,198,216,999,1000,4096,9690,16384,19556,24576,32768,40000,49151};
    size_t before=bm_heap_available();
    State *s=new_state(m,8);
    bm_puts("SELFTEST BEGIN\n");
    for (unsigned pos=0;pos<4;pos++) {
        forward(m,s,tokens[pos],1);
        bm_puts("LOGITS "); bm_uint(pos); bm_putc(' '); bm_uint(greedy(s->logits));
        for (unsigned j=0;j<sizeof(probes)/sizeof(*probes);j++) {
            uint32_t bits; memcpy(&bits,s->logits+probes[j],4);
            bm_putc(' '); bm_hex(bits,8);
        }
        bm_putc('\n');
    }
    free_state(s);
    if (bm_heap_available()!=before) bm_panic("selftest leaked heap memory");
    bm_puts("SELFTEST END heap restored\n");
}
static void respond(Model *m, State *s, int *ids, int n, int limit) {
    uint64_t start=bm_ticks;
    for (int i=0;i<n;i++) forward(m,s,ids[i],i==n-1);
    uint64_t ready=bm_ticks;
    int count=0,ended=0;
    bm_puts("AI> ");
    while (count<limit && s->pos<s->ctx-2) {
        int token=greedy(s->logits);
        if (token==2 || token==0) { ended=1; break; }
        Word w=m->words[token];
        if ((uint32_t)token>=m->nspecial) bm_write(w.p,w.n);
        count++;
        forward(m,s,token,1);
    }
    uint64_t end=bm_ticks;
    forward(m,s,2,0); forward(m,s,(int)m->byte_id['\n'],0);
    bm_puts("\n[prompt "); bm_uint((unsigned)n); bm_puts(" tokens, "); elapsed(ready-start);
    bm_puts("s; output "); bm_uint((unsigned)count); bm_puts(" tokens, "); elapsed(end-ready);
    bm_puts(ended ? "s]\n" : "s; limit reached]\n");
}
_Noreturn void bm_main(void) {
    bm_init();
    bm_puts("Smol bare metal / x86-64 UEFI\nUEFI USB -> RAM -> neural.c\n");
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
    bm_puts("SmolLM2-1.7B-Instruct Q4; context "); bm_uint(BOOT_CONTEXT); bm_puts(" tokens.\n");
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
        if (!strcmp(line,"/stats")) {
            bm_puts("Context "); bm_uint((unsigned)s->pos); bm_putc('/'); bm_uint(BOOT_CONTEXT);
            bm_puts("; free heap "); bm_uint(bm_heap_available()); bm_puts(" bytes; uptime ");
            elapsed(bm_ticks); bm_puts("s\n"); continue;
        }
        if (!memcmp(line,"/tokens ",8)) {
            int value=integer(line+8,1,MAXCTX);
            if (value<0) bm_puts("Use /tokens 1..8192\n");
            else { limit=value; bm_puts("Answer limit: "); bm_uint((unsigned)limit); bm_putc('\n'); }
            continue;
        }
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
