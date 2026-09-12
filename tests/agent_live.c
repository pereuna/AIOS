/* Optional slow test: actual Q4 model -> asm1 -> ring3 -> model answer. */
#include "../neural.c"
#include "../baremetal/agent_model.h"

static unsigned successes, answers;
static void log_text(const char *s) {
    while (*s) __asm__ volatile("outb %0,$0xe9" :: "a"(*s++));
    __asm__ volatile("outb %0,$0xe9" :: "a"((unsigned char)'\n'));
}
static _Noreturn void finish(int ok) {
    log_text(ok ? "PROCESS PASS: live model -> asm1 -> ring3 -> final answer" : "FAIL: live agent");
    __asm__ volatile("outl %0,$0xf4" :: "a"(ok ? 0x10 : 0x11));
    for (;;) __asm__ volatile("hlt");
}
static void display(void *context, const char *text, int tool) {
    (void)context;
    log_text(tool ? "TOOL:" : "ANSWER:"); log_text(text);
    if (tool && !memcmp(text,"ok; value=42;",13)) successes++;
    if (!tool) for (size_t i=0;text[i] && text[i+1];i++)
        if (text[i]=='4' && text[i+1]=='2') { answers++; break; }
}
static void generate(void *context, unsigned budget, unsigned reserve, agent_reply *reply) {
    agent_model_generate(context,budget,reserve,reply);
    log_text("GENERATED:"); log_text(reply->text);
}
_Noreturn void bm_main(void) {
    bm_init(); bm_fp_prepare();
    log_text("LIVE AGENT BEGIN: loading model");
    bm_reserve_heap(sizeof(State)+(size_t)2048*(2*L*KD+NH+HS)*4+sizeof(Model)+16*1024*1024);
    const void *data=bm_load_model(MODEL_BYTES);
    Model *m=alloc(sizeof(*m)); init_model(m,data,MODEL_BYTES);
    State *s=new_state(m,2048); int *ids=alloc(MAXCTX*sizeof(int));
    int n=turn_tokens(m,"What is 17+25?",1,ids,MAXCTX);
    if (n+AGENT_CONTEXT_RESERVE+64>s->ctx) finish(0);
    log_text("LIVE AGENT: prompt inference");
    bm_parallel_begin();
    for (int i=0;i<n;i++) forward(m,s,ids[i],i==n-1);
    bm_parallel_end();
    agent_model model={m,s,ids};
    agent_io io={&model,generate,agent_model_feedback,display};
    agent_result result=agent_run(&io,512);
    log_text(agent_stop(result.stop));
    finish(result.stop==AGENT_END && successes && answers);
}
