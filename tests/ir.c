#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ir_cases.h"

int main(int argc,char **argv) {
    if (argc==2 && !strcmp(argv[1],"--dump")) {
        for (unsigned i=0;i<IR_CASE_COUNT;i++) {
            printf("%d %016llx %zu\n",ir_cases[i].status,(unsigned long long)ir_cases[i].answer,strlen(ir_cases[i].source));
            puts(ir_cases[i].source);
        }
        return 0;
    }
    bm_ir_program p,q;
    for (unsigned i=0;i<IR_CASE_COUNT;i++) {
        assert(bm_ir_compile(ir_cases[i].source,&p)==BM_IR_OK);
        assert(p.bytes>0 && p.bytes<=BM_PROCESS_MAX_CODE);
        /* Independent check of the process validator's forbidden byte set. */
        for (size_t j=0;j<p.bytes;j++) {
            assert(p.code[j]!=0x9d && p.code[j]!=0xcf && p.code[j]!=0x8e);
            if (j+1<p.bytes) assert(!((p.code[j]==0x0f && p.code[j+1]==0xb2) ||
                                    (p.code[j]==0xc7 && p.code[j+1]==0xf8)));
        }
    }
    assert(bm_ir_compile(IR("%s = add i64 1, 100 ret i64 %s"),&p)==BM_IR_OK);
    assert(p.literal && p.a==1 && p.b==100 && p.op=='+');
    assert(bm_ir_compile(IR("%other = add i64 -157, -207 ret i64 %other"),&q)==BM_IR_OK);
    assert(p.bytes==q.bytes && !memcmp(p.code,q.code,p.bytes));
    assert(memcmp(p.input,q.input,p.input_count*sizeof(*p.input)));
    assert(bm_ir_compile(IR("%a = add i64 1, 2 ret i64 101"),&p)==BM_IR_FORMAT && !p.bytes);
    const char *bad[]={
        "define", "define i64 @other() { ret i64 1 }",
        "define i64 @calc(i64 %a) { ret i64 %a }",
        IR("entry : ret i64 1"),
        IR("%a = add i32 1, 2 ret i64 %a"), IR("%a = add i64 %a, 2 ret i64 %a"),
        IR("%a = add i64 %b, 2 %b = add i64 1, 2 ret i64 %a"),
        IR("%a = add i64 1, 2 %a = sub i64 2, 1 ret i64 %a"),
        IR("entry: %entry = add i64 1, 2 ret i64 %entry"),
        IR("%0 = add i64 1, 2 ret i64 %0"), IR("ret i64 %missing"), IR("ret i64 1"),
        IR("ret i64 9223372036854775808"), IR("ret i64 -9223372036854775809"),
        IR("ret i64 1.5"), IR("ret i64 +1"), IR("ret i64 undef"), IR("ret i64 poison"),
        IR("%v = add nuw i64 1, 2 ret i64 %v"), IR("%v = sdiv exact i64 5, 2 ret i64 %v"),
        IR("%v = call i64 @evil() ret i64 %v"), IR("%v = load i64, ptr null ret i64 %v"),
        IR("br label %entry"), IR("%v = mul i64 2, 3"),
        IR("%v = add i64 1, 2 ret i64 %v ret i64 %v"),
        IR("%v = add i64 1, 2 ret i64 %v") " trailing",
        IR("%v = add i64 1, 2 ret i64 %v") IR("%x = add i64 1, 2 ret i64 %x"),
        NULL
    };
    for (unsigned i=0;bad[i];i++) {
        assert(bm_ir_compile(bad[i],&p)==BM_IR_FORMAT && !p.bytes);
    }
    assert(bm_ir_compile("Hello!",&p)==BM_IR_NOT_CALL && !p.bytes);
    assert(bm_ir_compile("defined",&p)==BM_IR_NOT_CALL);
    assert(bm_ir_compile(IR("%v = sdiv nsw i64 -17, 5 ret i64 %v"),&p)==BM_IR_SDIV_FLAGS && !p.bytes);
    const char *full=IR("%a = add nsw i64 1, 100 %b = mul nsw i64 %a, 3 ret i64 %b");
    for (size_t n=0;n<strlen(full);n++) {
        char truncated[256]; memcpy(truncated,full,n); truncated[n]=0;
        assert(bm_ir_compile(truncated,&p)!=BM_IR_OK && !p.bytes);
    }
    char large[8192]; size_t used=(size_t)sprintf(large,"define i64 @calc() { ");
    for (unsigned i=0;i<BM_IR_OPS;i++) used+=(size_t)sprintf(large+used,"%%v%u = add i64 1, 2 ",i);
    strcpy(large+used,"ret i64 %v63 }");
    assert(bm_ir_compile(large,&p)==BM_IR_OK && p.operations==64 && p.input_count==256);
    strcpy(large+used,"%v64 = add i64 1, 2 ret i64 %v64 }");
    assert(bm_ir_compile(large,&p)==BM_IR_LIMIT && !p.bytes);
    assert(bm_ir_compile(IR("%abcdefghijklmnopqrstuvwxyzabcdef = add i64 1, 2 ret i64 1"),&p)==BM_IR_LIMIT);
    puts("LLVM IR: strict subset, SSA, bounds, complete functions, separate data, stable code PASS");
}
