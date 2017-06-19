#!/bin/bash

set -x

source /opt/qt*/bin/qt*-env.sh
/opt/qt*/bin/qmake CONFIG+=release CONFIG+=force_debug_info linuxdeployqt.pro
make -j

mkdir -p linuxdeployqt.AppDir/usr/bin/
cp /usr/bin/patchelf /usr/local/bin/{appimagetool,mksquashfs,zsyncmake} linuxdeployqt.AppDir/usr/bin/
find linuxdeployqt.AppDir/
export VERSION=continuous
cp ./bin/linuxdeployqt linuxdeployqt.AppDir/usr/bin/
./bin/linuxdeployqt linuxdeployqt.AppDir/linuxdeployqt.desktop -verbose=3 -appimage
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
