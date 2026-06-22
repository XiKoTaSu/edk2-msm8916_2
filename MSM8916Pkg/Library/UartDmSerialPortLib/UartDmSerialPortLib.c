#include <PiDxe.h>
#include <Library/LKEnvLib.h>
#include <Library/SerialPortLib.h>
#include "uartdm_p.h"

RETURN_STATUS
EFIAPI
SerialPortInitialize(VOID)
{
  g_uart_dm_base = 0x078B0000;
  return RETURN_SUCCESS;
}

RETURN_STATUS
EFIAPI
UartDmSerialPortLibInitialize(VOID)
{
  g_uart_dm_base = 0x078B0000;
  return RETURN_SUCCESS;
}

UINTN EFIAPI SerialPortWrite(IN UINT8 *Buffer, IN UINTN NumberOfBytes) {
  UINTN Num = 0;
  UINT8 *CONST Final = &Buffer[NumberOfBytes];
  while (Buffer < Final) {
    if (uart_putc(*Buffer++) <= 0) break;
    Num++;
  }
  return Num;
}

UINTN EFIAPI SerialPortRead(OUT UINT8 *Buffer, IN UINTN NumberOfBytes) {
  UINTN Num = 0;
  UINT8 *CONST Final = &Buffer[NumberOfBytes];
  while (Buffer < Final) {
    if (uart_getc(Buffer++, TRUE) <= 0) break;
    Num++;
  }
  return Num;
}

BOOLEAN EFIAPI SerialPortPoll(VOID) { return uart_tstc() == 1; }
RETURN_STATUS EFIAPI SerialPortSetControl(IN UINT32 Control) { return RETURN_UNSUPPORTED; }
RETURN_STATUS EFIAPI SerialPortGetControl(OUT UINT32 *Control) { return RETURN_UNSUPPORTED; }
RETURN_STATUS EFIAPI SerialPortSetAttributes(IN OUT UINT64 *BaudRate, IN OUT UINT32 *ReceiveFifoDepth, IN OUT UINT32 *Timeout, IN OUT EFI_PARITY_TYPE *Parity, IN OUT UINT8 *DataBits, IN OUT EFI_STOP_BITS_TYPE *StopBits) { return RETURN_UNSUPPORTED; }
