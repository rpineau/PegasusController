#!/bin/bash

PACKAGE_NAME="PegasusController_X2.pkg"
BUNDLE_NAME="org.rti-zone.PegasusControllerX2"

if [ ! -z "$app_id_signature" ]; then
    codesign -f -s "$app_id_signature" --verbose ../build/Release/libPegasusController.dylib
fi

mkdir -p ROOT/tmp/PegasusController_X2/
cp "../PegasusController.ui" ROOT/tmp/PegasusController_X2/
cp "../PegasusAstro.png" ROOT/tmp/PegasusController_X2/
cp "../focuserlist PegasusController.txt" ROOT/tmp/PegasusController_X2/
cp "../build/Release/libPegasusController.dylib" ROOT/tmp/PegasusController_X2/

if [ ! -z "$installer_signature" ]; then
	# signed package using env variable installer_signature
	pkgbuild --root ROOT --identifier "$BUNDLE_NAME" --sign "$installer_signature" --scripts Scripts --version 1.0 "$PACKAGE_NAME"
	pkgutil --check-signature "./${PACKAGE_NAME}"
	xcrun notarytool submit "$PACKAGE_NAME" --keychain-profile "$AC_PROFILE" --wait
	xcrun stapler staple $PACKAGE_NAME
else
	pkgbuild --root ROOT --identifier $BUNDLE_NAME --scripts Scripts --version 1.0 $PACKAGE_NAME
fi


rm -rf ROOT
