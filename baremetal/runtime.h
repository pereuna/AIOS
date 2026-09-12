#ifndef SMOL_RUNTIME_H
#define SMOL_RUNTIME_H
#include <stddef.h>
#include <stdint.h>

void *memcpy(void *, const void *, size_t);
void *memmove(void *, const void *, size_t);
void *memset(void *, int, size_t);
int memcmp(const void *, const void *, size_t);
size_t strlen(const char *);
int strcmp(const char *, const char *);
void *calloc(size_t, size_t);
void free(void *);

float bm_expf(float);
float bm_powf(float, float);
float bm_sinf(float);
float bm_cosf(float);
float bm_sqrtf(float);
float bm_ldexpf(float, int);
#define expf bm_expf
#define powf bm_powf
#define sinf bm_sinf
#define cosf bm_cosf
#define sqrtf bm_sqrtf
#define ldexpf bm_ldexpf
#define isfinite(x) __builtin_isfinite(x)

void bm_init(void);
enum { BM_MAX_THREADS=4 };
typedef void (*bm_row_task)(void *, int, int);
unsigned bm_parallel_begin(void);
void bm_parallel_end(void);
void bm_parallel_rows(bm_row_task, void *, int);
unsigned bm_parallel_count(void);
unsigned bm_parallel_limit(void);
void bm_parallel_set_limit(unsigned);
const char *bm_parallel_mode(void);
typedef struct { uint64_t rflags, cr4, xcr0; unsigned changed; } bm_avx2_scope;
int bm_cpu_avx2_hardware(void);
int bm_cpu_avx2_available(void);
int bm_avx2_begin(bm_avx2_scope *);
void bm_avx2_end(bm_avx2_scope *);
const char *bm_cpu_avx2_status(void);
int bm_simd_auto(void);
void bm_simd_set_auto(int);
void bm_simd_reset(void);
void bm_simd_record(int);
const char *bm_simd_used(void);
void bm_fp_prepare(void);
void bm_check_finite(const char *, const float *, size_t, int, int);
_Noreturn void bm_main(void);
void bm_reserve_heap(size_t);
int bm_pages_alloc(size_t, uintptr_t *);
void bm_pages_free(uintptr_t, size_t);
const void *bm_load_model(size_t);
void bm_heap_init(uintptr_t, uintptr_t);
size_t bm_heap_available(void);
void bm_putc(char);
void bm_write(const void *, size_t);
void bm_puts(const char *);
void bm_uint(uint64_t);
void bm_hex(uint64_t, int);
int bm_readline(char *, size_t);
uint32_t bm_crc32(const void *, size_t);
_Noreturn void bm_panic(const char *);
_Noreturn void bm_shutdown(void);
extern volatile uint64_t bm_ticks;
#endif
