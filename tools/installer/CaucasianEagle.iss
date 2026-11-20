; Caucasian Eagle installer (Inno Setup 6).
; Compiled by tools\package.ps1, which passes AppVersion, SourceDir and OutputDir.
; Per-user install: no administrator prompt. Settings and saves live in
; %LOCALAPPDATA%\CaucasianEagle and are left in place on uninstall.

#ifndef AppVersion
  #define AppVersion "0.5.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\..\dist\CaucasianEagle-" + AppVersion + "-win64"
#endif
#ifndef OutputDir
  #define OutputDir "..\..\dist"
#endif

[Setup]
AppId={{7E3C1B52-9A4D-4F6E-8B21-CA0CA51A0E61}
AppName=Caucasian Eagle
AppVersion={#AppVersion}
AppPublisher=Giorgi Samkharadze
DefaultDirName={localappdata}\Programs\Caucasian Eagle
DefaultGroupName=Caucasian Eagle
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#OutputDir}
OutputBaseFilename=CaucasianEagle-{#AppVersion}-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName=Caucasian Eagle
UninstallDisplayIcon={app}\CaucasianEagle.exe
SetupIconFile=..\..\game\CaucasianEagle.ico
AppPublisherURL=https://github.com/kalk1t/Caucasian-Eagle
VersionInfoVersion={#AppVersion}.0

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
; 0.5: CaucasianEagle.exe starts the Eagle's Ascent (the owner, 2026-11-20); the road of 0.4 keeps a shortcut (--road)
Name: "{group}\Caucasian Eagle"; Filename: "{app}\CaucasianEagle.exe"; WorkingDir: "{app}"
Name: "{group}\Caucasian Eagle - The Road (0.4)"; Filename: "{app}\CaucasianEagle.exe"; Parameters: "--road"; WorkingDir: "{app}"
Name: "{autodesktop}\Caucasian Eagle"; Filename: "{app}\CaucasianEagle.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\CaucasianEagle.exe"; WorkingDir: "{app}"; Description: "{cm:LaunchProgram,Caucasian Eagle}"; Flags: nowait postinstall skipifsilent
