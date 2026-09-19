/* Host tests for command parsing. Actual ring3 execution is tested in OVMF. */
#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include "../baremetal/process.h"
#include "../baremetal/process_console.h"

static unsigned calls;
static unsigned char captured[BM_PROCESS_MAX_CODE];
static size_t captured_size;
static char output[32768];
static size_t out_size;
void bm_putc(char c) { assert(out_size+1<sizeof(output)); output[out_size++]=c; output[out_size]=0; }
void bm_puts(const char *s) { while (*s) bm_putc(*s++); }
void bm_uint(uint64_t n) { char s[32]; snprintf(s,sizeof(s),"%llu",(unsigned long long)n); bm_puts(s); }
void bm_hex(uint64_t n, int digits) {
    char s[32]; snprintf(s,sizeof(s),"%0*llx",digits,(unsigned long long)n); bm_puts(s);
}
int bm_process_run(const void *p, size_t n, bm_process_result *r) {
    calls++; captured_size=n; assert(n<=sizeof(captured)); memcpy(captured,p,n);
    memset(r,0,sizeof(*r)); r->status=BM_PROCESS_FAULT_BASE; r->fault_vector=0;
    return r->status; /* Specifically exercise the old vector-zero display bug. */
}
int bm_process_run_input(const void *p, size_t n, const uint32_t *input, size_t count, bm_process_result *r) {
    (void)input; (void)count; return bm_process_run(p,n,r);
}
static void reset(void) { out_size=0; output[0]=0; }
static void reject(const char *s) {
    unsigned before=calls; reset(); assert(bm_process_command(s)); assert(calls==before);
}
static void addition(const char *s, uint32_t expected_a, uint32_t expected_b) {
    reset(); unsigned before=calls; assert(bm_process_command(s) && calls==before+1);
    /* Decode the tiny nibble builder independently, checking the requested
     * operands, including numbers whose raw bytes resemble POPF/IRET/MOV SS. */
    size_t at=0;
    for (unsigned operand=0;operand<2;operand++) {
        assert(captured[at++]==0x31 && captured[at++]==0xc0);
        uint32_t value=0;
        for (unsigned i=0;i<8;i++) {
            assert(captured[at++]==0xc1 && captured[at++]==0xe0 && captured[at++]==4);
            assert(captured[at++]==0x0c && captured[at]<16);
            value=(value<<4)|captured[at++];
        }
        assert(value==(operand ? expected_b : expected_a));
        if (!operand) assert(captured[at++]==0x89 && captured[at++]==0xc1);
    }
    assert(captured[at++]==1 && captured[at++]==0xc8);
    assert(captured[at++]==0xcd && captured[at++]==0x80 && at==captured_size);
}
int main(void) {
    unsigned char bytes[16]={0};
    assert(bm_process_parse_hex(" b8 2A000000 ",bytes,sizeof(bytes))==7);
    const unsigned char expected[]={0xb8,42,0,0,0,0xcd,0x80};
    assert(!memcmp(bytes,expected,sizeof(expected)));
    assert(bm_process_parse_hex("9",bytes,sizeof(bytes))<0);
    assert(bm_process_parse_hex("90 9",bytes,sizeof(bytes))<0);
    assert(bm_process_parse_hex("9 0",bytes,sizeof(bytes))<0);
    assert(bm_process_parse_hex("zz",bytes,sizeof(bytes))<0);
    assert(bm_process_parse_hex("",bytes,sizeof(bytes))<0);
    assert(bm_process_parse_hex("90",bytes,2)<0);
    assert(bm_process_parse_hex("9090",bytes,3)<0);
    assert(bm_process_parse_hex("90",bytes,3)==3);
    assert(bm_process_parse_hex(NULL,bytes,16)<0);
    long page=sysconf(_SC_PAGESIZE);
    char *guard=mmap(NULL,(size_t)page*2,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    assert(guard!=MAP_FAILED && !mprotect(guard+page,(size_t)page,PROT_NONE));
    guard[page-2]='9'; guard[page-1]=0;
    assert(bm_process_parse_hex(guard+page-2,bytes,16)<0);
    assert(!munmap(guard,(size_t)page*2));
    assert(!bm_process_command("normal question"));
    assert(!bm_process_command("/runner"));
    reject("/run help"); reject("/run nonsense"); reject("/exec 9");
    reject("/run add"); reject("/run add 1"); reject("/run add -1 2");
    reject("/run add 4294967296 2"); reject("/run add 1 2 junk");
    reject("/run add 1x 2"); reject("/run add 99999999999999999999 1");
    addition("/run add 12 30",12,30);
    addition("/run add 157 207",157,207);
    addition("/run add 4294967295 0",UINT32_MAX,0);
    addition("/run add\t142\t0  ",142,0);
    reset(); assert(bm_process_command("/run div0"));
    assert(strstr(output,"divide by zero (#DE)") && strstr(output,"vector 0"));
    reset(); unsigned before=calls;
    assert(bm_process_command("/excec 90") && calls==before+1);
    assert(captured_size==3 && captured[0]==0x90);
    reset();
    puts("Process console: named commands, uint32 inputs, malformed/guarded HEX and #DE passed");
}
