#!/usr/bin/env sh
#This is only for Windows with Visual Studio 2015.
#Run in Git bash or equivalent. git, conan and cmake have to be in PATH for this to work.
#Assumed directory layout:
# %DEV%/warpcoil
# %DEV%/build/warpcoil
#Run this script from %DEV%!
git clone git@github.com:TyRoXx/warpcoil.git
mkdir -p build/warpcoil
cd build/warpcoil
conan install ../../warpcoil --build=missing
cmake ../../warpcoil -G "Visual Studio 14 2015 Win64"
