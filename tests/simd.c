/* Real SIMD kernels, independent scalar sum tree and guarded allocations.
 * The linker wrapper can hide AVX2 from individual fake APs, never add it. */
#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include "../neural.c"
#include "../baremetal/cpu.h"
#include "mp_firmware.h"

_Noreturn void bm_panic(const char *s) { fprintf(stderr,"PANIC: %s\n",s); abort(); }
static uint64_t hidden_cpus;
int __real_bm_avx2_begin(bm_avx2_scope *);
int __wrap_bm_avx2_begin(bm_avx2_scope *scope) {
    if (hidden_cpus&(UINT64_C(1)<<test_mp_cpu())) { scope->changed=0; return 0; }
    return __real_bm_avx2_begin(scope);
}
typedef struct { void *mapping; unsigned char *data; size_t size; } Guard;
static Guard guarded(size_t bytes) {
    size_t page=(size_t)sysconf(_SC_PAGESIZE);
    size_t usable=(bytes+page-1)/page*page;
    void *p=mmap(NULL,usable+page,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    assert(p!=MAP_FAILED);
    assert(!mprotect((unsigned char *)p+usable,page,PROT_NONE));
    return (Guard){p,(unsigned char *)p+usable-bytes,usable+page};
}
static void release(Guard g) { assert(!munmap(g.mapping,g.size)); }
static uint32_t random_bits=0x12345678;
static uint32_t random32(void) {
    random_bits^=random_bits<<13; random_bits^=random_bits>>17; random_bits^=random_bits<<5;
    return random_bits;
}
static void features(void) {
    uint32_t f=BM_XSAVE|BM_OSXSAVE|BM_AVX;
    for (unsigned x=0;x<256;x++) assert(!!bm_avx2_state_ok(f,BM_AVX2,x)==((x&6)==6));
    for (unsigned bit=0;bit<32;bit++)
        assert(!!bm_avx2_state_ok(f&~(1u<<bit),BM_AVX2,7)==(!(f&(1u<<bit))));
    assert(!bm_avx2_state_ok(f,0,7));
    assert(!bm_avx2_state_ok(0,BM_AVX2,7));
    f=BM_XSAVE|BM_AVX;
    assert(bm_avx2_hardware_ok(f,BM_AVX2,7));
    assert(!bm_avx2_hardware_ok(f|BM_OSXSAVE,0,7));
    assert(!bm_avx2_hardware_ok(BM_AVX,BM_AVX2,7));
    assert(!bm_avx2_hardware_ok(f,BM_AVX2,3));
}
/* Decode without the production integer bit-conversion, for finite scales. */
static float scale_ref(const unsigned char *p) {
    unsigned h=(unsigned)p[0]|((unsigned)p[1]<<8), e=(h>>10)&31, f=h&1023;
    float value;
    if (!e) value=(float)f*0x1p-24f;
    else {
        value=1.0f+(float)f/1024.0f;
        for (unsigned i=e;i<15;i++) value*=0.5f;
        for (unsigned i=15;i<e;i++) value*=2.0f;
    }
    return h&0x8000 ? -value : value;
}
static void reference(float *out, const unsigned char *w, const float *x, int rows, int cols) {
    for (int row=0;row<rows;row++) {
        float sums[4]={0};
        for (int j=0;j<cols;j+=32) {
            const unsigned char *p=w+((size_t)row*cols+j)/32*18;
            float products[32];
            for (unsigned k=0;k<32;k++) {
                int q=((p[2+k%16]>>(k<16 ? 0 : 4))&15)-8;
                products[k]=(float)q*x[j+k];
            }
            for (unsigned k=0;k<4;k++) {
                float a=(products[k]+products[k+4])+(products[k+8]+products[k+12]);
                float b=(products[k+16]+products[k+20])+(products[k+24]+products[k+28]);
                sums[k]+=scale_ref(p)*(a+b);
            }
        }
        out[row]=(sums[0]+sums[1])+(sums[2]+sums[3]);
    }
}
static void kernels(int rows, int cols, unsigned offset, int avx2) {
    Guard weights=guarded((size_t)rows*cols/32*18+offset);
    Guard input=guarded((size_t)cols*4+offset*4), output=guarded((size_t)rows*4);
    unsigned char *w=weights.data;
    float *x=(float *)input.data, *actual=(float *)output.data;
    float *expected=malloc((size_t)rows*4); assert(expected);
    const uint16_t scales[]={0,0x8000,1,0x8001,0x3ff,0x400,0x1234,0x3c00,0xbc00,0x7bff,0xfbff};
    for (int j=0;j<cols;j++) x[j]=(float)((int)(random32()%65537)-32768)*0x1p-12f;
    for (size_t block=0;block<(size_t)rows*cols/32;block++) {
        uint16_t h=scales[block%(sizeof(scales)/sizeof(*scales))];
        w[block*18]=(unsigned char)h; w[block*18+1]=(unsigned char)(h>>8);
        for (unsigned k=2;k<18;k++) w[block*18+k]=(unsigned char)random32();
    }
    reference(expected,w,x,rows,cols);
    MatvecJob job={actual,w,x,cols};
    matvec_rows_sse2(&job,0,rows);
    assert(!memcmp(expected,actual,(size_t)rows*4));
    if (avx2) {
        matvec_rows_avx2(&job,0,rows);
        assert(!memcmp(expected,actual,(size_t)rows*4));
        for (int i=0;i<rows;i++) actual[i]=-123.0f;
        matvec_rows_avx2(&job,1,rows-1);
        assert(actual[0]==-123.0f && actual[rows-1]==-123.0f);
        if (rows>2) assert(!memcmp(expected+1,actual+1,(size_t)(rows-2)*4));
    }
    /* Exercise dispatch on the executing AP, including mixed capabilities. */
    for (unsigned mode=0;mode<2;mode++) for (unsigned scenario=0;scenario<4;scenario++) {
        test_mp_setup((TestMP){.cpus=4,.busy_cpu=-1,.mode=(int)mode});
        hidden_cpus=scenario==1 ? 1 : scenario==2 ? 4 : scenario==3 ? 15 : 0;
        bm_simd_set_auto(1); bm_parallel_begin();
        matvec(actual,w,x,rows,cols);
        bm_parallel_end(); test_mp_assert_idle();
        assert(!memcmp(expected,actual,(size_t)rows*4));
        if (!avx2 || scenario==3) assert(!strcmp(bm_simd_used(),"SSE2"));
        else if (!scenario) assert(!strcmp(bm_simd_used(),"AVX2"));
        else if (rows>=4 && (!mode || scenario==2)) assert(!strcmp(bm_simd_used(),"AVX2 + SSE2"));
    }
    hidden_cpus=0; bm_simd_set_auto(0); bm_parallel_begin();
    matvec(actual,w,x,rows,cols);
    bm_parallel_end(); test_mp_assert_idle();
    assert(!strcmp(bm_simd_used(),"SSE2"));
    assert(!memcmp(expected,actual,(size_t)rows*4));
    free(expected); release(weights); release(input); release(output);
}
int main(void) {
    bm_fp_prepare(); features();
    int avx2=bm_cpu_avx2_available();
    bm_avx2_scope scope;
    assert(!!bm_avx2_begin(&scope)==avx2);
    if (avx2) { assert(!scope.changed); bm_avx2_end(&scope); }
    if (avx2) for (unsigned h=0;h<65536;h++) {
        unsigned char p[2]={(unsigned char)h,(unsigned char)(h>>8)};
        float a=half(p), b=half_avx2(p);
        assert(!memcmp(&a,&b,4)); /* Includes signed zero, subnormals, Inf/NaN. */
    }
    const int rows[]={1,3,7,17}, cols[]={32,64,96,2048,8192};
    for (unsigned r=0;r<sizeof(rows)/sizeof(*rows);r++)
        for (unsigned c=0;c<sizeof(cols)/sizeof(*cols);c++)
            for (unsigned offset=0;offset<2;offset++) kernels(rows[r],cols[c],offset,avx2);
    puts(avx2 ? "SIMD: AVX2/SSE2/scalar bit-identical; FP16, guards and per-CPU fallback passed"
              : "SIMD: SSE2/scalar and fallback passed; AVX2 unavailable, execution tests skipped");
    return 0;
}
