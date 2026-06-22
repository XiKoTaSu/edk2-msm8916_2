#include <PiDxe.h>

#include <Library/LKEnvLib.h>

#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>

#include <Protocol/QcomI2cQup.h>
#include <Protocol/QcomClock.h>
#include <Protocol/QcomGpioTlmm.h>
#include <Protocol/QcomRpm.h>

#include <Chipset/i2c_qup.h>

#include "i2c_qup_p.h"

STATIC LIST_ENTRY mDevices = INITIALIZE_LIST_HEAD_VARIABLE(mDevices);

STATIC struct qup_i2c_dev *qup_i2c_get_dev(UINTN Id)
{
  struct qup_i2c_dev *Device;
  LIST_ENTRY *        Link;

  Device = NULL;
  for (Link = mDevices.ForwardLink; Link != &mDevices;
       Link = Link->ForwardLink) {
    Device = BASE_CR(Link, struct qup_i2c_dev, Link);
    if (Device->id == Id) {
      return Device;
    }
  }

  return NULL;
}

STATIC EFI_STATUS InternalRegisterI2cDevice(struct qup_i2c_dev *Device)
{
  STATIC UINTN NextDeviceId = 1;

  if (Device->id == -1)
    Device->id = NextDeviceId++;

  InsertTailList(&mDevices, &Device->Link);

  return EFI_SUCCESS;
}

STATIC EFIAPI EFI_STATUS RegisterI2cDevice(
    struct qup_i2c_dev *device, UINTN clk_freq, UINTN src_clk_rate)
{
  qup_i2c_sec_init(device, clk_freq, src_clk_rate);

  return InternalRegisterI2cDevice(device);
}

STATIC EFIAPI VOID qup_i2c_iterate(qup_i2c_iterate_cb_t cb)
{
  struct qup_i2c_dev *Device;
  LIST_ENTRY *        Link;

  Device = NULL;
  for (Link = mDevices.ForwardLink; Link != &mDevices;
       Link = Link->ForwardLink) {
    Device = BASE_CR(Link, struct qup_i2c_dev, Link);
    cb(Device->id);
  }
}

STATIC QCOM_I2C_QUP_PROTOCOL mI2cQup = {
    qup_i2c_get_dev,
    qup_i2c_xfer,
    qup_i2c_iterate,
};

// Define the bus
STATIC struct qup_i2c_dev mAdv7533I2cDev = {
  .id = 3, // I2C3
  .qup_base = 0x078B8000,
  .gsbi_base = 0,
};

STATIC EFI_STATUS I2cQupWriteReg(struct qup_i2c_dev *dev, UINT8 Addr, UINT8 Reg, UINT8 Val)
{
    UINT8 buf[2] = {Reg, Val};
    struct i2c_msg msg;
    msg.addr = Addr;
    msg.flags = I2C_M_WR;
    msg.len = 2;
    msg.buf = buf;
    
    int ret = qup_i2c_xfer(dev, &msg, 1);
    if (ret == 1) return EFI_SUCCESS;
    return EFI_DEVICE_ERROR;
}

STATIC EFI_STATUS I2cQupReadReg(struct qup_i2c_dev *dev, UINT8 Addr, UINT8 Reg, UINT8 *Val)
{
    struct i2c_msg msgs[2];
    
    msgs[0].addr = Addr;
    msgs[0].flags = I2C_M_WR;
    msgs[0].len = 1;
    msgs[0].buf = &Reg;
    
    msgs[1].addr = Addr;
    msgs[1].flags = I2C_M_RD;
    msgs[1].len = 1;
    msgs[1].buf = Val;
    
    int ret = qup_i2c_xfer(dev, msgs, 2);
    if (ret == 2) return EFI_SUCCESS;
    return EFI_DEVICE_ERROR;
}

