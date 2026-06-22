/** @file

  Copyright (c) 2011-2017, ARM Limited. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiPei.h>

#include <Library/CacheMaintenanceLib.h>
#include <Library/DebugAgentLib.h>
#include <Library/PrePiLib.h>
#include <Library/PrintLib.h>
#include <Library/PrePiHobListPointerLib.h>
#include <Library/TimerLib.h>
#include <Library/PerformanceLib.h>

#include <Ppi/GuidedSectionExtraction.h>
#include <Ppi/ArmMpCoreInfo.h>
#include <Ppi/SecPerformance.h>

#include "PrePi.h"

#define IS_XIP()  (((UINT64)FixedPcdGet64 (PcdFdBaseAddress) > mSystemMemoryEnd) ||\
                  ((FixedPcdGet64 (PcdFdBaseAddress) + FixedPcdGet32 (PcdFdSize)) <= FixedPcdGet64 (PcdSystemMemoryBase)))

UINT64  mSystemMemoryEnd = FixedPcdGet64 (PcdSystemMemoryBase) +
                           FixedPcdGet64 (PcdSystemMemorySize) - 1;

EFI_STATUS
GetPlatformPpi (
  IN  EFI_GUID  *PpiGuid,
  OUT VOID      **Ppi
  )
{
  UINTN                   PpiListSize;
  UINTN                   PpiListCount;
  EFI_PEI_PPI_DESCRIPTOR  *PpiList;
  UINTN                   Index;

  PpiListSize = 0;
  ArmPlatformGetPlatformPpiList (&PpiListSize, &PpiList);
  PpiListCount = PpiListSize / sizeof (EFI_PEI_PPI_DESCRIPTOR);
  for (Index = 0; Index < PpiListCount; Index++, PpiList++) {
    if (CompareGuid (PpiList->Guid, PpiGuid) == TRUE) {
      *Ppi = PpiList->Ppi;
      return EFI_SUCCESS;
    }
  }

  return EFI_NOT_FOUND;
}

#define PIPE_SSPP_SRC_FORMAT                    0x30
#define PIPE_SSPP_SRC_UNPACK_PATTERN            0x34
#define PIPE_BASE				0x1A15000
#define PIPE_BASE1				0x1A17000
#define PIPE_SSPP_SRC_YSTRIDE			0x24

#define MDP_BASE                    (0x1A00000)
#define REG_MDP(off)                (MDP_BASE + (off))
#define MDP_HW_REV                              REG_MDP(0x1000)
#define MDP_VP_0_VIG_0_BASE                     REG_MDP(0x5000)
#define MDP_VP_0_VIG_1_BASE                     REG_MDP(0x7000)
#define MDP_VP_0_RGB_0_BASE                     REG_MDP(0x15000)
#define MDP_VP_0_RGB_1_BASE                     REG_MDP(0x17000)
#define MDP_VP_0_DMA_0_BASE                     REG_MDP(0x25000)
#define MDP_VP_0_DMA_1_BASE                     REG_MDP(0x27000)
#define MDP_VP_0_MIXER_0_BASE                   REG_MDP(0x45000)
#define MDP_VP_0_MIXER_1_BASE                   REG_MDP(0x46000)

#define MDP_CTL_0_BASE                          0x1A02000
#define MDP_CTL_1_BASE                          0x1A02200
#define CTL_FLUSH				0x18

//
// TANI: tek karakteri dogrudan UART2 (0x078B0000) FIFO'suna yazar. Hicbir
// global/.rodata/udelay/lib KULLANMAZ -> ##MEP-B0## ile ayni guvenli yontem.
// Donmanin TAM yerini bulmak icin adim adim isaretci basariz.
//
STATIC
VOID
dbgc (
  IN  CHAR8  c
  )
{
  volatile UINT32  *U = (volatile UINT32 *)(UINTN)0x078B0000;
  UINT32           g;

  U[0x040 / 4] = 1;       // NO_CHARS_FOR_TX
  U[0x0A8 / 4] = 0x300;   // CR = GCMD_RES_TX_RDY_INT
  for (g = 0; g < 0x100000 && ((U[0x0A4 / 4] & 0x4) == 0); g++) {
  }
  U[0x100 / 4] = (UINT32)(UINT8)c;   // TF
  for (g = 0; g < 0x80000; g++) {    // drain
  }
}

VOID
PrePiMain (
  IN  UINTN   UefiMemoryBase,
  IN  UINTN   StacksBase,
  IN  UINT64  StartTimeStamp
  )
{
  EFI_HOB_HANDOFF_INFO_TABLE  *HobList;
  ARM_MP_CORE_INFO_PPI        *ArmMpCoreInfoPpi;
  UINTN                       ArmCoreCount;
  ARM_CORE_INFO               *ArmCoreInfoTable;
  EFI_STATUS                  Status;
  CHAR8                       Buffer[100];
  UINTN                       CharCount;
  UINTN                       StacksSize;
  FIRMWARE_SEC_PERFORMANCE    Performance;

  // If ensure the FD is either part of the System Memory or totally outside of the System Memory (XIP)
  ASSERT (
    IS_XIP () ||
    ((FixedPcdGet64 (PcdFdBaseAddress) >= FixedPcdGet64 (PcdSystemMemoryBase)) &&
     ((UINT64)(FixedPcdGet64 (PcdFdBaseAddress) + FixedPcdGet32 (PcdFdSize)) <= (UINT64)mSystemMemoryEnd))
    );

  /* Switch to 32bpp argb8888 */
  //MmioWrite32(PIPE_BASE + PIPE_SSPP_SRC_FORMAT, 0x000236FF);
  //MmioWrite32(PIPE_BASE + PIPE_SSPP_SRC_UNPACK_PATTERN, 0x03020001);
  //MmioWrite32(PIPE_BASE + PIPE_SSPP_SRC_YSTRIDE, 720 * 4);
  //MmioWrite32(MDP_CTL_0_BASE + CTL_FLUSH, (1 << (3)));

  dbgc ('6');   // PrePiMain'e girildi
  // Initialize the architecture specific bits
  ArchInitialize ();
  dbgc ('7');   // ArchInitialize sonrasi

  // Initialize the Serial Port
  SerialPortInitialize ();
  CharCount = AsciiSPrint (
                Buffer,
                sizeof (Buffer),
                "UEFI firmware (version %s built at %a on %a)\n\r",
                (CHAR16 *)PcdGetPtr (PcdFirmwareVersionString),
                __TIME__,
                __DATE__
                );
  SerialPortWrite ((UINT8 *)Buffer, CharCount);
  dbgc ('8');   // banner yazimi sonrasi

  // Initialize the Debug Agent for Source Level Debugging
  InitializeDebugAgent (DEBUG_AGENT_INIT_POSTMEM_SEC, NULL, NULL);
  SaveAndSetDebugTimerInterrupt (TRUE);
  dbgc ('9');   // DebugAgent sonrasi

  // Declare the PI/UEFI memory region
  HobList = HobConstructor (
              (VOID *)UefiMemoryBase,
              FixedPcdGet32 (PcdSystemMemoryUefiRegionSize),
              (VOID *)UefiMemoryBase,
              (VOID *)StacksBase // The top of the UEFI Memory is reserved for the stacks
              );
  PrePeiSetHobList (HobList);
  dbgc ('a');   // HOB hazir, MemoryPeim (MMU kurulumu) oncesi

  // Initialize MMU and Memory HOBs (Resource Descriptor HOBs)
  Status = MemoryPeim (UefiMemoryBase, FixedPcdGet32 (PcdSystemMemoryUefiRegionSize));
  ASSERT_EFI_ERROR (Status);
  dbgc ('b');   // MemoryPeim (MMU) SONRASI

  // Create the Stacks HOB (reserve the memory for all stacks)
  if (ArmIsMpCore ()) {
    StacksSize = PcdGet32 (PcdCPUCorePrimaryStackSize) +
                 ((FixedPcdGet32 (PcdCoreCount) - 1) * FixedPcdGet32 (PcdCPUCoreSecondaryStackSize));
  } else {
    StacksSize = PcdGet32 (PcdCPUCorePrimaryStackSize);
  }

  BuildStackHob (StacksBase, StacksSize);

  // Initialize QGIC
  dbgc ('c');   // QGicPeim oncesi
  Status = QGicPeim();
  dbgc ('d');   // QGicPeim SONRASI

  if (EFI_ERROR(Status)) {
    DEBUG((EFI_D_ERROR, "Failed to configure GIC\n"));
    CpuDeadLoop();
  }

  // TODO: Call CpuPei as a library
  BuildCpuHob (ArmGetPhysicalAddressBits (), PcdGet8 (PcdPrePiCpuIoSize));

  if (ArmIsMpCore ()) {
    // Only MP Core platform need to produce gArmMpCoreInfoPpiGuid
    Status = GetPlatformPpi (&gArmMpCoreInfoPpiGuid, (VOID **)&ArmMpCoreInfoPpi);

    // On MP Core Platform we must implement the ARM MP Core Info PPI (gArmMpCoreInfoPpiGuid)
    ASSERT_EFI_ERROR (Status);

    // Build the MP Core Info Table
    ArmCoreCount = 0;
    Status       = ArmMpCoreInfoPpi->GetMpCoreInfo (&ArmCoreCount, &ArmCoreInfoTable);
    if (!EFI_ERROR (Status) && (ArmCoreCount > 0)) {
      // Build MPCore Info HOB
      BuildGuidDataHob (&gArmMpCoreInfoGuid, ArmCoreInfoTable, sizeof (ARM_CORE_INFO) * ArmCoreCount);
    }
  }

  // Store timer value logged at the beginning of firmware image execution
  Performance.ResetEnd = GetTimeInNanoSecond (StartTimeStamp);

  // Build SEC Performance Data Hob
  BuildGuidDataHob (&gEfiFirmwarePerformanceGuid, &Performance, sizeof (Performance));

  // Set the Boot Mode
  SetBootMode (ArmPlatformGetBootMode ());

  // Initialize Platform HOBs (CpuHob and FvHob)
  Status = PlatformPeim ();
  ASSERT_EFI_ERROR (Status);
  dbgc ('e');   // PlatformPeim sonrasi

  // Now, the HOB List has been initialized, we can register performance information
  PERF_START (NULL, "PEI", NULL, StartTimeStamp);

  // SEC phase needs to run library constructors by hand.
  dbgc ('f');   // ProcessLibraryConstructorList oncesi
  ProcessLibraryConstructorList ();
  dbgc ('g');   // constructor'lar sonrasi, DecompressFirstFv oncesi

  // Assume the FV that contains the SEC (our code) also contains a compressed FV.
  Status = DecompressFirstFv ();
  ASSERT_EFI_ERROR (Status);
  dbgc ('h');   // DecompressFirstFv sonrasi, LoadDxeCoreFromFv oncesi

  // Load the DXE Core and transfer control to it
  Status = LoadDxeCoreFromFv (NULL, 0);
  ASSERT_EFI_ERROR (Status);
}

