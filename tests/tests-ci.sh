#!/bin/bash

set -x

source /opt/qt*/bin/qt*-env.sh
/opt/qt*/bin/qmake CONFIG+=release CONFIG+=force_debug_info linuxdeployqt.pro
make -j

# exit on failure
set -e

mkdir -p linuxdeployqt.AppDir/usr/{bin,lib}
cp /usr/bin/{patchelf,desktop-file-validate} /usr/local/bin/{appimagetool,zsyncmake} linuxdeployqt.AppDir/usr/bin/
cp ./bin/linuxdeployqt linuxdeployqt.AppDir/usr/bin/
cp -r /usr/local/lib/appimagekit linuxdeployqt.AppDir/usr/lib/
find linuxdeployqt.AppDir/
export VERSION=continuous
./bin/linuxdeployqt linuxdeployqt.AppDir/usr/bin/desktop-file-validate -verbose=3 -bundle-non-qt-libs
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

# error handling performed separately
set +e

# print version number
./linuxdeployqt-*-x86_64.AppImage --version

# TODO: reactivate tests
#bash -e tests/tests.sh
true
RESULT=$?

if [ $RESULT -ne 0 ]; then
  echo "FAILURE: linuxdeployqt CRASHED -- uploading files for debugging to transfer.sh"
  set -v
  [ -e /tmp/coredump ] && curl --upload-file /tmp/coredump https://transfer.sh/coredump
  curl --upload-file linuxdeployqt-*-x86_64.AppImage https://transfer.sh/linuxdeployqt-x86_64.AppImage
  find -type f -iname 'libQt5Core.so*' -exec curl --upload {} https://transfer.sh/libQt5Core.so \; || true
  exit $RESULT
fi
