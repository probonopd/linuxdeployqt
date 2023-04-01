#!/bin/bash

set -e

sudo add-apt-repository --yes ppa:beineri/opt-qt593-xenial
sudo apt-get update -qq

# wget https://ftp.fau.de/debian/pool/main/p/patchelf/patchelf_0.8-2_amd64.deb
# echo "5d506507df7c02766ae6c3ca0d15b4234f4cb79a80799190ded9d3ca0ac28c0c  patchelf_0.8-2_amd64.deb" | sha256sum -c
wget http://old-releases.ubuntu.com/ubuntu/pool/universe/p/patchelf/patchelf_0.8-2_amd64.deb
echo "b3b69f346afb16ab9e925b91afc4c77179a70b676c2eb58a2821f8664fd4cae6  patchelf_0.8-2_amd64.deb" | sha256sum -c

sudo dpkg -i patchelf_0.8-2_amd64.deb
# We want a newer patchelf since the one above is missing e.g., '--add-needed' which our users might want to use
# However, the newer patchelf versions are broken and cripple, e.g., libQt5Core; see https://github.com/NixOS/patchelf/issues/124
# git clone -o e1e39f3 https://github.com/NixOS/patchelf
# cd patchelf
# bash ./bootstrap.sh
# ./configure --prefix=/usr
# make -j$(nproc)
# sudo make install

cd /tmp/
# wget -c https://artifacts.assassinate-you.net/artifactory/AppImageKit/travis-2052/appimagetool-x86_64.AppImage # branch last-good, https://travis-ci.org/AppImage/AppImageKit/jobs/507462541
# wget -c "https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage" # See #542
wget -c "https://github.com/AppImage/AppImageKit/releases/download/13/appimagetool-x86_64.AppImage" # Workaround for #542
chmod +x appimagetool*AppImage
./appimagetool*AppImage --appimage-extract
sudo cp squashfs-root/usr/bin/* /usr/local/bin/
sudo cp -r squashfs-root/usr/lib/appimagekit /usr/local/lib/
sudo chmod +rx /usr/local/lib/appimagekit
cd -

sudo apt-get -y install qt59base qt59declarative qt59webengine binutils xpra zsync desktop-file-utils gcc g++ make libgl1-mesa-dev fuse psmisc qt59translations
