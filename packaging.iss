; SpeedyNote Inno Setup Script
[Setup]
AppName=SpeedyNote
AppVersion=0.6.0
DefaultDirName={autopf}\SpeedyNote
DefaultGroupName=SpeedyNote
OutputBaseFilename=SpeedyNoteInstaller
Compression=lzma
SolidCompression=yes
OutputDir=Output
DisableProgramGroupPage=yes
WizardStyle=modern

[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"
Name: "es"; MessagesFile: "compiler:Languages\Spanish.isl"
Name: "zh"; MessagesFile: "compiler:Languages\ChineseSimplified.isl"
Name: "fr"; MessagesFile: "compiler:Languages\French.isl"

[Files]
Source: "C:\Games\notecpp_2rowlowres_legacy\build\*"; DestDir: "{app}"; Flags: recursesubdirs ignoreversion

[Icons]
Name: "{group}\SpeedyNote"; Filename: "{app}\NoteApp.exe"; WorkingDir: "{app}"
Name: "{commondesktop}\SpeedyNote"; Filename: "{app}\NoteApp.exe"; WorkingDir: "{app}"; IconFilename: "{app}\NoteApp.exe"; Tasks: desktopicon
Name: "{group}\Uninstall SpeedyNote"; Filename: "{uninstallexe}"

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional icons:"

[Run]
Filename: "{app}\NoteApp.exe"; Description: "{cm:LaunchProgram,SpeedyNote}"; Flags: nowait postinstall skipifsilent
