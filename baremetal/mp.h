#ifndef SMOL_MP_H
#define SMOL_MP_H
#include <efi.h>

/* PI MP Services ABI (not shipped by the pinned GNU-EFI headers).
 * https://github.com/tianocore/edk2/blob/master/MdePkg/Include/Protocol/MpService.h
 * The extended topology is reserved here; we request the original location. */
typedef struct {
    UINT64 id;
    UINT32 flags;
    struct { UINT32 package, core, thread; } location;
    UINT32 extended[6];
} BM_CPU_INFO;
typedef void (EFIAPI *BM_AP_PROCEDURE)(void *);
typedef struct BM_MP_PROTOCOL BM_MP_PROTOCOL;
struct BM_MP_PROTOCOL {
    EFI_STATUS (EFIAPI *GetNumberOfProcessors)(BM_MP_PROTOCOL *, UINTN *, UINTN *);
    EFI_STATUS (EFIAPI *GetProcessorInfo)(BM_MP_PROTOCOL *, UINTN, BM_CPU_INFO *);
    EFI_STATUS (EFIAPI *StartupAllAPs)(BM_MP_PROTOCOL *, BM_AP_PROCEDURE, BOOLEAN,
                                     EFI_EVENT, UINTN, void *, UINTN **);
    EFI_STATUS (EFIAPI *StartupThisAP)(BM_MP_PROTOCOL *, BM_AP_PROCEDURE, UINTN,
                                     EFI_EVENT, UINTN, void *, BOOLEAN *);
    EFI_STATUS (EFIAPI *SwitchBSP)(BM_MP_PROTOCOL *, UINTN, BOOLEAN);
    EFI_STATUS (EFIAPI *EnableDisableAP)(BM_MP_PROTOCOL *, UINTN, BOOLEAN, UINT32 *);
    EFI_STATUS (EFIAPI *WhoAmI)(BM_MP_PROTOCOL *, UINTN *);
};
#define BM_MP_GUID {0x3fdda605,0xa76e,0x4f46,{0xad,0x29,0x12,0xf4,0x53,0x1b,0x3d,0x08}}
enum { BM_CPU_BSP=1, BM_CPU_ENABLED=2, BM_CPU_HEALTHY=4 };
void bm_mp_init(EFI_BOOT_SERVICES *);
#endif
