/* Real ring transitions, faults and firmware restoration in QEMU/OVMF. */
#include "../baremetal/runtime.h"
#include "../baremetal/process.h"
#include "../baremetal/process_console.h"
#include "../baremetal/calc.h"
#include "ir_cases.h"

static void log_line(const char *s) {
    while (*s) __asm__ volatile("outb %0,$0xe9" :: "a"(*s++));
    __asm__ volatile("outb %0,$0xe9" :: "a"((unsigned char)'\n'));
}
static _Noreturn void finish(unsigned code) {
    __asm__ volatile("outl %0,$0xf4" :: "a"(code));
    for (;;) __asm__ volatile("hlt");
}
static void require(int condition, const char *what) {
    if (!condition) { log_line(what); finish(0x11); }
}
int __real_bm_pages_alloc(size_t, uintptr_t *);
void __real_bm_pages_free(uintptr_t, size_t);
static struct { uintptr_t address; size_t pages; } allocations[32];
static unsigned live;
static int fail_after=-1;
int __wrap_bm_pages_alloc(size_t pages, uintptr_t *address) {
    if (fail_after==0) return -1;
    if (fail_after>0) fail_after--;
    int status=__real_bm_pages_alloc(pages,address);
    if (!status) {
        require(live<32,"FAIL: allocation tracking overflow");
        allocations[live].address=*address; allocations[live++].pages=pages;
    }
    return status;
}
void __wrap_bm_pages_free(uintptr_t address, size_t pages) {
    for (unsigned i=0;i<live;i++) if (allocations[i].address==address) {
        require(allocations[i].pages==pages,"FAIL: freed wrong page count");
        __real_bm_pages_free(address,pages);
        allocations[i]=allocations[--live]; return;
    }
    require(0,"FAIL: double/unknown free");
}
typedef struct __attribute__((packed)) { uint16_t limit; uint64_t base; } desc;
typedef struct {
    desc gdt,idt;
    uint64_t cr0,cr3,cr4,efer,fs,gs,flags;
    uint16_t tr,ss;
    uint32_t mxcsr;
    uint16_t fcw;
} snapshot;
static uint64_t msr(unsigned index) {
    unsigned lo,hi;
    __asm__ volatile("rdmsr" : "=a"(lo),"=d"(hi) : "c"(index));
    return (uint64_t)hi<<32|lo;
}
static snapshot state(void) {
    snapshot s;
    __asm__ volatile("sgdt %0; sidt %1; str %2; mov %%ss,%3"
        : "=m"(s.gdt),"=m"(s.idt),"=m"(s.tr),"=m"(s.ss));
    __asm__ volatile("mov %%cr0,%0; mov %%cr3,%1; mov %%cr4,%2; pushfq; popq %3"
        : "=r"(s.cr0),"=r"(s.cr3),"=r"(s.cr4),"=r"(s.flags));
    __asm__ volatile("stmxcsr %0; fnstcw %1" : "=m"(s.mxcsr),"=m"(s.fcw));
    s.efer=msr(0xc0000080); s.fs=msr(0xc0000100); s.gs=msr(0xc0000101);
    return s;
}
static void restored(snapshot a, snapshot b) {
    require(!memcmp(&a.gdt,&b.gdt,sizeof(desc)) && !memcmp(&a.idt,&b.idt,sizeof(desc)),
            "FAIL: descriptor tables changed");
    require(a.cr0==b.cr0 && a.cr3==b.cr3 && a.cr4==b.cr4 && a.efer==b.efer,
            "FAIL: control registers changed");
    require(a.fs==b.fs && a.gs==b.gs && a.ss==b.ss && (!a.tr || a.tr==b.tr),
            "FAIL: segments/TSS changed");
    require(!((a.flags^b.flags)&0x47700) && (b.flags&0x200), "FAIL: IF/TF/DF/IOPL/NT/AC changed");
    require(a.mxcsr==b.mxcsr && a.fcw==b.fcw,"FAIL: floating-point controls changed");
}
static unsigned cases;
enum { EITHER_SYSENTER_FAULT=-99 }; /* Intel #GP; AMD long mode #UD. */
static const unsigned char exit42[]={0xb8,42,0,0,0,0xcd,0x80};
static bm_process_result check(const char *name, const unsigned char *p, size_t n, int status) {
    log_line(name);
    snapshot before=state();
    bm_process_result r;
    int actual=bm_process_run(p,n,&r);
    snapshot after=state();
    require(!live,"FAIL: leaked process pages");
    if (status==EITHER_SYSENTER_FAULT &&
        (actual==BM_PROCESS_FAULT_BASE+6 || actual==BM_PROCESS_FAULT_BASE+13)) status=actual;
    if (actual!=status || r.status!=status) {
        bm_puts("status "); bm_hex((uint32_t)actual,8); bm_puts("; RIP ");
        bm_hex(r.rip,16); bm_puts("; vector "); bm_uint(r.fault_vector); bm_putc('\n');
        log_line("FAIL: unexpected process status"); finish(0x11);
    }
    restored(before,after);
    if (status>=BM_PROCESS_FAULT_BASE && status!=BM_PROCESS_TIMEOUT) {
        require(r.fault_vector==(unsigned)(status-BM_PROCESS_FAULT_BASE),"FAIL: vector");
        require((r.cs&3)==3,"FAIL: exception did not originate in ring3");
    }
    uint64_t began=bm_ticks;
    while (bm_ticks-began<2) __asm__ volatile("hlt"); /* Real firmware interrupts. */
    /* Print through firmware after every process, not just through debug I/O. */
    bm_puts("Process returned; firmware console/timer alive.\n");
    cases++;
    return r;
}
static void row_task(void *context, int first, int last) {
    unsigned *values=context;
    for (int i=first;i<last;i++) values[i]=(unsigned)i*3+1;
}
static void workers(void) {
    unsigned values[32]={0};
    bm_parallel_begin();
    bm_parallel_rows(row_task,values,32);
    bm_parallel_end();
    for (unsigned i=0;i<32;i++) require(values[i]==i*3+1,"FAIL: MP after process");
}
static void calculator(void) {
    static const struct { const char *text; int64_t answer; int status; } rows[]={
        {"/bytes 12 + 30 : 48 bb 00 20 00 00 00 40 00 00 48 63 03 48 63 4b 04 48 01 c8 cd 80",42,0},
        {"/bytes 17 - 25 : 48 bb 00 20 00 00 00 40 00 00 48 63 03 48 63 4b 04 48 29 c8 cd 80",-8,0},
        {"/bytes -157 * -207 : 48 bb 00 20 00 00 00 40 00 00 48 63 03 48 63 4b 04 48 0f af c1 cd 80",32499,0},
        {"/bytes -17 / 5 : 48 bb 00 20 00 00 00 40 00 00 48 63 03 48 63 4b 04 48 99 48 f7 f9 cd 80",-3,0},
        {"/bytes 2147483647 + 2147483647 : 48 bb 00 20 00 00 00 40 00 00 48 63 03 48 63 4b 04 48 01 c8 cd 80",4294967294ll,0},
        {"/bytes -2147483648 * -2147483648 : 48 bb 00 20 00 00 00 40 00 00 48 63 03 48 63 4b 04 48 0f af c1 cd 80",4611686018427387904ll,0},
        {"/bytes -2147483648 / -1 : 48 bb 00 20 00 00 00 40 00 00 48 63 03 48 63 4b 04 48 99 48 f7 f9 cd 80",2147483648ll,0},
        {"/bytes 7 / 0 : 48 bb 00 20 00 00 00 40 00 00 48 63 03 48 63 4b 04 48 99 48 f7 f9 cd 80",0,BM_PROCESS_FAULT_BASE},
    };
    for (unsigned i=0;i<sizeof(rows)/sizeof(*rows);i++) {
        snapshot before=state(); bm_calc_result result;
        require(bm_calc_execute(rows[i].text,NULL,&result)==rows[i].status,"FAIL: calc status");
        if (!rows[i].status) require(result.process.rax==(uint64_t)rows[i].answer,"FAIL: calc answer");
        restored(before,state()); require(!live,"FAIL: calc leaked pages"); workers();
    }
    log_line("CALC PASS: four operations, signed inputs, 64-bit results, division fault, firmware/MP restored");
    for (unsigned i=0;i<IR_CASE_COUNT;i++) {
        snapshot before=state(); bm_calc_result result;
        require(bm_calc_execute(ir_cases[i].source,NULL,&result)==ir_cases[i].status,"FAIL: LLVM IR status");
        if (!ir_cases[i].status) require(result.process.rax==ir_cases[i].answer,"FAIL: LLVM IR answer");
        restored(before,state()); require(!live,"FAIL: LLVM IR leaked pages"); workers();
    }
    log_line("LLVM IR PASS: SSA chains/reuse, i64 wrapping, nsw overflow, sdiv faults, firmware/MP restored");
}
/* Install a real busy TSS in a shadow firmware GDT, to exercise the case
 * which LTR(old_tr) alone cannot restore. Do not write to OVMF's GDT. */
