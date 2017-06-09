#!/bin/bash

TheSkyX_Install=~/Library/Application\ Support/Software\ Bisque/TheSkyX\ Professional\ Edition/TheSkyXInstallPath.txt
echo "TheSkyX_Install = $TheSkyX_Install"

if [ ! -f "$TheSkyX_Install" ]; then
    echo TheSkyXInstallPath.txt not found
    exit 1
fi


TheSkyX_Path=$(<"$TheSkyX_Install")
echo "Installing to $TheSkyX_Path"

if [ ! -d "$TheSkyX_Path" ]; then
    echo TheSkyX Install dir not exist
    exit 1
fi

cp "./focuserlist PegasusController.txt" "$TheSkyX_Path/Resources/Common/Miscellaneous Files/"
cp "./PegasusController.ui" "$TheSkyX_Path/Resources/Common/PlugIns/FocuserPlugIns/"
cp "./PegasusAstro.png" "$TheSkyX_Path/Resources/Common/PlugIns/FocuserPlugIns/"
cp "./libPegasusController.so" "$TheSkyX_Path/Resources/Common/PlugIns/FocuserPlugIns/"

app_owner=`/usr/bin/stat -c "%u" "$TheSkyX_Path" | xargs id -n -u`
if [ ! -z "$app_owner" ]; then
	chown $app_owner "$TheSkyX_Path/Resources/Common/Miscellaneous Files/focuserlist PegasusController.txt"
	chown $app_owner "$TheSkyX_Path/Resources/Common/PlugIns/FocuserPlugIns/PegasusController.ui"
	chown $app_owner "$TheSkyX_Path/Resources/Common/PlugIns/FocuserPlugIns/PegasusAstro.png"
	chown $app_owner "$TheSkyX_Path/Resources/Common/PlugIns/FocuserPlugIns/libPegasusController.so"
fi
chmod  755 "$TheSkyX_Path/Resources/Common/PlugIns/FocuserPlugIns/libPegasusController.so"
