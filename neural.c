/* Qwen2.5-Coder-1.5B-Instruct inference for the x86-64 UEFI application.
 * Q4 weights are loaded from USB once and read directly from RAM. */
#include "baremetal/runtime.h"
#include "baremetal/calc_prompt.h"
#include "baremetal/model_tokens.h"
#include <immintrin.h>

enum { D=1536, H=8960, L=28, NH=12, NK=2, V=151936, HS=128, KD=256,
       G=32, BLOCK=18, MAXCTX=8192, HASH=524288 };
#define MODEL_BYTES UINT64_C(872253632)
typedef struct { const unsigned char *p; uint32_t n; } Word;
typedef struct { uint32_t a, b, out, rank; } Merge;
typedef struct { const float *n1, *n2, *qb, *kb, *vb; const unsigned char *q,*k,*v,*o,*gate,*up,*down; } Layer;
typedef struct {
    const unsigned char *map, *end, *embed, *output, *ranges;
    size_t size;
    uint32_t byte_id[256], nadded, nrange;
    Word words[V];
    Merge merges[HASH];
    Layer layers[L];
    const float *norm;
    float eps, theta;
} Model;
typedef struct {
    int ctx, pos;
    float x[D], xb[D], q[D], k[KD], v[KD], hb[H], hb2[H], logits[V];
    float *keys, *values, *att, *rope;
} State;

