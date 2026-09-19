#include "runtime.h"
#include "calc.h"
_Static_assert(BM_PROCESS_STACK==UINT64_C(0x400000002000),"calc prefix must match input page");

static int space(char c) { return c==' ' || c=='\t' || c=='\r' || c=='\n'; }
static void skip(const char **s) { while (space(**s)) ++*s; }
static int number(const char **s, int64_t *out) {
    skip(s);
    int negative=0;
    if (**s=='-' || **s=='+') { negative=**s=='-'; ++*s; }
    if (**s<'0' || **s>'9') return -1;
    uint64_t n=0, max=negative ? UINT64_C(0x8000000000000000) : INT64_MAX;
    while (**s>='0' && **s<='9') {
        unsigned digit=(unsigned)(*(*s)++-'0');
        if (n>(max-digit)/10) return -1;
        n=n*10+digit;
    }
    if (negative) n=0-n;
    memcpy(out,&n,sizeof(n));
    return 0;
}
static int expression(const char **s, bm_calc_expression *e) {
    if (number(s,&e->a)) return -1;
    skip(s); e->op=*(*s);
    if (e->op!='+' && e->op!='-' && e->op!='*' && e->op!='/') return -1;
    ++*s;
    return number(s,&e->b);
}
int bm_calc_parse_expression(const char *s, bm_calc_expression *e) {
    if (!s || !e || expression(&s,e)) return BM_CALC_FORMAT;
    skip(&s); return *s ? BM_CALC_FORMAT : BM_CALC_CALL;
}
static int digit(char c) {
    if (c>='0' && c<='9') return c-'0';
    if (c>='a' && c<='f') return c-'a'+10;
    if (c>='A' && c<='F') return c-'A'+10;
    return -1;
}
int bm_calc_parse_call(const char *s, bm_calc_result *r) {
    if (!r) return BM_CALC_FORMAT;
    memset(r,0,sizeof(*r));
    if (!s) return r->status=BM_CALC_FORMAT;
    int ir=bm_ir_compile(s,&r->ir);
    if (ir!=BM_IR_NOT_CALL) {
        r->is_ir=1;
        return r->status=ir==BM_IR_OK ? BM_CALC_CALL : ir;
    }
    skip(&s);
    if (strlen(s)<6 || memcmp(s,"/bytes",6) || (s[6] && !space(s[6])))
        return r->status=BM_CALC_NOT_CALL;
    s+=6;
    if (expression(&s,&r->expression)) return r->status=BM_CALC_FORMAT;
    if (r->expression.a<INT32_MIN || r->expression.a>INT32_MAX ||
        r->expression.b<INT32_MIN || r->expression.b>INT32_MAX) return r->status=BM_CALC_FORMAT;
    skip(&s);
    if (*s!=':') return r->status=BM_CALC_FORMAT;
    s++;
    while (1) {
        skip(&s); if (!*s) break;
        int hi=digit(*s++);
        if (hi<0 || !*s) return r->status=BM_CALC_FORMAT;
        int lo=digit(*s++);
        if (lo<0 || r->bytes==sizeof(r->code)) return r->status=BM_CALC_FORMAT;
        r->code[r->bytes++]=(unsigned char)(hi*16+lo);
        if (*s && !space(*s)) return r->status=BM_CALC_FORMAT;
    }
    if (!r->bytes) return r->status=BM_CALC_FORMAT;
    return r->status=BM_CALC_CALL;
}
int bm_calc_execute(const char *text, const bm_calc_expression *expected, bm_calc_result *r) {
    int status=bm_calc_parse_call(text,r);
    if (status==BM_CALC_NOT_CALL) return r->status=BM_CALC_FORMAT;
    if (status!=BM_CALC_CALL) return status;
    if (r->is_ir) {
        bm_ir_program *p=&r->ir;
        if (expected && (!p->literal || p->a!=expected->a || p->b!=expected->b || p->op!=expected->op))
            return r->status=BM_CALC_MISMATCH;
        status=bm_process_run_input(p->code,p->bytes,p->input,p->input_count,&r->process);
        if (status==BM_PROCESS_FAULT_BASE+6) status=BM_CALC_OVERFLOW;
        if (status==BM_PROCESS_FAULT_BASE) status=BM_CALC_DIVISION;
        return r->status=status;
    }
    /* Fixed data ABI: RBX points at the process input cells, sign-extend A/B
     * into RAX/RCX. The model emits every byte, including this prefix. */
    static const unsigned char prefix[]={
        0x48,0xbb,0x00,0x20,0x00,0x00,0x00,0x40,0x00,0x00,
        0x48,0x63,0x03,0x48,0x63,0x4b,0x04};
    static const unsigned char add[]={0x48,0x01,0xc8};
    static const unsigned char sub[]={0x48,0x29,0xc8};
    static const unsigned char mul[]={0x48,0x0f,0xaf,0xc1};
    static const unsigned char div[]={0x48,0x99,0x48,0xf7,0xf9};
    const unsigned char *op=NULL; size_t n=0;
    switch (r->expression.op) {
    case '+':op=add;n=sizeof(add);break;
    case '-':op=sub;n=sizeof(sub);break;
    case '*':op=mul;n=sizeof(mul);break;
    case '/':op=div;n=sizeof(div);break;
    }
    if ((expected && (expected->a!=r->expression.a || expected->b!=r->expression.b ||
                      expected->op!=r->expression.op)) ||
        r->bytes!=sizeof(prefix)+n+2 || memcmp(r->code,prefix,sizeof(prefix)) ||
        memcmp(r->code+sizeof(prefix),op,n) ||
        r->code[r->bytes-2]!=0xcd || r->code[r->bytes-1]!=0x80)
        return r->status=BM_CALC_MISMATCH;
    uint32_t input[]={(uint32_t)r->expression.a,(uint32_t)r->expression.b};
    /* No host arithmetic, assembler, encoder, patching or answer fallback. */
    return r->status=bm_process_run_input(r->code,r->bytes,input,2,&r->process);
}
static void append(char **p,const char *s) { while (*s) *(*p)++=*s++; }
static void unsigned_number(char **p,uint64_t n) {
    char b[24]; unsigned i=0;
    do { b[i++]=(char)('0'+n%10); n/=10; } while (n);
    while (i) *(*p)++=b[--i];
}
static void signed_number(char **p,uint64_t bits) {
    if (bits>>63) { *(*p)++='-'; bits=0-bits; }
    unsigned_number(p,bits);
}
void bm_calc_feedback(const bm_calc_result *r,char text[BM_CALC_FEEDBACK]) {
    char *p=text;
    append(&p,"Tool result (calc): ");
    if (r->status==BM_PROCESS_OK) {
        append(&p,"ok; ");
        if (r->is_ir) {
            append(&p,"ir=llvm; operations="); unsigned_number(&p,r->ir.operations);
        } else {
            append(&p,"expression="); signed_number(&p,(uint64_t)(int64_t)r->expression.a);
            *p++=' '; *p++=r->expression.op; *p++=' ';
            signed_number(&p,(uint64_t)(int64_t)r->expression.b);
        }
        append(&p,"; value="); signed_number(&p,r->process.rax);
        append(&p,"; steps="); unsigned_number(&p,r->process.steps);
        append(&p,"; executor=ring3");
    } else if (r->status==BM_CALC_FORMAT || r->status==BM_IR_FORMAT)
        append(&p,"error E_FORMAT; emit one complete LLVM function named @calc returning i64. Use add/sub/mul [nsw] i64 or sdiv i64, previously defined %names and ret i64. No Markdown.");
    else if (r->status==BM_IR_LIMIT) append(&p,"error E_LIMIT; at most 64 operations, 128 constants, 31-character SSA names");
    else if (r->status==BM_IR_SDIV_FLAGS) append(&p,"error E_FORMAT; LLVM sdiv has no nsw flag. Use sdiv i64 A, B with the original operands, then ret its SSA result. Negative operands are supported.");
    else if (r->status==BM_CALC_MISMATCH) append(&p,"error E_MISMATCH; return the original two operands and operation as one instruction, then ret its SSA result");
    else if (r->status==BM_CALC_OVERFLOW) append(&p,"error E_OVERFLOW; nsw signed i64 overflow; no numeric result");
    else if (r->status==BM_CALC_DIVISION) append(&p,"error E_DIVISION; division by zero or signed i64 division overflow; no numeric result");
    else if (r->status==BM_PROCESS_FAULT_BASE) append(&p,"error E_DIVIDE_ZERO; no numeric result");
    else {
        append(&p,"error E_PROCESS; status="); signed_number(&p,(uint64_t)(int64_t)r->status);
    }
    *p=0;
}
