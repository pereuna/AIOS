/* Host simulation of MP firmware, calling the actual EFIAPI callbacks on
 * pthreads. No pthread code is linked into the EFI image. */
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <emmintrin.h>
#include "../baremetal/mp.h"
#include "mp_firmware.h"

typedef struct Thread Thread;
typedef struct { Thread *thread; } Event;
struct Thread {
    pthread_t handle;
    int live;
    UINTN cpu;
    BM_AP_PROCEDURE procedure;
    void *argument;
};
static Thread threads[64];
static TestMP config;
static _Thread_local UINTN current_cpu;
static unsigned events,starts;
static uint64_t started_cpus;
static BM_MP_PROTOCOL protocol;

static void require_bsp(void) { assert(current_cpu==config.bsp); }
static void *run_thread(void *p) {
    Thread *t=p;
    current_cpu=t->cpu;
    /* Deliberately wrong rounding mode: every worker must prepare SSE state. */
    _mm_setcsr(0x7f80);
    if (!(config.missing_completion && t->cpu!=config.bsp))
        t->procedure(t->argument);
    return NULL;
}
static void launch(UINTN cpu, BM_AP_PROCEDURE procedure, void *argument) {
    Thread *t=&threads[cpu];
    assert(!t->live);
    t->cpu=cpu; t->procedure=procedure; t->argument=argument; t->live=1;
    assert(!pthread_create(&t->handle,NULL,run_thread,t));
    starts++; started_cpus|=UINT64_C(1)<<cpu;
}
static void join(Thread *t) {
    assert(t->live);
    assert(!pthread_join(t->handle,NULL));
    t->live=0;
}
static EFI_STATUS EFIAPI locate(EFI_GUID *guid, void *registration, void **out) {
    require_bsp(); (void)registration;
    EFI_GUID expected=BM_MP_GUID;
    assert(!memcmp(guid,&expected,sizeof(expected)));
    if (config.mode==TEST_MP_MISSING) return EFI_NOT_FOUND;
    *out=&protocol; return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI numbers(BM_MP_PROTOCOL *self, UINTN *total, UINTN *enabled) {
    require_bsp(); assert(self==&protocol);
    *total=config.cpus; *enabled=0;
    for (unsigned i=0;i<config.cpus;i++) if (!(config.disabled&(UINT64_C(1)<<i))) (*enabled)++;
    return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI info(BM_MP_PROTOCOL *self, UINTN cpu, BM_CPU_INFO *out) {
    require_bsp(); assert(self==&protocol);
    if (cpu>=config.cpus) return EFI_NOT_FOUND;
    memset(out,0,sizeof(*out)); out->id=cpu;
    out->flags=(cpu==config.bsp ? BM_CPU_BSP : 0) |
        (config.disabled&(UINT64_C(1)<<cpu) ? 0 : BM_CPU_ENABLED) |
        (config.unhealthy&(UINT64_C(1)<<cpu) ? 0 : BM_CPU_HEALTHY);
    out->location.core=(UINT32)cpu/2; out->location.thread=(UINT32)cpu%2;
    return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI who(BM_MP_PROTOCOL *self, UINTN *cpu) {
    assert(self==&protocol); *cpu=current_cpu; return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI create(UINT32 type, EFI_TPL tpl, EFI_EVENT_NOTIFY notify,
                                void *context, EFI_EVENT *out) {
    require_bsp(); assert(!type && tpl==TPL_APPLICATION && !notify && !context);
    if (config.create_failure) return EFI_OUT_OF_RESOURCES;
    Event *event=calloc(1,sizeof(*event)); assert(event);
    *out=event; events++; return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI wait(UINTN n, EFI_EVENT *list, UINTN *index) {
    require_bsp(); assert(n==1);
    Event *event=list[0]; assert(event->thread);
    join(event->thread); event->thread=NULL;
    *index=0; return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI close_event(EFI_EVENT p) {
    require_bsp(); Event *event=p;
    assert(!event->thread); free(event); assert(events); events--; return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI start_one(BM_MP_PROTOCOL *self, BM_AP_PROCEDURE procedure,
                                   UINTN cpu, EFI_EVENT p, UINTN timeout,
                                   void *argument, BOOLEAN *finished) {
    require_bsp(); assert(self==&protocol && p && !timeout && !finished);
    if (config.mode==TEST_MP_BLOCKING) return EFI_UNSUPPORTED;
    if ((int)cpu==config.busy_cpu) return EFI_NOT_READY;
    assert(cpu!=config.bsp && cpu<config.cpus);
    assert(!((config.disabled|config.unhealthy)&(UINT64_C(1)<<cpu)));
    Event *event=p; event->thread=&threads[cpu];
    launch(cpu,procedure,argument); return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI start_all(BM_MP_PROTOCOL *self, BM_AP_PROCEDURE procedure,
                                   BOOLEAN sequential, EFI_EVENT event, UINTN timeout,
                                   void *argument, UINTN **failed) {
    require_bsp(); assert(self==&protocol && !sequential && !event && timeout && !failed);
    if (config.all_failure==1) return EFI_NOT_READY;
    for (unsigned i=0;i<config.cpus;i++)
        if (i!=config.bsp && !(config.disabled&(UINT64_C(1)<<i))) launch(i,procedure,argument);
    for (unsigned i=0;i<config.cpus;i++) if (threads[i].live) join(&threads[i]);
    /* Simulate a timeout return only AFTER all workers are quiescent, as PI
     * guarantees. The caller must recompute the full output and go serial. */
    return config.all_failure==2 ? EFI_TIMEOUT : EFI_SUCCESS;
}
void test_mp_assert_idle(void) {
    require_bsp(); assert(!events);
    for (unsigned i=0;i<64;i++) assert(!threads[i].live);
}
void test_mp_setup(TestMP settings) {
    test_mp_assert_idle();
    assert(settings.cpus && settings.cpus<=64 && settings.bsp<settings.cpus);
    config=settings; current_cpu=config.bsp; starts=0; started_cpus=0;
    protocol=(BM_MP_PROTOCOL){.GetNumberOfProcessors=numbers,.GetProcessorInfo=info,
        .StartupAllAPs=start_all,.StartupThisAP=start_one,.WhoAmI=who};
    static EFI_BOOT_SERVICES services;
    services=(EFI_BOOT_SERVICES){.LocateProtocol=locate,.CreateEvent=create,
        .WaitForEvent=wait,.CloseEvent=close_event};
    bm_mp_init(&services);
}
unsigned test_mp_starts(void) { return starts; }
uint64_t test_mp_started_cpus(void) { return started_cpus; }
