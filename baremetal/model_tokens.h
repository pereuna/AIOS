#ifndef AIOS_MODEL_TOKENS_H
#define AIOS_MODEL_TOKENS_H
enum {
    MODEL_BOS=151643, MODEL_EOS=151645,
    MODEL_ADDED_START=151643, MODEL_ADDED_COUNT=22,
    MODEL_SPECIAL_END=151657, MODEL_TOKEN_END=151665
};
static inline int model_token_end(int token) {
    return token==MODEL_EOS || token==MODEL_BOS;
}
static inline int model_token_text(int token) {
    return (unsigned)token<MODEL_ADDED_START ||
        (token>=MODEL_SPECIAL_END && token<MODEL_TOKEN_END);
}
#endif
