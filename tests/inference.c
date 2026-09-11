/* Host-only harness: execute the same transformer and math as the EFI build. */
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "../neural.c"

_Noreturn void bm_panic(const char *message) {
    fprintf(stderr,"PANIC: %s\n",message); exit(1);
}
void bm_check_finite(const char *where, const float *values, size_t n, int pos, int layer) {
    (void)pos; (void)layer;
    for (size_t i=0;i<n;i++) if (!isfinite(values[i])) bm_panic(where);
}
int main(int argc, char **argv) {
    if (argc<2 || argc>3) return 2;
    int fd=open(argv[1],O_RDONLY);
    struct stat st;
    if (fd<0 || fstat(fd,&st)) bm_panic("cannot open model");
    void *data=mmap(NULL,(size_t)st.st_size,PROT_READ,MAP_PRIVATE,fd,0);
    if (data==MAP_FAILED) bm_panic("cannot map model");
    Model *m=alloc(sizeof(*m)); init_model(m,data,(size_t)st.st_size);
    if (argc==3) {
        State *s=new_state(m,256);
        int ids[256], n=turn_tokens(m,argv[2],1,ids,256);
        for (int i=0;i<n;i++) forward(m,s,ids[i],i==n-1);
        for (int i=0;i<24;i++) {
            int token=greedy(s->logits);
            if (token==0 || token==2) break;
            if ((unsigned)token>=m->nspecial) fwrite(m->words[token].p,1,m->words[token].n,stdout);
            forward(m,s,token,1);
        }
        putchar('\n'); free_state(s);
    } else {
        const int tokens[]={1,9690,198,19556};
        State *s=new_state(m,8);
        for (unsigned i=0;i<sizeof(tokens)/sizeof(*tokens);i++) {
            forward(m,s,tokens[i],1);
            if (fwrite(s->logits,sizeof(float),V,stdout)!=V) return 1;
        }
        free_state(s);
    }
    free(m); munmap(data,(size_t)st.st_size); close(fd);
    return 0;
}
