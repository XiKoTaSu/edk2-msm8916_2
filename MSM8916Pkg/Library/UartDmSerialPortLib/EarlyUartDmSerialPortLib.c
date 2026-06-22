#include <PiPei.h>
#include <Library/LKEnvLib.h>
#include <Library/SerialPortLib.h>
#include "uartdm_p.h"

STATIC
VOID
PutStr(IN CONST CHAR8 *s)
{
  while (*s != '\0') {
    uart_putc(*s++);
  }
}

RETURN_STATUS
EFIAPI
SerialPortInitialize(VOID)
{
  // DB410c console UART2. LK bunu zaten yapilandirdigi icin dogrudan yazariz.
  g_uart_dm_base = 0x078B0000;
  PutStr("\r\n>>>> EDK2 SEC ALIVE (DB410c) <<<<\r\n");
  return RETURN_SUCCESS;
}

//
// TEK KABLO icin: micro-USB'yi board'dan CH340'a takip screen acana kadar
// banner'i ~30-60 sn boyunca TEKRAR TEKRAR basar. Boylece swap sonrasi
// hala akiyor olur. (Board 12V barrel'dan beslenmeli ki USB cikinca resetlenmesin.)
// Calistiktan sonra REPEAT'i kucultup tekrar derleyebilirsin.
//
#define ANNOUNCE_REPEAT  30
VOID
EFIAPI
SerialPortAnnounce(VOID)
{
  UINTN n;
  g_uart_dm_base = 0x078B0000;
  for (n = 0; n < ANNOUNCE_REPEAT; n++) {
    PutStr("\r\n>>>> EDK2 SEC ALIVE (DB410c) -- kabloyu CH340'a tak <<<<\r\n");
    // ~1-2 sn busy-wait (timer yok; deger CPU/cache'e gore yaklasik).
    for (volatile UINT32 i = 0; i < 0x00C00000; i++) {
    }
  }
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
