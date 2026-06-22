#ifndef LK_SHIM_H
#define LK_SHIM_H

#include <PiDxe.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseLib.h>
#include <Library/IoLib.h>
#include <Library/TimerLib.h>
#include <Library/CacheMaintenanceLib.h>

typedef UINTN addr_t;

#define ntohl(x) SwapBytes32(x)
#define readl_relaxed(a) readl(a)
#define writel_relaxed(v, a) writel(v, a)

#define arch_clean_invalidate_cache_range(addr, size) WriteBackInvalidateDataCacheRange((VOID *)(UINTN)(addr), size)
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>

typedef UINT8 uint8_t;
typedef UINT16 uint16_t;
typedef UINT32 uint32_t;
typedef UINT64 uint64_t;
typedef INT8 int8_t;
typedef INT16 int16_t;
typedef INT32 int32_t;
typedef INT64 int64_t;
typedef BOOLEAN bool;

#define true TRUE
#define false FALSE

#define writel(v, a) MmioWrite32((UINTN)(a), (UINT32)(UINTN)(v))
#define readl(a) MmioRead32((UINTN)(a))

#define udelay(us) MicroSecondDelay(us)
#define mdelay(ms) MicroSecondDelay((ms)*1000)

#define dprintf(level, fmt, ...) DEBUG((EFI_D_ERROR, fmt, ##__VA_ARGS__))

#define malloc(s) AllocatePool(s)
#define free(p) FreePool(p)
#define memalign(a, s) AllocatePool(s)
#define memset(d, c, n) SetMem(d, n, c)
#define memcpy(d, s, n) CopyMem(d, s, n)

#define INFO 0
#define CRITICAL 0
#define SPEW 0

#define NO_ERROR 0
#define ERROR -1
#define ERR_INVALID_ARGS -2
#define ERR_NOT_VALID -3

#define SECURE_DEVICE_MDSS 1

#define DISPLAY_TYPE_MDSS 1
#define DISPLAY_TYPE_DSI6G 1

#define MMSS_SFPB_GPREG 0
#define DSIPHY_PLL_CTRL(x) 0

static inline int restore_secure_cfg(uint32_t id) { return 0; }
static inline int target_cont_splash_screen(void) { return 0; }
static inline int target_panel_auto_detect_enabled(void) { return 0; }
int mdp_get_revision(void);
static inline void dsb(void) {}
static inline void dmb(void) {}

void mdss_dsi_phy_sw_reset(uint32_t ctl_base);


#define BIT(x) (1 << (x))

#endif
