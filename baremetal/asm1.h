#ifndef SMOL_ASM1_H
#define SMOL_ASM1_H

#include <stddef.h>
#include <stdint.h>
#include "process.h"

#define ASM1_MAX_SOURCE 4096
#define ASM1_MAX_INPUT 64
#define ASM1_MAX_INSTRUCTIONS 128

enum {
    ASM1_OK = 0,
    ASM1_SOURCE = -200,
    ASM1_SYNTAX = -201,
    ASM1_REGISTER = -202,
    ASM1_NUMBER = -203,
    ASM1_LABEL = -204,
    ASM1_DUPLICATE = -205,
    ASM1_NO_EXIT = -206,
    ASM1_TOO_LARGE = -207
};

typedef struct {
    unsigned char code[BM_PROCESS_MAX_CODE];
    size_t code_size;
    uint32_t input[ASM1_MAX_INPUT];
    size_t input_count;
    int error;
    unsigned error_line;
} asm1_program;

int asm1_compile(const char *source, asm1_program *out);
const char *asm1_error(int code);

#endif
