#!/bin/sh

mkdir acme/dis

export ROOT=$PWD

export PATH=$PWD/Linux/386/bin:$PATH

unzip -n dis_folders.zip

ln -sf mkconfig.Linux-386 mkconfig

#mk mkdirs

mk install
