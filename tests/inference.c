/* Host-only harness: execute the same transformer and math as the EFI build. */
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "../neural.c"
#include "mp_firmware.h"

_Noreturn void bm_panic(const char *message) {
    fprintf(stderr,"PANIC: %s\n",message); exit(1);
}
void bm_check_finite(const char *where, const float *values, size_t n, int pos, int layer) {
    (void)pos; (void)layer;
    for (size_t i=0;i<n;i++) if (!isfinite(values[i])) bm_panic(where);
}
int main(int argc, char **argv) {
    if (argc==2 && !strcmp(argv[1],"--simd")) {
        puts(bm_cpu_avx2_available() ? "AVX2" : "SSE2"); return 0;
    }
    if (argc<2 || argc>3) return 2;
    const char *simd=getenv("SMOL_SIMD");
    if (simd && strcmp(simd,"auto") && strcmp(simd,"sse2") && strcmp(simd,"avx2")) return 2;
    /* 'avx2' requires actual AVX2 execution; it never bypasses CPU checks. */
    if (simd && !strcmp(simd,"avx2") && !bm_cpu_avx2_available()) return 77;
    bm_simd_set_auto(!simd || strcmp(simd,"sse2"));
    const char *threads=getenv("SMOL_THREADS"), *mode=getenv("SMOL_MP_MODE");
    unsigned wanted=threads ? (unsigned)strtoul(threads,NULL,10) : 4;
    if (wanted<1 || wanted>4) return 2;
    test_mp_setup((TestMP){.cpus=4,.busy_cpu=-1,
        .mode=mode && !strcmp(mode,"blocking") ? TEST_MP_BLOCKING : TEST_MP_ASYNC});
    bm_parallel_set_limit(wanted);
    int fd=open(argv[1],O_RDONLY);
    struct stat st;
    if (fd<0 || fstat(fd,&st)) bm_panic("cannot open model");
    void *data=mmap(NULL,(size_t)st.st_size,PROT_READ,MAP_PRIVATE,fd,0);
    if (data==MAP_FAILED) bm_panic("cannot map model");
    Model *m=alloc(sizeof(*m)); init_model(m,data,(size_t)st.st_size);
    if (argc==3) {
        State *s=new_state(m,2048);
        int ids[MAXCTX], n=turn_tokens(m,argv[2],1,ids,MAXCTX);
        if (n+256>s->ctx) bm_panic("smoke prompt exceeds context");
        fprintf(stderr,"Prompt: %d tokens\n",n);
        bm_parallel_begin();
        for (int i=0;i<n;i++) forward(m,s,ids[i],i==n-1);
        for (int i=0;i<256;i++) {
            int token=greedy(s->logits);
            if (token==0 || token==2) break;
            if ((unsigned)token>=m->nspecial) fwrite(m->words[token].p,1,m->words[token].n,stdout);
            forward(m,s,token,1);
        }
        bm_parallel_end();
        putchar('\n'); free_state(s);
    } else {
        const int tokens[]={1,9690,198,19556};
        State *s=new_state(m,8);
        bm_parallel_begin();
        for (unsigned i=0;i<sizeof(tokens)/sizeof(*tokens);i++) {
            forward(m,s,tokens[i],1);
            if (fwrite(s->logits,sizeof(float),V,stdout)!=V) return 1;
        }
        bm_parallel_end(); free_state(s);
    }
    free(m); munmap(data,(size_t)st.st_size); close(fd);
    test_mp_assert_idle();
    fprintf(stderr,"Matvec: %s\n",bm_simd_used());
    if (simd && !strcmp(simd,"avx2") && strcmp(bm_simd_used(),"AVX2")) return 1;
    return 0;
}
