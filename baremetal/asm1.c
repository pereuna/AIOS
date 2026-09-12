#include "runtime.h"
#include "asm1.h"

typedef struct { unsigned op, a, b, imm, line; char label[4]; } insn;
typedef struct { char name[4]; size_t at; } label;
enum { O_LI, O_MOV, O_ADD, O_SUB, O_MUL, O_UDIV, O_UMOD, O_AND, O_OR,
       O_XOR, O_SHL, O_SHR, O_EQ, O_LT, O_LD, O_ST, O_JMP, O_JZ, O_EXIT,
       O_LABEL };

static int ws(char c) { return c==' ' || c=='\t' || c=='\r'; }
static int regno(const char *s) {
    /* Virtual registers never alias RSP or the EAX/ECX/EDX/R10/R11 scratch set. */
    static const unsigned physical[]={3,6,7,8,9,12,13,14,15,5};
    if (s[0]!='r' || s[1]<'0' || s[1]>'9' || s[2]) return -1;
    return (int)physical[s[1]-'0'];
}
static int label_ok(const char *s) {
    if (s[0]!='l' || s[1]<'0' || s[1]>'9') return 0;
    if (!s[2]) return 1;
    return s[2]>='0' && s[2]<='9' && !s[3] && (s[1]-'0')*10+s[2]-'0'<=31;
}
static int number(const char *s, uint32_t *v) {
    uint32_t n=0; if (!*s) return -1;
    do { if (*s<'0'||*s>'9' || n>(UINT32_MAX-(unsigned)(*s-'0'))/10) return -1;
         n=n*10+(unsigned)(*s++-'0'); } while (*s);
    *v=n; return 0;
}
static char *word(char **p, char *dst, size_t cap) {
    char *s=*p; while (ws(*s)) s++; if (!*s) { *p=s; return NULL; }
    size_t n=0; while (*s && !ws(*s)) { if (n+1>=cap) { dst[0]='!'; dst[1]=0; while (*s&&!ws(*s)) s++; *p=s; return dst; } dst[n++]=*s++; }
    dst[n]=0; *p=s; return dst;
}
static int emit1(asm1_program *p, size_t *n, unsigned char x) {
    if (*n>=sizeof(p->code)) return p->error=ASM1_TOO_LARGE;
    p->code[(*n)++]=(unsigned char)x; return 0;
}
static int emit4(asm1_program *p, size_t *n, uint32_t x) {
    for (unsigned i=0;i<4;i++) if (emit1(p,n,(unsigned char)(x>>(8*i)))) return ASM1_TOO_LARGE;
    return 0;
}
static int rex(unsigned r, unsigned m) {
    return 0x40 | (r>=8 ? 4:0) | (m>=8 ? 1:0);
}
static int rr(asm1_program *p,size_t *n,unsigned char op,unsigned d,unsigned s) {
    if (d>=16||s>=16) return ASM1_REGISTER;
    /* 0xcf is rejected even inside ModRM by the process byte filter. */
    if (((d&7)<<3 | (s&7))==15) {
        rr(p,n,0x89,d,0);
        return rr(p,n,op,0,s);
    }
    if (d>=8||s>=8) emit1(p,n,rex(d,s));
    emit1(p,n,op); emit1(p,n,0xc0|((d&7)<<3)|(s&7)); return 0;
}
static int rr2(asm1_program *p,size_t *n,unsigned char op1,unsigned char op2,unsigned d,unsigned s) {
    if (d>=16||s>=16) return ASM1_REGISTER;
    if (((d&7)<<3 | (s&7))==15) {
        rr(p,n,0x89,s,0);
        return rr2(p,n,op1,op2,d,0);
    }
    if (d>=8||s>=8) emit1(p,n,rex(d,s));
    emit1(p,n,op1); emit1(p,n,op2); emit1(p,n,0xc0|((d&7)<<3)|(s&7)); return 0;
}
static int mov_imm(asm1_program *p,size_t *n,unsigned d,uint32_t v) {
    if (d>=16) return ASM1_REGISTER;
    if (d>=8) emit1(p,n,0x41);
    emit1(p,n,(unsigned char)(0xb8+(d&7))); return emit4(p,n,v);
}
static int mov_const(asm1_program *p,size_t *n,unsigned d,uint32_t v) {
    int unsafe=0; for (unsigned i=0;i<4;i++) { unsigned char b=(unsigned char)(v>>(8*i)); if (b==0x9d||b==0xcf||b==0x8e) unsafe=1; }
    if (!unsafe) return mov_imm(p,n,d,v);
    /* Build only from nibbles so the process' conservative byte filter cannot
     * mistake an immediate for POPF/IRET/MOV SS. */
    if (*n+68>=sizeof(p->code)) return ASM1_TOO_LARGE;
    emit1(p,n,0x45); emit1(p,n,0x31); emit1(p,n,0xd2);
    for (int shift=28;shift>=0;shift-=4) {
        emit1(p,n,0x41); emit1(p,n,0xc1); emit1(p,n,0xe2); emit1(p,n,4);
        emit1(p,n,0x41); emit1(p,n,0x83); emit1(p,n,0xca); emit1(p,n,(unsigned char)((v>>shift)&15));
    }
    return rr(p,n,0x89,10,d);
}
/* Stack cells begin at RSP-8176 when the process enters user mode. */
static int memop(asm1_program *p,size_t *n,unsigned char op,unsigned r,unsigned i) {
    if (r>=16||i>=256) return ASM1_REGISTER;
    int32_t disp=-(int32_t)(8192-16-i*4);
    if (r>=8) emit1(p,n,0x44);
    emit1(p,n,op); emit1(p,n,0x84|((r&7)<<3)); emit1(p,n,0x24); return emit4(p,n,(uint32_t)disp);
}
static int setcc(asm1_program *p,size_t *n,unsigned char cc,unsigned d) {
    if (d>=16) return ASM1_REGISTER;
    if (d>=8) emit1(p,n,0x41);
    emit1(p,n,0x0f); emit1(p,n,cc); emit1(p,n,(unsigned char)(0xc0|(d&7)));
    return 0;
}
static int compare_set(asm1_program *p,size_t *n,unsigned d,unsigned s,unsigned char cc) {
    int e=rr(p,n,0x31,10,10); if (e) return e;
    e=rr(p,n,0x39,s,d); if (e) return e;
    e=setcc(p,n,cc,10); if (e) return e;
    return rr(p,n,0x89,10,d);
}
static int divop(asm1_program *p,size_t *n,unsigned d,unsigned s,int rem) {
    /* r10 = dividend, r11 = divisor; EDX:EAX is the x86 DIV pair. */
    int e=rr(p,n,0x89,d,10); if (e) return e;
    e=rr(p,n,0x89,s,11); if (e) return e;
    e=emit1(p,n,0x31); if (e) return e; emit1(p,n,0xd2); /* xor edx,edx */
    e=rr(p,n,0x89,10,0); if (e) return e;
    emit1(p,n,0x41); emit1(p,n,0xf7); emit1(p,n,0xf3); /* div r11d */
    return rr(p,n,0x89,rem?2:0,d);
}
static int shiftop(asm1_program *p,size_t *n,unsigned d,unsigned s,int right) {
    int e=rr(p,n,0x89,d,10); if (e) return e; /* scratch destination */
    e=rr(p,n,0x89,s,1); if (e) return e; /* CL = count */
    emit1(p,n,0x41); emit1(p,n,0xd3); emit1(p,n,(unsigned char)((right?0xe8:0xe0)|2));
    return rr(p,n,0x89,10,d);
}
static int find_label(label *ls,unsigned n,const char *name) {
    for (unsigned i=0;i<n;i++) if (!strcmp(ls[i].name,name)) return (int)i;
    return -1;
}
int asm1_compile(const char *source, asm1_program *p) {
    if (!p) return ASM1_SOURCE;
    memset(p,0,sizeof(*p));
    if (!source) return p->error=ASM1_SOURCE;
    size_t length=0;
    while (length<ASM1_MAX_SOURCE && source[length]) length++;
    if (length==ASM1_MAX_SOURCE) return p->error=ASM1_TOO_LARGE;
    insn a[ASM1_MAX_INSTRUCTIONS]; label labels[32]; unsigned na=0,nl=0; int saw_exit=0;
    const char *cur=source; unsigned line=1;
    while (*cur) {
        char buf[ASM1_MAX_SOURCE], *q, *t, w[24]={0}, x[24]={0}, y[24]={0}; size_t k=0;
        unsigned source_line=line;
        while (*cur && *cur!='\n' && *cur!=';') { if (*cur=='#') while (*cur&&*cur!='\n'&&*cur!=';') cur++; else if (k+1<sizeof(buf)) buf[k++]=*cur++; else return p->error=ASM1_SYNTAX;
        }
        if (*cur=='\n') { cur++; line++; } else if (*cur==';') cur++;
        while (k&&ws(buf[k-1])) k--;
        buf[k]=0; q=buf; t=word(&q,w,sizeof(w)); if (!t||w[0]==0) continue;
        p->error_line=source_line;
        if (p->complete) return p->error=ASM1_SYNTAX;
        if (!strcmp(w,"asm1")||!strcmp(w,"end")) {
            while (ws(*q)) q++;
            if (*q) return p->error=ASM1_SYNTAX;
            if (!strcmp(w,"end")) p->complete=1;
            continue;
        }
        if (!strcmp(w,"input")) {
            while ((t=word(&q,x,sizeof(x)))) { uint32_t v; if (p->input_count>=ASM1_MAX_INPUT||number(x,&v)) return p->error=ASM1_NUMBER; p->input[p->input_count++]=v; }
            continue;
        }
        if (na>=ASM1_MAX_INSTRUCTIONS) return p->error=ASM1_TOO_LARGE;
        unsigned op=99; const char *names[] = {"li","mov","add","sub","mul","udiv","umod","and","or","xor","shl","shr","eq","lt","ld","st","jmp","jz","exit","label"};
        for (unsigned i=0;i<sizeof(names)/sizeof(*names);i++) if (!strcmp(w,names[i])) op=i;
        if (op==99) return p->error=ASM1_SYNTAX;
        a[na]=(insn){.op=op,.line=source_line};
        if (op==O_LABEL||op==O_JMP) { if (!word(&q,x,sizeof(x))||!label_ok(x)) return p->error=ASM1_LABEL; memcpy(a[na].label,x,4); if (op==O_LABEL) { if (nl>=32||find_label(labels,nl,x)>=0) return p->error=ASM1_DUPLICATE; memcpy(labels[nl].name,x,4); labels[nl++].at=na; } }
        else if (op==O_EXIT) { if (!word(&q,x,sizeof(x))||regno(x)<0) return p->error=ASM1_REGISTER; a[na].a=(unsigned)regno(x); saw_exit=1; }
        else if (op==O_ST) { if (!word(&q,x,sizeof(x))||!word(&q,y,sizeof(y))||regno(y)<0||number(x,&a[na].imm)||a[na].imm>=256) return p->error=ASM1_SYNTAX; a[na].b=(unsigned)regno(y); }
        else if (op==O_LD) { if (!word(&q,x,sizeof(x))||!word(&q,y,sizeof(y))||regno(x)<0||number(y,&a[na].imm)||a[na].imm>=256) return p->error=ASM1_SYNTAX; a[na].a=(unsigned)regno(x); }
        else if (op==O_LI) { if (!word(&q,x,sizeof(x))||!word(&q,y,sizeof(y))||regno(x)<0||number(y,&a[na].imm)) return p->error=ASM1_SYNTAX; a[na].a=(unsigned)regno(x); }
        else if (op==O_JZ) { if (!word(&q,x,sizeof(x))||!word(&q,y,sizeof(y))||regno(x)<0||!label_ok(y)) return p->error=ASM1_SYNTAX; a[na].a=(unsigned)regno(x); memcpy(a[na].label,y,4); }
        else { if (!word(&q,x,sizeof(x))||!word(&q,y,sizeof(y))||regno(x)<0||regno(y)<0) return p->error=ASM1_REGISTER; a[na].a=(unsigned)regno(x); a[na].b=(unsigned)regno(y); }
        while (ws(*q)) q++;
        if (*q) return p->error=ASM1_SYNTAX;
        na++;
    }
    p->error_line=0;
    if (!saw_exit) return p->error=ASM1_NO_EXIT;
    size_t n=0; size_t at[ASM1_MAX_INSTRUCTIONS];
    if (mov_const(p,&n,(unsigned)regno("r0"),(uint32_t)p->input_count)) { p->error=ASM1_TOO_LARGE; return p->error; }
    typedef struct { size_t pos; unsigned target; } fixup;
    fixup fix[ASM1_MAX_INSTRUCTIONS]; unsigned nf=0;
    for (unsigned i=0;i<na;i++) {
        at[i]=n; insn *in=&a[i]; int e=0;
        if (in->op==O_LABEL) continue;
        switch(in->op) {
        case O_LI:e=mov_const(p,&n,in->a,in->imm);break; case O_MOV:e=rr(p,&n,0x89,in->b,in->a);break;
        case O_ADD:e=rr(p,&n,0x01,in->b,in->a);break; case O_SUB:e=rr(p,&n,0x29,in->b,in->a);break;
        case O_MUL:e=rr2(p,&n,0x0f,0xaf,in->a,in->b);break; case O_UDIV:e=divop(p,&n,in->a,in->b,0);break; case O_UMOD:e=divop(p,&n,in->a,in->b,1);break;
        case O_AND:e=rr(p,&n,0x21,in->b,in->a);break;
        case O_OR:e=rr(p,&n,0x09,in->b,in->a);break; case O_XOR:e=rr(p,&n,0x31,in->b,in->a);break;
        case O_SHL:e=shiftop(p,&n,in->a,in->b,0);break; case O_SHR:e=shiftop(p,&n,in->a,in->b,1);break;
        case O_EQ:e=compare_set(p,&n,in->a,in->b,0x94);break;
        case O_LT:e=compare_set(p,&n,in->a,in->b,0x92);break;
        case O_LD:e=memop(p,&n,0x8b,in->a,in->imm);break; case O_ST:e=memop(p,&n,0x89,in->b,in->imm);break;
        case O_EXIT:e=rr(p,&n,0x89,in->a,0); emit1(p,&n,0xcd); emit1(p,&n,0x80);break;
        case O_JMP: { int target=find_label(labels,nl,in->label); emit1(p,&n,0xe9); if (target<0) e=ASM1_LABEL; else if (nf>=ASM1_MAX_INSTRUCTIONS) e=ASM1_TOO_LARGE; else { fix[nf].pos=n; fix[nf++].target=(unsigned)target; emit4(p,&n,0); } } break;
        case O_JZ: { int target=find_label(labels,nl,in->label); if (in->a>=8) emit1(p,&n,0x41); e=emit1(p,&n,0x83)||emit1(p,&n,(unsigned char)(0xf8|(in->a&7)))||emit1(p,&n,0); emit1(p,&n,0x0f);emit1(p,&n,0x84); if (target<0) e=ASM1_LABEL; else if (nf<ASM1_MAX_INSTRUCTIONS) { fix[nf].pos=n; fix[nf++].target=(unsigned)target; emit4(p,&n,0); } } break;
        default: break;
        }
        if (e || p->error) { p->error_line=in->line; return p->error=p->error ? p->error : e; }
    }
    for (unsigned i=0;i<nl;i++) { if (labels[i].at>=na) return p->error=ASM1_LABEL; labels[i].at=at[labels[i].at]; }
    for (unsigned i=0;i<nf;i++) { if ((int)fix[i].target<0 || fix[i].target>=nl) return p->error=ASM1_LABEL; int32_t d=(int32_t)labels[fix[i].target].at-(int32_t)(fix[i].pos+4); memcpy(p->code+fix[i].pos,&d,4); }
    p->code_size=n; return p->error=ASM1_OK;
}
const char *asm1_error(int c) { switch(c) { case ASM1_INCOMPLETE:return "E_INCOMPLETE"; case ASM1_NO_EXIT:return "E_NO_EXIT"; case ASM1_LABEL:return "E_LABEL"; case ASM1_REGISTER:return "E_REGISTER"; case ASM1_NUMBER:return "E_NUMBER"; case ASM1_TOO_LARGE:return "E_TOO_LARGE"; case ASM1_DUPLICATE:return "E_DUPLICATE"; case ASM1_SOURCE:return "E_SOURCE"; default:return "E_SYNTAX"; } }
