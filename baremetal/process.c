/* BSP-only instruction sandbox; no firmware calls while our IDT is active. */
#include <cpuid.h>
#include "runtime.h"
#include "process.h"

#define PAGE 4096u
#define P 1ull
#define W 2ull
#define U 4ull
#define NX (1ull<<63)
#define ADDRESS 0x000ffffffffff000ull
enum { GDT_SLOTS=64, USER_CS=60*8+3, USER_SS=61*8+3, TSS_SELECTOR=62*8,
       MAX_ALLOCS=32, STACK_PAGES=2 };

typedef struct __attribute__((packed)) { uint16_t limit; uintptr_t base; } descriptor_ptr;
typedef struct __attribute__((packed)) {
    uint16_t low, selector;
    uint8_t ist, attributes;
    uint16_t mid;
    uint32_t high, reserved;
} idt_gate;
typedef struct __attribute__((packed)) {
    uint32_t reserved0;
    uint64_t rsp[3], reserved1, ist[7], reserved2;
    uint16_t reserved3, iomap;
} tss64;
_Static_assert(sizeof(tss64)==104, "architectural 64-bit TSS size");
_Static_assert(offsetof(tss64,ist)==36, "architectural IST offset");
_Static_assert(offsetof(tss64,iomap)==102, "architectural I/O map offset");
_Static_assert(sizeof(idt_gate)==16, "64-bit IDT gate");

/* Assembly accesses only supervisor-mapped image text/data. */
uint64_t bm_process_reason, bm_process_steps, bm_process_user_rax;
uint64_t bm_process_rip, bm_process_error, bm_process_address, bm_process_cs;
uint64_t bm_process_kernel_cr3, bm_process_saved_rsp, bm_process_user_cr3;
uint16_t bm_process_kernel_cs, bm_process_kernel_ss;
static uint64_t gdt[GDT_SLOTS] __attribute__((aligned(16)));
static idt_gate idt[256] __attribute__((aligned(16)));
static tss64 tss __attribute__((aligned(16)));
static int active, fallback_tr;
extern char _text[], _etext[], _data[], _edata[];
extern unsigned char bm_process_trap_stack[], bm_process_emergency_stack[];
extern const uintptr_t bm_process_exception_table[32];
extern void bm_process_enter(void), bm_process_syscall(void), bm_process_unexpected(void);

static uint64_t rdmsr(unsigned index) {
    unsigned lo,hi;
    __asm__ volatile("rdmsr" : "=a"(lo),"=d"(hi) : "c"(index));
    return (uint64_t)hi<<32|lo;
}
static void wrmsr(unsigned index, uint64_t value) {
    __asm__ volatile("wrmsr" :: "a"((unsigned)value),"d"((unsigned)(value>>32)),"c"(index) : "memory");
}
typedef struct {
    uintptr_t pages[MAX_ALLOCS];
    size_t counts[MAX_ALLOCS];
    unsigned count;
    uint64_t *root;
} process_memory;
static int pages_alloc(process_memory *m, size_t count, uintptr_t *address) {
    if (m->count==MAX_ALLOCS || bm_pages_alloc(count,address)) return -1;
    memset((void *)*address,0,count*PAGE);
    m->pages[m->count]=*address; m->counts[m->count++]=count;
    return 0;
}
static void cleanup(process_memory *m) {
    while (m->count) { --m->count; bm_pages_free(m->pages[m->count],m->counts[m->count]); }
}
static uint64_t *next_table(process_memory *m, uint64_t *table, unsigned index) {
    if (!(table[index]&P)) {
        uintptr_t address;
        if (pages_alloc(m,1,&address)) return NULL;
        table[index]=address|P|W|U;
    }
    return (uint64_t *)(uintptr_t)(table[index]&ADDRESS);
}
static int map_page(process_memory *m, uintptr_t va, uintptr_t pa, uint64_t flags) {
    uint64_t *table=m->root;
    for (unsigned shift=39;shift>12;shift-=9) {
        table=next_table(m,table,(va>>shift)&511);
        if (!table) return -1;
    }
    uint64_t *entry=&table[(va>>12)&511];
    if (*entry&P) return -1; /* Never overwrite a supervisor mapping. */
    *entry=(pa&ADDRESS)|P|flags;
    return 0;
}
static int map_identity(process_memory *m, uintptr_t start, uintptr_t end, uint64_t flags) {
    for (uintptr_t at=start&~(uintptr_t)(PAGE-1);at<end;at+=PAGE)
        if (map_page(m,at,at,flags)) return -1;
    return 0;
}
static void gate(unsigned vector, uintptr_t handler, unsigned ist, unsigned dpl) {
    idt[vector]=(idt_gate){(uint16_t)handler,bm_process_kernel_cs,(uint8_t)ist,
        (uint8_t)(0x8e|(dpl<<5)),(uint16_t)(handler>>16),(uint32_t)(handler>>32),0};
}
typedef struct {
    descriptor_ptr gdtr,idtr;
    uint16_t cs,ss,ds,es,fs,gs,tr,ldt;
    uint64_t flags,cr0,cr2,cr4,efer,sysenter,fs_base,gs_base,dr6,dr7,tss[2];
    int sep;
} firmware_state;

