/* Baseline SSE2 code: never execute XGETBV before checking OSXSAVE.
 * Do not change CR4/XCR0 owned by firmware. Probe on the executing CPU, since
 * AP state need not match the BSP's state. See Intel Optimization Manual,
 * "Detection of Intel AVX2". */
#include <cpuid.h>
#include <stdatomic.h>
#include "runtime.h"
#include "cpu.h"

static int automatic=1; /* BSP changes policy only while all workers are idle. */
static atomic_uint used;

static void cpu_features(unsigned *leaf1, unsigned *leaf7, unsigned *leafd) {
    unsigned a,b,c,d;
    *leaf1=*leaf7=*leafd=0;
    if (__get_cpuid_max(0,NULL)<7) return;
    __cpuid_count(1,0,a,b,c,d);
    *leaf1=c;
    __cpuid_count(7,0,a,b,c,d);
    *leaf7=b;
    if (*leaf1&BM_XSAVE) { __cpuid_count(13,0,a,b,c,d); *leafd=a; }
}
static uint64_t xgetbv0(void) {
    unsigned lo,hi;
    __asm__ volatile("xgetbv" : "=a"(lo),"=d"(hi) : "c"(0));
    return ((uint64_t)hi<<32)|lo;
}
static void xsetbv0(uint64_t value) {
    __asm__ volatile("xsetbv" :: "a"((unsigned)value),"d"((unsigned)(value>>32)),"c"(0) : "memory");
}
static int ring0(void) {
    unsigned short cs;
    __asm__ volatile("mov %%cs,%0" : "=r"(cs));
    return !(cs&3);
}
int bm_cpu_avx2_hardware(void) {
    unsigned leaf1,leaf7,leafd;
    cpu_features(&leaf1,&leaf7,&leafd);
    return bm_avx2_hardware_ok(leaf1,leaf7,leafd);
}
int bm_cpu_avx2_available(void) {
    unsigned leaf1,leaf7,leafd;
    cpu_features(&leaf1,&leaf7,&leafd);
    if (!bm_avx2_hardware_ok(leaf1,leaf7,leafd) || !(leaf1&BM_OSXSAVE)) return 0;
    return bm_avx2_state_ok(leaf1,leaf7,xgetbv0());
}
int bm_avx2_begin(bm_avx2_scope *scope) {
    scope->changed=0;
    if (!bm_cpu_avx2_hardware()) return 0;
    if (bm_cpu_avx2_available()) return 1;
    if (!ring0()) return 0;

    /* UEFI applications execute at CPL 0. Mask interrupts only while changing
     * the control state; the matvec itself runs with the caller's IF restored.
     * vzeroupper in the AVX2 kernel clears live upper halves before end(). */
    uint64_t flags,cr4;
    __asm__ volatile("pushfq; popq %0; cli; mov %%cr4,%1"
                     : "=r"(flags),"=r"(cr4) :: "memory");
    if (!(cr4&(UINT64_C(1)<<18))) {
        uint64_t enabled=cr4|(UINT64_C(1)<<18);
        __asm__ volatile("mov %0,%%cr4" :: "r"(enabled) : "memory");
    }
    uint64_t xcr0=xgetbv0();
    if ((xcr0&7)!=7) xsetbv0(xcr0|7);
    scope->rflags=flags; scope->cr4=cr4; scope->xcr0=xcr0; scope->changed=1;
    __asm__ volatile("pushq %0; popfq" :: "r"(flags) : "memory","cc");
    return 1;
}
void bm_avx2_end(bm_avx2_scope *scope) {
    if (!scope->changed) return;
    uint64_t flags;
    __asm__ volatile("pushfq; popq %0; cli" : "=r"(flags) :: "memory");
    xsetbv0(scope->xcr0);
    __asm__ volatile("mov %0,%%cr4" :: "r"(scope->cr4) : "memory");
    scope->changed=0;
    __asm__ volatile("pushq %0; popfq" :: "r"(flags) : "memory","cc");
}
const char *bm_cpu_avx2_status(void) {
    if (!bm_cpu_avx2_hardware()) return "SSE2 only";
    if (bm_cpu_avx2_available()) return "AVX2 ready";
    return ring0() ? "AVX2 hw; runtime YMM" : "AVX2 hw; YMM disabled";
}
int bm_simd_auto(void) { return automatic; }
void bm_simd_set_auto(int enabled) { automatic=!!enabled; bm_simd_reset(); }
void bm_simd_reset(void) { atomic_store_explicit(&used,0,memory_order_relaxed); }
void bm_simd_record(int avx2) {
    atomic_fetch_or_explicit(&used,avx2 ? 2u : 1u,memory_order_relaxed);
}
const char *bm_simd_used(void) {
    unsigned flags=atomic_load_explicit(&used,memory_order_relaxed);
    return flags==3 ? "AVX2 + SSE2" : flags==2 ? "AVX2" : flags==1 ? "SSE2" : "not run yet";
}
