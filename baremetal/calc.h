#ifndef AIOS_CALC_H
#define AIOS_CALC_H
#include "process.h"
#include "ir.h"

enum { BM_CALC_TEXT = 4096, BM_CALC_CODE = 32, BM_CALC_FEEDBACK = 256 };
enum { BM_CALC_NOT_CALL = 0, BM_CALC_CALL = 1, BM_CALC_FORMAT = -300,
       BM_CALC_MISMATCH = -301, BM_CALC_OVERFLOW = -302, BM_CALC_DIVISION = -303 };
typedef struct { int64_t a, b; char op; } bm_calc_expression;
typedef struct {
    bm_calc_expression expression;
    unsigned char code[BM_CALC_CODE];
    size_t bytes;
    bm_process_result process;
    int status;
    int is_ir;
    bm_ir_program ir;
} bm_calc_result;

/* Parse only a literal two-operand expression; never evaluates arithmetic. */
int bm_calc_parse_expression(const char *, bm_calc_expression *);
/* Model output is a complete LLVM IR function. Legacy /bytes is retained
 * for the original raw-byte experiment, but not advertised to the model. */
int bm_calc_parse_call(const char *, bm_calc_result *);
/* Compile/check the program and optional literal /calc request, then run
 * arithmetic in ring3. Legacy /bytes calls still pass their bytes unchanged. */
int bm_calc_execute(const char *, const bm_calc_expression *, bm_calc_result *);
void bm_calc_feedback(const bm_calc_result *, char [BM_CALC_FEEDBACK]);

#endif
