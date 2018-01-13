#!/bin/bash

set -e

sudo add-apt-repository --yes ppa:beineri/opt-qt593-trusty
sudo apt-get update -qq

wget https://ftp.fau.de/debian/pool/main/p/patchelf/patchelf_0.8-2_amd64.deb
echo "5d506507df7c02766ae6c3ca0d15b4234f4cb79a80799190ded9d3ca0ac28c0c  patchelf_0.8-2_amd64.deb" | sha256sum -c
sudo dpkg -i patchelf_0.8-2_amd64.deb

cd /tmp/
wget -c "https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage"
chmod +x appimagetool*AppImage
./appimagetool*AppImage --appimage-extract
sudo cp squashfs-root/usr/bin/* /usr/local/bin/
sudo cp -r squashfs-root/usr/lib/appimagekit /usr/local/lib/
sudo chmod +rx /usr/local/lib/appimagekit
cd -

sudo apt-get -y install qt59base qt59declarative qt59webengine binutils xpra zsync desktop-file-utils gcc g++ make libgl1-mesa-dev fuse psmisc qt59translations
