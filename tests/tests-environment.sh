#!/bin/bash

set -e

sudo add-apt-repository --yes ppa:beineri/opt-qt593-xenial
sudo apt-get update -qq

wget https://ftp.fau.de/debian/pool/main/p/patchelf/patchelf_0.8-2_amd64.deb
echo "5d506507df7c02766ae6c3ca0d15b4234f4cb79a80799190ded9d3ca0ac28c0c  patchelf_0.8-2_amd64.deb" | sha256sum -c
sudo dpkg -i patchelf_0.8-2_amd64.deb
# We want a newer patchelf since the one above is missing e.g., '--add-needed' which our users might want to use
# However, the newer patchelf versions are broken and cripple, e.g., libQt5Core; see https://github.com/NixOS/patchelf/issues/124
# git clone -o e1e39f3 https://github.com/NixOS/patchelf
# cd patchelf
# bash ./bootstrap.sh
# ./configure --prefix=/usr
# make -j$(nproc)
# sudo make install

pushd /tmp/
# wget -c https://artifacts.assassinate-you.net/artifactory/AppImageKit/travis-2052/appimagetool-x86_64.AppImage # branch last-good, https://travis-ci.org/AppImage/AppImageKit/jobs/507462541
wget -c "https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage"
chmod +x appimagetool*AppImage
./appimagetool*AppImage --appimage-extract
mv squashfs-root/ appimagekit.AppDir/
sudo ln -s "$(readlink -f appimagekit.AppDir/AppRun)" /usr/bin/appimagetool
popd

sudo apt-get -y install qt59base qt59declarative qt59webengine binutils xpra zsync desktop-file-utils gcc g++ make libgl1-mesa-dev psmisc qt59translations
