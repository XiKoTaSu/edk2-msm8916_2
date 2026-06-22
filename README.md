# edk2-msm8916

UEFI firmware (EDK II) for Qualcomm **MSM8916 / APQ8016** boards, currently targeting the
**96Boards DragonBoard 410c**.

The goal is a clean, bare-metal UEFI port that boots a stock AArch64 Linux (e.g. Armbian)
on real hardware — built from source for the actual SoC peripherals rather than relying on
prebuilt vendor blobs.

> Lineage: this work descends from the imbushuo / Ivaylo Ivanov Windows-on-ARM MSM8916
> effort (Lumia950XLPkg and friends), heavily reworked and cleaned up for the DragonBoard 410c.

---

## Target hardware

| | |
|---|---|
| Board | DragonBoard 410c (96Boards CE) |
| SoC | Qualcomm APQ8016 (MSM8916), 4× Cortex-A53 @ 1.2 GHz |
| RAM | 1 GB LPDDR3 |
| Storage | eMMC + microSD |

---

## Status

**Working**
- Boots to the UEFI Boot Manager / UEFI Shell
- ACPI tables + PSCI (CPU power / reset)
- Serial console over UART (debug UART)
- eMMC / microSD (SDHCI)
- USB host — keyboard/mouse/mass-storage via EHCI + ChipIdea + on-board USB hub
  (see the [S6 switch note](#important--s6-dip-switch) below)
- I²C (BLSP QUP) and PMIC regulators via the RPM/SMD co-processor
- Boots stock AArch64 Linux (tested with Armbian, kernel 6.x)

**Work in progress**
- HDMI output via the on-board ADV7533 (MIPI-DSI → HDMI) bridge: the I²C/control path and
  power-up are done; the DSI/MDP5 display pipeline that feeds it is still being brought up.

---

## Building

Tested on Ubuntu 22.04 (x86-64).

The repository expects the EDK II trees to live **next to it** (siblings):

```
<parent>/edk2-msm8916     <- this repository
<parent>/edk2             <- fetched by init.sh
<parent>/edk2-platforms   <- fetched by init.sh
```

**1. One-time setup** — installs dependencies, fetches EDK II sources and builds BaseTools:

```bash
./init.sh
```

**2. Build + package** — compiles the firmware and produces a fastboot-bootable image:

```bash
./build.sh
```

Output: `db410c_uefi.img`.

---

## Booting

Put the board into fastboot mode and boot the image (nothing is flashed permanently):

```bash
fastboot boot db410c_uefi.img
```

A serial console (1.8 V UART on the low-speed expansion header, 115200 8N1) is recommended
to watch the boot.

---

## Important — S6 DIP switch

USB on the DragonBoard 410c is muxed by switch **S6-3**:

| S6-3 | USB mode | Effect |
|------|----------|--------|
| **OFF** | device / peripheral | `fastboot` works (host PC sees the board) |
| **ON**  | host / OTG | USB host (keyboard / Type-A ports / hub) works, but `fastboot` does **not** |

So: flash/boot with **S6-3 OFF**, then set it **ON** if you want to use a USB keyboard.

---

## Repository layout

```
init.sh             One-time setup (deps + EDK II sources + BaseTools)
build.sh            Build + package -> db410c_uefi.img
bootimg_db410c.cfg  Boot image parameters (load addresses)
asl.exe             ACPI (DSDT) compiler, run under wine
MSM8916Pkg/         The EDK II platform package
  Devices/db410c.dsc  Per-device build entry (includes MSM8916Pkg.dsc)
  Drivers/            SoC drivers (GPIO, SPMI, PMIC, clock, SDHCI, I2C, USB, RPM, ...)
  Library/            Platform libraries
  AcpiTables/         ACPI sources (DSDT, FADT, MADT, GTDT, ...)
```

### Adding another MSM8916 device

The package is structured so other MSM8916 boards can be added without touching the shared
core: drop a `MSM8916Pkg/Devices/<board>.dsc` (with that board's PCDs / memory map / DTB) that
`!include`s `MSM8916Pkg.dsc`, and build it the same way.

---

## Credits

- imbushuo (Bingxing Wang) and the WOA-Project — [Lumia950XLPkg](https://github.com/WOA-Project/Lumia950XLPkg)
  (the original MSM8916 EDK II groundwork, including the SimpleFbDxe framebuffer driver)
- Ivaylo Ivanov — MSM8916 device-tree / bring-up work
- TianoCore — [EDK II](https://github.com/tianocore/edk2)

## License

EDK II portions are BSD-2-Clause-Patent (see the upstream EDK II license). Files retain their
original per-file license headers.
