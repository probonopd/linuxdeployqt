#!/bin/bash

set -e

wget -c "https://ftp.fau.de/debian/pool/main/p/patchelf/patchelf_0.8-2_amd64.deb"
echo "5d506507df7c02766ae6c3ca0d15b4234f4cb79a80799190ded9d3ca0ac28c0c  patchelf_0.8-2_amd64.deb" | sha256sum -c
sudo dpkg -i patchelf_0.8-2_amd64.deb

cd /tmp/
wget -c "https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage"
chmod +x appimagetool*AppImage
./appimagetool*AppImage --appimage-extract
sudo cp squashfs-root/usr/bin/* /usr/local/bin
cd -

sudo apt-get -y install binutils xpra zsync desktop-file-utils

# Download Qt from qt.io
. qtdownload
QTSUBCOMPONENT=qtdeclarative ; . qtdownload
QTCOMPONENT=qt.qt5.5100.qtwebengine.gcc_64 ;  QTSUBCOMPONENT=qtwebengine ; . qtdownload
