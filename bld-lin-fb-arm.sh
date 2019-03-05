#!/bin/sh

mkdir Linux/arm/bin
mkdir Linux/arm/lib

export PATH=/opt/gcc-arm-8.2-2018.11-x86_64-arm-linux-gnueabihf/bin:$PWD/Linux/386/bin:$PATH

export ROOT=$PWD

export CROSS=arm-linux-gnueabihf-

unzip -n dis_folders.zip

mk install
