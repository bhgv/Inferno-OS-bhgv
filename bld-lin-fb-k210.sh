#!/bin/sh

#mkdir Linux/arm/bin
#mkdir Linux/arm/lib


#mkdir out

export K210_TOOLCHAIN_PATH=/opt/kendryte-toolchain

export PATH=$K210_TOOLCHAIN_PATH/bin:$PWD/Linux/386/bin:$PATH

export ROOT=$PWD

export CROSS=riscv64-unknown-elf-

export K210_SDK=$ROOT/../kendryte-freertos-sdk

export FreeRTOS_ROOT=$K210_SDK/lib/freertos

export K210_SDK_LIBS=$ROOT/extSDK/lib

#a path where is it: crti.o, crtbegin.o, crtend.o, crtn.o
export K210_CRT_BEGIN_END_PATH=$K210_TOOLCHAIN_PATH/lib/gcc/riscv64-unknown-elf/8.2.0

export EMU_LD_SCRIPT=$K210_SDK/lds/kendryte.ld

#unzip -n dis_folders.zip

ln -sf mkconfig.FreeRTOS-riscv64 mkconfig

mk install

#cp Linux/arm/bin/emu out