static void die(const char *s) { bm_panic(s); }
static void *alloc(size_t n) { void *p=calloc(1,n ? n : 1); if (!p) die("out of memory"); return p; }
static uint32_t u32(const void *p) { uint32_t n; memcpy(&n,p,4); return n; }
static const unsigned char *take(Model *m, const unsigned char **p, size_t n) {
    if (n > (size_t)(m->end-*p)) die("truncated model");
    const unsigned char *r=*p; *p+=n; return r;
}
static uint32_t hash_pair(uint32_t a, uint32_t b) {
    return (a*0x9e3779b1u ^ b*0x85ebca6bu) & (HASH-1);
}
static Merge *pair(Model *m, uint32_t a, uint32_t b) {
    uint32_t i=hash_pair(a,b);
    while (m->merges[i].rank && (m->merges[i].a!=a || m->merges[i].b!=b)) i=(i+1)&(HASH-1);
    return &m->merges[i];
}
static void init_model(Model *m, const void *data, size_t size) {
    if (sizeof(float)!=4 || u32("\1\0\0\0")!=1) die("requires little-endian IEEE float32 host");
    if (size!=MODEL_BYTES) die("invalid Qwen2.5-Coder-1.5B model size");
    m->size=size; m->map=data; m->end=m->map+size;
    const unsigned char *p=m->map;
    if (memcmp(p,"QWENQ4\0\0",8)) die("bad model magic");
    const uint32_t expected[]={1,D,H,L,NH,NK,V,32768,G,MODEL_BOS,MODEL_EOS};
    for (unsigned i=0;i<sizeof(expected)/sizeof(*expected);i++)
        if (u32(p+8+4*i)!=expected[i]) die("unsupported model configuration");
    m->nadded=u32(p+52);
    uint32_t nm=u32(p+56); m->nrange=u32(p+60);
    memcpy(&m->eps,p+64,4); memcpy(&m->theta,p+68,4);
    if (m->nadded!=MODEL_ADDED_COUNT || nm!=151387 || m->nrange>5000 ||
        !isfinite(m->eps) || m->eps<=0 || !isfinite(m->theta) || m->theta<=0)
        die("invalid model header");
    p+=256;
    for (int i=0;i<256;i++) {
        m->byte_id[i]=u32(take(m,&p,4));
        if (m->byte_id[i]>=V && m->byte_id[i]!=UINT32_MAX) die("invalid byte token");
    }
    for (int i=0;i<V;i++) {
        m->words[i].n=u32(take(m,&p,4));
        if (!m->words[i].n || m->words[i].n>16384) die("invalid token length");
        m->words[i].p=take(m,&p,m->words[i].n);
    }
    for (uint32_t r=1;r<=nm;r++) {
        uint32_t a=u32(take(m,&p,4)), b=u32(take(m,&p,4)), out=u32(take(m,&p,4));
        if (a>=V || b>=V || out>=V) die("invalid merge token");
        Merge *slot=pair(m,a,b);
        if (!slot->rank) *slot=(Merge){a,b,out,r};
    }
    m->ranges=take(m,&p,(size_t)m->nrange*12);
    uint32_t last=0;
    for (uint32_t i=0;i<m->nrange;i++) {
        const unsigned char *r=m->ranges+12*i;
        uint32_t a=u32(r), b=u32(r+4), kind=u32(r+8);
        if (a>b || b>0x10ffff || (i && a<=last) || kind<1 || kind>3) die("invalid Unicode table");
        last=b;
    }
    take(m,&p, (64-(size_t)(p-m->map)%64)%64);
    m->embed=take(m,&p,(size_t)V*D/G*BLOCK);
    m->norm=(const float *)take(m,&p,D*4);
    for (int i=0;i<L;i++) {
        Layer *l=&m->layers[i];
        l->n1=(const float *)take(m,&p,D*4); l->n2=(const float *)take(m,&p,D*4);
        l->qb=(const float *)take(m,&p,D*4);
        l->kb=(const float *)take(m,&p,KD*4); l->vb=(const float *)take(m,&p,KD*4);
        l->q=take(m,&p,D*D/G*BLOCK); l->k=take(m,&p,KD*D/G*BLOCK);
        l->v=take(m,&p,KD*D/G*BLOCK); l->o=take(m,&p,D*D/G*BLOCK);
        l->gate=take(m,&p,H*D/G*BLOCK); l->up=take(m,&p,H*D/G*BLOCK);
        l->down=take(m,&p,D*H/G*BLOCK);
    }
    m->output=m->embed; /* Tied embedding/output weights. */
    if (p!=m->end) die("unexpected trailing model data");
}
/* UTF-8 decoder: reject malformed input instead of silently changing the prompt. */
static uint32_t utf8(const unsigned char *s, size_t n, size_t *len) {
    uint32_t c=s[0];
    if (c<128) { *len=1; return c; }
    size_t k=c>=0xf0 ? 4 : c>=0xe0 ? 3 : 2;
    if (c<0xc2 || c>0xf4 || k>n) die("invalid UTF-8 input");
    c&=(1u<<(7-k))-1;
    for (size_t j=1;j<k;j++) {
        if ((s[j]&0xc0)!=0x80) die("invalid UTF-8 input");
        c=(c<<6)|(s[j]&63);
    }
    if (c<(k==2 ? 128u : k==3 ? 2048u : 65536u) || c>0x10ffff || (c>=0xd800 && c<=0xdfff))
        die("invalid UTF-8 input");
    *len=k; return c;
}
static int category(Model *m, uint32_t c) {
    uint32_t lo=0,hi=m->nrange;
    while (lo<hi) {
        uint32_t mid=lo+(hi-lo)/2;
        const unsigned char *r=m->ranges+12*mid;
        if (c<u32(r)) hi=mid;
        else if (c>u32(r+4)) lo=mid+1;
        else return (int)u32(r+8);
    }
    return 0;
}
static int kind_at(Model *m, const unsigned char *s, size_t n, size_t *k) {
    return category(m,utf8(s,n,k));
}
static void emit(int *ids, int *n, int cap, int id) {
    if (*n>=cap) die("input exceeds context; shorten it or increase -c");
    ids[(*n)++]=id;
}
static void bpe(Model *m, const unsigned char *s, size_t len, int *ids, int *n, int cap) {
    int *tmp=alloc(len*sizeof(int));
    size_t used=0;
    for (size_t i=0;i<len;i++)
        if (m->byte_id[s[i]]!=UINT32_MAX) tmp[used++]=(int)m->byte_id[s[i]];
    len=used;
    /* Small, transparent O(n^2) BPE within each pre-token; exact merge ranks. */
    while (len>1) {
        uint32_t rank=UINT32_MAX, out=0; size_t best=0;
        for (size_t i=0;i+1<len;i++) {
            Merge *p=pair(m,(uint32_t)tmp[i],(uint32_t)tmp[i+1]);
            if (p->rank && p->rank<rank) { rank=p->rank; best=i; out=p->out; }
        }
        if (rank==UINT32_MAX) break;
        tmp[best]=(int)out;
        memmove(tmp+best+1,tmp+best+2,(len-best-2)*sizeof(int)); len--;
    }
    for (size_t i=0;i<len;i++) emit(ids,n,cap,tmp[i]);
    free(tmp);
}
#include "baremetal/nfc.h"
/* Qwen's case-insensitive contractions and Unicode pre-token regex. */
static void ordinary(Model *m, const unsigned char *input, size_t length, int *ids, int *n, int cap) {
    size_t len;
    unsigned char *s=normalize_nfc(input,length,&len);
    static const char *contractions[]={"'s","'t","'re","'ve","'m","'ll","'d"};
    size_t i=0;
    while (i<len) {
        size_t end=i,k;
        for (unsigned j=0;j<7;j++) {
            size_t l=strlen(contractions[j]),matched=0;
            if (l>len-i) continue;
            while (matched<l) {
                unsigned char c=s[i+matched];
                if (c>='A' && c<='Z') c+=32;
                if (c!=(unsigned char)contractions[j][matched]) break;
                matched++;
            }
            if (matched==l) { end=i+l; break; }
        }
        int cat=kind_at(m,s+i,len-i,&k);
        if (end==i) {
            /* An optional non-letter/number/newline prefix before letters. */
            size_t start=i;
            if (cat!=1 && cat!=2 && s[i]!='\r' && s[i]!='\n' && i+k<len) {
                size_t next;
                if (kind_at(m,s+i+k,len-i-k,&next)==1) start=i+k;
            }
            size_t letter;
            if (kind_at(m,s+start,len-start,&letter)==1) {
                end=start+letter;
                while (end<len && kind_at(m,s+end,len-end,&letter)==1) end+=letter;
            }
        }
        if (end==i && cat==2) end=i+k;
        if (end==i) {
            size_t start=i+(s[i]==' ' && i+1<len),symbol;
            int c=kind_at(m,s+start,len-start,&symbol);
            if (c==0) {
                end=start+symbol;
                while (end<len && kind_at(m,s+end,len-end,&symbol)==0) end+=symbol;
                while (end<len && (s[end]=='\r' || s[end]=='\n')) end++;
            }
        }
        if (end==i && cat==3) {
            size_t scan=i,previous=i,newline=i;
            while (scan<len && kind_at(m,s+scan,len-scan,&k)==3) {
                previous=scan; scan+=k;
                if (s[previous]=='\r' || s[previous]=='\n') newline=scan;
            }
            end=newline>i ? newline : scan<len && previous>i ? previous : scan;
        }
        if (end<=i) die("tokenizer made no progress");
        bpe(m,s+i,end-i,ids,n,cap); i=end;
    }
    free(s);
}
static int tokenize(Model *m, const char *text, int *ids, int cap, int special) {
    const unsigned char *s=(const unsigned char *)text;
    size_t len=strlen(text),start=0,i=0;
    int n=0;
    while (i<len) {
        int found=-1;
        if (special && s[i]=='<') for (uint32_t j=MODEL_ADDED_START;j<MODEL_ADDED_START+m->nadded;j++) {
            Word w=m->words[j];
            if (w.n<=len-i && !memcmp(s+i,w.p,w.n)) { found=(int)j; break; }
        }
        if (found>=0) {
            ordinary(m,s+start,i-start,ids,&n,cap); emit(ids,&n,cap,found);
            i+=m->words[found].n; start=i;
        } else i++;
    }
    ordinary(m,s+start,len-start,ids,&n,cap);
    return n;
}

