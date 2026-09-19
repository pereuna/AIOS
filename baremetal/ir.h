/* A deliberately small, valid subset of textual LLVM SSA IR. */
#ifndef AIOS_IR_H
#define AIOS_IR_H
#include "process.h"

enum { BM_IR_OPS=64, BM_IR_CONSTANTS=128, BM_IR_NAME=32 };
enum { BM_IR_NOT_CALL=0, BM_IR_OK=1, BM_IR_FORMAT=-310, BM_IR_LIMIT=-311,
       BM_IR_SDIV_FLAGS=-312 };
typedef struct {
    unsigned char code[BM_PROCESS_MAX_CODE];
    size_t bytes;
    uint32_t input[BM_IR_CONSTANTS*2];
    unsigned input_count, operations;
    /* Only a single literal binary expression returned unchanged qualifies. */
    int literal;
    int64_t a,b;
    char op;
} bm_ir_program;

/* define i64 @calc() { [entry:] %name = add [nsw] i64 ... ret i64 %name }
 * Also sub, mul, sdiv; named SSA values, signed decimal i64 constants.
 * No evaluation/constant folding here: all arithmetic runs in the process.
 * Constants live in NX input memory; emitted code depends on structure only.
 * Non-success always leaves bytes=0. */
int bm_ir_compile(const char *,bm_ir_program *);
#endif
