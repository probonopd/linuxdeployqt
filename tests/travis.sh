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
# . qtdownload
# QTSUBCOMPONENT=qtdeclarative ; . qtdownload
# QTCOMPONENT=qt.qt5.5100.qtwebengine.gcc_64 ;  QTSUBCOMPONENT=qtwebengine ; . qtdownload
wget -c "https://raw.githubusercontent.com/RPCS3/rpcs3/master/qt-installer-noninteractive.qs"
wget -c "http://download.qt.io/official_releases/online_installers/qt-unified-linux-x64-online.run"
chmod a+x ./qt-unified-linux-x64-online.run
export QT_QPA_PLATFORM=minimal
./qt-unified-linux-x64-online.run --script qt-installer-noninteractive.qs --no-force-installations --verbose
export CMAKE_PREFIX_PATH=$(readlink -f ~/Qt/*/gcc_64/lib/cmake)
export LD_LIBRARY_PATH=$(readlink -f ~/Qt/*/gcc_64/lib)
export PATH=$(readlink -f ~/Qt/*/gcc_64/bin/):${PATH}
qmake -v

qmake CONFIG+=release CONFIG+=force_debug_info linuxdeployqt.pro
make -j

mkdir -p linuxdeployqt.AppDir/usr/bin/
cp /usr/bin/{patchelf,desktop-file-validate} /usr/local/bin/{appimagetool,mksquashfs,zsyncmake} linuxdeployqt.AppDir/usr/bin/
find linuxdeployqt.AppDir/
export VERSION=continuous
cp ./linuxdeployqt linuxdeployqt.AppDir/usr/bin/
./linuxdeployqt linuxdeployqt.AppDir/usr/bin/desktop-file-validate -verbose=3 -bundle-non-qt-libs
./linuxdeployqt linuxdeployqt.AppDir/linuxdeployqt.desktop -verbose=3 -appimage
ls -lh
find *.AppDir
xpra start :99

export DISPLAY=:99

until xset -q
do
        echo "Waiting for X server to start..."
        sleep 1;
done

# enable core dumps
echo "/tmp/coredump" | sudo tee /proc/sys/kernel/core_pattern

ulimit -c unlimited
ulimit -a -S
ulimit -a -H

bash -e tests/tests.sh

if [ $? -ne 0 ]; then
  echo "FAILURE: linuxdeployqt CRASHED -- uploading files for debugging to transfer.sh"
  set -v
  [ -e /tmp/coredump ] && curl --upload-file /tmp/coredump https://transfer.sh/coredump
  curl --upload-file linuxdeployqt-*-x86_64.AppImage https://transfer.sh/linuxdeployqt-x86_64.AppImage
  find -type f -iname 'libQt5Core.so*' -exec curl --upload {} https://transfer.sh/libQt5Core.so \; || true
  exit $RESULT
fi
