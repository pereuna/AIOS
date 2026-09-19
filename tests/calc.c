/* Reject malformed/model-inconsistent data before any process is entered. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../baremetal/calc.h"

static unsigned calls;
static unsigned char code[BM_PROCESS_MAX_CODE];
static uint32_t operands[256];
static size_t bytes;
int bm_process_run_input(const void *p,size_t n,const uint32_t *input,size_t count,bm_process_result *r) {
    assert(count<=256 && n<=sizeof(code));
    calls++; memcpy(code,p,n); bytes=n; memcpy(operands,input,count*sizeof(*input));
    memset(r,0,sizeof(*r)); r->rax=(uint64_t)(int64_t)-8; r->steps=5;
    return 0;
}
#define PREFIX "48 bb 00 20 00 00 00 40 00 00 48 63 03 48 63 4b 04 "
int main(void) {
    bm_calc_result r; bm_calc_expression e;
    assert(bm_calc_execute(NULL,NULL,&r)==BM_CALC_FORMAT && r.status==BM_CALC_FORMAT);
    assert(bm_calc_parse_expression(" -2147483648 / -1 ",&e)==BM_CALC_CALL);
    assert(e.a==INT32_MIN && e.b==-1 && e.op=='/');
    assert(bm_calc_parse_expression("-9223372036854775808 + 9223372036854775807",&e)==BM_CALC_CALL);
    assert(e.a==INT64_MIN && e.b==INT64_MAX);
    for (const char **s=(const char *[]){"9223372036854775808 + 1","-9223372036854775809 * 2","1+2+3","1.5+2","1","",NULL};*s;s++)
        assert(bm_calc_parse_expression(*s,&e)==BM_CALC_FORMAT);
    assert(bm_calc_parse_expression("17-25",&e)==BM_CALC_CALL);
    const char *valid="/bytes 17 - 25 : " PREFIX "48 29 c8 cd 80";
    assert(bm_calc_execute(valid,&e,&r)==0 && calls==1);
    assert(bytes==22 && operands[0]==17 && operands[1]==25);
    assert(!memcmp(code,r.code,bytes)); /* Model bytes survive unchanged. */
    char feedback[BM_CALC_FEEDBACK]; bm_calc_feedback(&r,feedback);
    assert(strstr(feedback,"expression=17 - 25; value=-8") && strstr(feedback,"executor=ring3"));
    assert(bm_calc_parse_call("The answer is 42.",&r)==BM_CALC_NOT_CALL);
    assert(bm_calc_parse_call("/bytesX",&r)==BM_CALC_NOT_CALL);
    const char *bad[]={"/bytes", "/bytes 1 + 2 : 9", "/bytes 1 + 2 : zz", "/bytes 1 + 2 : 9090",
                       "/bytes 1 + 2 :", "/bytes 1 + 2 : 90 prose", NULL};
    for (unsigned i=0;bad[i];i++) assert(bm_calc_execute(bad[i],NULL,&r)==BM_CALC_FORMAT);
    assert(bm_calc_execute("/bytes 17 + 25 : " PREFIX "48 29 c8 cd 80",NULL,&r)==BM_CALC_MISMATCH);
    assert(bm_calc_execute("/bytes 17 - 26 : " PREFIX "48 29 c8 cd 80",&e,&r)==BM_CALC_MISMATCH);
    assert(bm_calc_execute("/bytes 17 - 25 : " PREFIX "48 29 c8",&e,&r)==BM_CALC_MISMATCH);
    assert(bm_calc_execute("/bytes 17 - 25 : " PREFIX "48 29 c8 cd 80 90",&e,&r)==BM_CALC_MISMATCH);
    assert(calls==1);
    assert(bm_calc_execute("/bytes -157 * -207 : " PREFIX "48 0f af c1 cd 80",NULL,&r)==0);
    assert(operands[0]==(uint32_t)-157 && operands[1]==(uint32_t)-207);
    for (size_t i=0;i<strlen(valid);i++) {
        char truncated[BM_CALC_TEXT]; memcpy(truncated,valid,i); truncated[i]=0;
        unsigned before=calls; assert(bm_calc_execute(truncated,&e,&r)!=0 && calls==before);
    }
    const char *ir="define i64 @calc() { %v0 = sub nsw i64 17, 25 ret i64 %v0 }";
    assert(bm_calc_execute(ir,&e,&r)==0 && r.is_ir && r.ir.operations==1);
    assert(!memcmp(code,r.ir.code,bytes) && operands[0]==17 && operands[1]==0 && operands[2]==25);
    unsigned before=calls;
    assert(bm_calc_execute("define i64 @calc() { ret i64 -8 }",&e,&r)==BM_IR_FORMAT);
    assert(bm_calc_execute("define i64 @calc() { %v = sub i64 17, 25 ret i64 -8 }",&e,&r)==BM_IR_FORMAT);
    assert(bm_calc_execute("define i64 @calc() { %v = sub i64 17, 26 ret i64 %v }",&e,&r)==BM_CALC_MISMATCH);
    assert(bm_calc_execute("define i64 @calc() { %v = add i64 17, 25 ret i64 %v }",&e,&r)==BM_CALC_MISMATCH);
    assert(bm_calc_execute("define i64 @calc() { %v = sub i64 17, 25 %x = add i64 %v, 1 ret i64 %x }",&e,&r)==BM_CALC_MISMATCH);
    assert(calls==before);
    assert(bm_calc_execute("define i64 @calc() { ret i64 101 }",NULL,&r)==BM_IR_FORMAT && calls==before);
    for (size_t i=0;i<strlen(ir);i++) {
        char truncated[256]; memcpy(truncated,ir,i); truncated[i]=0;
        assert(bm_calc_execute(truncated,&e,&r)!=0 && calls==before);
    }
    assert(bm_calc_execute(ir,&e,&r)==0);
    bm_calc_feedback(&r,feedback);
    assert(strstr(feedback,"ir=llvm; operations=1; value=-8"));
    puts("Calc: i64 requests, LLVM IR execution/routing, literal matching, legacy bytes, truncation PASS");
}