/* Fail closed on modes this four-level/SSE2 sandbox cannot restore. */
static int prepare(firmware_state *s) {
    unsigned a,b,c,d;
    __asm__ volatile("mov %%cs,%0" : "=r"(s->cs));
    if (s->cs&3) return -1;
    __asm__ volatile("sgdt %0; sidt %1; str %2; sldt %3"
        : "=m"(s->gdtr),"=m"(s->idtr),"=m"(s->tr),"=m"(s->ldt));
    __asm__ volatile("mov %%ss,%0; mov %%ds,%1; mov %%es,%2; mov %%fs,%3; mov %%gs,%4"
        : "=m"(s->ss),"=m"(s->ds),"=m"(s->es),"=m"(s->fs),"=m"(s->gs));
    __asm__ volatile("mov %%cr0,%0; mov %%cr2,%1; mov %%cr4,%2; mov %%cr3,%3"
        : "=r"(s->cr0),"=r"(s->cr2),"=r"(s->cr4),"=r"(bm_process_kernel_cr3));
    if ((s->cs&7) || (s->ss&7) || s->cs>=60*8 || s->ss>=60*8 ||
        s->cs+7>s->gdtr.limit || (s->ss && s->ss+7>s->gdtr.limit) ||
        (s->cr0&12) || (s->cr4&((1ull<<12)|(1ull<<17)|(1ull<<23)))) return -1;
    /* LA57, PCID and CET require separate transition implementations. */
    if (!__get_cpuid(0x80000001,&a,&b,&c,&d) || !(d&(1u<<20))) return -1;
    __cpuid(1,a,b,c,d); s->sep=!!(d&(1u<<11));
    s->efer=rdmsr(0xc0000080);
    s->sysenter=s->sep ? rdmsr(0x174) : 0;
    s->fs_base=rdmsr(0xc0000100); s->gs_base=rdmsr(0xc0000101);
    __asm__ volatile("mov %%dr6,%0; mov %%dr7,%1" : "=r"(s->dr6),"=r"(s->dr7));
    /* LTR cannot load selector zero. Firmware with no TSS uses our static
     * fallback cache after the first run; the backing TSS is never freed. */
    if (fallback_tr && s->tr==TSS_SELECTOR) s->tr=0;
    if (s->tr) {
        if ((s->tr&7) || s->tr>=60*8 || s->tr+15>s->gdtr.limit) return -1;
        memcpy(s->tss,(void *)(s->gdtr.base+s->tr),sizeof(s->tss));
        if ((s->tss[0]>>40&0x9f)!=0x8b) return -1;
    }
    memset(gdt,0,sizeof(gdt));
    memcpy(&gdt[s->cs/8],(void *)(s->gdtr.base+s->cs),8);
    if (s->ss) memcpy(&gdt[s->ss/8],(void *)(s->gdtr.base+s->ss),8);
    gdt[USER_CS/8]=0x00affa000000ffffull;
    gdt[USER_SS/8]=0x00cff2000000ffffull;
    uintptr_t base=(uintptr_t)&tss;
    gdt[TSS_SELECTOR/8]=(sizeof(tss)-1)|((base&0xffffffull)<<16)|
        (0x89ull<<40)|(((base>>24)&255)<<56);
    gdt[TSS_SELECTOR/8+1]=base>>32;
    memset(&tss,0,sizeof(tss));
    tss.rsp[0]=(uintptr_t)bm_process_trap_stack+8192;
    tss.ist[0]=(uintptr_t)bm_process_emergency_stack+4096;
    tss.iomap=sizeof(tss); /* Beyond TSS limit: deny every I/O port. */
    bm_process_kernel_cs=s->cs; bm_process_kernel_ss=s->ss;
    for (unsigned i=0;i<256;i++) gate(i,(uintptr_t)bm_process_unexpected,0,0);
    for (unsigned i=0;i<32;i++) gate(i,bm_process_exception_table[i],i==8 || i==2 ? 1 : 0,i==3 ? 3 : 0);
    gate(128,(uintptr_t)bm_process_syscall,0,3);
    return 0;
}
static void activate(firmware_state *s) {
    descriptor_ptr gdtr={sizeof(gdt)-1,(uintptr_t)gdt}, idtr={sizeof(idt)-1,(uintptr_t)idt};
    __asm__ volatile("pushfq; popq %0; cli" : "=r"(s->flags) :: "memory");
    /* Flush GLOBAL translations too. Disable extensions not saved by FXSAVE
     * (AVX/XSAVE, FSGSBASE, PKRU and user interrupts) during this run. */
    uint64_t cr4=s->cr4&~((1ull<<7)|(1ull<<16)|(1ull<<18)|(1ull<<22)|(1ull<<25));
    __asm__ volatile("mov %0,%%cr4; mov %1,%%cr0; mov %2,%%dr7"
        :: "r"(cr4),"r"(s->cr0|(1ull<<16)),"r"((uint64_t)0) : "memory");
    wrmsr(0xc0000080,(s->efer|(1ull<<11))&~1ull); /* NXE; no SYSCALL escape. */
    if (s->sep) wrmsr(0x174,0); /* No SYSENTER escape. */
    __asm__ volatile("lgdt %0; lidt %1; ltr %2; lldt %3"
        :: "m"(gdtr),"m"(idtr),"r"((uint16_t)TSS_SELECTOR),"r"((uint16_t)0) : "memory");
    __asm__ volatile("mov %0,%%ds; mov %0,%%es; mov %0,%%fs; mov %0,%%gs"
        :: "r"((uint16_t)0) : "memory");
    wrmsr(0xc0000100,0); wrmsr(0xc0000101,0);
}
static void restore(const firmware_state *s) {
    /* LTR rejects a busy TSS. Reload through a writable shadow descriptor;
     * never modify the firmware's own (possibly RO) GDT or attempt LTR 0. */
    if (s->tr) {
        gdt[s->tr/8]=s->tss[0]&~(2ull<<40);
        gdt[s->tr/8+1]=s->tss[1];
        __asm__ volatile("ltr %0" :: "r"(s->tr) : "memory");
        fallback_tr=0;
    } else fallback_tr=1;
    __asm__ volatile("lgdt %0; lidt %1; lldt %2"
        :: "m"(s->gdtr),"m"(s->idtr),"r"(s->ldt) : "memory");
    __asm__ volatile("mov %0,%%ds; mov %1,%%es; mov %2,%%fs; mov %3,%%gs"
        :: "r"(s->ds),"r"(s->es),"r"(s->fs),"r"(s->gs) : "memory");
    wrmsr(0xc0000100,s->fs_base); wrmsr(0xc0000101,s->gs_base);
    if (s->sep) wrmsr(0x174,s->sysenter);
    wrmsr(0xc0000080,s->efer);
    __asm__ volatile("mov %0,%%cr4; mov %1,%%cr0; mov %2,%%cr2; mov %3,%%dr6; mov %4,%%dr7"
        :: "r"(s->cr4),"r"(s->cr0),"r"(s->cr2),"r"(s->dr6),"r"(s->dr7) : "memory");
    __asm__ volatile("pushq %0; popfq" :: "r"(s->flags) : "memory","cc");
}

