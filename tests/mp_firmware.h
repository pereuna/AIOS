#ifndef TEST_MP_FIRMWARE_H
#define TEST_MP_FIRMWARE_H
#include <stdint.h>
enum { TEST_MP_ASYNC, TEST_MP_BLOCKING, TEST_MP_MISSING };
typedef struct {
    unsigned cpus, bsp;
    int mode, busy_cpu, create_failure, all_failure, missing_completion;
    uint64_t disabled, unhealthy;
} TestMP;
void test_mp_setup(TestMP);
void test_mp_assert_idle(void);
unsigned test_mp_starts(void);
uint64_t test_mp_started_cpus(void);
#endif
