#!/bin/bash

set -e

sudo add-apt-repository --yes ppa:beineri/opt-qt571-trusty
sudo apt-get update -qq

git clone -o 44b7f95 https://github.com/NixOS/patchelf.git
cd patchelf
bash ./bootstrap.sh
./configure
make -j2
sudo make install

cd -

cd /tmp/
wget -c "https://github.com/probonopd/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage"
chmod +x appimagetool*AppImage
./appimagetool*AppImage --appimage-extract
sudo cp squashfs-root/usr/bin/* /usr/local/bin
cd -

sudo apt-get -y install qt57base qt57declarative qt57webengine binutils xpra
