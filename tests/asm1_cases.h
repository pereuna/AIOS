/* Independent expected values; exercised with the real ring3 compiler/runtime. */
static const struct { const char *source; uint32_t value; int status; } asm_examples[]={
    {"input 12 30; ld r0 0; ld r1 1; add r0 r1; exit r0; end",42,0},
    {"input 48 18; ld r0 0; ld r1 1; label l0; jz r1 l1; mov r2 r0; umod r2 r1; mov r0 r1; mov r1 r2; jmp l0; label l1; exit r0; end",6,0},
    {"input 8 9 10; exit r0",3,0},
    {"ld r0 255; exit r0",0,0},
    {"li r0 157; st 255 r0; li r0 0; ld r0 255; exit r0",157,0},
    {"li r0 207; exit r0",207,0},
    {"li r0 142; exit r0",142,0},
    {"li r0 4294967295; li r1 1; add r0 r1; exit r0",0,0},
    {"li r0 0; li r1 1; sub r0 r1; exit r0",UINT32_MAX,0},
    {"li r0 4294967295; li r1 2; mul r0 r1; exit r0",UINT32_MAX-1,0},
    {"li r0 4294967295; li r1 2; udiv r0 r1; exit r0",2147483647,0},
    {"li r0 4294967295; li r1 2; umod r0 r1; exit r0",1,0},
    {"li r0 4294967295; li r1 1; lt r0 r1; exit r0",0,0},
    {"li r0 1; li r1 4294967295; lt r0 r1; exit r0",1,0},
    {"li r0 2147483648; li r1 31; shr r0 r1; exit r0",1,0},
    {"li r0 7; li r1 32; shl r0 r1; exit r0",7,0},
    {"li r0 7; li r1 33; shr r0 r1; exit r0",3,0},
    {"li r0 99; li r1 0; udiv r0 r1; exit r0",0,BM_PROCESS_FAULT_BASE},
    {"li r0 99; li r1 0; umod r0 r1; exit r0",0,BM_PROCESS_FAULT_BASE},
    {"label l0; jmp l0; exit r0",0,BM_PROCESS_TIMEOUT},
    {"jmp l0; li r0 99; label l0; li r0 42; exit r0",42,0},
    {"li r0 0; jz r0 l0; li r0 99; label l0; exit r0",0,0},
    {"li r0 1; jz r0 l0; li r0 42; label l0; exit r0",42,0},
};
static void asm_append(char **p, const char *s) { while (*s) *(*p)++=*s++; **p=0; }
static void asm_reg(char **p, unsigned r) { *(*p)++='r'; *(*p)++=(char)('0'+r); **p=0; }
static uint32_t asm_reference(unsigned op, uint32_t a, uint32_t b) {
    switch (op) {
    case 0:return b; case 1:return a+b; case 2:return a-b; case 3:return a*b;
    case 4:return a/b; case 5:return a%b; case 6:return a&b; case 7:return a|b;
    case 8:return a^b; case 9:return a<<(b&31); case 10:return a>>(b&31);
    case 11:return a==b; default:return a<b;
    }
}
static void asm_cases(void (*check_asm)(const char *, uint32_t, int)) {
    for (unsigned i=0;i<sizeof(asm_examples)/sizeof(*asm_examples);i++)
        check_asm(asm_examples[i].source,asm_examples[i].value,asm_examples[i].status);
    const char *ops[]={"mov ","add ","sub ","mul ","udiv ","umod ","and ","or ","xor ","shl ","shr ","eq ","lt "};
    for (unsigned d=0;d<10;d++) {
        /* Every virtual register supports memory access and exit, including r4. */
        char src[256], *p=src;
        asm_append(&p,"li "); asm_reg(&p,d); asm_append(&p," 42; st 255 "); asm_reg(&p,d);
        asm_append(&p,"; li "); asm_reg(&p,d); asm_append(&p," 0; ld "); asm_reg(&p,d);
        asm_append(&p," 255; exit "); asm_reg(&p,d);
        check_asm(src,42,0);
        for (unsigned op=0;op<13;op++) for (unsigned same=0;same<2;same++) {
            unsigned s=same ? d : (d+3)%10;
            p=src;
            asm_append(&p,"li "); asm_reg(&p,d); asm_append(&p," 21; li "); asm_reg(&p,s);
            asm_append(&p,same ? " 21; " : " 4; ");
            asm_append(&p,ops[op]); asm_reg(&p,d); asm_append(&p," "); asm_reg(&p,s);
            asm_append(&p,"; exit "); asm_reg(&p,d);
            check_asm(src,asm_reference(op,21,same ? 21 : 4),0);
            if (!same) { p[-1]=(char)('0'+s); check_asm(src,4,0); } /* Source survives. */
        }
    }
    /* Explicitly cover the ModRM=cf combinations handled by the byte-filter workaround. */
    check_asm("li r4 21; mov r8 r4; exit r8",21,0);
    check_asm("li r4 3; li r8 7; mul r4 r8; exit r4",21,0);
}