EFI_STATUS
EFIAPI
I2cQupDxeInitialize(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_HANDLE Handle = NULL;
  EFI_STATUS Status;

  QCOM_CLOCK_PROTOCOL *ClockProtocol;
  QCOM_GPIO_TLMM_PROTOCOL *GpioProtocol;
  QCOM_RPM_PROTOCOL *RpmProtocol;

  Status = gBS->LocateProtocol (&gQcomClockProtocolGuid, NULL, (VOID **)&ClockProtocol);
  ASSERT_EFI_ERROR(Status);

  Status = gBS->LocateProtocol (&gQcomGpioTlmmProtocolGuid, NULL, (VOID **)&GpioProtocol);
  ASSERT_EFI_ERROR(Status);

  Status = gBS->LocateProtocol (&gQcomRpmProtocolGuid, NULL, (VOID **)&RpmProtocol);
  
  if (!EFI_ERROR(Status) && RpmProtocol != NULL) {
    DEBUG ((EFI_D_ERROR, "I2cQupDxe: Enabling L6(1.8V) and L17(3.3V) via RPM for ADV7533...\n"));
    #define LDOA_RES_TYPE 0x616f646c
    #define KEY_SOFTWARE_ENABLE 0x6e657773
    #define KEY_MICRO_VOLT 0x7675
    #define KEY_CURRENT 0x616d
    #define GENERIC_ENABLE 1
    #define RPM_REQUEST_TYPE 0x716572
    
    STATIC UINT32 ldo6[]  = { LDOA_RES_TYPE,  6, KEY_SOFTWARE_ENABLE, 4, GENERIC_ENABLE, KEY_MICRO_VOLT, 4, 1800000, KEY_CURRENT, 4, 0 };
    STATIC UINT32 ldo17[] = { LDOA_RES_TYPE, 17, KEY_SOFTWARE_ENABLE, 4, GENERIC_ENABLE, KEY_MICRO_VOLT, 4, 3300000, KEY_CURRENT, 4, 0 };
    
    RpmProtocol->rpm_send_data(ldo6,  36, RPM_REQUEST_TYPE);
    RpmProtocol->rpm_send_data(ldo17, 36, RPM_REQUEST_TYPE);
    
    gBS->Stall(200000); // 200ms LDO ramp-up
  }

  // Enable Clocks
  ClockProtocol->clk_get_set_enable("blsp1_qup4_ahb_iface_clk", 0, 1);
  ClockProtocol->clk_get_set_enable("gcc_blsp1_qup4_i2c_apps_clk", 19200000, 1);

  // Configure I2C4 Pins (SDA: 14, SCL: 15)
  GpioProtocol->SetFunction(14, 2);
  GpioProtocol->SetDriveStrength(14, 16); // 16mA
  GpioProtocol->SetPull(14, GPIO_PULL_NONE);

  GpioProtocol->SetFunction(15, 2);
  GpioProtocol->SetDriveStrength(15, 16); // 16mA
  GpioProtocol->SetPull(15, GPIO_PULL_NONE);

  // Configure ADV7533 HDMI_TX_EN (Enable) GPIO 32
  GpioProtocol->SetFunction(32, 0); // GPIO function
  GpioProtocol->SetDriveStrength(32, 16); // 16mA
  GpioProtocol->SetPull(32, GPIO_PULL_NONE); // No pull
  GpioProtocol->DirectionOutput(32, 0); // Drive LOW (Enable)

  // Configure ADV7533 interrupt suspend pin (GPIO 31)
  GpioProtocol->SetFunction(31, 0);
  GpioProtocol->SetDriveStrength(31, 16);
  GpioProtocol->SetPull(31, GPIO_PULL_NONE);
  GpioProtocol->DirectionInput(31);
  
  // Wait for ADV7533 to wake up completely
  gBS->Stall(200000); // 200ms

  RegisterI2cDevice(&mAdv7533I2cDev, 100000, 19200000);

  Status = gBS->InstallMultipleProtocolInterfaces(
      &Handle, &gQcomI2cQupProtocolGuid, &mI2cQup, NULL);
  ASSERT_EFI_ERROR(Status);
  
  // Test Ping ADV7533
  {
      struct qup_i2c_dev *dev = qup_i2c_get_dev(3);
      if (dev) {
          UINT8 val = 0;
          EFI_STATUS Status;

          Status = I2cQupReadReg(dev, 0x39, 0x00, &val); // Read chip revision
          DEBUG ((EFI_D_ERROR, "I2cQupDxe: ADV7533 Chip Revision (Reg 0x00): 0x%02x (Status: %r)\n", val, Status));
          
          // 1. HDP_SRC=NONE: ADV7533 reg 0xd6 bits[7:6]=11 (=0xc0)
          I2cQupWriteReg(dev, 0x39, 0xd6, 0xc0);
          DEBUG ((EFI_D_ERROR, "I2cQupDxe: ADV7533 set HPD_SRC=NONE (Reg 0xd6 = 0xc0)\n"));

          // 2. Power-up: reg 0x41 bit6 (POWER_DOWN) temizle
          I2cQupReadReg(dev, 0x39, 0x41, &val);
          DEBUG ((EFI_D_ERROR, "I2cQupDxe: ADV7533 Reg 0x41 before power up: 0x%02x\n", val));
          
          I2cQupWriteReg(dev, 0x39, 0x41, val & ~0x40);
          
          I2cQupReadReg(dev, 0x39, 0x41, &val);
          DEBUG ((EFI_D_ERROR, "I2cQupDxe: ADV7533 Reg 0x41 after power up: 0x%02x\n", val));
      }
  }

  return Status;
}

EFI_STATUS
EFIAPI
I2cQupDxeUnload(IN EFI_HANDLE ImageHandle)
{
  struct qup_i2c_dev *Device;

  while (!IsListEmpty(&mDevices)) {
    Device = BASE_CR(mDevices.ForwardLink, struct qup_i2c_dev, Link);
    RemoveEntryList(&Device->Link);

    qup_i2c_deinit(Device);
  }

  return EFI_SUCCESS;
}
