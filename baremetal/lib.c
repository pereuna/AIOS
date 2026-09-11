#include "runtime.h"

void *memset(void *p, int c, size_t n) {
    unsigned char *d=p;
    while (n--) *d++=(unsigned char)c;
    return p;
}
void *memcpy(void *p, const void *q, size_t n) {
    unsigned char *d=p; const unsigned char *s=q;
    while (n--) *d++=*s++;
    return p;
}
void *memmove(void *p, const void *q, size_t n) {
    unsigned char *d=p; const unsigned char *s=q;
    if ((uintptr_t)d<(uintptr_t)s) while (n--) *d++=*s++;
    else { d+=n; s+=n; while (n--) *--d=*--s; }
    return p;
}
int memcmp(const void *p, const void *q, size_t n) {
    const unsigned char *a=p,*b=q;
    for (size_t i=0;i<n;i++) if (a[i]!=b[i]) return a[i]<b[i] ? -1 : 1;
    return 0;
}
size_t strlen(const char *s) { size_t n=0; while (s[n]) n++; return n; }
int strcmp(const char *a, const char *b) {
    while (*a && *a==*b) { a++; b++; }
    return (unsigned char)*a-(unsigned char)*b;
}

/* Aligned first-fit heap. Free blocks coalesce; tokenization leaves no residue. */
typedef struct Block { size_t size; struct Block *next; uint64_t used, magic; } Block;
static Block *heap;
static uintptr_t heap_end;
#define HEAP_MAGIC UINT64_C(0x534d4f4c48454150)
void bm_heap_init(uintptr_t start, uintptr_t end) {
    start=(start+15)&~(uintptr_t)15;
    if (end<=start+sizeof(Block)) bm_panic("not enough RAM for heap");
    heap=(Block *)start; heap_end=end;
    *heap=(Block){end-start-sizeof(Block),NULL,0,HEAP_MAGIC};
}
void *calloc(size_t count, size_t size) {
    if (size && count>SIZE_MAX/size) return NULL;
    size_t n=count*size;
    if (n>SIZE_MAX-15) return NULL;
    n=(n+15)&~(size_t)15;
    if (!n) n=16;
    for (Block *b=heap;b;b=b->next) if (!b->used && b->size>=n) {
        if (b->size>=n+sizeof(Block)+16) {
            Block *next=(Block *)((unsigned char *)(b+1)+n);
            *next=(Block){b->size-n-sizeof(Block),b->next,0,HEAP_MAGIC};
            b->size=n; b->next=next;
        }
        b->used=1;
        return memset(b+1,0,n);
    }
    return NULL;
}
void free(void *p) {
    if (!p) return;
    if ((uintptr_t)p<(uintptr_t)(heap+1) || (uintptr_t)p>=heap_end || ((uintptr_t)p&15))
        bm_panic("invalid heap pointer");
    Block *b=(Block *)p-1;
    if (b->magic!=HEAP_MAGIC || !b->used) bm_panic("invalid or double free");
    b->used=0;
    for (b=heap;b && b->next;) {
        if (!b->used && !b->next->used) {
            b->size+=sizeof(Block)+b->next->size;
            b->next=b->next->next;
        } else b=b->next;
    }
}
size_t bm_heap_available(void) {
    size_t n=0;
    for (Block *b=heap;b;b=b->next) if (!b->used) n+=b->size;
    return n;
}
