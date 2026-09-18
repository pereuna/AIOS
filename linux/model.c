/* Persistent, framed stdin/stdout worker for console.py. Model output is data,
 * never a protocol command. A model panic is confined to this child process. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include "../neural.c"

static double seconds(void) {
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC,&t)) die("clock_gettime failed");
    return (double)t.tv_sec+(double)t.tv_nsec/1e9;
}
static int integer(const char *text, int lo, int hi) {
    char *end;
    long n=strtol(text,&end,10);
    if (end==text || (*end && strcmp(end,"\n")) || n<lo || n>hi) return -1;
    return (int)n;
}
static void status(State *s, int tokens) {
    printf("STATE %d %d %u %d %s\n",s->pos,s->ctx,bm_parallel_limit(),
           tokens,bm_simd_auto() ? "auto" : "sse2");
}
static void chat(Model *m, State *s, const char *prompt, int tokens) {
    /* Bounded UTF-8 request plus system text fits even before BPE merging. */
    int *ids=alloc(32768*sizeof(int));
    int n=turn_tokens(m,prompt,s->pos==0,ids,32768), reset=0;
    if (s->pos+n+3>s->ctx) {
        n=turn_tokens(m,prompt,1,ids,32768);
        if (n+3>s->ctx) {
            puts("ERROR Question exceeds context; shorten it or increase --context.");
            free(ids); return;
        }
        s->pos=0; reset=1;
    }
    printf("BEGIN %d %d\n",n,reset); fflush(stdout);
    bm_simd_reset(); bm_parallel_begin();
    double start=seconds();
    for (int i=0;i<n;i++) {
        forward(m,s,ids[i],i==n-1);
        if ((i+1)%64==0 || i+1==n) {
            printf("PREFILL %d %d\n",i+1,n); fflush(stdout);
        }
    }
    double ready=seconds();
    int count=0;
    const char *stop="token_limit";
    while (count<tokens) {
        if (s->pos+2>=s->ctx) { stop="context_limit"; break; }
        int token=greedy(s->logits); count++;
        if (model_token_end(token)) { stop="complete"; break; }
        if (!model_token_text(token)) { stop="invalid_token"; break; }
        Word w=m->words[token];
        if (memchr(w.p,0,w.n)) { stop="invalid_token"; break; }
        printf("TEXT %u\n",w.n);
        if (fwrite(w.p,1,w.n,stdout)!=w.n || fflush(stdout)) die("output pipe failed");
        forward(m,s,token,1);
    }
    /* Same message closure and context accounting as baremetal/main.c. */
    forward(m,s,MODEL_EOS,0); forward(m,s,(int)m->byte_id['\n'],0);
    bm_parallel_end(); free(ids);
    printf("DONE %d %d %s %.6f %.6f\n",s->pos,count,stop,ready-start,seconds()-ready);
}
int main(int argc, char **argv) {
    if (argc!=6) {
        fputs("Usage: aios-model MODEL CONTEXT TOKENS THREADS auto|sse2\n",stderr); return 2;
    }
    int ctx=integer(argv[2],8,MAXCTX), tokens=integer(argv[3],1,MAXCTX);
    int threads=integer(argv[4],1,BM_MAX_THREADS);
    if (ctx<0 || tokens<0 || threads<0 || (strcmp(argv[5],"auto") && strcmp(argv[5],"sse2")))
        die("invalid model settings");
    bm_parallel_set_limit((unsigned)threads); bm_simd_set_auto(!strcmp(argv[5],"auto"));
    int fd=open(argv[1],O_RDONLY);
    if (fd<0) { perror(argv[1]); return 1; }
    struct stat st;
    if (fstat(fd,&st) || st.st_size!=(off_t)MODEL_BYTES)
        die("expected an 872253632-byte QWENQ4 model; use make console-model or --model");
    void *data=mmap(NULL,(size_t)st.st_size,PROT_READ,MAP_PRIVATE,fd,0);
    if (data==MAP_FAILED) { perror("mmap model"); close(fd); return 1; }
    Model *m=alloc(sizeof(*m)); init_model(m,data,(size_t)st.st_size);
    State *s=new_state(m,ctx);
    puts("READY QWENQ4"); fflush(stdout);
    char line[128];
    while (fgets(line,sizeof(line),stdin)) {
        if (!strcmp(line,"QUIT\n")) break;
        if (!strcmp(line,"RESET\n")) { s->pos=0; status(s,tokens); }
        else if (!strcmp(line,"STATS\n")) status(s,tokens);
        else if (!strncmp(line,"TOKENS ",7)) {
            int n=integer(line+7,1,MAXCTX);
            if (n<0) puts("ERROR Invalid token limit."); else { tokens=n; status(s,tokens); }
        } else if (!strncmp(line,"THREADS ",8)) {
            int n=integer(line+8,1,BM_MAX_THREADS);
            if (n<0) puts("ERROR Invalid thread limit.");
            else { bm_parallel_set_limit((unsigned)n); status(s,tokens); }
        } else if (!strcmp(line,"SIMD auto\n") || !strcmp(line,"SIMD sse2\n")) {
            bm_simd_set_auto(!strcmp(line,"SIMD auto\n")); status(s,tokens);
        } else if (!strncmp(line,"CHAT ",5)) {
            int n=integer(line+5,1,4095);
            if (n<0) die("invalid request size");
            char prompt[4096];
            if (fread(prompt,1,(size_t)n,stdin)!=(size_t)n) die("incomplete request");
            prompt[n]=0;
            if (memchr(prompt,0,(size_t)n)) puts("ERROR NUL in prompt.");
            else chat(m,s,prompt,tokens);
        } else puts("ERROR Unknown worker command.");
        fflush(stdout);
    }
    free_state(s); free(m); munmap(data,(size_t)st.st_size); close(fd);
    return 0;
}
