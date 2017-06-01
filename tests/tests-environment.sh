#!/bin/bash

set -e

sudo add-apt-repository --yes ppa:beineri/opt-qt58-trusty
sudo apt-get update -qq

wget http://ftp.de.debian.org/debian/pool/main/p/patchelf/patchelf_0.8-2_amd64.deb
sudo dpkg -i patchelf_0.8-2_amd64.deb

cd /tmp/
wget -c "https://github.com/probonopd/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage"
chmod +x appimagetool*AppImage
./appimagetool*AppImage --appimage-extract
sudo cp squashfs-root/usr/bin/* /usr/local/bin
cd -

sudo apt-get -y install qt58base qt58declarative qt58webengine binutils xpra
