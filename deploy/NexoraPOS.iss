; Build after running windeployqt against the Release executable.
#define AppName "Nexora POS"
#define AppVersion "0.1.0"
#define AppPublisher "Nexora"
#define AppExeName "wholesale_pos.exe"

[Setup]
AppId={{E4E735C7-3AB2-4610-A33D-6BC1A6A43F4E}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\Nexora POS
DefaultGroupName={#AppName}
OutputDir=output
OutputBaseFilename=NexoraPOS-Setup
Compression=lzma
SolidCompression=yes
WizardStyle=modern

[Files]
Source: "..\deploy\*"; DestDir: "{app}"; Flags: recursesubdirs ignoreversion

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#AppExeName}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; Flags: unchecked

[Run]
Filename: "{app}\{#AppExeName}"; Description: "Launch {#AppName}"; Flags: nowait postinstall skipifsilent
