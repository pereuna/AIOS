#ifndef SMOL_AGENT_H
#define SMOL_AGENT_H
#include "asm1_tool.h"

enum { AGENT_MAX_CALLS=3, AGENT_FINAL_TOKENS=32, AGENT_CONTEXT_RESERVE=320 };
enum { AGENT_END, AGENT_TOKEN_LIMIT, AGENT_CONTEXT_LIMIT, AGENT_OUTPUT_LIMIT,
       AGENT_INVALID_TOKEN, AGENT_CALL_LIMIT };
typedef struct {
    char text[ASM1_MAX_SOURCE];
    unsigned tokens;
    int stop;
} agent_reply;
typedef struct {
    void *context;
    /* Produces one assistant message, preserving its KV context. */
    void (*generate)(void *, unsigned budget, unsigned reserve, agent_reply *);
    /* Adds trusted runtime feedback and opens the next assistant turn.
     * Returns 0 without changing context if it cannot fit, 1 to continue,
     * or 2 if only a final answer fits (further calls are disabled). */
    int (*feedback)(void *, const char *text, int final_only);
    void (*display)(void *, const char *text, int tool);
} agent_io;
typedef struct { unsigned tokens, calls; int stop; } agent_result;

agent_result agent_run(const agent_io *io, unsigned token_budget);
const char *agent_stop(int stop);
#endif
