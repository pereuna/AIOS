/* Native inference test backend. NEVER executes generated code on the host. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "../baremetal/process.h"
void bm_init(void) {}
void bm_reserve_heap(size_t n) { (void)n; }
const void *bm_load_model(size_t bytes) {
    int fd=open("model.bin",O_RDONLY); struct stat st;
    if (fd<0 || fstat(fd,&st) || (uint64_t)st.st_size!=bytes) { fputs("invalid model.bin\n",stderr); exit(1); }
    void *data=mmap(NULL,bytes,PROT_READ,MAP_PRIVATE,fd,0); close(fd);
    if (data==MAP_FAILED) { perror("mmap model");exit(1); }
    return data;
}
int bm_process_run_input(const void *program,size_t bytes,const uint32_t *input,size_t count,bm_process_result *result) {
    memset(result,0,sizeof(*result)); result->status=BM_PROCESS_INTERNAL;
    if (!program || !bytes || bytes>BM_PROCESS_MAX_CODE || count>256 || (count && !input)) return result->status;
    FILE *header=fopen(".build/calc-request.h","w");
    if (!header) return result->status;
    fputs("static const unsigned char request_code[]={",header);
    const unsigned char *code=program;
    for (size_t i=0;i<bytes;i++) fprintf(header,"%s0x%02x",i ? "," : "",code[i]);
    fputs("};\nstatic const uint32_t request_input[]={",header);
    for (size_t i=0;i<count;i++) fprintf(header,"%s%uu",i ? "," : "",input[i]);
    if (!count) fputs("0",header);
    fprintf(header,"};\nstatic const size_t request_input_count=%zu;\n",count);
    if (fclose(header)) return result->status;
    /* The command is constant. Model output is encoded as integer DATA in
     * the header above; no generated text enters a shell command. */
    if (system("make .build/calc-request.efi > .build/calc-request-build.log 2>&1 && python3 tests/process_qemu.py --image .build/calc-request.efi --log .build/calc-request.log > .build/calc-request-run.log 2>&1")) {
        fputs("QEMU transport failed; see .build/calc-request-run.log and build.log\n",stderr);
        return result->status;
    }
    FILE *log=fopen(".build/calc-request.log","r"); if (!log) return result->status;
    char line[256]; unsigned status,vector; unsigned long long rax,steps; int found=0;
    while (fgets(line,sizeof(line),log)) {
        if (sscanf(line,"CALC_RESULT %x %llx %llx %x",&status,&rax,&steps,&vector)==4) {
            result->status=(int32_t)status;result->rax=rax;result->steps=steps;result->fault_vector=vector;
            found=1;break;
        }
    }
    fclose(log); return found ? result->status : BM_PROCESS_INTERNAL;
}
