/* Host-only fake firmware: exercise the real boot-volume loader and its errors. */
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../baremetal/platform.c"

enum { OK, SHORT_READ, NO_FS, NO_VOLUME, MISSING, BAD_INFO, BAD_SIZE,
       DIRECTORY, NO_RAM, READ_ERROR, EARLY_EOF, OVERSIZED_READ, CLOSE_ERROR };
static int scenario, closes;
static size_t position, length=3*1024*1024+7;
static unsigned char *allocation;
static char message[2048];
static size_t message_length;
static jmp_buf stopped;
static EFI_FILE_PROTOCOL root_file, model_file;
static EFI_SIMPLE_FILE_SYSTEM_PROTOCOL filesystem;
static EFI_LOADED_IMAGE_PROTOCOL loaded_image;

_Noreturn void bm_main(void) { abort(); }
void bm_heap_init(uintptr_t start, uintptr_t end) { (void)start; (void)end; }

static EFI_STATUS EFIAPI output(SIMPLE_TEXT_OUTPUT_INTERFACE *self, CHAR16 *text) {
    (void)self;
    while (*text && message_length+1<sizeof(message)) message[message_length++]=(char)*text++;
    message[message_length]=0;
    return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI stall(UINTN duration) { (void)duration; longjmp(stopped,1); }
static EFI_STATUS EFIAPI handle(EFI_HANDLE device, EFI_GUID *guid, void **result) {
    EFI_GUID loaded_id=EFI_LOADED_IMAGE_PROTOCOL_GUID;
    EFI_GUID fs_id=EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    if (!memcmp(guid,&loaded_id,sizeof(*guid))) {
        assert(device==image_handle); *result=&loaded_image;
    } else {
        assert(!memcmp(guid,&fs_id,sizeof(*guid)));
        assert(device==loaded_image.DeviceHandle);
        if (scenario==NO_FS) return EFI_UNSUPPORTED;
        *result=&filesystem;
    }
    return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI volume(EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *self, EFI_FILE_PROTOCOL **result) {
    assert(self==&filesystem); *result=&root_file;
    return scenario==NO_VOLUME ? EFI_DEVICE_ERROR : EFI_SUCCESS;
}
static EFI_STATUS EFIAPI open_file(EFI_FILE_PROTOCOL *self, EFI_FILE_PROTOCOL **result,
                                   CHAR16 *path, UINT64 mode, UINT64 attributes) {
    assert(self==&root_file && mode==EFI_FILE_MODE_READ && attributes==0);
    assert(!memcmp(path,L"\\model.bin",sizeof(L"\\model.bin")));
    *result=&model_file;
    return scenario==MISSING ? EFI_NOT_FOUND : EFI_SUCCESS;
}
static EFI_STATUS EFIAPI information(EFI_FILE_PROTOCOL *self, EFI_GUID *guid, UINTN *size, void *buffer) {
    EFI_GUID expected=EFI_FILE_INFO_ID;
    assert(self==&model_file && !memcmp(guid,&expected,sizeof(*guid)));
    assert(*size>=sizeof(EFI_FILE_INFO));
    EFI_FILE_INFO *info=buffer;
    memset(info,0,sizeof(*info));
    info->FileSize=length+(scenario==BAD_SIZE);
    info->Attribute=scenario==DIRECTORY ? EFI_FILE_DIRECTORY : 0;
    *size=sizeof(*info);
    return scenario==BAD_INFO ? EFI_DEVICE_ERROR : EFI_SUCCESS;
}
static EFI_STATUS EFIAPI pages(EFI_ALLOCATE_TYPE type, EFI_MEMORY_TYPE memory, UINTN count,
                               EFI_PHYSICAL_ADDRESS *address) {
    assert(type==AllocateAnyPages && memory==EfiLoaderData && count==(length+4095)/4096);
    if (scenario==NO_RAM) return EFI_OUT_OF_RESOURCES;
    allocation=malloc(count*4096); assert(allocation);
    *address=(uintptr_t)allocation;
    return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI read_file(EFI_FILE_PROTOCOL *self, UINTN *count, void *buffer) {
    assert(self==&model_file && *count<=1024*1024 && *count<=length-position);
    assert(buffer==allocation+position);
    if (scenario==READ_ERROR) return EFI_DEVICE_ERROR;
    if (scenario==EARLY_EOF && position) { *count=0; return EFI_SUCCESS; }
    if (scenario==OVERSIZED_READ) { ++*count; return EFI_SUCCESS; }
    if (scenario==SHORT_READ && *count>12345) *count=12345;
    unsigned char *out=buffer;
    for (UINTN i=0;i<*count;i++) out[i]=(unsigned char)((position+i)%251);
    position+=*count;
    return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI close_file(EFI_FILE_PROTOCOL *self) {
    assert(self==(closes ? &root_file : &model_file)); ++closes;
    return scenario==CLOSE_ERROR ? EFI_DEVICE_ERROR : EFI_SUCCESS;
}
int main(void) {
    EFI_BOOT_SERVICES boot={.HandleProtocol=handle,.AllocatePages=pages,.Stall=stall};
    SIMPLE_TEXT_OUTPUT_INTERFACE console_output={.OutputString=output};
    EFI_SYSTEM_TABLE table={.BootServices=&boot,.ConOut=&console_output};
    system_table=&table; services=&boot;
    image_handle=(EFI_HANDLE)(uintptr_t)1;
    loaded_image.DeviceHandle=(EFI_HANDLE)(uintptr_t)2;
    filesystem.OpenVolume=volume;
    root_file.Open=open_file; root_file.Close=close_file;
    model_file.GetInfo=information; model_file.Read=read_file; model_file.Close=close_file;
    assert(bm_crc32("123456789",9)==0xcbf43926u);
    const char *errors[]={NULL,NULL,"boot filesystem","boot volume","model.bin missing",
        "wrong model.bin size","wrong model.bin size","wrong model.bin size",
        "not enough RAM","cannot read model.bin","cannot read model.bin",
        "cannot read model.bin","cannot close model file"};
    for (scenario=OK;scenario<=CLOSE_ERROR;scenario++) {
        allocation=NULL; position=0; closes=0; message_length=0; message[0]=0;
        if (!setjmp(stopped)) {
            const unsigned char *data=bm_load_model(length);
            assert(scenario==OK || scenario==SHORT_READ);
            assert(data==allocation && position==length && closes==2);
            for (size_t i=0;i<length;i++) assert(data[i]==i%251);
        } else {
            assert(errors[scenario] && strstr(message,errors[scenario]));
            if (scenario==CLOSE_ERROR) assert(closes==2);
        }
        free(allocation);
    }
    puts("UEFI file loader: 13 cases passed; CRC32 known vector passed");
    return 0;
}
