#ifndef SMOL_CPU_H
#define SMOL_CPU_H
#include <stdint.h>

enum { BM_XSAVE=1u<<26, BM_OSXSAVE=1u<<27, BM_AVX=1u<<28, BM_AVX2=1u<<5 };
static inline int bm_avx2_state_ok(uint32_t leaf1_ecx, uint32_t leaf7_ebx, uint64_t xcr0) {
    const uint32_t required=BM_XSAVE|BM_OSXSAVE|BM_AVX;
    return (leaf1_ecx&required)==required && (leaf7_ebx&BM_AVX2) && (xcr0&6)==6;
}
static inline int bm_avx2_hardware_ok(uint32_t leaf1_ecx, uint32_t leaf7_ebx,
                                      uint32_t leafd_eax) {
    const uint32_t required=BM_XSAVE|BM_AVX;
    return (leaf1_ecx&required)==required && (leaf7_ebx&BM_AVX2) && (leafd_eax&7)==7;
}
#endif
