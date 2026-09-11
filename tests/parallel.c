#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <emmintrin.h>
#include "../baremetal/runtime.h"
#include "mp_firmware.h"

_Noreturn void bm_panic(const char *s) { fprintf(stderr,"PANIC: %s\n",s); abort(); }
typedef struct { unsigned out[49154], salt; } Job;
static void rows(void *p, int first, int last) {
    Job *job=p;
    assert(_mm_getcsr()==0x1f80);
    for (int i=first;i<last;i++) job->out[i+1]=((unsigned)i*1234567u)^job->salt;
}
static void check_rows(int n, unsigned salt) {
    Job job; job.salt=salt;
    for (unsigned i=0;i<49154;i++) job.out[i]=0xdeadbeef;
    bm_parallel_rows(rows,&job,n);
    assert(job.out[0]==0xdeadbeef && job.out[n+1]==0xdeadbeef);
    for (int i=0;i<n;i++) assert(job.out[i+1]==(((unsigned)i*1234567u)^salt));
}
static void exercise(TestMP config, unsigned wanted, const char *mode, unsigned count) {
    test_mp_setup(config); bm_parallel_set_limit(wanted);
    assert(bm_parallel_begin()==count);
    assert(!strcmp(bm_parallel_mode(),mode));
    const int sizes[]={0,1,2,3,4,5,17,2048,8192,49152};
    unsigned started=test_mp_starts();
    for (unsigned i=0;i<sizeof(sizes)/sizeof(*sizes);i++) check_rows(sizes[i],i+42);
    if (!strcmp(mode,"MP pool")) {
        for (unsigned i=0;i<2000;i++) check_rows(17,i);
        assert(test_mp_starts()==started); /* Persistent APs, not per-matvec startups. */
    }
    bm_parallel_end(); test_mp_assert_idle();
    /* Every response gets a fresh pool. No dangling event or AP survives. */
    bm_parallel_begin(); check_rows(33,9876); bm_parallel_end(); test_mp_assert_idle();
}
int main(void) {
    bm_fp_prepare();
    TestMP config={.cpus=8,.busy_cpu=-1};
    for (unsigned n=1;n<=4;n++) exercise(config,n,n==1 ? "serial" : "MP pool",n);
    assert(test_mp_started_cpus()==((UINT64_C(1)<<2)|(UINT64_C(1)<<4)|(UINT64_C(1)<<6)));
    config.cpus=4;
    exercise(config,4,"MP pool",4);
    config.cpus=2; exercise(config,4,"MP pool",2);
    config.cpus=1; exercise(config,4,"serial",1);
    config.cpus=8; config.bsp=3; exercise(config,4,"MP pool",4);
    config.bsp=0; config.disabled=UINT64_C(1)<<2; config.unhealthy=UINT64_C(1)<<4;
    exercise(config,4,"MP pool",4);
    assert(!(test_mp_started_cpus()&(config.disabled|config.unhealthy|1)));
    config.disabled=config.unhealthy=0; config.cpus=4; config.busy_cpu=2;
    exercise(config,4,"MP pool",3); /* Partial startup remains safe. */
    config.busy_cpu=-1; config.mode=TEST_MP_MISSING;
    exercise(config,4,"serial",1);
    config.mode=TEST_MP_BLOCKING;
    exercise(config,4,"MP blocking",3);
    config.cpus=8; exercise(config,4,"MP blocking",4);
    config.cpus=2; exercise(config,4,"serial",1);
    config.cpus=4; config.mode=TEST_MP_ASYNC; config.create_failure=1;
    exercise(config,4,"MP blocking",3);
    config.create_failure=0; config.mode=TEST_MP_BLOCKING;
    for (int failure=1;failure<=3;failure++) {
        config.all_failure=failure<3 ? failure : 0;
        config.missing_completion=failure==3;
        test_mp_setup(config); bm_parallel_begin(); check_rows(17,777);
        assert(!strcmp(bm_parallel_mode(),"serial") && bm_parallel_count()==1);
        check_rows(17,888); bm_parallel_end(); test_mp_assert_idle();
    }
    puts("MP workers: row boundaries, publication, topology, lifecycle and firmware fallbacks passed");
    return 0;
}
