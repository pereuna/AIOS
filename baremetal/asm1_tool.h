#ifndef SMOL_ASM1_TOOL_H
#define SMOL_ASM1_TOOL_H
#include "asm1.h"

enum { ASM1_FEEDBACK_SIZE=192 };
typedef struct {
    int compile_status;
    unsigned error_line;
    int executed;
    bm_process_result process;
} asm1_result;

/* No console output. Both manual commands and the agent use this path. */
void asm1_execute(const char *source, int require_end, asm1_result *result);
void asm1_feedback(const asm1_result *result, char text[ASM1_FEEDBACK_SIZE]);
#endif
