[Defines]
  PLATFORM_NAME                  = MSM8916Pkg
  PLATFORM_GUID                  = 28f1a3bf-193a-47e3-a7b9-5a435eaab2ef
  PLATFORM_VERSION               = 0.1
  DSC_SPECIFICATION              = 0x00010019
  OUTPUT_DIRECTORY               = Build/$(PLATFORM_NAME)
  SUPPORTED_ARCHITECTURES        = AARCH64
  BUILD_TARGETS                  = DEBUG|RELEASE
  SKUID_IDENTIFIER               = DEFAULT
  FLASH_DEFINITION               = MSM8916Pkg/MSM8916Pkg.fdf

!include MSM8916Pkg/MSM8916Pkg.dsc

[PcdsFixedAtBuild.common]
  gEfiMdeModulePkgTokenSpaceGuid.PcdImageProtectionPolicy|0x0
  # System Memory (DB410c: 1GB = 0x80000000..0xBFFFFFFF)
  # DIKKAT: 0x60000000 (1.5GB) idi -> stack tepesi 0xE0000000 = var olmayan RAM
  # -> CEntryPoint'e girer girmez stack push'ta cokme (banner bile basilmadan).
  gQcomTokenSpaceGuid.PcdMsmSharedBase|0x86300000
  gQcomTokenSpaceGuid.PcdApcsAlias0IpcInterrupt|0x0B011008
  gQcomTokenSpaceGuid.PcdSmdIrq|168
  gArmTokenSpaceGuid.PcdSystemMemoryBase|0x80000000
  gArmTokenSpaceGuid.PcdSystemMemorySize|0x40000000
  
  # Framebuffer
  gMSM8916PkgTokenSpaceGuid.PcdMipiFrameBufferAddress|0x8E000000
  gMSM8916PkgTokenSpaceGuid.PcdMipiFrameBufferWidth|854
  gMSM8916PkgTokenSpaceGuid.PcdMipiFrameBufferHeight|480
  gMSM8916PkgTokenSpaceGuid.PcdMipiFrameBufferVisibleWidth|854
  gMSM8916PkgTokenSpaceGuid.PcdMipiFrameBufferVisibleHeight|480

[PcdsFeatureFlag.common]
  gQcomTokenSpaceGuid.PcdInstallRpmProtocol|TRUE
