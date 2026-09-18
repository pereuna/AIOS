/* Linux scheduling adapter. Neural kernels, SIMD detection, floating-point
 * setup and math still come from the bare-metal sources. */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include "../baremetal/runtime.h"

static pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t work=PTHREAD_COND_INITIALIZER, done=PTHREAD_COND_INITIALIZER;
static pthread_t workers[BM_MAX_THREADS-1];
static unsigned limit=BM_MAX_THREADS, active=1, pending, epoch;
static int stopping, running, rows;
static bm_row_task task;
static void *argument;

static void check(int result) { if (result) bm_panic("pthread operation failed"); }
static void *worker(void *opaque) {
    unsigned index=(unsigned)(uintptr_t)opaque, seen=0;
    bm_fp_prepare();
    check(pthread_mutex_lock(&mutex));
    for (;;) {
        while (!stopping && seen==epoch) check(pthread_cond_wait(&work,&mutex));
        if (stopping) break;
        seen=epoch;
        bm_row_task fn=task;
        void *arg=argument;
        int first=(int)((uint64_t)rows*index/active);
        int last=(int)((uint64_t)rows*(index+1)/active);
        check(pthread_mutex_unlock(&mutex));
        fn(arg,first,last);
        check(pthread_mutex_lock(&mutex));
        if (!--pending) check(pthread_cond_signal(&done));
    }
    check(pthread_mutex_unlock(&mutex));
    return NULL;
}
unsigned bm_parallel_begin(void) {
    if (running) bm_panic("nested parallel region");
    running=1; stopping=0; epoch=0; active=limit;
    for (unsigned i=1;i<active;i++)
        check(pthread_create(&workers[i-1],NULL,worker,(void *)(uintptr_t)i));
    return active;
}
void bm_parallel_end(void) {
    if (!running) return;
    check(pthread_mutex_lock(&mutex));
    stopping=1;
    check(pthread_cond_broadcast(&work));
    check(pthread_mutex_unlock(&mutex));
    for (unsigned i=1;i<active;i++) check(pthread_join(workers[i-1],NULL));
    running=0;
}
void bm_parallel_rows(bm_row_task fn, void *arg, int count) {
    if (!running || active==1) { fn(arg,0,count); return; }
    check(pthread_mutex_lock(&mutex));
    task=fn; argument=arg; rows=count; pending=active-1; epoch++;
    check(pthread_cond_broadcast(&work));
    check(pthread_mutex_unlock(&mutex));
    fn(arg,0,count/(int)active);
    check(pthread_mutex_lock(&mutex));
    while (pending) check(pthread_cond_wait(&done,&mutex));
    check(pthread_mutex_unlock(&mutex));
}
unsigned bm_parallel_count(void) { return active; }
unsigned bm_parallel_limit(void) { return limit; }
void bm_parallel_set_limit(unsigned n) {
    if (running || n<1 || n>BM_MAX_THREADS) bm_panic("invalid thread limit");
    limit=n;
}
const char *bm_parallel_mode(void) { return "Linux pthreads"; }
_Noreturn void bm_panic(const char *message) {
    fprintf(stderr,"AIOS model: %s\n",message);
    exit(1);
}
void bm_check_finite(const char *name, const float *values, size_t n, int pos, int layer) {
    for (size_t i=0;i<n;i++) if (!isfinite(values[i])) {
        fprintf(stderr,"%s: position=%d layer=%d index=%zu\n",name,pos,layer,i);
        bm_panic("non-finite calculation");
    }
}
