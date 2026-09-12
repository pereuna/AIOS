#ifndef SMOL_PROCESS_H
#define SMOL_PROCESS_H

#define BM_PROCESS_CODE 0x0000400000000000
#define BM_PROCESS_STACK 0x0000400000002000
#define BM_PROCESS_MAX_CODE 4096
#define BM_PROCESS_STEP_LIMIT 100000
#define BM_PROCESS_TIMEOUT 0x180
#ifndef __ASSEMBLER__
#include <stddef.h>
#include <stdint.h>

/* A process is deliberately one-shot for now.  The caller owns the program
 * bytes; bm_process_run copies them into a non-writable user page. */
enum {
    BM_PROCESS_OK = 0,
    BM_PROCESS_INVALID = -1,
    BM_PROCESS_NO_MEMORY = -2,
    BM_PROCESS_UNSAFE = -3,
    BM_PROCESS_UNSUPPORTED = -4,
    BM_PROCESS_BUSY = -5,
    BM_PROCESS_INTERNAL = -6,
    BM_PROCESS_FAULT_BASE = 0x100
};

typedef struct {
    int status;
    unsigned fault_vector;
    uint64_t steps;
    uint64_t rax;
    uint64_t rip, error, address, cs;
} bm_process_result;

/* BSP only: execute restricted x86-64 bytes in a fresh ring3 address space.
 * INT 80 exits with RAX; faults/budget exhaustion return a structured result.
 * User registers and x87/SSE2 state start clean. No user-accessible kernel
 * mappings. AVX and TF-suppressing instructions are unavailable in this
 * version; see docs/process.md. CPU modes we cannot restore fail closed. */
int bm_process_run(const void *, size_t, bm_process_result *);
int bm_process_run_input(const void *, size_t, const uint32_t *, size_t, bm_process_result *);
int bm_process_validate(const void *, size_t);

#endif
#endif
