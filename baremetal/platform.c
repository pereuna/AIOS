/* UEFI supplies console, USB keyboard, timers and RAM allocation. No storage I/O.
 * Boot Services deliberately remain active for the firmware's device drivers. */
#include <efi.h>
#include "runtime.h"

volatile uint64_t bm_ticks;
static EFI_SYSTEM_TABLE *system_table;
static EFI_BOOT_SERVICES *services;
static EFI_EVENT timer;

EFI_STATUS efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *table) {
    (void)image;
    system_table=table;
    services=table->BootServices;
    bm_main();
}
/* GNU-EFI >= 3.0.18 calls _entry after relocation. We have no constructors
 * and supply our own libc, so its libefi initialization wrapper is unnecessary. */
EFI_STATUS _entry(EFI_HANDLE image, EFI_SYSTEM_TABLE *table) {
    return efi_main(image,table);
}

static void console(uint32_t c) {
    CHAR16 text[3]; unsigned n=0;
    if (c=='\n') text[n++]='\r';
    /* Simple Text Output uses UCS-2; preserve BMP characters including Finnish. */
    text[n++]=(CHAR16)(c>0xffff || (c>=0xd800 && c<=0xdfff) ? '?' : c);
    text[n]=0;
    system_table->ConOut->OutputString(system_table->ConOut,text);
}
void bm_putc(char ch) {
    static uint32_t code,minimum;
    static unsigned remaining;
    unsigned char c=(unsigned char)ch;
    if (remaining && (c&0xc0)==0x80) {
        code=(code<<6)|(c&63);
        if (!--remaining) console(code<minimum || code>0x10ffff ? '?' : code);
        return;
    }
    if (remaining) { remaining=0; console('?'); }
    if (c<128) console(c);
    else if (c>=0xc2 && c<=0xf4) {
        remaining=c>=0xf0 ? 3 : c>=0xe0 ? 2 : 1;
        minimum=remaining==3 ? 0x10000 : remaining==2 ? 0x800 : 0x80;
        code=c&((1u<<(6-remaining))-1);
    } else console('?');
}
void bm_write(const void *p, size_t n) { const char *s=p; while (n--) bm_putc(*s++); }
void bm_puts(const char *s) { bm_write(s,strlen(s)); }
void bm_uint(uint64_t n) {
    char s[24]; unsigned i=0;
    do { s[i++]=(char)('0'+n%10); n/=10; } while (n);
    while (i) bm_putc(s[--i]);
}
void bm_hex(uint64_t n, int digits) {
    const char *hex="0123456789abcdef";
    while (digits--) bm_putc(hex[(n>>(digits*4))&15]);
}
void bm_check_finite(const char *where, const float *values, size_t count, int position, int layer) {
    /* Inspect IEEE-754 bits with integer instructions, even if the FP state is bad.
     * Do not call firmware on the successful path through an active calculation. */
    for (size_t i=0;i<count;i++) {
        uint32_t bits; memcpy(&bits,values+i,sizeof(bits));
        if ((bits&0x7f800000u)!=0x7f800000u) continue;
        uint32_t mxcsr;
        __asm__ volatile("stmxcsr %0" : "=m"(mxcsr));
        bm_puts("\nNon-finite value in "); bm_puts(where);
        if (position>=0) { bm_puts("; position "); bm_uint((unsigned)position); }
        if (layer>=0) { bm_puts("; layer "); bm_uint((unsigned)layer); }
        bm_puts("; index "); bm_uint(i); bm_puts("; bits 0x"); bm_hex(bits,8);
        bm_puts("; MXCSR 0x"); bm_hex(mxcsr,8); bm_putc('\n');
        bm_panic("numerical check failed");
    }
}
_Noreturn void bm_panic(const char *s) {
    bm_puts("\nPANIC: "); bm_puts(s); bm_puts("\nRestart the machine to retry.\n");
    for (;;) services->Stall(1000000);
}
_Noreturn void bm_shutdown(void) {
    bm_puts("Goodbye.\n");
    system_table->RuntimeServices->ResetSystem(EfiResetShutdown,EFI_SUCCESS,0,NULL);
    for (;;) services->Stall(1000000);
}
static void EFIAPI tick(EFI_EVENT event, void *context) {
    (void)event; (void)context;
    bm_ticks++;
}
void bm_init(void) {
    if (EFI_ERROR(services->SetWatchdogTimer(0,0,0,NULL)))
        bm_panic("cannot disable firmware watchdog");
    system_table->ConOut->SetAttribute(system_table->ConOut,EFI_LIGHTGRAY|EFI_BACKGROUND_BLACK);
    system_table->ConOut->ClearScreen(system_table->ConOut);
    system_table->ConOut->EnableCursor(system_table->ConOut,TRUE);
    if (EFI_ERROR(services->CreateEvent(EVT_TIMER|EVT_NOTIFY_SIGNAL,TPL_CALLBACK,tick,NULL,&timer)) ||
        EFI_ERROR(services->SetTimer(timer,TimerPeriodic,100000)))
        bm_panic("cannot create UEFI timer");
}
void bm_reserve_heap(size_t bytes) {
    EFI_PHYSICAL_ADDRESS address=0;
    if (bytes>SIZE_MAX-4095) bm_panic("heap size overflow");
    size_t pages=(bytes+4095)/4096;
    if (EFI_ERROR(services->AllocatePages(AllocateAnyPages,EfiLoaderData,pages,&address)))
        bm_panic("not enough RAM for context; use more RAM or a smaller CONTEXT");
    bm_heap_init((uintptr_t)address,(uintptr_t)address+pages*4096);
}
static uint32_t getch(void) {
    for (;;) {
        EFI_INPUT_KEY key;
        EFI_STATUS status=system_table->ConIn->ReadKeyStroke(system_table->ConIn,&key);
        if (status==EFI_SUCCESS) {
            if (key.UnicodeChar && !(key.UnicodeChar>=0xd800 && key.UnicodeChar<=0xdfff))
                return key.UnicodeChar;
        } else if (status==EFI_NOT_READY) {
            UINTN index;
            if (EFI_ERROR(services->WaitForEvent(1,&system_table->ConIn->WaitForKey,&index)))
                bm_panic("UEFI keyboard event failed");
        } else bm_panic("UEFI keyboard read failed");
    }
}
int bm_readline(char *s, size_t cap) {
    static int after_cr;
    size_t n=0; int overflow=0,escape=0;
    for (;;) {
        uint32_t c=getch();
        if (after_cr) { after_cr=0; if (c=='\n') continue; }
        if (c==27) { escape=1; continue; }
        if (escape) { if (escape==1 && (c=='[' || c=='O')) escape=2; else escape=0; continue; }
        if (c=='\r' || c=='\n') {
            after_cr=c=='\r'; bm_putc('\n'); s[n]=0;
            return overflow ? -1 : (int)n;
        }
        if (c==4 && !n) return -2;
        if (c==8 || c==127) {
            if (n) {
                do { n--; } while (n && ((unsigned char)s[n]&0xc0)==0x80);
                bm_puts("\b \b");
            }
        } else if (c>=32 || c=='\t') {
            unsigned bytes=c<128 ? 1 : c<2048 ? 2 : 3;
            if (n+bytes<cap && !overflow) {
                if (bytes==3) s[n++]=(char)(0xe0|(c>>12));
                if (bytes>=2) s[n++]=(char)((bytes==2 ? 0xc0 : 0x80)|((c>>6)&(bytes==2 ? 31 : 63)));
                s[n++]=(char)(bytes==1 ? c : 0x80|(c&63));
                console(c);
            } else overflow=1;
        }
    }
}
uint32_t bm_crc32(const void *p, size_t n) {
    uint32_t table[256];
    for (uint32_t i=0;i<256;i++) {
        uint32_t c=i;
        for (int j=0;j<8;j++) c=(c>>1)^((c&1) ? 0xedb88320u : 0);
        table[i]=c;
    }
    const unsigned char *s=p; uint32_t crc=UINT32_MAX;
    while (n--) crc=table[(crc^*s++)&255]^(crc>>8);
    return ~crc;
}
