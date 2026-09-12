#include "runtime.h"
#include "process.h"
#include "process_console.h"
#include "asm1_tool.h"

static const struct {
    const char *name, *description;
    unsigned char code[16];
    size_t bytes;
    int expected;
} demos[]={
    {"ok","mov eax,42; int 0x80",{0xb8,42,0,0,0,0xcd,0x80},7,BM_PROCESS_OK},
    {"div0","divide by zero",{0x31,0xc9,0xf7,0xf1},4,BM_PROCESS_FAULT_BASE},
    {"ud2","invalid instruction",{0x0f,0x0b},2,BM_PROCESS_FAULT_BASE+6},
    {"memory","read unmapped address zero",{0x31,0xc0,0x48,0x8b,0x00},5,BM_PROCESS_FAULT_BASE+14},
    {"hlt","privileged instruction",{0xf4},1,BM_PROCESS_FAULT_BASE+13},
    {"loop","infinite loop (stopped at the step limit)",{0xeb,0xfe},2,BM_PROCESS_TIMEOUT},
};
static void help(void) {
    bm_puts("/run                return 42 from ring3\n"
            "/run add 12 30      add two unsigned 32-bit numbers\n"
            "/run div0           test division by zero\n"
            "/run ud2            test an invalid instruction\n"
            "/run memory         test memory protection\n"
            "/run hlt            test a privileged instruction\n"
            "/run loop           test the instruction limit\n"
            "/run tests          run all six checks\n"
            "/run help           show these commands\n"
            "/exec HEX           advanced: raw machine code, exit appended\n"
            "/asm SOURCE         compile and run asm1 (use ';' for newlines)\n");
}
static int hex_digit(char c) {
    if (c>='0' && c<='9') return c-'0';
    if (c>='a' && c<='f') return c-'a'+10;
    if (c>='A' && c<='F') return c-'A'+10;
    return -1;
}
static int space(char c) { return c==' ' || c=='\t'; }
int bm_process_parse_hex(const char *s, unsigned char *program, size_t cap) {
    if (!s || !program || cap<3 || cap>BM_PROCESS_MAX_CODE) return -1;
    size_t n=0;
    while (*s) {
        while (space(*s)) s++;
        if (!*s) break;
        int hi=hex_digit(*s++);
        if (hi<0 || !*s) return -1; /* Check NUL before reading the low digit. */
        int lo=hex_digit(*s++);
        if (lo<0 || n>=cap-2) return -1;
        program[n++]=(unsigned char)(hi*16+lo);
    }
    if (!n) return -1;
    program[n++]=0xcd; program[n++]=0x80;
    return (int)n;
}
static const char *fault_name(unsigned vector) {
    switch (vector) {
    case 0: return "divide by zero (#DE)";
    case 3: return "breakpoint/end of code (#BP)";
    case 6: return "invalid/unsupported instruction (#UD)";
    case 12: return "stack fault (#SS)";
    case 13: return "privilege/protection fault (#GP)";
    case 14: return "memory protection fault (#PF)";
    default: return "processor exception";
    }
}
static int run(const char *description, const unsigned char *program, size_t bytes,
               bm_process_result *result) {
    bm_puts("Running ring3: "); bm_puts(description); bm_putc('\n');
    int status=bm_process_run(program,bytes,result);
    bm_puts("ring3: ");
    if (status==BM_PROCESS_OK) bm_puts("exit");
    else if (status==BM_PROCESS_TIMEOUT) bm_puts("stopped: instruction limit reached");
    else if (status>=BM_PROCESS_FAULT_BASE) {
        bm_puts(fault_name(result->fault_vector));
        bm_puts("; vector "); bm_uint(result->fault_vector);
    } else {
        switch (status) {
        case BM_PROCESS_UNSAFE: bm_puts("rejected: bytes may disable single-stepping"); break;
        case BM_PROCESS_UNSUPPORTED: bm_puts("unsupported CPU/firmware state"); break;
        case BM_PROCESS_NO_MEMORY: bm_puts("not enough process memory"); break;
        case BM_PROCESS_BUSY: bm_puts("a process is already running"); break;
        case BM_PROCESS_INTERNAL: bm_puts("kernel transition fault"); break;
        default: bm_puts("invalid program"); break;
        }
    }
    bm_puts("; RAX="); bm_uint(result->rax); bm_puts(" (0x"); bm_hex(result->rax,16);
    bm_puts("); steps="); bm_uint(result->steps);
    if (status>=BM_PROCESS_FAULT_BASE && status!=BM_PROCESS_TIMEOUT) {
        bm_puts("; RIP=0x"); bm_hex(result->rip,16);
        bm_puts("; error=0x"); bm_hex(result->error,8);
        if (result->fault_vector==14) { bm_puts("; address=0x"); bm_hex(result->address,16); }
    }
    bm_putc('\n');
    return status;
}
static void run_asm(const char *source) {
    asm1_result r; char text[ASM1_FEEDBACK_SIZE];
    asm1_execute(source,0,&r);
    asm1_feedback(&r,text);
    bm_puts("asm1: "); bm_puts(text); bm_putc('\n');
}
/* A command must end or have a separator: /runner is not /run. */
static const char *argument(const char *line, const char *command) {
    size_t n=strlen(command);
    if (strlen(line)<n || memcmp(line,command,n) || (line[n] && !space(line[n]))) return NULL;
    line+=n;
    while (space(*line)) line++;
    return line;
}
static int number(const char **input, uint32_t *result) {
    const char *s=*input;
    while (space(*s)) s++;
    if (*s<'0' || *s>'9') return -1;
    uint32_t value=0;
    do {
        unsigned digit=(unsigned)(*s++-'0');
        if (value>(UINT32_MAX-digit)/10) return -1;
        value=value*10+digit;
    } while (*s>='0' && *s<='9');
    if (*s && !space(*s)) return -1;
    *input=s; *result=value;
    return 0;
}
int bm_process_command(const char *line) {
    const char *args=argument(line,"/run");
    unsigned char program[BM_PROCESS_MAX_CODE];
    bm_process_result result;
    if (!args) {
        args=argument(line,"/asm");
        if (args) { run_asm(args); return 1; }
        args=argument(line,"/exec");
        if (!args) args=argument(line,"/excec"); /* Common spelling from the console. */
        if (!args) return 0;
        int bytes=bm_process_parse_hex(args,program,sizeof(program));
        if (bytes<0) bm_puts("Use /run help, or /exec with complete hex byte pairs.\n");
        else run("raw machine code",program,(size_t)bytes,&result);
        return 1;
    }
    if (!strcmp(args,"help")) { help(); return 1; }
    if (!strcmp(args,"tests")) {
        unsigned passed=0;
        for (unsigned i=0;i<sizeof(demos)/sizeof(*demos);i++) {
            int status=run(demos[i].description,demos[i].code,demos[i].bytes,&result);
            int ok=status==demos[i].expected && (i || result.rax==42);
            passed+=(unsigned)ok;
            bm_puts(ok ? "PASS\n" : "FAIL\n");
        }
        bm_puts("Process checks: "); bm_uint(passed); bm_puts("/6 passed.\n");
        return 1;
    }
    const char *add=argument(args,"add");
    if (add) {
        uint32_t a,b;
        if (number(&add,&a) || number(&add,&b)) {
            bm_puts("Use /run add A B (0..4294967295; 32-bit wraparound).\n"); return 1;
        }
        while (space(*add)) add++;
        if (*add) { bm_puts("Use /run add A B\n"); return 1; }
        /* Build constants nibble by nibble so every uint32 is usable even
         * when its immediate bytes would trip the conservative TF filter. */
        size_t n=0;
        for (unsigned operand=0;operand<2;operand++) {
            uint32_t value=operand ? b : a;
            program[n++]=0x31; program[n++]=0xc0; /* xor eax,eax */
            for (int shift=28;shift>=0;shift-=4) {
                program[n++]=0xc1; program[n++]=0xe0; program[n++]=4; /* shl eax,4 */
                program[n++]=0x0c; program[n++]=(unsigned char)(value>>shift&15); /* or al,N */
            }
            if (!operand) { program[n++]=0x89; program[n++]=0xc1; } /* mov ecx,eax */
        }
        program[n++]=0x01; program[n++]=0xc8; /* add eax,ecx */
        program[n++]=0xcd; program[n++]=0x80;
        run("32-bit addition",program,n,&result);
        return 1;
    }
    for (unsigned i=0;i<sizeof(demos)/sizeof(*demos);i++)
        if ((!*args && !i) || !strcmp(args,demos[i].name)) {
            run(demos[i].description,demos[i].code,demos[i].bytes,&result);
            return 1;
        }
    bm_puts("Unknown process test.\n"); help(); return 1;
}
