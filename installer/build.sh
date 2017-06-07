#!/bin/bash

mkdir -p ROOT/tmp/PegasusController_X2/
cp "../PegasusController.ui" ROOT/tmp/PegasusController_X2/
cp "../PegasusAstro.png" ROOT/tmp/PegasusController_X2/
cp "../focuserlist PegasusController.txt" ROOT/tmp/PegasusController_X2/
cp "../build/Release/libPegasusController.dylib" ROOT/tmp/PegasusController_X2/

if [ ! -z "$installer_signature" ]; then
# signed package using env variable installer_signature
pkgbuild --root ROOT --identifier org.rti-zone.PegasusController_X2 --sign "$installer_signature" --scripts Scripts --version 1.0 PegasusController_X2.pkg
pkgutil --check-signature ./PegasusController_X2.pkg
else
pkgbuild --root ROOT --identifier org.rti-zone.PegasusController_X2 --scripts Scripts --version 1.0 PegasusController_X2.pkg
fi

rm -rf ROOT
