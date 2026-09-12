#include "runtime.h"
#include "agent.h"

agent_result agent_run(const agent_io *io, unsigned budget) {
    agent_result result={0};
    int final_only=budget<=AGENT_FINAL_TOKENS;
    for (;;) {
        unsigned left=budget-result.tokens;
        if (!left) { result.stop=AGENT_TOKEN_LIMIT; break; }
        agent_reply reply={0};
        io->generate(io->context,final_only ? left : left-AGENT_FINAL_TOKENS,
                     final_only ? 2 : AGENT_CONTEXT_RESERVE,&reply);
        result.tokens+=reply.tokens;
        int tool=!memcmp(reply.text,"/asm ",5);
        if (!tool) {
            io->display(io->context,reply.text,0);
            result.stop=reply.stop;
            break;
        }
        /* An end directive alone is insufficient: require the model's EOS too.
         * No execution after output truncation, token/context limit or bad token. */
        if (reply.stop!=AGENT_END) { result.stop=reply.stop; break; }
        if (final_only || result.calls==AGENT_MAX_CALLS) { result.stop=AGENT_CALL_LIMIT; break; }
        asm1_result execution;
        char text[ASM1_FEEDBACK_SIZE];
        asm1_execute(reply.text+5,1,&execution);
        result.calls++; /* Includes malformed calls: repairs also have a budget. */
        asm1_feedback(&execution,text);
        io->display(io->context,text,1);
        final_only=result.calls==AGENT_MAX_CALLS || budget-result.tokens<=AGENT_FINAL_TOKENS;
        int next=io->feedback(io->context,text,final_only);
        if (!next) { result.stop=AGENT_CONTEXT_LIMIT; break; }
        if (next==2) final_only=1;
    }
    return result;
}
const char *agent_stop(int stop) {
    switch (stop) {
    case AGENT_END:return "complete";
    case AGENT_TOKEN_LIMIT:return "token limit reached";
    case AGENT_CONTEXT_LIMIT:return "context limit reached";
    case AGENT_OUTPUT_LIMIT:return "output too long; tool not executed";
    case AGENT_INVALID_TOKEN:return "invalid control token; tool not executed";
    default:return "tool call limit reached";
    }
}
