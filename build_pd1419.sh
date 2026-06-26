#!/bin/bash
#
# Vivo Y23L UEFI — derle + paketle (tek komut).
# Once bir kez ./init.sh calistirilmali.
# Cikti: pd1419_uefi.img   ->   fastboot boot pd1419_uefi.img
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

cd BootShim
make REQUIRES_KERNEL_HEADER=0 FD_BASE=0x80080000 FD_SIZE=0x00200000
cd -
# ---- EDK2 derle (-n 4: dusuk RAM icin paralel thread siniri) ----
GCC5_AARCH64_PREFIX=aarch64-linux-gnu- build -s -n 4 -a AARCH64 -t GCC5 -p MSM8916Pkg/Devices/pd1419.dsc

# ---- Paketle: fastboot boot imaji (kerneladdr, MSM8916Pkg.fdf BaseAddress 0x80080000 ile ayni) ----
FD=workspace/Build/MSM8916Pkg/DEBUG_GCC5/FV/MSM8916PKG_UEFI.fd
DTB="${DTB:-MSM8916Pkg/pd1419.dtb}"           # repo-ici pd1419 DTB (override: DTB=/yol ./build.sh)
INITRD="${INITRD:-workspace/dummy-initrd.gz}" # UEFI ramdisk'i kullanmaz; minimal uretilir
CFG=bootimg_pd1419.cfg
OUT=pd1419_uefi.img

[ -f "$FD" ]  || { echo "HATA: $FD yok."; exit 1; }
[ -f "$DTB" ] || { echo "HATA: DTB yok: $DTB"; exit 1; }
[ -f "$INITRD" ] || { mkdir -p workspace; printf '' | gzip -c > "$INITRD"; }

echo "[paket] FD gzip + pd1419 DTB ekleniyor..."
cat BootShim/BootShim.bin >> MSM8916PKG_UEFI.fd
gzip -c "$FD" > uefi_kernel_pd1419.gz
cat "$DTB" >> uefi_kernel_pd1419.gz

echo "[paket] abootimg ile $OUT olusturuluyor..."
abootimg --create "$OUT" -f "$CFG" -k uefi_kernel_pd1419.gz -r "$INITRD"

echo
echo "HAZIR: $OUT"
echo "Test:  fastboot boot $OUT"
