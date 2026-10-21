#!/bin/sh
# Fetches the SDL2 MinGW development package the Windows build links against
# (third_party/sdl2-mingw: include/ and lib/ for x86_64-w64-mingw32).
set -e
cd "$(dirname "$0")/.."
V=2.30.9
[ -d third_party/sdl2-mingw/x86_64-w64-mingw32 ] && { echo "third_party/sdl2-mingw is already there"; exit 0; }
mkdir -p build third_party
curl -L -o build/sdl2-mingw.tar.gz "https://github.com/libsdl-org/SDL/releases/download/release-$V/SDL2-devel-$V-mingw.tar.gz"
tar -xzf build/sdl2-mingw.tar.gz -C build
rm -rf third_party/sdl2-mingw && mv "build/SDL2-$V" third_party/sdl2-mingw
echo "SDL2 $V (MinGW) in third_party/sdl2-mingw"
