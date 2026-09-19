/* Deterministic generation fixtures test routing/control, not model accuracy. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../baremetal/calc.h"
#define MAXCTX 8192
#define MODEL_EOS 2
typedef struct { const unsigned char *p; unsigned n; } Word;
typedef struct { Word words[3]; unsigned byte_id[256]; } Model;
typedef struct { int pos,ctx; float logits[1]; } State;
static const char **replies;
static unsigned stage, emitted, executions, feedbacks;
static int process_status;
static char last_feedback[512], output[16384];
static void bm_puts(const char *s) { assert(strlen(output)+strlen(s)<sizeof(output)); strcat(output,s); }
static void bm_putc(char c) { char b[]={c,0}; bm_puts(b); }
static void bm_write(const void *text,size_t n) { const char *s=text; while (n--) bm_putc(*s++); }
static void bm_parallel_begin(void) {}
static void bm_parallel_end(void) {}
static int model_token_end(int t) { return t==MODEL_EOS; }
static int model_token_text(int t) { return t==1; }
static int greedy(const float *logits) { (void)logits; return emitted ? MODEL_EOS : 1; }
static void forward(Model *m,State *s,int token,int logits) {
    (void)logits; s->pos++;
    if (token==100) { assert(replies[stage]); m->words[1].p=(const unsigned char *)replies[stage]; m->words[1].n=(unsigned)strlen(replies[stage]); emitted=0; }
    if (token==1) emitted=1;
    if (token==MODEL_EOS) stage++;
}
static int turn_tokens(Model *m,const char *text,int first,int *ids,int max) {
    (void)m;(void)first;(void)max; feedbacks++;
    assert(strlen(text)<sizeof(last_feedback)); strcpy(last_feedback,text); ids[0]=100; return 1;
}
#include "../baremetal/calc_model.h"
int bm_process_run_input(const void *p,size_t n,const uint32_t *input,size_t count,bm_process_result *r) {
    (void)p;(void)n;(void)input; assert(count>=2 && count<=256); executions++;
    memset(r,0,sizeof(*r)); r->rax=(uint64_t)(int64_t)-8; r->status=process_status;
    return process_status;
}
static calc_chat_result run(const char **messages,int forced,int tokens,int ctx) {
    replies=messages; stage=emitted=executions=feedbacks=0; output[0]=last_feedback[0]=0;
    Model m={0}; m.byte_id['\n']=3; State s={.ctx=ctx}; int ids[MAXCTX]={100};
    bm_calc_expression target={17,25,'-'}; calc_chat_result result;
    calc_respond(&m,&s,ids,1,tokens,forced,forced ? &target : NULL,&result);
    return result;
}
int main(void) {
    const char *good="define i64 @calc() { %v0 = sub nsw i64 17, 25 ret i64 %v0 }";
    const char *ok[]={good,"The result is -8.",NULL};
    calc_chat_result r=run(ok,1,20,200);
    assert(r.success && r.final_answer && executions==1 && feedbacks==1);
    assert(strstr(last_feedback,"value=-8") && strstr(output,"The result is -8."));
    const char *normal[]={"Hello!",NULL}; r=run(normal,0,20,200);
    assert(r.final_answer && !r.success && !executions && !feedbacks);
    const char *repair[]={"The result is -8.",good,"The CPU returned -8.",NULL};
    r=run(repair,1,20,200); assert(r.success && r.errors==1 && executions==1 && feedbacks==2);
    const char *mismatch[]={"define i64 @calc() { %v0 = add i64 17, 25 ret i64 %v0 }",good,"-8",NULL};
    r=run(mismatch,1,20,200); assert(r.success && r.errors==1 && r.calls==2 && executions==1);
    r=run(ok,1,1,200); assert(!r.complete && !executions && !feedbacks);
    r=run(ok,1,20,5); assert(r.success && !r.final_answer && executions==1 && feedbacks==1);
    const char *repeat[]={good,good,NULL}; r=run(repeat,1,20,200);
    assert(r.success && executions==1 && !r.final_answer);
    const char *refuse[]={"No", "No", "No", "No",NULL}; r=run(refuse,1,20,200);
    assert(!r.success && !executions && feedbacks==4);
    char huge[BM_CALC_TEXT+1]; memset(huge,'x',sizeof(huge)-1); huge[sizeof(huge)-1]=0;
    const char *overflow[]={huge,NULL}; r=run(overflow,1,20,200); assert(!r.complete && !executions);
    r=run(overflow,0,20,200);
    assert(r.complete && r.final_answer && !executions && strlen(output)==strlen(huge)+5);
    const char *zero[]={"define i64 @calc() { %v0 = sdiv i64 7, 0 ret i64 %v0 }",
                        "Division by zero has no result.",NULL};
    process_status=BM_PROCESS_FAULT_BASE;
    r=run(zero,0,20,200);
    assert(!r.success && r.final_answer && executions==1 && r.errors==1);
    assert(strstr(last_feedback,"E_DIVISION") && !strstr(last_feedback,"value="));
    process_status=BM_PROCESS_FAULT_BASE+6;
    const char *overflow_reply[]={good,"Signed overflow has no result.",NULL};
    r=run(overflow_reply,1,20,200);
    assert(!r.success && r.final_answer && executions==1 && r.errors==1 && strstr(last_feedback,"E_OVERFLOW"));
    process_status=0;
    const char *bad_ir[]={"define i64 @calc() { %v0 = add i64 %missing, 1 ret i64 %v0 }",good,"-8",NULL};
    r=run(bad_ir,0,20,200);
    assert(r.success && r.calls==2 && r.errors==1 && executions==1);
    const char *bad_div[]={"define i64 @calc() { %v = sdiv nsw i64 -17, 5 ret i64 %v }",
                          "define i64 @calc() { %v = sdiv i64 -17, 5 ret i64 %v }", "-3", NULL};
    r=run(bad_div,0,20,200);
    assert(r.success && r.calls==2 && r.errors==1 && executions==1);
    puts("Calc model routing: LLVM calls, forced routing, repair, feedback, bounds, terminal arithmetic faults PASS");
}
