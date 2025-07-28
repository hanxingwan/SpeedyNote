; SpeedyNote Inno Setup Script
[Setup]
AppName=SpeedyNote
AppVersion=0.7.0
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
Source: ".\build\*"; DestDir: "{app}"; Flags: recursesubdirs ignoreversion

[Icons]
Name: "{group}\SpeedyNote"; Filename: "{app}\NoteApp.exe"; WorkingDir: "{app}"
Name: "{commondesktop}\SpeedyNote"; Filename: "{app}\NoteApp.exe"; WorkingDir: "{app}"; IconFilename: "{app}\NoteApp.exe"; Tasks: desktopicon
Name: "{group}\Uninstall SpeedyNote"; Filename: "{uninstallexe}"

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional icons:"
Name: "pdfassociation"; Description: "Associate PDF files with SpeedyNote (adds SpeedyNote to 'Open with' menu)"; GroupDescription: "File associations:"
Name: "spnassociation"; Description: "Associate .spn files with SpeedyNote (SpeedyNote Package files)"; GroupDescription: "File associations:"

[Registry]
; PDF file association entries (only if task is selected)
Root: HKCR; Subkey: "Applications\NoteApp.exe"; ValueType: string; ValueName: "FriendlyAppName"; ValueData: "SpeedyNote"; Tasks: pdfassociation
Root: HKCR; Subkey: "Applications\NoteApp.exe\DefaultIcon"; ValueType: string; ValueData: "{app}\NoteApp.exe,0"; Tasks: pdfassociation
Root: HKCR; Subkey: "Applications\NoteApp.exe\shell\open"; ValueType: string; ValueName: "FriendlyName"; ValueData: "Open with SpeedyNote"; Tasks: pdfassociation
Root: HKCR; Subkey: "Applications\NoteApp.exe\shell\open\command"; ValueType: string; ValueData: """{app}\NoteApp.exe"" ""%1"""; Tasks: pdfassociation
Root: HKCR; Subkey: ".pdf\OpenWithList\NoteApp.exe"; Tasks: pdfassociation
Root: HKCR; Subkey: ".pdf\OpenWithProgids"; ValueType: string; ValueName: "SpeedyNote.PDF"; ValueData: ""; Tasks: pdfassociation
Root: HKCR; Subkey: "SpeedyNote.PDF"; ValueType: string; ValueData: "PDF Document (SpeedyNote)"; Tasks: pdfassociation
Root: HKCR; Subkey: "SpeedyNote.PDF"; ValueType: string; ValueName: "FriendlyTypeName"; ValueData: "PDF Document for SpeedyNote"; Tasks: pdfassociation
Root: HKCR; Subkey: "SpeedyNote.PDF\DefaultIcon"; ValueType: string; ValueData: "{app}\NoteApp.exe,0"; Tasks: pdfassociation
Root: HKCR; Subkey: "SpeedyNote.PDF\shell\open"; ValueType: string; ValueName: "FriendlyName"; ValueData: "Open with SpeedyNote"; Tasks: pdfassociation
Root: HKCR; Subkey: "SpeedyNote.PDF\shell\open\command"; ValueType: string; ValueData: """{app}\NoteApp.exe"" ""%1"""; Tasks: pdfassociation
Root: HKLM; Subkey: "SOFTWARE\RegisteredApplications"; ValueType: string; ValueName: "SpeedyNote"; ValueData: "SOFTWARE\SpeedyNote\Capabilities"; Tasks: pdfassociation
Root: HKLM; Subkey: "SOFTWARE\SpeedyNote\Capabilities"; ValueType: string; ValueName: "ApplicationName"; ValueData: "SpeedyNote"; Tasks: pdfassociation
Root: HKLM; Subkey: "SOFTWARE\SpeedyNote\Capabilities"; ValueType: string; ValueName: "ApplicationDescription"; ValueData: "Fast Digital Note Taking Application with PDF Support"; Tasks: pdfassociation
Root: HKLM; Subkey: "SOFTWARE\SpeedyNote\Capabilities\FileAssociations"; ValueType: string; ValueName: ".pdf"; ValueData: "SpeedyNote.PDF"; Tasks: pdfassociation

; ✅ SPN file association entries (only if task is selected)
Root: HKCR; Subkey: ".spn"; ValueType: string; ValueData: "SpeedyNote.SPN"; Tasks: spnassociation
Root: HKCR; Subkey: "SpeedyNote.SPN"; ValueType: string; ValueData: "SpeedyNote Package"; Tasks: spnassociation
Root: HKCR; Subkey: "SpeedyNote.SPN"; ValueType: string; ValueName: "FriendlyTypeName"; ValueData: "SpeedyNote Package File"; Tasks: spnassociation
Root: HKCR; Subkey: "SpeedyNote.SPN\DefaultIcon"; ValueType: string; ValueData: "{app}\NoteApp.exe,0"; Tasks: spnassociation
Root: HKCR; Subkey: "SpeedyNote.SPN\shell\open"; ValueType: string; ValueName: "FriendlyName"; ValueData: "Open with SpeedyNote"; Tasks: spnassociation
Root: HKCR; Subkey: "SpeedyNote.SPN\shell\open\command"; ValueType: string; ValueData: """{app}\NoteApp.exe"" ""%1"""; Tasks: spnassociation
Root: HKLM; Subkey: "SOFTWARE\SpeedyNote\Capabilities\FileAssociations"; ValueType: string; ValueName: ".spn"; ValueData: "SpeedyNote.SPN"; Tasks: spnassociation

[Run]
Filename: "{app}\NoteApp.exe"; Description: "{cm:LaunchProgram,SpeedyNote}"; Flags: nowait postinstall skipifsilent