int bm_process_validate(const void *program, size_t bytes) {
    if (!program || !bytes || bytes>BM_PROCESS_MAX_CODE) return BM_PROCESS_INVALID;
    const unsigned char *p=program;
    /* Scan EVERY byte, including immediates: branches may enter at any byte.
     * Prevent disabling/suppressing TF (POPF, IRET, MOV SS, LSS) and XBEGIN.
     * Immutable code + NX data close aliases. False rejections are intentional. */
    for (size_t i=0;i<bytes;i++) {
        if (p[i]==0x9d || p[i]==0xcf || p[i]==0x8e) return BM_PROCESS_UNSAFE;
        if (i+1<bytes && ((p[i]==0x0f && p[i+1]==0xb2) ||
                         (p[i]==0xc7 && p[i+1]==0xf8))) return BM_PROCESS_UNSAFE;
    }
    return BM_PROCESS_OK;
}
int bm_process_run_input(const void *program, size_t bytes, const uint32_t *input, size_t input_count, bm_process_result *result) {
    if (!result || input_count>256 || (input_count && !input)) return BM_PROCESS_INVALID;
    memset(result,0,sizeof(*result));
    int status=bm_process_validate(program,bytes);
    if (status) return result->status=status;
    if (active) return result->status=BM_PROCESS_BUSY;
    active=1;
    bm_parallel_end(); /* Public API is BSP-only; join APs before changing CPU state. */
    firmware_state saved;
    process_memory memory={0};
    uintptr_t code,stack,root;
    status=BM_PROCESS_UNSUPPORTED;
    if (prepare(&saved)) goto done;
    status=BM_PROCESS_NO_MEMORY;
    if (pages_alloc(&memory,1,&code) || pages_alloc(&memory,STACK_PAGES,&stack) ||
        pages_alloc(&memory,1,&root)) goto done;
    memset((void *)code,0xcc,PAGE); /* Falling off a fragment gives #BP. */
    memcpy((void *)code,program,bytes);
    if (input_count) memcpy((void *)stack,input,input_count*sizeof(uint32_t));
    memory.root=(uint64_t *)root;
    if (map_identity(&memory,(uintptr_t)_text,(uintptr_t)_etext,0) ||
        map_identity(&memory,(uintptr_t)_data,(uintptr_t)_edata,W|NX) ||
        map_page(&memory,BM_PROCESS_CODE,code,U)) goto done;
    for (unsigned i=0;i<STACK_PAGES;i++)
        if (map_page(&memory,BM_PROCESS_STACK+i*PAGE,stack+i*PAGE,U|W|NX)) goto done;
    bm_process_reason=0; bm_process_steps=0; bm_process_user_rax=0;
    bm_process_rip=0; bm_process_error=0; bm_process_address=0; bm_process_cs=0;
    bm_process_user_cr3=root;
    activate(&saved);
    bm_process_enter();
    restore(&saved);
    result->steps=bm_process_steps; result->rax=bm_process_user_rax;
    result->rip=bm_process_rip; result->error=bm_process_error;
    result->address=bm_process_address; result->cs=bm_process_cs;
    result->fault_vector=(unsigned)bm_process_reason;
    if (bm_process_reason==128) status=BM_PROCESS_OK;
    else if (bm_process_reason==BM_PROCESS_TIMEOUT) status=BM_PROCESS_TIMEOUT;
    else if ((bm_process_cs&3)!=3) status=BM_PROCESS_INTERNAL;
    else status=BM_PROCESS_FAULT_BASE+(int)bm_process_reason;
done:
    cleanup(&memory);
    active=0;
    return result->status=status;
}
int bm_process_run(const void *program, size_t bytes, bm_process_result *result) {
    return bm_process_run_input(program,bytes,NULL,0,result);
}
