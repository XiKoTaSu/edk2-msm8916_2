#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/NonDiscoverableDeviceRegistrationLib.h>
#include <stdbool.h>
#include <stdint.h>
#include <Protocol/QcomClock.h>
#include <Protocol/QcomRpm.h>
#include <Protocol/QcomPm8x41.h>
#define USB_HS_BASE 0x078D9000
// GCC reset/clock register'lari (CLK_CTL_BASE=0x01800000) — Debian rontgen + iomap.h
#define GCC_USB_HS_BCR           0x01841000  // USB HS controller ("core") block reset
#define GCC_USB2A_PHY_BCR        0x01841028  // USB2A HS-PHY ("por") block reset
#define GCC_USB2A_PHY_SLEEP_CBCR 0x0184102C  // PHY sleep clock branch
#define USB_ULPI_VIEWPORT        (USB_HS_BASE + 0x170)

// ci_hdrc ULPI viewport yazimi: RUN(bit30)|WRITE(bit29)|ADDR(<<16)|DATA, RUN temizlenene kadar bekle
STATIC VOID UlpiPhyWrite (UINT8 Addr, UINT8 Data)
{
  UINT32 To = 100000;
  MmioWrite32 (USB_ULPI_VIEWPORT, (1u << 30) | (1u << 29) | ((UINT32)Addr << 16) | Data);
  while ((MmioRead32 (USB_ULPI_VIEWPORT) & (1u << 30)) && --To) { ; }
}

