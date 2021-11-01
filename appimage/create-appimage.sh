#!/bin/bash

if ! test -f "linuxdeploy-x86_64.AppImage"; then
    wget https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
fi
if ! test -f "linuxdeploy-plugin-qt-x86_64.AppImage"; then
    wget https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage
fi
chmod u+x ./*.AppImage
cp ../build/release/KarbowanecWallet .
mkdir -p usr/share/icons
mkdir -p usr/lib
cp ../src/images/karbowanez.png usr/share/icons/karbowanec.png
./linuxdeploy-x86_64.AppImage --executable ./KarbowanecWallet --desktop-file karbowanecwallet.desktop --appdir . --output appimage --plugin qt