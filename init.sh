#!/bin/bash
#
# DragonBoard 410c UEFI — bir kerelik kurulum.
#   1) APT bagimliliklarini yukler
#   2) EDK2 kaynaklarini KARDES dizinlere ceker (../edk2, ../edk2-platforms)
#   3) EDK2 BaseTools'u derler
# Kullanim:  ./init.sh    (sonra: ./build.sh)
#
# Beklenen dizin yapisi (yan yana):
#   <parent>/db410c-uefi   <- bu repo
#   <parent>/edk2
#   <parent>/edk2-platforms
#
set -e
REPO="$(cd "$(dirname "$0")" && pwd)"

echo "==> [1/3] APT bagimliliklari yukleniyor"
sudo apt update
sudo apt install -y \
  build-essential uuid-dev iasl git nasm python3 python-is-python3 \
  crossbuild-essential-arm64 crossbuild-essential-armhf \
  bc abootimg gzip wine

echo "==> [2/3] EDK2 kaynaklari (../edk2, ../edk2-platforms)"
cd "$REPO/.."
if [ ! -d edk2 ]; then
  git clone https://github.com/tianocore/edk2.git -b edk2-stable202302 --recursive
else
  echo "    ../edk2 zaten var — atlandi"
fi
if [ ! -d edk2-platforms ]; then
  git clone https://github.com/tianocore/edk2-platforms.git
else
  echo "    ../edk2-platforms zaten var — atlandi"
fi

echo "==> [3/3] EDK2 BaseTools derleniyor"
cd "$REPO"
make -C ../edk2/BaseTools

echo
echo "==> KURULUM TAMAM."
echo "    Derle + paketle:  ./build.sh"
echo "    Test:             fastboot boot db410c_uefi.img"