/* Binary16 -> binary32, including subnormals; no F16C CPU extension needed. */
static float half(const unsigned char *p) {
    uint16_t h; memcpy(&h,p,2);
    uint32_t sign=(uint32_t)(h&0x8000)<<16, e=(h>>10)&31, f=h&1023, bits;
    if (!e) return (h&0x8000 ? -1.0f : 1.0f)*ldexpf((float)f,-24);
    bits=sign | ((e==31 ? 255 : e+112)<<23) | (f<<13);
    float v; memcpy(&v,&bits,4); return v;
}
static __m128 dot16(__m128i q, const float *x) {
    __m128i z=_mm_setzero_si128();
    __m128i lo=_mm_unpacklo_epi8(q,z), hi=_mm_unpackhi_epi8(q,z);
    __m128 eight=_mm_set1_ps(8.0f);
    __m128 a=_mm_sub_ps(_mm_cvtepi32_ps(_mm_unpacklo_epi16(lo,z)),eight);
    __m128 b=_mm_sub_ps(_mm_cvtepi32_ps(_mm_unpackhi_epi16(lo,z)),eight);
    __m128 c=_mm_sub_ps(_mm_cvtepi32_ps(_mm_unpacklo_epi16(hi,z)),eight);
    __m128 d=_mm_sub_ps(_mm_cvtepi32_ps(_mm_unpackhi_epi16(hi,z)),eight);
    return _mm_add_ps(_mm_add_ps(_mm_mul_ps(a,_mm_loadu_ps(x)),_mm_mul_ps(b,_mm_loadu_ps(x+4))),
                      _mm_add_ps(_mm_mul_ps(c,_mm_loadu_ps(x+8)),_mm_mul_ps(d,_mm_loadu_ps(x+12))));
}
typedef struct { float *out; const unsigned char *w; const float *x; int cols; } MatvecJob;
static void matvec_rows_sse2(void *argument, int first, int last) {
    const MatvecJob *job=argument;
    float *out=job->out;
    const unsigned char *w=job->w;
    const float *x=job->x;
    int cols=job->cols;
    for (int row=first;row<last;row++) {
        const unsigned char *p=w+(size_t)row*(cols/G)*BLOCK;
        __m128 sum=_mm_setzero_ps();
        __m128i mask=_mm_set1_epi8(15);
        for (int j=0;j<cols;j+=G,p+=BLOCK) {
            __m128i q=_mm_loadu_si128((const __m128i *)(p+2));
            __m128 a=dot16(_mm_and_si128(q,mask),x+j);
            __m128 b=dot16(_mm_and_si128(_mm_srli_epi16(q,4),mask),x+j+16);
            sum=_mm_add_ps(sum,_mm_mul_ps(_mm_set1_ps(half(p)),_mm_add_ps(a,b)));
        }
        float s[4]; _mm_storeu_ps(s,sum); out[row]=(s[0]+s[1])+(s[2]+s[3]);
    }
}
/* Each AVX2 multiply processes eight weights. Reduce its two 128-bit halves
 * before adding the next product, preserving dot16's exact SSE2 sum tree.
 * No FMA/F16C requirement, and no reassociation or contraction of FP sums. */
