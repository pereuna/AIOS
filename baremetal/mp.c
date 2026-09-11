/* Up to four independent row ranges. Only the BSP owns the pool and calls
 * console/allocator/event services. APs never allocate or print. */
#include <stdatomic.h>
#include "runtime.h"
#include "mp.h"

_Static_assert(ATOMIC_INT_LOCK_FREE==2, "MP workers require lock-free unsigned atomics");
enum { IDLE, RUN, STOP };
enum { SERIAL, POOL, BLOCKING };
typedef struct {
    _Alignas(64) atomic_uint command;
    bm_row_task task;
    void *context;
    int first, last;
    EFI_EVENT event;
} Worker;
static Worker workers[BM_MAX_THREADS-1];
static EFI_BOOT_SERVICES *boot;
static BM_MP_PROTOCOL *mp;
static UINTN candidates[BM_MAX_THREADS];
static BM_CPU_INFO locations[BM_MAX_THREADS+1];
static unsigned candidate_count, requested=BM_MAX_THREADS, count=1, running;
static int mode, active, async_unavailable;

static void pause_cpu(void) { __asm__ volatile("pause"); }
static void EFIAPI worker_loop(void *argument) {
    Worker *w=argument;
    for (;;) {
        unsigned command=atomic_load_explicit(&w->command,memory_order_acquire);
        if (command==STOP) return;
        if (command==IDLE) { pause_cpu(); continue; }
        bm_fp_prepare();
        w->task(w->context,w->first,w->last);
        atomic_store_explicit(&w->command,IDLE,memory_order_release);
    }
}
static int same_core(const BM_CPU_INFO *a, const BM_CPU_INFO *b) {
    return a->location.package==b->location.package && a->location.core==b->location.core;
}
void bm_mp_init(EFI_BOOT_SERVICES *services) {
    boot=services; mp=NULL; candidate_count=0; requested=BM_MAX_THREADS;
    count=1; running=0; mode=SERIAL; active=0; async_unavailable=0;
    EFI_GUID guid=BM_MP_GUID;
    if (!boot->LocateProtocol ||
        EFI_ERROR(boot->LocateProtocol(&guid,NULL,(void **)&mp)) || !mp) { mp=NULL; return; }
    UINTN total,enabled,bsp;
    if (!mp->GetNumberOfProcessors || !mp->GetProcessorInfo || !mp->WhoAmI ||
        EFI_ERROR(mp->GetNumberOfProcessors(mp,&total,&enabled)) || enabled<2 ||
        EFI_ERROR(mp->WhoAmI(mp,&bsp)) || bsp>=total ||
        EFI_ERROR(mp->GetProcessorInfo(mp,bsp,&locations[0]))) { mp=NULL; return; }
    /* Prefer different physical cores/packages, then fill with SMT siblings.
     * Four AP candidates also support blocking mode, where the BSP must wait. */
    for (unsigned pass=0;pass<2;pass++) for (UINTN cpu=0;cpu<total;cpu++) {
        if (candidate_count==BM_MAX_THREADS) break;
        BM_CPU_INFO info;
        if (cpu==bsp || EFI_ERROR(mp->GetProcessorInfo(mp,cpu,&info)) ||
            (info.flags&(BM_CPU_ENABLED|BM_CPU_HEALTHY))!=(BM_CPU_ENABLED|BM_CPU_HEALTHY) ||
            (info.flags&BM_CPU_BSP)) continue;
        int duplicate=0,shared=same_core(&info,&locations[0]);
        for (unsigned i=0;i<candidate_count;i++) {
            if (candidates[i]==cpu) duplicate=1;
            if (same_core(&info,&locations[i+1])) shared=1;
        }
        if (duplicate || (!pass && shared)) continue;
        candidates[candidate_count]=cpu;
        locations[++candidate_count]=info;
    }
}
unsigned bm_parallel_limit(void) { return requested; }
void bm_parallel_set_limit(unsigned limit) {
    if (active || limit<1 || limit>BM_MAX_THREADS) bm_panic("invalid worker limit");
    requested=limit;
}
unsigned bm_parallel_count(void) { return count; }
const char *bm_parallel_mode(void) {
    return mode==POOL ? "MP pool" : mode==BLOCKING ? "MP blocking" : "serial";
}
static void join_workers(void) {
    for (unsigned i=0;i<running;i++)
        atomic_store_explicit(&workers[i].command,STOP,memory_order_release);
    for (unsigned i=0;i<running;i++) {
        UINTN index;
        if (EFI_ERROR(boot->WaitForEvent(1,&workers[i].event,&index)))
            bm_panic("cannot join MP worker");
        /* The firmware event, not our IDLE flag, confirms the AP has returned. */
        if (EFI_ERROR(boot->CloseEvent(workers[i].event))) bm_panic("cannot close MP event");
    }
    running=0;
    bm_fp_prepare();
}
unsigned bm_parallel_begin(void) {
    if (active) bm_panic("nested worker pool");
    active=1; count=1; mode=SERIAL;
    if (!mp || !candidate_count || requested==1) return count;
    if (!async_unavailable && mp->StartupThisAP && boot->CreateEvent &&
        boot->WaitForEvent && boot->CloseEvent) {
        for (unsigned i=0;i<candidate_count && running+1<requested;i++) {
            Worker *w=&workers[running];
            atomic_store_explicit(&w->command,IDLE,memory_order_relaxed);
            if (EFI_ERROR(boot->CreateEvent(0,TPL_APPLICATION,NULL,NULL,&w->event))) break;
            /* Zero timeout: keep this AP alive until the response ends. No MP
             * protocol call is needed for each of the hundreds of matvecs. */
            EFI_STATUS status=mp->StartupThisAP(mp,worker_loop,candidates[i],w->event,0,w,NULL);
            if (EFI_ERROR(status)) {
                if (EFI_ERROR(boot->CloseEvent(w->event))) bm_panic("cannot close MP event");
                if (status==EFI_UNSUPPORTED) { async_unavailable=1; break; }
                continue; /* A busy AP is skipped; already launched APs still work. */
            }
            running++;
        }
    }
    if (running) { count=running+1; mode=POOL; }
    else if (mp->StartupAllAPs && candidate_count>=2) {
        /* PI requires rejecting non-blocking calls after ReadyToBoot. Blocking
         * calls still work, but use APs only: the BSP is inside the firmware. */
        count=candidate_count<requested ? candidate_count : requested;
        mode=BLOCKING;
    }
    bm_fp_prepare();
    return count;
}
void bm_parallel_end(void) {
    if (!active) return;
    active=0;
    join_workers();
}
typedef struct {
    bm_row_task task;
    void *context;
    int rows;
    unsigned parts;
    atomic_uint completed;
} BlockingJob;
static void EFIAPI blocking_rows(void *argument) {
    BlockingJob *job=argument;
    UINTN cpu;
    /* WhoAmI is explicitly permitted on APs by PI. Ignore unselected CPUs. */
    if (EFI_ERROR(mp->WhoAmI(mp,&cpu))) return;
    for (unsigned i=0;i<job->parts;i++) if (candidates[i]==cpu) {
        bm_fp_prepare();
        int first=(int)((size_t)job->rows*i/job->parts);
        int last=(int)((size_t)job->rows*(i+1)/job->parts);
        job->task(job->context,first,last);
        atomic_fetch_or_explicit(&job->completed,1u<<i,memory_order_release);
        break;
    }
}
void bm_parallel_rows(bm_row_task task, void *context, int rows) {
    if (rows<=0) return;
    if (!active || mode==SERIAL || (unsigned)rows<count) { task(context,0,rows); return; }
    if (mode==BLOCKING) {
        BlockingJob job={.task=task,.context=context,.rows=rows,.parts=count};
        atomic_init(&job.completed,0);
        /* On timeout PI terminates the AP procedure before returning. Therefore
         * a failed dispatch can safely be retried completely on the BSP. */
        EFI_STATUS status=mp->StartupAllAPs(mp,blocking_rows,FALSE,NULL,10000000,&job,NULL);
        bm_fp_prepare();
        if (EFI_ERROR(status) ||
            atomic_load_explicit(&job.completed,memory_order_acquire)!=((1u<<count)-1)) {
            mode=SERIAL; count=1;
            task(context,0,rows);
        }
        return;
    }
    for (unsigned i=0;i<running;i++) {
        Worker *w=&workers[i];
        w->task=task; w->context=context;
        w->first=(int)((size_t)rows*(i+1)/count);
        w->last=(int)((size_t)rows*(i+2)/count);
        atomic_store_explicit(&w->command,RUN,memory_order_release);
    }
    task(context,0,rows/(int)count); /* BSP computes the first row range. */
    for (unsigned i=0;i<running;i++)
        while (atomic_load_explicit(&workers[i].command,memory_order_acquire)!=IDLE) pause_cpu();
}
