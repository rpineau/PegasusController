; Script generated by the Inno Setup Script Wizard.
; SEE THE DOCUMENTATION FOR DETAILS ON CREATING INNO SETUP SCRIPT FILES!

#define MyAppName "Pegasus DMFC X2 Driver"
#define MyAppVersion "1.4"
#define MyAppPublisher "RTI-Zone"
#define MyAppURL "https://rti-zone.org"

[Setup]
; NOTE: The value of AppId uniquely identifies this application.
; Do not use the same AppId value in installers for other applications.
; (To generate a new GUID, click Tools | Generate GUID inside the IDE.)
AppId={{B3AEF9EE-248D-11E8-8528-4FE071C4F632}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
;AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={code:TSXInstallDir}\Resources\Common
DefaultGroupName={#MyAppName}

; Need to customise these
; First is where you want the installer to end up
OutputDir=installer
; Next is the name of the installer
OutputBaseFilename=PegasusController_X2_Installer
; Final one is the icon you would like on the installer. Comment out if not needed.
SetupIconFile=rti_zone_logo.ico
Compression=lzma
SolidCompression=yes
; We don't want users to be able to select the drectory since read by TSXInstallDir below
DisableDirPage=yes
; Uncomment this if you don't want an uninstaller.
;Uninstallable=no
CloseApplications=yes
DirExistsWarning=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Dirs]
Name: "{app}\Plugins\FocuserPlugins";
Name: "{app}\Plugins64\FocuserPlugins";

[Files]
; WIll also need to customise these!
Source: "focuserlist PegasusController.txt";                            DestDir: "{app}\Miscellaneous Files"; Flags: ignoreversion
Source: "focuserlist PegasusController.txt";                            DestDir: "{app}\Miscellaneous Files"; DestName: "focuserlist64 PegasusController.txt"; Flags: ignoreversion
; 32 bits
Source: "libPegasusController\Win32\Release\libPegasusController.dll";  DestDir: "{app}\Plugins\FocuserPlugins"; Flags: ignoreversion
Source: "PegasusController.ui";                                         DestDir: "{app}\Plugins\FocuserPlugins"; Flags: ignoreversion
Source: "PegasusAstro.png";                                             DestDir: "{app}\Plugins\FocuserPlugins"; Flags: ignoreversion
;64 bits
Source: "libPegasusController\x64\Release\libPegasusController.dll";   DestDir: "{app}\Plugins64\FocuserPlugins"; Flags: ignoreversion; Check: DirExists(ExpandConstant('{app}\Plugins64\FocuserPlugins'))
Source: "PegasusController.ui";                                         DestDir: "{app}\Plugins64\FocuserPlugins"; Flags: ignoreversion; Check: DirExists(ExpandConstant('{app}\Plugins64\FocuserPlugins'))
Source: "PegasusAstro.png";                                             DestDir: "{app}\Plugins64\FocuserPlugins"; Flags: ignoreversion; Check: DirExists(ExpandConstant('{app}\Plugins64\FocuserPlugins'))


[Code]
{* Below is a function to read TheSkyXInstallPath.txt and confirm that the directory does exist
   This is then used in the DefaultDirName above
   *}
var
  Location: String;
  LoadResult: Boolean;

function TSXInstallDir(Param: String) : String;
begin
  LoadResult := LoadStringFromFile(ExpandConstant('{userdocs}') + '\Software Bisque\TheSkyX Professional Edition\TheSkyXInstallPath.txt', Location);
  { Check that could open the file}
  if not LoadResult then
    RaiseException('Unable to find the installation path for The Sky X');
  {Check that the file exists}
  if not DirExists(Location) then
    RaiseException('The SkyX installation directory ' + Location + ' does not exist');
  Result := Location;
end;