static void busy_tss(void) {
    static uint64_t table[64] __attribute__((aligned(16)));
    static unsigned char tss[104] __attribute__((aligned(16)));
    desc original;
    __asm__ volatile("sgdt %0" : "=m"(original));
    require(original.limit+1u<=58*8,"FAIL: fixture GDT too large");
    memset(table,0,sizeof(table));
    memcpy(table,(void *)(uintptr_t)original.base,original.limit+1u);
    uintptr_t base=(uintptr_t)tss;
    table[58]=103|((base&0xffffffull)<<16)|(0x89ull<<40)|(((base>>24)&255)<<56);
    table[59]=base>>32;
    desc temporary={sizeof(table)-1,(uintptr_t)table};
    __asm__ volatile("lgdt %0; ltr %1" :: "m"(temporary),"r"((uint16_t)(58*8)) : "memory");
    bm_process_result r=check("busy firmware TSS",exit42,sizeof(exit42),0);
    require(r.rax==42 && ((table[58]>>40)&15)==11,"FAIL: original TSS busy bit changed");
    /* Keep this GDT/TSS alive until the test VM exits. */
}
_Noreturn void bm_main(void) {
    bm_init();
    bm_parallel_set_limit(BM_MAX_THREADS);
    workers();
    log_line("PROCESS BEGIN");
    calculator();
    bm_process_result r=check("exit-42",exit42,sizeof(exit42),0);
    require(r.rax==42 && r.steps==1,"FAIL: exit value/steps");
    unsigned char divide[]={0x31,0xc9,0xf7,0xf1};
    check("divide-zero",divide,sizeof(divide),BM_PROCESS_FAULT_BASE);
    unsigned char ud2[]={0x0f,0x0b},hlt[]={0xf4},cli[]={0xfa},sti[]={0xfb};
    check("invalid opcode",ud2,sizeof(ud2),BM_PROCESS_FAULT_BASE+6);
    check("HLT",hlt,sizeof(hlt),BM_PROCESS_FAULT_BASE+13);
    check("CLI",cli,sizeof(cli),BM_PROCESS_FAULT_BASE+13);
    check("STI",sti,sizeof(sti),BM_PROCESS_FAULT_BASE+13);
    unsigned char io[]={0xe6,0xe9},in[]={0xe4,0x80};
    check("OUT denied",io,sizeof(io),BM_PROCESS_FAULT_BASE+13);
    check("IN denied",in,sizeof(in),BM_PROCESS_FAULT_BASE+13);
    unsigned char null[]={0x31,0xc0,0x48,0x8b,0x00};
    r=check("null read",null,sizeof(null),BM_PROCESS_FAULT_BASE+14);
    require(r.address==0 && (r.error&4),"FAIL: user page fault data");
    /* MOVABS RAX,address; MOV [RAX],RBX (writes kernel data/code). */
    unsigned char memory[]={0x48,0xb8,0,0,0,0,0,0,0,0,0x48,0x89,0x18};
    static uint64_t canary=0x12345678;
    uint64_t address=(uintptr_t)&canary;
    memcpy(memory+2,&address,8);
    check("kernel write",memory,sizeof(memory),BM_PROCESS_FAULT_BASE+14);
    require(canary==0x12345678,"FAIL: kernel canary overwritten");
    address=BM_PROCESS_CODE; memcpy(memory+2,&address,8);
    r=check("code write",memory,sizeof(memory),BM_PROCESS_FAULT_BASE+14);
    require((r.error&7)==7,"FAIL: RX code permissions");
    /* MOVABS RAX,stack; JMP RAX: NX fault, not an instruction fetch. */
    unsigned char jump[]={0x48,0xb8,0,0,0,0,0,0,0,0,0xff,0xe0};
    address=BM_PROCESS_STACK; memcpy(jump+2,&address,8);
    r=check("NX stack",jump,sizeof(jump),BM_PROCESS_FAULT_BASE+14);
    require(r.error&16,"FAIL: NX bit not enforced");
    unsigned char stack[]={0x31,0xe4,0x50}; /* xor esp,esp; push rax */
    check("bad user stack",stack,sizeof(stack),BM_PROCESS_FAULT_BASE+14);
    unsigned char df[]={0xfd,0xcd,0x80},df_fault[]={0xfd,0x0f,0x0b};
    check("DF on exit",df,sizeof(df),0);
    check("DF on fault",df_fault,sizeof(df_fault),BM_PROCESS_FAULT_BASE+6);
    unsigned char dirty_fp[]={0x6a,0,0x0f,0xae,0x14,0x24,0xdb,0xe3,0xcd,0x80};
    uint32_t mxcsr=0x3f80; uint16_t fcw=0x077f;
    __asm__ volatile("ldmxcsr %0; fldcw %1" :: "m"(mxcsr),"m"(fcw));
    check("FP controls restored",dirty_fp,sizeof(dirty_fp),0);
    bm_fp_prepare();
    unsigned char syscall[]={0x0f,0x05},sysenter[]={0x0f,0x34};
    check("SYSCALL disabled",syscall,sizeof(syscall),BM_PROCESS_FAULT_BASE+6);
    check("SYSENTER disabled",sysenter,sizeof(sysenter),EITHER_SYSENTER_FAULT);
    unsigned char loop[]={0xeb,0xfe};
    r=check("infinite loop",loop,sizeof(loop),BM_PROCESS_TIMEOUT);
    require(r.steps==BM_PROCESS_STEP_LIMIT,"FAIL: step budget");
    unsigned char popf[]={0x9d},iret[]={0xcf},movss[]={0x8e,0xd0},
                  lss[]={0x0f,0xb2,0x00},xbegin[]={0xc7,0xf8,0,0,0,0};
    check("reject POPF",popf,sizeof(popf),BM_PROCESS_UNSAFE);
    check("reject IRET",iret,sizeof(iret),BM_PROCESS_UNSAFE);
    check("reject MOV SS",movss,sizeof(movss),BM_PROCESS_UNSAFE);
    check("reject LSS",lss,sizeof(lss),BM_PROCESS_UNSAFE);
    check("reject XBEGIN",xbegin,sizeof(xbegin),BM_PROCESS_UNSAFE);
    unsigned char nop[]={0x90};
    check("fall off fragment",nop,sizeof(nop),BM_PROCESS_FAULT_BASE+3);
    for (unsigned i=0;i<16;i++) {
        r=check("repeat exit after faults",exit42,sizeof(exit42),0);
        require(r.rax==42,"FAIL: repeat exit");
    }
    bm_avx2_scope scope;
    if (bm_avx2_begin(&scope)) {
        r=check("restore AVX-enabled firmware",exit42,sizeof(exit42),0);
        require(r.rax==42 && bm_cpu_avx2_available(),"FAIL: AVX state lost");
        bm_avx2_end(&scope);
    }
    busy_tss();
    for (int i=0;i<32;i++) {
        fail_after=i;
        snapshot before=state();
        int status=bm_process_run(exit42,sizeof(exit42),&r);
        restored(before,state());
        require(!live,"FAIL: leaked pages on allocation failure");
        require(status==0 || status==BM_PROCESS_NO_MEMORY,"FAIL: allocation-failure status");
        if (!status) break;
        require(i!=31,"FAIL: never recovered from allocation failures");
    }
    fail_after=-1;
    log_line("allocation failures: cleanup and firmware state restored");
    check("null program",NULL,1,BM_PROCESS_INVALID);
    check("empty program",exit42,0,BM_PROCESS_INVALID);
    check("oversize program",exit42,BM_PROCESS_MAX_CODE+1,BM_PROCESS_INVALID);
    workers();
    require(bm_process_command("/run add 12 30"),"FAIL: console command dispatch");
    require(bm_process_command("/run tests"),"FAIL: console tests dispatch");
    check("exit after console tests",exit42,sizeof(exit42),0);
    require(!live,"FAIL: console leaked pages");
    float value=bm_sqrtf(4.0f);
    require(value==2.0f,"FAIL: SSE2 math after process");
    bm_puts("Process cases: "); bm_uint(cases); bm_putc('\n');
    log_line("PROCESS PASS: faults, budget, repeat, FP, firmware timer, MP, console");
    finish(0x10);
}
