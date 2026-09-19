#include "runtime.h"
#include "ir.h"

_Static_assert(BM_PROCESS_STACK==UINT64_C(0x400000002000),"IR data ABI");
_Static_assert(BM_IR_CONSTANTS*2<=256,"process input limit");
_Static_assert((BM_IR_CONSTANTS+BM_IR_OPS)*8<=4096,"IR slots fit the first data page");

typedef struct {
    const char *s;
    bm_ir_program *p;
    char names[BM_IR_OPS][BM_IR_NAME];
    int error;
} parser;
static int letter(char c) {
    return (c>='a' && c<='z') || (c>='A' && c<='Z') || c=='_' || c=='.' || c=='$' || c=='-';
}
static int digit(char c) { return c>='0' && c<='9'; }
static int word(char c) { return letter(c) || digit(c); }
static void skip(parser *r) {
    for (;;) {
        while (*r->s==' ' || *r->s=='\t' || *r->s=='\r' || *r->s=='\n') r->s++;
        if (*r->s!=';') return;
        while (*r->s && *r->s!='\n') r->s++;
    }
}
static int token(parser *r,const char *text) {
    skip(r);
    const char *s=r->s,*t=text;
    while (*t && *s==*t) { s++; t++; }
    if (*t || (word(text[strlen(text)-1]) && word(*s))) return 0;
    r->s=s; return 1;
}
static int name(parser *r,char text[BM_IR_NAME]) {
    if (!token(r,"%") || !letter(*r->s)) return 0;
    unsigned n=0;
    while (word(*r->s)) {
        if (n+1==BM_IR_NAME) { r->error=BM_IR_LIMIT; return 0; }
        text[n++]=*r->s++;
    }
    text[n]=0; return 1;
}
static int lookup(const parser *r,const char *text) {
    for (unsigned i=0;i<r->p->operations;i++)
        if (!strcmp(text,r->names[i])) return (int)(BM_IR_CONSTANTS+i);
    return -1;
}
static int reference(parser *r,unsigned *slot) {
    char text[BM_IR_NAME];
    if (!name(r,text)) return 0;
    int index=lookup(r,text);
    if (index<0) return 0;
    *slot=(unsigned)index; return 1;
}
static int operand(parser *r,unsigned *slot) {
    skip(r);
    if (*r->s=='%') return reference(r,slot); /* No forward/self/undefined refs. */
    int negative=*r->s=='-';
    if (negative) r->s++;
    if (!digit(*r->s)) return 0;
    uint64_t value=0,maximum=negative ? UINT64_C(0x8000000000000000) : INT64_MAX;
    while (digit(*r->s)) {
        unsigned d=(unsigned)(*r->s++-'0');
        if (value>(maximum-d)/10) return 0;
        value=value*10+d;
    }
    if (word(*r->s)) return 0;
    if (negative) value=0-value;
    bm_ir_program *p=r->p;
    if (p->input_count==BM_IR_CONSTANTS*2) { r->error=BM_IR_LIMIT; return 0; }
    *slot=p->input_count/2;
    p->input[p->input_count++]=(uint32_t)value;
    p->input[p->input_count++]=(uint32_t)(value>>32);
    return 1;
}
static int64_t constant(const bm_ir_program *p,unsigned slot) {
    uint64_t bits=(uint64_t)p->input[slot*2+1]<<32|p->input[slot*2];
    int64_t value; memcpy(&value,&bits,sizeof(value)); return value;
}
static void emit(parser *r,const unsigned char *code,size_t n) {
    bm_ir_program *p=r->p;
    if (n>sizeof(p->code)-p->bytes) { r->error=BM_IR_LIMIT; return; }
    memcpy(p->code+p->bytes,code,n); p->bytes+=n;
}
/* Every slot has an 8-byte-aligned, bounded displacement. Constants never
 * become instruction immediates, including values resembling POPF/IRET.
 * These encodings and displacements pass the existing all-byte TF filter. */
