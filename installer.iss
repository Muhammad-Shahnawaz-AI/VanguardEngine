[Setup]
AppName=VanguardEngine
AppVersion=1.0.0
AppPublisher=Muhammad-Shahnawaz-AI
DefaultDirName={autopf}\VanguardEngine
DefaultGroupName=VanguardEngine
OutputDir=.
OutputBaseFilename=VanguardEngine-Setup-x64
Compression=lzma2/ultra64
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\VanguardGame.exe
WizardStyle=modern

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "dist\VanguardEngine\VanguardGame.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "dist\VanguardEngine\assets\*"; DestDir: "{app}\assets"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\VanguardEngine"; Filename: "{app}\VanguardGame.exe"; WorkingDir: "{app}"
Name: "{group}\Uninstall VanguardEngine"; Filename: "{uninstallexe}"
Name: "{autodesktop}\VanguardEngine"; Filename: "{app}\VanguardGame.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\VanguardGame.exe"; WorkingDir: "{app}"; Description: "{cm:LaunchProgram,VanguardEngine}"; Flags: nowait postinstall skipifsilent
