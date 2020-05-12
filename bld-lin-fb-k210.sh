#!/bin/sh

#mkdir Linux/arm/bin
#mkdir Linux/arm/lib


#mkdir out

export PATH=/opt/kendryte-toolchain/bin:$PWD/Linux/386/bin:$PATH

export ROOT=$PWD

export CROSS=riscv64-unknown-elf-

export K210_SDK=$ROOT/../kendryte-freertos-sdk

export FreeRTOS_ROOT=$K210_SDK/lib/freertos

#unzip -n dis_folders.zip

ln -sf mkconfig.FreeRTOS-riscv64 mkconfig

mk install

#cp Linux/arm/bin/emu out