/* Include after neural.c. Shared by the console and the real-model VM test. */
#ifndef AIOS_CALC_MODEL_H
#define AIOS_CALC_MODEL_H
#include "calc.h"

/* The firmware console needs a heartbeat while a 1.5B model pre-fills its
 * prompt.  Host fixtures keep the old quiet output unless the caller opts in.
 */
#ifndef BM_CALC_PROGRESS
#define BM_CALC_PROGRESS(done,total) ((void)(done),(void)(total))
#endif

typedef struct {
    unsigned calls, errors;
    int success, complete, final_answer;
    bm_calc_result last;
    char final_text[BM_CALC_FEEDBACK];
} calc_chat_result;

static int calc_generate(Model *m,State *s,int *ids,int n,unsigned *budget,
                         char text[BM_CALC_TEXT],int *overflow) {
    size_t used=0; int complete=0;
    *overflow=0;
    BM_CALC_PROGRESS(0,n);
    bm_parallel_begin();
    for (int i=0;i<n;i++) {
        forward(m,s,ids[i],i==n-1);
        if ((i+1)%64==0 || i+1==n) BM_CALC_PROGRESS((unsigned)(i+1),(unsigned)n);
    }
    bm_puts("AI> ");
    while (*budget && s->pos+2<s->ctx) {
        int token=greedy(s->logits); --*budget;
        if (model_token_end(token)) { complete=1; break; }
        if (!model_token_text(token)) break;
        Word w=m->words[token];
        int invalid=0;
        for (unsigned i=0;i<w.n;i++) if (!w.p[i]) invalid=1;
        if (invalid) break;
        /* Keep ordinary chat streaming at its existing token/context limits.
         * Only executable calls need the complete bounded capture. */
        bm_write(w.p,w.n);
        size_t keep=w.n;
        if (keep>=BM_CALC_TEXT-used) { keep=BM_CALC_TEXT-used-1; *overflow=1; }
        memcpy(text+used,w.p,keep); used+=keep;
        forward(m,s,token,1);
    }
    text[used]=0;
    forward(m,s,MODEL_EOS,0); forward(m,s,(int)m->byte_id['\n'],0);
    bm_parallel_end();
    return complete;
}
static void calc_respond(Model *m,State *s,int *ids,int n,int limit,
                         int forced,const bm_calc_expression *expected,
                         calc_chat_result *summary) {
    memset(summary,0,sizeof(*summary));
    unsigned budget=(unsigned)limit;
    int finished=0;
    for (unsigned round=0;round<4;round++) {
        char reply[BM_CALC_TEXT], feedback[BM_CALC_FEEDBACK+192];
        int overflow=0;
        int complete=calc_generate(m,s,ids,n,&budget,reply,&overflow);
        bm_putc('\n');
        summary->complete=complete;
        if (!complete) {
            bm_puts("calc: incomplete model reply; no bytes executed\n"); return;
        }
        bm_calc_result parsed;
        int call=bm_calc_parse_call(reply,&parsed);
        if (call==BM_CALC_NOT_CALL && (!forced || finished)) {
            summary->final_answer=1;
            size_t length=strlen(reply);
            if (length>=sizeof(summary->final_text)) length=sizeof(summary->final_text)-1;
            memcpy(summary->final_text,reply,length); summary->final_text[length]=0;
            return;
        }
        if (overflow) {
            summary->complete=0;
            bm_puts("calc: tool reply exceeds buffer; no bytes executed\n"); return;
        }
        if (finished || summary->calls>=3) {
            bm_puts("calc: tool call limit reached; no bytes executed\n"); return;
        }
        if (call==BM_CALC_NOT_CALL) {
            summary->errors++;
            bm_puts("calc: /calc requires a LLVM IR tool call; retrying\n");
            const char *message="This is a /calc request. Do not answer directly. Return only a complete define i64 @calc() function using the original operands and returning the SSA result.";
            memcpy(feedback,message,strlen(message)+1);
        } else {
            summary->calls++;
            bm_calc_execute(reply,expected,&summary->last);
            bm_calc_feedback(&summary->last,feedback);
            bm_puts(feedback); bm_putc('\n');
            if (summary->last.status==BM_PROCESS_OK) summary->success=1;
            else summary->errors++;
            if (summary->last.status==BM_PROCESS_OK || summary->last.status>=BM_PROCESS_FAULT_BASE ||
                summary->last.status==BM_CALC_OVERFLOW || summary->last.status==BM_CALC_DIVISION) {
                finished=1;
                const char *instruction="\nAnswer the original question using this tool result. Do not call the tool again. Do not invent a result on error.";
                size_t k=strlen(feedback); memcpy(feedback+k,instruction,strlen(instruction)+1);
            }
        }
        if (!budget) { bm_puts("calc: model token budget exhausted\n"); return; }
        n=turn_tokens(m,feedback,0,ids,MAXCTX);
        if (s->pos+n+3>s->ctx) {
            bm_puts("calc: result displayed; context full, no model continuation\n"); return;
        }
    }
    bm_puts("calc: model round limit reached\n");
}
#endif
