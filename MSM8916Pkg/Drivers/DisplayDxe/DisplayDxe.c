#include <PiDxe.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/IoLib.h>
#include <Library/TimerLib.h>

#include <Protocol/QcomClock.h>
#include <Protocol/QcomI2cQup.h>

#include "adv7533_panel.h"
#include "mdp5.h"
#include "mipi_dsi.h"

int mdss_dsi_post_on(struct msm_fb_panel_data *panel);

#define ADV7533_I2C_ADDR 0x39

STATIC QCOM_I2C_QUP_PROTOCOL *mI2cQup;
STATIC struct qup_i2c_dev *mAdv7533Dev;

STATIC EFI_STATUS I2cWriteReg(UINT8 Addr, UINT8 Reg, UINT8 Val)
{
    UINT8 buf[2] = {Reg, Val};
    struct i2c_msg msg;
    msg.addr = Addr;
    msg.flags = I2C_M_WR;
    msg.len = 2;
    msg.buf = buf;
    
    int ret = mI2cQup->Transfer(mAdv7533Dev, &msg, 1);
    if (ret == 1) return EFI_SUCCESS;
    return EFI_DEVICE_ERROR;
}

STATIC EFI_STATUS Adv7533_Init(VOID)
{
    // ADV7533 init sequence
    DEBUG((EFI_D_ERROR, "DisplayDxe: Initializing ADV7533...\n"));
    
    // Power down clear
    I2cWriteReg(0x39, 0x41, 0x10); 
    
    // HPD override
    I2cWriteReg(0x39, 0xD6, 0xC0);
    
    // Set CEC_DSI address to 0x3C (0x78 shifted by 1)
    I2cWriteReg(0x39, 0xE1, 0x78);

    // Some required magic writes for DSI
    I2cWriteReg(0x39, 0xE4, 0x40);
    I2cWriteReg(0x39, 0xE5, 0x80);

    I2cWriteReg(0x3C, 0x17, 0xD0); 
    I2cWriteReg(0x3C, 0x24, 0x20);
    I2cWriteReg(0x3C, 0x57, 0x11); 

    // Configure 4 DSI lanes
    I2cWriteReg(0x3C, 0x1C, (4 << 4));

    // HDMI mode
    I2cWriteReg(0x39, 0xAF, 0x16);

    // HDMI Output Enable
    I2cWriteReg(0x3C, 0x03, 0x89);

    // GC packet enable
    I2cWriteReg(0x39, 0x40, 0x80);
    // Input color depth 24-bit
    I2cWriteReg(0x39, 0x4C, 0x03);
    // Down dither
    I2cWriteReg(0x39, 0x49, 0xFC);

    // Internal timing disabled
    I2cWriteReg(0x3C, 0x27, 0x0B);
    
    return EFI_SUCCESS;
}

#define MDP_GDSCR 0x0184D078
#define GDSC_POWER_ON_BIT 0x80000000
#define GDSC_POWER_ON_STATUS_BIT 0x20000000

STATIC VOID Mdss_Gdsc_Enable(VOID)
{
    UINT32 Reg = MmioRead32(MDP_GDSCR);
    UINT32 Timeout = 1000;
    
    // In Linux/LK, SW_COLLAPSE is BIT(0) and PWR_ON_STATUS is BIT(31).
    #define SW_COLLAPSE_BIT 0x00000001
    #define PWR_ON_STATUS_BIT 0x80000000
    
    DEBUG((EFI_D_ERROR, "DisplayDxe: Turning on MDSS GDSC (Reg: 0x%08X)...\n", Reg));
    
    // If it's already on, status bit should be 1.
    if (!(Reg & PWR_ON_STATUS_BIT)) {
        // Clear SW_COLLAPSE to turn it on
        Reg &= ~SW_COLLAPSE_BIT;
        MmioWrite32(MDP_GDSCR, Reg);
        
        // Wait at least 1ms before polling
        MicroSecondDelay(1000);
        
        while (!(MmioRead32(MDP_GDSCR) & PWR_ON_STATUS_BIT) && Timeout > 0) {
            MicroSecondDelay(100);
            Timeout--;
        }
        
        if (Timeout == 0) {
            DEBUG((EFI_D_ERROR, "DisplayDxe: MDSS GDSC Power-on TIMEOUT! Reg: 0x%08X\n", MmioRead32(MDP_GDSCR)));
            return;
        }
    }
    DEBUG((EFI_D_ERROR, "DisplayDxe: MDSS GDSC is ON!\n"));
}