static void memory(parser *r,unsigned opcode,unsigned modrm,unsigned slot) {
    unsigned offset=slot*8;
    unsigned char code[]={0x48,(unsigned char)opcode,(unsigned char)modrm,
        (unsigned char)offset,(unsigned char)(offset>>8),0,0};
    emit(r,code,sizeof(code));
}
static int body(parser *r) {
    bm_ir_program *p=r->p;
    if (!token(r,"i64") || !token(r,"@calc") || !token(r,"(") ||
        !token(r,")") || !token(r,"{")) return 0;
    /* LLVM lexes a label and its colon as one token: "entry :" is invalid. */
    (void)token(r,"entry:");
    static const unsigned char prefix[]={0x48,0xbb,0,0x20,0,0,0,0x40,0,0};
    emit(r,prefix,sizeof(prefix));
    for (;;) {
        if (token(r,"ret")) {
            unsigned result;
            if (!token(r,"i64") || !reference(r,&result) || !token(r,"}")) return 0;
            skip(r); if (*r->s) return 0; /* Exactly one complete function. */
            p->literal=p->literal && p->operations==1 && result==BM_IR_CONSTANTS;
            memory(r,0x8b,0x83,result); /* mov rax,[rbx+result] */
            static const unsigned char exit[]={0xcd,0x80};
            emit(r,exit,sizeof(exit));
            return !r->error;
        }
        if (p->operations==BM_IR_OPS) { r->error=BM_IR_LIMIT; return 0; }
        char dest[BM_IR_NAME]; unsigned a,b;
        if (!name(r,dest) || !strcmp(dest,"entry") || lookup(r,dest)>=0 || !token(r,"=")) return 0;
        char op=token(r,"add") ? '+' : token(r,"sub") ? '-' :
                token(r,"mul") ? '*' : token(r,"sdiv") ? '/' : 0;
        if (!op) return 0;
        if (op=='/' && token(r,"nsw")) { r->error=BM_IR_SDIV_FLAGS; return 0; }
        int nsw=op!='/' && token(r,"nsw");
        if (!token(r,"i64") || !operand(r,&a) || !token(r,",") || !operand(r,&b)) return 0;
        if (!p->operations && a<BM_IR_CONSTANTS && b<BM_IR_CONSTANTS) {
            p->literal=1; p->a=constant(p,a); p->b=constant(p,b); p->op=op;
        }
        memory(r,0x8b,0x83,a); /* mov rax,[rbx+a] */
        memory(r,0x8b,0x8b,b); /* mov rcx,[rbx+b] */
        static const unsigned char add[]={0x48,0x01,0xc8},sub[]={0x48,0x29,0xc8},
            mul[]={0x48,0x0f,0xaf,0xc1},div[]={0x48,0x99,0x48,0xf7,0xf9},
            check[]={0x71,0x02,0x0f,0x0b}; /* jno +2; ud2 on nsw overflow */
        switch (op) {
        case '+': emit(r,add,sizeof(add)); break;
        case '-': emit(r,sub,sizeof(sub)); break;
        case '*': emit(r,mul,sizeof(mul)); break;
        case '/': emit(r,div,sizeof(div)); break;
        }
        if (nsw) emit(r,check,sizeof(check));
        memory(r,0x89,0x83,BM_IR_CONSTANTS+p->operations); /* store SSA result */
        memcpy(r->names[p->operations],dest,strlen(dest)+1);
        p->operations++;
        if (r->error) return 0;
    }
}
int bm_ir_compile(const char *text,bm_ir_program *p) {
    if (!p) return BM_IR_FORMAT;
    memset(p,0,sizeof(*p));
    if (!text) return BM_IR_FORMAT;
    parser r={.s=text,.p=p};
    if (!token(&r,"define")) return BM_IR_NOT_CALL;
    if (body(&r)) return BM_IR_OK;
    p->bytes=0;
    return r.error ? r.error : BM_IR_FORMAT;
}
