#!/bin/bash

if ! test -f "linuxdeploy-x86_64.AppImage"; then
    wget https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
fi
if ! test -f "linuxdeploy-plugin-qt-x86_64.AppImage"; then
    wget https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage
fi
chmod u+x ./*.AppImage
mkdir -p usr/share/icons
mkdir -p usr/lib
mkdir -p usr/translations/karbo/
cp ../src/images/karbowanez.png usr/share/icons/karbowanec.png
cp ../build/release/languages/*.qm usr/translations/karbo/
./linuxdeploy-x86_64.AppImage --executable ../build/release/KarbowanecWallet --desktop-file karbowanecwallet.desktop --appdir . --output appimage --plugin qt
