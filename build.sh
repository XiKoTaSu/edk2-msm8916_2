#!/bin/bash
#
# DragonBoard 410c UEFI — derle + paketle (tek komut).
# Once bir kez ./init.sh calistirilmali.
# Cikti: db410c_uefi.img   ->   fastboot boot db410c_uefi.img
#
set -e
cd "$(dirname "$0")"
rm -rf workspace/Build
mkdir -p workspace          # edksetup WORKSPACE'in var olmasini bekler (fresh clone)

# ---- EDK2 ortami ----
export PACKAGES_PATH="$PWD/../edk2:$PWD/../edk2-platforms:$PWD"
export WORKSPACE="$PWD/workspace"
. ../edk2/edksetup.sh

# ---- ACPI DSDT derle (Windows asl.exe, wine ile; Windows+Linux ortak yapi ileride) ----
wine asl.exe MSM8916Pkg/AcpiTables/Dsdt/Dsdt.asl
mv DSDT.AML MSM8916Pkg/AcpiTables/DSDT.AML

# ---- EDK2 derle (-n 4: dusuk RAM icin paralel thread siniri) ----
GCC5_AARCH64_PREFIX=aarch64-linux-gnu- build -s -n 4 -a AARCH64 -t GCC5 -p MSM8916Pkg/Devices/db410c.dsc

# ---- Paketle: fastboot boot imaji (kerneladdr, MSM8916Pkg.fdf BaseAddress 0x80080000 ile ayni) ----
FD=workspace/Build/MSM8916Pkg/DEBUG_GCC5/FV/MSM8916PKG_UEFI.fd
DTB="${DTB:-MSM8916Pkg/DB410c.dtb}"           # repo-ici DB410c DTB (override: DTB=/yol ./build.sh)
INITRD="${INITRD:-workspace/dummy-initrd.gz}" # UEFI ramdisk'i kullanmaz; minimal uretilir
CFG=bootimg_db410c.cfg
OUT=db410c_uefi.img

[ -f "$FD" ]  || { echo "HATA: $FD yok."; exit 1; }
[ -f "$DTB" ] || { echo "HATA: DTB yok: $DTB"; exit 1; }
[ -f "$INITRD" ] || { mkdir -p workspace; printf '' | gzip -c > "$INITRD"; }

echo "[paket] FD gzip + DB410c DTB ekleniyor..."
gzip -c "$FD" > uefi_kernel_db410c.gz
cat "$DTB" >> uefi_kernel_db410c.gz

echo "[paket] abootimg ile $OUT olusturuluyor..."
abootimg --create "$OUT" -f "$CFG" -k uefi_kernel_db410c.gz -r "$INITRD"

echo
echo "HAZIR: $OUT"
echo "Test:  fastboot boot $OUT"