#define AVX2_TARGET __attribute__((target("avx2,no-fma")))
static inline AVX2_TARGET __m128 dot16_avx2(__m128i q, const float *x) {
    __m256 eight=_mm256_set1_ps(8.0f);
    __m256 a=_mm256_sub_ps(_mm256_cvtepi32_ps(_mm256_cvtepu8_epi32(q)),eight);
    __m256 b=_mm256_sub_ps(_mm256_cvtepi32_ps(_mm256_cvtepu8_epi32(_mm_srli_si128(q,8))),eight);
    a=_mm256_mul_ps(a,_mm256_loadu_ps(x));
    b=_mm256_mul_ps(b,_mm256_loadu_ps(x+8));
    return _mm_add_ps(_mm_add_ps(_mm256_castps256_ps128(a),_mm256_extractf128_ps(a,1)),
                      _mm_add_ps(_mm256_castps256_ps128(b),_mm256_extractf128_ps(b,1)));
}
static inline AVX2_TARGET float half_avx2(const unsigned char *p) {
    /* Keep scale decoding inline: a legacy SSE call in this inner loop would
     * spill vector registers and add AVX/SSE transition overhead per block. */
    uint32_t h=(uint32_t)p[0]|((uint32_t)p[1]<<8);
    uint32_t sign=(h&0x8000)<<16, e=(h>>10)&31, f=h&1023;
    if (!e) return (h&0x8000 ? -1.0f : 1.0f)*((float)f*0x1p-24f);
    uint32_t bits=sign|((e==31 ? 255 : e+112)<<23)|(f<<13);
    float value; __builtin_memcpy(&value,&bits,sizeof(value)); return value;
}
static AVX2_TARGET __attribute__((noinline)) void matvec_rows_avx2(void *argument, int first, int last) {
    const MatvecJob *job=argument;
    for (int row=first;row<last;row++) {
        const unsigned char *p=job->w+(size_t)row*(job->cols/G)*BLOCK;
        __m128 sum=_mm_setzero_ps();
        const __m128i mask=_mm_set1_epi8(15);
        for (int j=0;j<job->cols;j+=G,p+=BLOCK) {
            __m128i q=_mm_loadu_si128((const __m128i *)(p+2));
            __m128 a=dot16_avx2(_mm_and_si128(q,mask),job->x+j);
            __m128 b=dot16_avx2(_mm_and_si128(_mm_srli_epi16(q,4),mask),job->x+j+16);
            sum=_mm_add_ps(sum,_mm_mul_ps(_mm_set1_ps(half_avx2(p)),_mm_add_ps(a,b)));
        }
        float s[4]; _mm_storeu_ps(s,sum); job->out[row]=(s[0]+s[1])+(s[2]+s[3]);
    }
    _mm256_zeroupper(); /* Return to the SSE2 runtime with no live YMM values. */
}
static void matvec_rows(void *argument, int first, int last) {
    /* This runs on each BSP/AP, after its FP preparation, not just at boot. */
    bm_avx2_scope scope;
    int avx2=bm_simd_auto() && bm_avx2_begin(&scope);
    bm_simd_record(avx2);
    if (avx2) { matvec_rows_avx2(argument,first,last); bm_avx2_end(&scope); }
    else matvec_rows_sse2(argument,first,last);
}
static void matvec(float *out, const unsigned char *w, const float *x, int rows, int cols) {
    MatvecJob job={out,w,x,cols};
    bm_parallel_rows(matvec_rows,&job,rows);
}
static void embedding(float *x, const unsigned char *p) {
    for (int j=0;j<D;j+=G,p+=BLOCK) {
        float s=half(p);
        for (int k=0;k<16;k++) {
            x[j+k]=s*((int)(p[k+2]&15)-8);
            x[j+k+16]=s*((int)(p[k+2]>>4)-8);
        }
    }
}
static void rmsnorm(float *out, const float *x, const float *w, float eps) {
    float sum=0;
    for (int i=0;i<D;i++) sum+=x[i]*x[i];
    float scale=1.0f/sqrtf(sum/D+eps);
    for (int i=0;i<D;i++) out[i]=x[i]*scale*w[i];
}
static void softmax(float *x, int n) {
    float max=x[0],sum=0;
    for (int i=1;i<n;i++) if (x[i]>max) max=x[i];
    for (int i=0;i<n;i++) { x[i]=expf(x[i]-max); sum+=x[i]; }
    for (int i=0;i<n;i++) x[i]/=sum;
}
static State *new_state(Model *m, int ctx) {
    bm_fp_prepare();
    State *s=alloc(sizeof(*s)); s->ctx=ctx;
    s->keys=alloc((size_t)L*ctx*KD*sizeof(float));
    s->values=alloc((size_t)L*ctx*KD*sizeof(float));
    s->att=alloc((size_t)NH*ctx*sizeof(float));
    s->rope=alloc((size_t)ctx*HS*sizeof(float));
    for (int p=0;p<ctx;p++) for (int i=0;i<HS/2;i++) {
        float angle=(float)p/powf(m->theta,(float)(2*i)/HS);
        s->rope[p*HS+i]=cosf(angle); s->rope[p*HS+i+HS/2]=sinf(angle);
    }
    bm_check_finite("RoPE table",s->rope,(size_t)ctx*HS,-1,-1);
    return s;
}
static void free_state(State *s) {
    free(s->keys); free(s->values); free(s->att); free(s->rope); free(s);
}
static void rotate(float *x, int heads, const float *rope) {
    for (int h=0;h<heads;h++) for (int j=0;j<HS/2;j++) {
        int a=h*HS+j,b=a+HS/2;
        float u=x[a],v=x[b],c=rope[j],s=rope[j+HS/2];
        x[a]=u*c-v*s; x[b]=v*c+u*s;
    }
}
static void forward(Model *m, State *s, int token, int project) {
    bm_fp_prepare();
    int pos=s->pos,ctx=s->ctx;
    const float attention_scale=1.0f/sqrtf((float)HS);
    if (pos>=ctx || token<0 || token>=V) die("forward input out of bounds");
    embedding(s->x,m->embed+(size_t)token*(D/G)*BLOCK);
    bm_check_finite("embedding",s->x,D,pos,-1);
    for (int l=0;l<L;l++) {
        Layer *w=&m->layers[l];
        rmsnorm(s->xb,s->x,w->n1,m->eps);
        matvec(s->q,w->q,s->xb,D,D); matvec(s->k,w->k,s->xb,KD,D); matvec(s->v,w->v,s->xb,KD,D);
        for (int j=0;j<D;j++) s->q[j]+=w->qb[j];
        for (int j=0;j<KD;j++) { s->k[j]+=w->kb[j]; s->v[j]+=w->vb[j]; }
        rotate(s->q,NH,s->rope+pos*HS); rotate(s->k,NK,s->rope+pos*HS);
        float *kc=s->keys+(size_t)l*ctx*KD, *vc=s->values+(size_t)l*ctx*KD;
        memcpy(kc+(size_t)pos*KD,s->k,sizeof(s->k)); memcpy(vc+(size_t)pos*KD,s->v,sizeof(s->v));
        for (int h=0;h<NH;h++) {
            float *a=s->att+(size_t)h*ctx, *q=s->q+h*HS;
            int kh=h/(NH/NK)*HS;
            for (int t=0;t<=pos;t++) {
                const float *k=kc+(size_t)t*KD+kh;
                float dot=0;
                for (int j=0;j<HS;j++) dot+=q[j]*k[j];
                a[t]=dot*attention_scale;
            }
            softmax(a,pos+1);
            float *out=s->xb+h*HS; memset(out,0,HS*sizeof(float));
            for (int t=0;t<=pos;t++) {
                const float *v=vc+(size_t)t*KD+kh;
                for (int j=0;j<HS;j++) out[j]+=a[t]*v[j];
            }
        }
        matvec(s->q,w->o,s->xb,D,D);
        for (int i=0;i<D;i++) s->x[i]+=s->q[i];
        rmsnorm(s->xb,s->x,w->n2,m->eps);
        matvec(s->hb,w->gate,s->xb,H,D); matvec(s->hb2,w->up,s->xb,H,D);
        for (int i=0;i<H;i++) s->hb[i]=s->hb[i]/(1.0f+expf(-s->hb[i]))*s->hb2[i];
        matvec(s->xb,w->down,s->hb,D,H);
        for (int i=0;i<D;i++) s->x[i]+=s->xb[i];
        bm_check_finite("layer output",s->x,D,pos,l);
    }
    if (project) {
        rmsnorm(s->xb,s->x,m->norm,m->eps);
        matvec(s->logits,m->output,s->xb,V,D); /* Tied output projection. */
        bm_check_finite("logits",s->logits,V,pos,L);
    }
    s->pos++;
}

static int greedy(const float *logits) {
    bm_fp_prepare();
    int best=0;
    for (int i=0;i<V;i++) {
        if (!isfinite(logits[i])) die("non-finite logits");
        if (logits[i]>logits[best]) best=i;
    }
    return best;
}
static int turn_tokens(Model *m, const char *prompt, int first, int *ids, int cap) {
    const char *system="<|im_start|>system\n" CALC_SYSTEM_PROMPT "<|im_end|>\n";
    size_t size=strlen(prompt)+strlen(system)+128;
    char *text=alloc(size);
    const char *parts[]={first ? system : "","<|im_start|>user\n",prompt,
        "<|im_end|>\n<|im_start|>assistant\n"};
    size_t at=0;
    for (unsigned i=0;i<4;i++) { size_t len=strlen(parts[i]); memcpy(text+at,parts[i],len); at+=len; }
    text[at]=0;
    int n=tokenize(m,text,ids,cap,1);
    free(text);
    return n;
}