VOID
CEntryPoint (
  IN  UINTN  MpId,
  IN  UINTN  UefiMemoryBase,
  IN  UINTN  StacksBase
  )
{
  UINT64  StartTimeStamp;

  dbgc ('1');   // CEntryPoint'e girildi
  if (ArmPlatformIsPrimaryCore (MpId)) {
    SerialPortInitialize ();
  }
  dbgc ('2');   // SerialPortInitialize (banner) sonrasi

  // Initialize the platform specific controllers
  ArmPlatformInitialize (MpId);
  dbgc ('3');   // ArmPlatformInitialize sonrasi

  if (ArmPlatformIsPrimaryCore (MpId) && PerformanceMeasurementEnabled ()) {
    // Initialize the Timer Library to setup the Timer HW controller
    TimerConstructor ();
    // We cannot call yet the PerformanceLib because the HOB List has not been initialized
    StartTimeStamp = GetPerformanceCounter ();
  } else {
    StartTimeStamp = 0;
  }

  // Data Cache enabled on Primary core when MMU is enabled.
  ArmDisableDataCache ();
  // Invalidate instruction cache
  ArmInvalidateInstructionCache ();
  // Enable Instruction Caches on all cores.
  ArmEnableInstructionCache ();

  // Define the Global Variable region when we are not running in XIP
  if (!IS_XIP ()) {
    if (ArmPlatformIsPrimaryCore (MpId)) {
      if (ArmIsMpCore ()) {
        // Signal the Global Variable Region is defined (event: ARM_CPU_EVENT_DEFAULT)
        ArmCallSEV ();
      }
    } else {
      // Wait the Primary core has defined the address of the Global Variable region (event: ARM_CPU_EVENT_DEFAULT)
      ArmCallWFE ();
    }
  }

  dbgc ('4');   // cache/global-region sonrasi, PrimaryMain oncesi

  // If not primary Jump to Secondary Main
  if (ArmPlatformIsPrimaryCore (MpId)) {
    InvalidateDataCacheRange (
      (VOID *)UefiMemoryBase,
      FixedPcdGet32 (PcdSystemMemoryUefiRegionSize)
      );

    dbgc ('5');   // PrimaryMain cagrilmadan hemen once
    // Goto primary Main.
    PrimaryMain (UefiMemoryBase, StacksBase, StartTimeStamp);
  } else {
    SecondaryMain (MpId);
  }

  // DXE Core should always load and never return
  ASSERT (FALSE);
}
