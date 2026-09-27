; Inno Setup script for Invento.
; Compiled by tools/build-installer.ps1, which passes AppVersion and DistDir.
; The installer contains PROGRAM FILES ONLY — never a database, backups, receipts or settings.

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef DistDir
  #define DistDir "..\dist"
#endif
#define AppName "Invento"
#define AppExe "invento.exe"
#define AppPublisher "Invento"

[Setup]
; A fixed AppId ties upgrades together — a newer Setup replaces the older install in place.
AppId={{F6039825-7E23-47DA-80C2-06AB8CD25D40}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\Invento
DefaultGroupName=Invento
DisableProgramGroupPage=yes
OutputBaseFilename=Invento_Setup_{#AppVersion}
OutputDir=dist-installer
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
; Needs admin to write Program Files and to create the shared ProgramData data folder.
PrivilegesRequired=admin
ArchitecturesInstallIn64BitMode=x64compatible
SetupIconFile=..\src\ui\assets\app_icon.ico
UninstallDisplayName={#AppName}
UninstallDisplayIcon={app}\{#AppExe}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Additional shortcuts:"

[Dirs]
; Shared data folder, writable by every Windows user on this PC (a POS must share data).
Name: "{commonappdata}\Invento"; Permissions: users-modify

[Files]
; The whole verified dist folder (exe + Qt DLLs/plugins + MinGW runtime). No data files.
Source: "{#DistDir}\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion

[Icons]
Name: "{group}\Invento"; Filename: "{app}\{#AppExe}"
Name: "{group}\Uninstall Invento"; Filename: "{uninstallexe}"
Name: "{autodesktop}\Invento"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExe}"; Description: "Launch Invento"; Flags: nowait postinstall skipifsilent

[Messages]
; Shown on the final page as a reminder.
FinishedLabel=Setup has installed Invento. Your business data is kept separately in %PROGRAMDATA%\Invento and is never removed by uninstalling.

[Code]
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usPostUninstall then
    MsgBox('Invento has been removed.' + #13#10 + #13#10 +
           'Your business data (database, backups, settings) was NOT deleted. It is kept at:' + #13#10 +
           ExpandConstant('{commonappdata}\Invento') + #13#10 + #13#10 +
           'Delete that folder manually only if you are sure you no longer need your data.',
           mbInformation, MB_OK);
end;
