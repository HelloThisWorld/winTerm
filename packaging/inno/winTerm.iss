; Copyright (c) winTerm contributors.
; Licensed under the MIT license.

#ifndef StageDir
  #error StageDir must point to a verified unpackaged winTerm stage.
#endif
#ifndef AppVersion
  #error AppVersion must be supplied by build-installer.ps1.
#endif
#ifndef OutputDir
  #error OutputDir must be supplied by build-installer.ps1.
#endif
#ifndef NumericVersion
  #error NumericVersion must be supplied by build-installer.ps1.
#endif
#ifndef PlatformLabel
  #define PlatformLabel "x64"
#endif

[Setup]
AppId={{0B9D8F09-5F04-4E9C-879F-14B65E3A8917}
AppName=winTerm
AppVersion={#AppVersion}
AppVerName=winTerm {#AppVersion}
AppPublisher=helloThisWorld
AppPublisherURL=https://github.com/HelloThisWorld
AppSupportURL=https://github.com/HelloThisWorld/winTerm/issues
AppUpdatesURL=https://github.com/HelloThisWorld/winTerm/releases
DefaultDirName={autopf}\winTerm
DefaultGroupName=winTerm
UninstallDisplayName=winTerm
UninstallDisplayIcon={app}\winTerm.exe
OutputDir={#OutputDir}
OutputBaseFilename=winTerm-{#AppVersion}-setup-{#PlatformLabel}
SetupIconFile=..\..\assets\winterm\icons\winterm.ico
Compression=lzma2/max
SolidCompression=yes
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=dialog commandline
#if PlatformLabel == "arm64"
ArchitecturesAllowed=arm64
ArchitecturesInstallIn64BitMode=arm64
#else
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
#endif
; Windows 11 x64 remains the supported user target. Windows Server 2022 build
; 20348 is admitted so GitHub's hosted Windows runner can execute the exact
; installer acceptance tests. Windows 10 builds remain below this boundary.
MinVersion=10.0.20348
CloseApplications=yes
RestartApplications=no
UsePreviousAppDir=yes
UsePreviousGroup=yes
VersionInfoCompany=helloThisWorld
VersionInfoProductName=winTerm
VersionInfoDescription=winTerm Setup
VersionInfoVersion={#NumericVersion}
WizardStyle=modern
DisableProgramGroupPage=yes
LicenseFile=..\..\LICENSE

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "startmenu"; Description: "Create a Start Menu shortcut"; Flags: checkedonce
Name: "desktopicon"; Description: "Create a desktop shortcut"; Flags: unchecked
Name: "command"; Description: "Register the winterm command"; Flags: checkedonce
Name: "contextmenu"; Description: "Add Open winTerm here to File Explorer"; Flags: unchecked

[Files]
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\winTerm"; Filename: "{app}\winTerm.exe"; Tasks: startmenu
Name: "{autodesktop}\winTerm"; Filename: "{app}\winTerm.exe"; Tasks: desktopicon

[Registry]
Root: HKA; Subkey: "Software\Microsoft\Windows\CurrentVersion\App Paths\winterm.exe"; ValueType: string; ValueName: ""; ValueData: "{app}\winterm.exe"; Flags: uninsdeletekey; Tasks: command
Root: HKA; Subkey: "Software\Classes\Directory\shell\winTerm"; ValueType: string; ValueName: ""; ValueData: "Open winTerm here"; Flags: uninsdeletekey; Tasks: contextmenu
Root: HKA; Subkey: "Software\Classes\Directory\shell\winTerm"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\winTerm.exe"; Tasks: contextmenu
Root: HKA; Subkey: "Software\Classes\Directory\shell\winTerm\command"; ValueType: string; ValueName: ""; ValueData: """{app}\winTerm.exe"" -d ""%V"""; Tasks: contextmenu
Root: HKA; Subkey: "Software\Classes\Directory\Background\shell\winTerm"; ValueType: string; ValueName: ""; ValueData: "Open winTerm here"; Flags: uninsdeletekey; Tasks: contextmenu
Root: HKA; Subkey: "Software\Classes\Directory\Background\shell\winTerm"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\winTerm.exe"; Tasks: contextmenu
Root: HKA; Subkey: "Software\Classes\Directory\Background\shell\winTerm\command"; ValueType: string; ValueName: ""; ValueData: """{app}\winTerm.exe"" -d ""%V"""; Tasks: contextmenu

[Run]
Filename: "{app}\winTerm.exe"; Description: "Launch winTerm"; Flags: nowait postinstall skipifsilent

[Code]
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  DataPath: string;
begin
  if CurUninstallStep <> usUninstall then
    exit;
  if UninstallSilent then
    exit;

  DataPath := ExpandConstant('{localappdata}\winTerm');
  if not DirExists(DataPath) then
    exit;

  if MsgBox('Keep winTerm settings, themes, workspaces, snapshots, logs, and updates?',
    mbConfirmation, MB_YESNO or MB_DEFBUTTON1) = IDNO then
  begin
    if MsgBox('Permanently delete all winTerm user data? This cannot be undone.',
      mbConfirmation, MB_YESNO or MB_DEFBUTTON2) = IDYES then
    begin
      DelTree(DataPath, True, True, True);
    end;
  end;
end;