EFI_STATUS
EFIAPI
Msm8916UsbDxeInitialize (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  EFI_STATUS           Status;
  QCOM_CLOCK_PROTOCOL  *ClockProtocol;
  QCOM_RPM_PROTOCOL    *RpmProtocol;

  DEBUG ((EFI_D_ERROR, "Msm8916UsbDxe: Initializing USB Host...\n"));

  Status = gBS->LocateProtocol (&gQcomClockProtocolGuid, NULL, (VOID **)&ClockProtocol);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Msm8916UsbDxe: Failed to locate Clock Protocol\n"));
    return Status;
  }

  Status = gBS->LocateProtocol (&gQcomRpmProtocolGuid, NULL, (VOID **)&RpmProtocol);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Msm8916UsbDxe: Failed to locate RPM Protocol\n"));
    return Status;
  }

  // 1. Enable L7 (1.8V) and L13 (3.3V) for USB PHY
  STATIC UINT32 ldo7[]  = { 0x616f646c,  7, 0x6e657773, 4, 1, 0x746c6f76, 4, 1800000, 0x616d6370, 4, 0 };
  STATIC UINT32 ldo13[] = { 0x616f646c, 13, 0x6e657773, 4, 1, 0x746c6f76, 4, 3300000, 0x616d6370, 4, 0 }; // Debian rontgen: L13=3.3V (2.95V YANLISTI)
  RpmProtocol->rpm_send_data(ldo7,  36, 0x716572);
  RpmProtocol->rpm_send_data(ldo13, 36, 0x716572);

  QCOM_PM8X41_PROTOCOL *Pm8x41Protocol;

  Status = gBS->LocateProtocol (&gQcomPm8x41ProtocolGuid, NULL, (VOID **)&Pm8x41Protocol);
  if (!EFI_ERROR (Status)) {
    // Configure and assert PMIC GPIO 3 (USB_HUB_RESET_N_PM) HIGH
    struct pm8x41_gpio gpio_conf = {
      .direction = 1, // PM_GPIO_DIR_OUT
      .output_buffer = 0, // PM_GPIO_OUT_CMOS
      .output_value = 1,
      .pull = 4, // PM_GPIO_PULLDOWN_10
      .vin_sel = 0,
      .out_strength = 1, // PM_GPIO_OUT_DRIVE_LOW
      .function = 0, // PM_GPIO_FUNC_LOW (normal)
      .inv_int_pol = 0,
      .disable_pin = 0
    };

    Pm8x41Protocol->pm8x41_gpio_config(3, &gpio_conf);
    Pm8x41Protocol->pm8x41_gpio_set(3, 1);
    
    // DELAY 200ms for USB2514 PLL Lock and internal initialization
    gBS->Stall(200000);

    // Configure and assert PMIC GPIO 4 (USB_SW_SEL_PM) HIGH to switch MUX to HUB
    Pm8x41Protocol->pm8x41_gpio_config(4, &gpio_conf);
    Pm8x41Protocol->pm8x41_gpio_set(4, 1);
    
    DEBUG ((EFI_D_ERROR, "Msm8916UsbDxe: Set PMIC GPIO 3 High -> Wait 200ms -> Set GPIO 4 High for USB Hub!\n"));
  }

  gBS->Stall(50000); // 50ms wait

  // KRITIK: CLK_LOOKUP tablosunda USB clock'lari "usb_iface_clk"/"usb_core_clk" isimleriyle
  // kayitli (gcc_usb_hs_* DEGIL). Yanlis isim -> clk_get NULL -> clock acilmiyordu -> USB MMIO
  // (0x78D9000) abort -> watchdog reset. Dogru lookup isimleri:
  struct clk *AhbClk = ClockProtocol->clk_get("usb_iface_clk");   // -> gcc_usb_hs_ahb_clk (100MHz)
  if (AhbClk) ClockProtocol->clk_enable(AhbClk);

  struct clk *SysClk = ClockProtocol->clk_get("usb_core_clk");    // -> gcc_usb_hs_system_clk (80MHz)
  if (SysClk) ClockProtocol->clk_enable(SysClk);

  // ===== USB PHY + Controller bring-up (Debian rontgen + Linux ci_hdrc_msm / phy-qcom-usb-hs) =====
  // (a) PHY sleep clock'u enable (rontgen: gcc_usb2a_phy_sleep_clk acik). CBCR bit0=CLK_ENABLE.
  MmioWrite32 (GCC_USB2A_PHY_SLEEP_CBCR, MmioRead32 (GCC_USB2A_PHY_SLEEP_CBCR) | 1);

  // (b) GCC USB HS controller ("core") block reset: assert -> deassert
  MmioWrite32 (GCC_USB_HS_BCR, 1); gBS->Stall (1000);
  MmioWrite32 (GCC_USB_HS_BCR, 0); gBS->Stall (1000);

  // (c) GCC USB2A HS-PHY ("por") block reset: assert -> deassert
  MmioWrite32 (GCC_USB2A_PHY_BCR, 1); gBS->Stall (1000);
  MmioWrite32 (GCC_USB2A_PHY_BCR, 0); gBS->Stall (1000);

  // (d) Controller soft reset (USBCMD RST biti) + temizlenene kadar bekle
  MmioWrite32 (USB_HS_BASE + 0x140, MmioRead32 (USB_HS_BASE + 0x140) | 2);
  { UINT32 To = 100000; while ((MmioRead32 (USB_HS_BASE + 0x140) & 2) && --To) { ; } }
  gBS->Stall (5000);

  // --- NEW PHY INIT STEPS ---
  // Assert USB HSPHY_POR
  MmioWrite32 (USB_HS_BASE + 0x240, MmioRead32 (USB_HS_BASE + 0x240) | 1); // HSPHY_CTRL (0x240) |= HSPHY_POR_ASSERT (BIT 0)
  gBS->Stall (20);

  // Deassert USB HSPHY_POR
  MmioWrite32 (USB_HS_BASE + 0x240, MmioRead32 (USB_HS_BASE + 0x240) & ~1u);
  gBS->Stall (20);

  // Clear AHB burst and set correct AHB mode
  MmioWrite32 (USB_HS_BASE + 0x090, 0); // USB_AHBBURST
  MmioWrite32 (USB_HS_BASE + 0x098, 0x08); // USB_AHBMODE - should be 0x08!

  // Workaround for rx buffer collision issue
  MmioWrite32 (USB_HS_BASE + 0x09C, MmioRead32 (USB_HS_BASE + 0x09C) & ~(1u << 4)); // HSPHY_GENCONFIG (0x9C) &= ~HSPHY_TXFIFO_IDLE_FORCE_DIS (BIT 4)
  MmioWrite32 (USB_HS_BASE + 0x0A0, MmioRead32 (USB_HS_BASE + 0x0A0) | (1u << 7)); // HSPHY_GENCONFIG_2 (0xA0) |= HSPHY_SESS_VLD_CTRL_EN (BIT 7)

  // (e) ULPI HS-PHY tune dizisi (rontgen qcom,init-seq=<0x44016b 0x2240313> -> ULPI ext 0x80..0x83)
  UlpiPhyWrite (0x80, 0x44);
  UlpiPhyWrite (0x81, 0x6b);
  UlpiPhyWrite (0x82, 0x24);
  UlpiPhyWrite (0x83, 0x13);

  // Disable OTG COMP
  UlpiPhyWrite (0x88 + 1, 1); // 0x88 (ULPI_PWR_CLK_MNG_REG) + 1 (ULPI_SET), 1 (ULPI_PWR_OTG_COMP_DISABLE)

  // VBUS Valid External CLEAR (Crucial for host mode!)
  UlpiPhyWrite (0x96 + 2, 3); // 0x96 (ULPI_MISC_A) + 2 (ULPI_CLR), 3 (VBUSVLDEXTSEL | VBUSVLDEXT)
  
  // Clear HSPHY_SESS_VLD_CTRL in USBCMD
  MmioWrite32 (USB_HS_BASE + 0x140, MmioRead32 (USB_HS_BASE + 0x140) & ~(1u << 25)); // HSPHY_USBCMD &= ~HSPHY_SESS_VLD_CTRL

  // Set OPMODE_NORMAL
  UlpiPhyWrite (0x04 + 2, 0x18); // 0x04 (ULPI_FUNC_CTRL) + 2 (ULPI_CLR), 0x18 (ULPI_FUNC_CTRL_OPMODE_MASK)
  // -------------------------

  // (f) Host Mode (USBMODE = 0x3) — USB_HS_BASE + 0x1A8
  { UINT32 m = MmioRead32 (USB_HS_BASE + 0x1A8); m = (m & ~0x3u) | 0x3u; MmioWrite32 (USB_HS_BASE + 0x1A8, m); }

  DEBUG ((EFI_D_ERROR, "Msm8916UsbDxe: PHY init + host mode done.\n"));

  // Register NonDiscoverable Device so EhciDxe picks it up!
  // The EHCI capability registers start at USB_HS_BASE + 0x100
  Status = RegisterNonDiscoverableMmioDevice (
             NonDiscoverableDeviceTypeEhci,
             NonDiscoverableDeviceDmaTypeNonCoherent,
             NULL,
             NULL,
             1,
             USB_HS_BASE + 0x100, 0x1000
             );

  DEBUG ((EFI_D_ERROR, "Msm8916UsbDxe: USB Registered! Status = %r\n", Status));

  return EFI_SUCCESS;
}
