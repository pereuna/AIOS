#ifndef SMOL_AGENT_MODEL_H
#define SMOL_AGENT_MODEL_H
/* Include after neural.c. This adapter is shared by UEFI and the host smoke test. */
#include "agent.h"
typedef struct { Model *model; State *state; int *ids; } agent_model;

static void agent_model_generate(void *context, unsigned budget, unsigned reserve, agent_reply *reply) {
    agent_model *a=context; Model *m=a->model; State *s=a->state;
    size_t size=0;
    reply->stop=AGENT_TOKEN_LIMIT;
    bm_parallel_begin();
    while (reply->tokens<budget) {
        if (s->pos+(int)reserve>=s->ctx) { reply->stop=AGENT_CONTEXT_LIMIT; break; }
        int token=greedy(s->logits);
        reply->tokens++;
        if (token==2) { reply->stop=AGENT_END; break; }
        if ((unsigned)token<m->nspecial) { reply->stop=AGENT_INVALID_TOKEN; break; }
        Word w=m->words[token];
        if (w.n>=sizeof(reply->text)-size) { reply->stop=AGENT_OUTPUT_LIMIT; break; }
        int invalid=0;
        for (unsigned i=0;i<w.n;i++) if (!w.p[i]) invalid=1;
        if (invalid) { reply->stop=AGENT_INVALID_TOKEN; break; }
        memcpy(reply->text+size,w.p,w.n); size+=w.n; reply->text[size]=0;
        forward(m,s,token,1);
    }
    /* Always close the assistant message, including an interrupted one. */
    if (s->pos+2<=s->ctx) {
        forward(m,s,2,0); forward(m,s,(int)m->byte_id['\n'],0);
    }
    bm_parallel_end();
}
static int agent_model_feedback(void *context, const char *result, int final_only) {
    agent_model *a=context;
    /* Use the model's existing user/assistant ChatML roles. Runtime output has
     * no model-controlled text or special tokens. No new tokenizer IDs needed. */
    char text[ASM1_FEEDBACK_SIZE+256]; size_t n=0;
    int success=!memcmp(result,"ok;",3);
    const char *instruction=success ? "\nAnswer the original question using this actual execution result."
        : "\nNo valid result. Return ONLY a corrected /asm call with exit rN; end. Do not answer the question yet.";
    if (final_only) instruction=success ? "\nNo tool calls remain. Answer using this actual execution result."
        : "\nNo tool calls remain. Explain that the calculation failed. Do not claim a computed result.";
    const char *parts[]={"<|im_start|>user\nTool result (asm1): ",result,instruction,
        "<|im_end|>\n<|im_start|>assistant\n"};
    for (unsigned i=0;i<4;i++) { size_t k=strlen(parts[i]); memcpy(text+n,parts[i],k); n+=k; }
    text[n]=0;
    n=(size_t)tokenize(a->model,text,a->ids,MAXCTX,1);
    if ((size_t)a->state->pos+n+AGENT_FINAL_TOKENS+2>(size_t)a->state->ctx) return 0;
    if (!final_only && (size_t)a->state->pos+n+AGENT_CONTEXT_RESERVE+64>(size_t)a->state->ctx) {
        /* Preserve the opportunity to answer even when another tool round cannot fit. */
        return agent_model_feedback(context,result,1) ? 2 : 0;
    }
    bm_parallel_begin();
    for (size_t i=0;i<n;i++) forward(a->model,a->state,a->ids[i],i+1==n);
    bm_parallel_end();
    return final_only ? 2 : 1;
}
#endif
