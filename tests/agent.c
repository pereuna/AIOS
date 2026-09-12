#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../baremetal/agent.h"

static unsigned executions;
static int process_status;
int bm_process_run_input(const void *code, size_t size, const uint32_t *input,
                         size_t count, bm_process_result *r) {
    (void)code; (void)input; (void)count;
    assert(size && size<=BM_PROCESS_MAX_CODE);
    executions++;
    *r=(bm_process_result){.status=process_status,.rax=42,.steps=10};
    return process_status;
}
typedef struct {
    const char *reply[5];
    const char *feedback[4];
    unsigned count, feeds, visible;
    int stop, no_room, final_only;
} script;
static void generate(void *ctx, unsigned budget, unsigned reserve, agent_reply *r) {
    script *s=ctx;
    assert(s->count<5 && s->reply[s->count]);
    assert(budget && (reserve==2 || reserve==AGENT_CONTEXT_RESERVE));
    strcpy(r->text,s->reply[s->count++]); r->tokens=1; r->stop=s->stop;
}
static int feedback(void *ctx, const char *text, int final_only) {
    script *s=ctx;
    assert(s->feedback[s->feeds] && strstr(text,s->feedback[s->feeds++]));
    s->final_only=final_only;
    return !s->no_room;
}
static void display(void *ctx, const char *text, int tool) {
    script *s=ctx; (void)text;
    if (!tool) s->visible++;
}
static agent_result run(script *s, unsigned budget) {
    agent_io io={s,generate,feedback,display};
    return agent_run(&io,budget);
}
int main(void) {
    const char *call="/asm asm1; li r0 42; exit r0; end";
    script s={.reply={call,"The result is 42."},.feedback={"ok; value=42"}};
    agent_result r=run(&s,128);
    assert(r.stop==AGENT_END && r.calls==1 && executions==1 && s.feeds==1 && s.visible==1);
    s=(script){.reply={"/asm li r0 42; end",call,"42"},
               .feedback={"E_NO_EXIT","ok; value=42"}};
    r=run(&s,128);
    assert(r.calls==2 && executions==2 && s.feeds==2 && s.visible==1);
    s=(script){.reply={"/asm li r0 42; exit r0",call,"42"},
               .feedback={"E_INCOMPLETE","ok; value=42"}};
    r=run(&s,128); assert(r.calls==2 && executions==3);
    process_status=BM_PROCESS_TIMEOUT;
    s=(script){.reply={call,call,call,"Could not complete the calculation."},
               .feedback={"E_STEP_LIMIT","E_STEP_LIMIT","E_STEP_LIMIT"}};
    r=run(&s,128); assert(r.calls==3 && s.final_only && executions==6);
    s=(script){.reply={call,call,call,call},
               .feedback={"E_STEP_LIMIT","E_STEP_LIMIT","E_STEP_LIMIT"}};
    r=run(&s,128); assert(r.stop==AGENT_CALL_LIMIT && executions==9);
    for (int stop=AGENT_TOKEN_LIMIT;stop<=AGENT_INVALID_TOKEN;stop++) {
        s=(script){.reply={call},.stop=stop};
        r=run(&s,128); assert(r.stop==stop && executions==9 && !s.feeds);
    }
    s=(script){.reply={call}};
    r=run(&s,1); assert(r.stop==AGENT_CALL_LIMIT && executions==9);
    s=(script){.reply={"Prose mentions /asm li r0 1; exit r0; end"}};
    r=run(&s,128); assert(!r.calls && executions==9 && s.visible==1);
    s=(script){.reply={"/exec b8 2a 00 00 00"}};
    r=run(&s,128); assert(!r.calls && executions==9);
    s=(script){.reply={call},.feedback={"E_STEP_LIMIT"},.no_room=1};
    r=run(&s,128); assert(r.stop==AGENT_CONTEXT_LIMIT && s.count==1);
    s=(script){0}; r=run(&s,0); assert(r.stop==AGENT_TOKEN_LIMIT && !s.count);
    puts("Agent: automatic result/repair/final loop, call budgets, truncation and context stop passed");
}
