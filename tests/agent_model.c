/* Drive the actual UEFI inference adapter with scripted token chunks. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../baremetal/runtime.h"
enum { MAXCTX=8192 };
typedef struct { const unsigned char *p; unsigned n; } Word;
typedef struct { unsigned byte_id[256]; Word words[32]; } Model;
typedef struct { int pos,ctx; float logits[1]; } State;
static int next[8192], cursor, parallel, fed;
static char feedback_text[512];
unsigned bm_parallel_begin(void) { assert(!parallel); parallel=1; return 1; }
void bm_parallel_end(void) { assert(parallel); parallel=0; }
static int greedy(const float *logits) { (void)logits; return next[cursor++]; }
static void forward(Model *m, State *s, int token, int output) {
    (void)m; (void)token; (void)output;
    assert(parallel && s->pos<s->ctx); s->pos++; fed++;
}
static int tokenize(Model *m, const char *text, int *ids, int cap, int special) {
    (void)m; assert(special && cap==MAXCTX);
    strcpy(feedback_text,text);
    for (int i=0;i<80;i++) ids[i]=17;
    return 80;
}
#include "../baremetal/agent_model.h"
int main(void) {
    Model m={0}; State s={.ctx=2048}; int ids[MAXCTX];
    m.byte_id['\n']=17;
    m.words[17]=(Word){(const unsigned char *)"/a",2};
    m.words[18]=(Word){(const unsigned char *)"sm ",3};
    m.words[19]=(Word){(const unsigned char *)"li r0 42; exit r0; end",sizeof("li r0 42; exit r0; end")-1};
    agent_model a={&m,&s,ids}; agent_reply r={0};
    next[0]=17; next[1]=18; next[2]=19; next[3]=MODEL_EOS;
    agent_model_generate(&a,64,AGENT_CONTEXT_RESERVE,&r);
    assert(r.stop==AGENT_END && !strcmp(r.text,"/asm li r0 42; exit r0; end"));
    assert(r.tokens==4 && s.pos==5 && !parallel);
    assert(agent_model_feedback(&a,"ok; value=42; steps=5",0));
    assert(strstr(feedback_text,"<|im_start|>user\nTool result (asm1): ok; value=42"));
    assert(strstr(feedback_text,"<|im_start|>assistant\n") && !parallel);
    s.pos=2000; int before=fed;
    assert(!agent_model_feedback(&a,"ok; value=42",1) && fed==before && s.pos==2000);
    assert(strstr(feedback_text,"No tool calls remain"));
    s.pos=0;
    assert(agent_model_feedback(&a,"compile_error E_NO_EXIT",0));
    assert(strstr(feedback_text,"Return ONLY a corrected /asm call with exit rN; end"));
    s.pos=0;
    assert(agent_model_feedback(&a,"runtime_error E_STEP_LIMIT",1)==2);
    assert(strstr(feedback_text,"Do not claim a computed result"));
    s.pos=1800;
    assert(agent_model_feedback(&a,"ok; value=42",0)==2 && s.pos==1880);
    assert(strstr(feedback_text,"No tool calls remain"));
    s.pos=0; cursor=0; r=(agent_reply){0};
    agent_model_generate(&a,2,AGENT_CONTEXT_RESERVE,&r);
    assert(r.stop==AGENT_TOKEN_LIMIT && !strcmp(r.text,"/asm "));
    s.pos=s.ctx-AGENT_CONTEXT_RESERVE; cursor=0; r=(agent_reply){0};
    agent_model_generate(&a,64,AGENT_CONTEXT_RESERVE,&r);
    assert(r.stop==AGENT_CONTEXT_LIMIT && !r.tokens && cursor==0);
    s.pos=0; cursor=0; next[0]=151644; r=(agent_reply){0};
    agent_model_generate(&a,64,2,&r); assert(r.stop==AGENT_INVALID_TOKEN);
    s.pos=0; cursor=0; next[0]=MODEL_BOS; r=(agent_reply){0};
    agent_model_generate(&a,64,2,&r); assert(r.stop==AGENT_END);
    s.pos=0; cursor=0; next[0]=MODEL_TOKEN_END; r=(agent_reply){0};
    agent_model_generate(&a,64,2,&r); assert(r.stop==AGENT_INVALID_TOKEN);
    assert(model_token_text(0) && model_token_text(1) && model_token_text(2));
    assert(model_token_text(MODEL_SPECIAL_END) && !model_token_text(-1));
    static unsigned char huge[ASM1_MAX_SOURCE]; memset(huge,'x',sizeof(huge));
    m.words[20]=(Word){huge,sizeof(huge)};
    s.pos=0; cursor=0; next[0]=20; r=(agent_reply){0};
    agent_model_generate(&a,64,2,&r); assert(r.stop==AGENT_OUTPUT_LIMIT && !r.text[0]);
    m.words[20]=(Word){(const unsigned char *)"a\0b",3};
    s.pos=0; cursor=0; r=(agent_reply){0};
    agent_model_generate(&a,64,2,&r); assert(r.stop==AGENT_INVALID_TOKEN);
    puts("Agent model adapter: split prefixes, EOS, budgets, overflow, feedback framing and KV capacity passed");
}
