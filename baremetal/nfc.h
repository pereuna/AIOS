/* Include after UTF-8 decoding/allocation helpers in neural.c. */
#include "nfc_data.h"

static const uint32_t *nfc_lookup(const uint32_t *table, size_t count, size_t width, uint32_t cp) {
    size_t lo=0,hi=count;
    while (lo<hi) {
        size_t mid=lo+(hi-lo)/2;
        const uint32_t *row=table+mid*width;
        if (cp<row[0]) hi=mid;
        else if (cp>row[0]) lo=mid+1;
        else return row;
    }
    return NULL;
}
static unsigned combining(uint32_t cp) {
    const uint32_t *r=nfc_lookup(*nfc_class,sizeof(nfc_class)/sizeof(*nfc_class),2,cp);
    return r ? r[1] : 0;
}
static uint32_t compose(uint32_t a, uint32_t b) {
    /* Algorithmic Hangul composition. */
    if (a>=0x1100 && a<0x1113 && b>=0x1161 && b<0x1176)
        return 0xac00+((a-0x1100)*21+b-0x1161)*28;
    if (a>=0xac00 && a<0xd7a4 && (a-0xac00)%28==0 && b>0x11a7 && b<0x11c3)
        return a+b-0x11a7;
    size_t lo=0,hi=sizeof(nfc_comp)/sizeof(*nfc_comp);
    while (lo<hi) {
        size_t mid=lo+(hi-lo)/2;
        const uint32_t *r=nfc_comp[mid];
        if (a<r[0] || (a==r[0] && b<r[1])) hi=mid;
        else if (a>r[0] || (a==r[0] && b>r[1])) lo=mid+1;
        else return r[2];
    }
    return 0;
}
static unsigned char *normalize_nfc(const unsigned char *text, size_t len, size_t *bytes) {
    uint32_t *cp=alloc((len+1)*4*sizeof(uint32_t));
    size_t count=0;
    for (size_t i=0,k;i<len;i+=k) {
        uint32_t c=utf8(text+i,len-i,&k);
        if (c>=0xac00 && c<0xd7a4) {
            uint32_t s=c-0xac00;
            cp[count++]=0x1100+s/(21*28); cp[count++]=0x1161+(s/28)%21;
            if (s%28) cp[count++]=0x11a7+s%28;
        } else {
            const uint32_t *r=nfc_lookup(*nfc_decomp,sizeof(nfc_decomp)/sizeof(*nfc_decomp),6,c);
            if (r) for (unsigned j=0;j<r[1];j++) cp[count++]=r[2+j];
            else cp[count++]=c;
        }
    }
    for (size_t i=1;i<count;i++) {
        unsigned cls=combining(cp[i]);
        if (!cls) continue;
        size_t j=i;
        while (j && combining(cp[j-1])>cls) {
            uint32_t tmp=cp[j]; cp[j]=cp[j-1]; cp[--j]=tmp;
        }
    }
    size_t used=0,starter=0; unsigned previous=0;
    for (size_t i=0;i<count;i++) {
        uint32_t c=cp[i]; unsigned cls=combining(c);
        uint32_t combined=used && (!previous || previous<cls) ? compose(cp[starter],c) : 0;
        if (combined) cp[starter]=combined;
        else {
            if (!cls) starter=used;
            cp[used++]=c; previous=cls;
        }
    }
    unsigned char *out=alloc(used*4+1); size_t n=0;
    for (size_t i=0;i<used;i++) {
        uint32_t c=cp[i];
        if (c<0x80) out[n++]=(unsigned char)c;
        else if (c<0x800) { out[n++]=0xc0|(c>>6); out[n++]=0x80|(c&63); }
        else if (c<0x10000) {
            out[n++]=0xe0|(c>>12); out[n++]=0x80|((c>>6)&63); out[n++]=0x80|(c&63);
        } else {
            out[n++]=0xf0|(c>>18); out[n++]=0x80|((c>>12)&63);
            out[n++]=0x80|((c>>6)&63); out[n++]=0x80|(c&63);
        }
    }
    free(cp); *bytes=n; return out;
}
