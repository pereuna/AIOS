#include "runtime.h"
#include "asm1_tool.h"

void asm1_execute(const char *source, int require_end, asm1_result *r) {
    memset(r,0,sizeof(*r));
    asm1_program p;
    r->compile_status=asm1_compile(source,&p);
    r->error_line=p.error_line;
    if (!r->compile_status && require_end && !p.complete)
        r->compile_status=ASM1_INCOMPLETE;
    if (r->compile_status) return;
    r->executed=1;
    r->process.status=bm_process_run_input(p.code,p.code_size,p.input,p.input_count,&r->process);
}
static void append(char **p, const char *s) {
    while (*s) *(*p)++=*s++;
}
static void decimal(char **p, uint64_t value) {
    char digits[20]; unsigned n=0;
    do { digits[n++]=(char)('0'+value%10); value/=10; } while (value);
    while (n) *(*p)++=digits[--n];
}
void asm1_feedback(const asm1_result *r, char text[ASM1_FEEDBACK_SIZE]) {
    /* Only fixed trusted strings and bounded integers: <192 bytes total. */
    char *p=text;
    if (r->compile_status) {
        append(&p,"compile_error "); append(&p,asm1_error(r->compile_status));
        if (r->error_line) { append(&p," line "); decimal(&p,r->error_line); }
    } else if (!r->process.status) {
        append(&p,"ok; value="); decimal(&p,r->process.rax);
        append(&p,"; steps="); decimal(&p,r->process.steps);
    } else {
        append(&p,"runtime_error ");
        int status=r->process.status;
        if (status==BM_PROCESS_TIMEOUT) append(&p,"E_STEP_LIMIT");
        else if (status==BM_PROCESS_UNSAFE) append(&p,"E_ENCODING_REJECTED");
        else if (status>=BM_PROCESS_FAULT_BASE) {
            append(&p,"E_FAULT vector="); decimal(&p,r->process.fault_vector);
        } else append(&p,"E_PROCESS");
        append(&p,"; steps="); decimal(&p,r->process.steps);
    }
    *p=0;
}
