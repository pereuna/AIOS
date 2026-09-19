/* Shared by host parser tests, the LLVM oracle and actual QEMU ring3 tests. */
#ifndef AIOS_IR_CASES_H
#define AIOS_IR_CASES_H
#include "../baremetal/calc.h"
#define IR(body) "define i64 @calc() { " body " }"
static const struct { const char *source; uint64_t answer; int status; } ir_cases[]={
    {IR("%sum = add nsw i64 1, 100 ret i64 %sum"),101,0},
    {IR("entry: %v0 = sub nsw i64 17, 25 ret i64 %v0"),(uint64_t)-8,0},
    {IR("%v0 = mul nsw i64 -157, -207 ret i64 %v0"),32499,0},
    {IR("%v0 = sdiv i64 -17, 5 ret i64 %v0"),(uint64_t)-3,0},
    {IR("%v0 = add nsw i64 1, 100 %v1 = mul nsw i64 %v0, 3 ret i64 %v1"),303,0},
    {IR("%a = add i64 12, 3 %b = sub i64 17, 7 %c = mul i64 %a, %b ret i64 %c"),150,0},
    {IR("%a = sub i64 3, 20 %b = sdiv i64 %a, -2 %c = sub i64 1, %b ret i64 %c"),(uint64_t)-7,0},
    {IR("%a = add i64 2, 3 %b = mul i64 %a, %a %c = add i64 %b, %a ret i64 %c"),30,0},
    {IR("%a = add i64 2, 3 %b = mul i64 %a, 9 ret i64 %a"),5,0},
    {IR("%v = add i64 9223372036854775807, 1 ret i64 %v"),UINT64_C(0x8000000000000000),0},
    {IR("%v = sub i64 -9223372036854775808, 1 ret i64 %v"),INT64_MAX,0},
    {IR("%v = mul i64 9223372036854775807, 2 ret i64 %v"),(uint64_t)-2,0},
    {IR("%v = mul nsw i64 -2147483648, -2147483648 ret i64 %v"),UINT64_C(4611686018427387904),0},
    {IR("%v = sdiv i64 -2147483648, -1 ret i64 %v"),UINT64_C(2147483648),0},
    {IR("%v = add i64 -9223372036854775808, 0 ret i64 %v"),UINT64_C(0x8000000000000000),0},
    {"; LLVM comment\ndefine i64 @calc() {\nentry:\n %foo.bar = add i64 142, 207 ; data\n ret i64 %foo.bar\n}\n",349,0},
    {IR("%v = add nsw i64 9223372036854775807, 1 ret i64 %v"),0,BM_CALC_OVERFLOW},
    {IR("%v = sub nsw i64 -9223372036854775808, 1 ret i64 %v"),0,BM_CALC_OVERFLOW},
    {IR("%v = mul nsw i64 9223372036854775807, 2 ret i64 %v"),0,BM_CALC_OVERFLOW},
    {IR("%v = sdiv i64 7, 0 ret i64 %v"),0,BM_CALC_DIVISION},
    {IR("%v = sdiv i64 -9223372036854775808, -1 ret i64 %v"),0,BM_CALC_DIVISION},
    {IR("%a = sub i64 9, 9 %v = sdiv i64 7, %a ret i64 %v"),0,BM_CALC_DIVISION},
    {IR("%a = mul nsw i64 2147483647, 2147483647 %v = mul nsw i64 %a, 3 ret i64 %v"),0,BM_CALC_OVERFLOW},
};
#define IR_CASE_COUNT (sizeof(ir_cases)/sizeof(*ir_cases))
#endif