STATIC EFI_STATUS Mdss_Init(QCOM_CLOCK_PROTOCOL *ClockProtocol)
{
    DEBUG((EFI_D_ERROR, "DisplayDxe: Initializing MDSS Clocks...\n"));
    
    // Enable GDSC first!
    Mdss_Gdsc_Enable();
    
    // Enable MDSS AHB, AXI, MDP, VSYNC clocks
    DEBUG((EFI_D_ERROR, "DisplayDxe: Enable mdp_ahb_clk\n"));
    ClockProtocol->clk_get_set_enable("mdp_ahb_clk", 0, TRUE);
    DEBUG((EFI_D_ERROR, "DisplayDxe: Enable mdss_axi_clk\n"));
    ClockProtocol->clk_get_set_enable("mdss_axi_clk", 0, TRUE);
    DEBUG((EFI_D_ERROR, "DisplayDxe: Enable mdss_mdp_clk_src\n"));
    ClockProtocol->clk_get_set_enable("mdss_mdp_clk_src", 320000000, TRUE);
    DEBUG((EFI_D_ERROR, "DisplayDxe: Enable mdss_mdp_clk\n"));
    ClockProtocol->clk_get_set_enable("mdss_mdp_clk", 0, TRUE);
    DEBUG((EFI_D_ERROR, "DisplayDxe: Enable mdss_vsync_clk\n"));
    ClockProtocol->clk_get_set_enable("mdss_vsync_clk", 0, TRUE);
    
    DEBUG((EFI_D_ERROR, "DisplayDxe: MDSS Clocks Enabled!\n"));
    
    // To be implemented: DSI PLL, DSI PHY, MDP5.
    return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
DisplayDxeInitialize(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable)
{
    EFI_STATUS Status;
    QCOM_CLOCK_PROTOCOL *ClockProtocol;

    DEBUG((EFI_D_ERROR, "DisplayDxe: Entry\n"));

    Status = gBS->LocateProtocol(&gQcomClockProtocolGuid, NULL, (VOID **)&ClockProtocol);
    if (EFI_ERROR(Status)) return Status;

    Status = gBS->LocateProtocol(&gQcomI2cQupProtocolGuid, NULL, (VOID **)&mI2cQup);
    if (EFI_ERROR(Status)) return Status;

    mAdv7533Dev = mI2cQup->GetDevice(3);
    if (!mAdv7533Dev) {
        DEBUG((EFI_D_ERROR, "DisplayDxe: Failed to get I2C3 device\n"));
        return EFI_NOT_FOUND;
    }

    Adv7533_Init();
    Mdss_Init(ClockProtocol);

    DEBUG((EFI_D_ERROR, "DisplayDxe: Setting up DSI/MDP...\n"));
    
    Status = mdss_dsi_config(&adv7533_panel);
    if (EFI_ERROR(Status)) {
        DEBUG((EFI_D_ERROR, "DisplayDxe: mdss_dsi_config failed: %r\n", Status));
    }

    Status = mdp_dsi_video_config(&adv7533_panel.panel_info, &adv7533_panel.fb);
    if (EFI_ERROR(Status)) {
        DEBUG((EFI_D_ERROR, "DisplayDxe: mdp_dsi_video_config failed: %r\n", Status));
    }

    DEBUG((EFI_D_ERROR, "DisplayDxe: Turning on display...\n"));
    Status = mdp_dsi_video_on(&adv7533_panel.panel_info);
    if (EFI_ERROR(Status)) {
        DEBUG((EFI_D_ERROR, "DisplayDxe: mdp_dsi_video_on failed: %r\n", Status));
    }

    Status = mdss_dsi_post_on(&adv7533_panel);
    if (EFI_ERROR(Status)) {
        DEBUG((EFI_D_ERROR, "DisplayDxe: mdss_dsi_post_on failed: %r\n", Status));
    }

    DEBUG((EFI_D_ERROR, "DisplayDxe: Init Done\n"));

    return EFI_SUCCESS;
}
