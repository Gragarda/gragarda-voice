; Installeur Gragarda Voice (Inno Setup 6). Build : ISCC installer\gragarda-voice.iss  (apres cmake --build)
; Installe dans le dossier d'OBS trouve dans le registre (HKLM\SOFTWARE\OBS Studio), modifiable par l'utilisateur.

#define AppVersion "0.5.0"
#ifndef Config
  #define Config "RelWithDebInfo"
#endif
#define Build "..\build_x64\" + Config
#define Model "..\..\DeepFilterNet\models\DeepFilterNet3_onnx.tar.gz"

[Setup]
AppId={{6B2C7E0A-9F1D-4C55-8E1B-6A0D2F4C9A17}
AppName=Gragarda Voice
AppVersion={#AppVersion}
AppPublisher=Gragarda
AppPublisherURL=https://github.com/Gragarda/gragarda-voice
LicenseFile=..\LICENSE
DefaultDirName={code:ObsDir}
DirExistsWarning=no
DisableProgramGroupPage=yes
OutputDir=..\release
OutputBaseFilename=gragarda-voice-{#AppVersion}-setup
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
Compression=lzma2
SolidCompression=yes
UninstallFilesDir={app}\data\obs-plugins\gragarda-voice\uninstall

[Languages]
; anglais par defaut, francais si Windows est en francais
Name: "en"; MessagesFile: "compiler:Default.isl"
Name: "fr"; MessagesFile: "compiler:Languages\French.isl"

[CustomMessages]
en.ObsRunning=OBS is running: close it, then run the installer again.
fr.ObsRunning=OBS est ouvert : ferme-le puis relance l'installation.
en.ObsMissing=OBS was not found in this folder (bin\64bit\obs64.exe). Continue anyway?
fr.ObsMissing=OBS introuvable dans ce dossier (bin\64bit\obs64.exe). Continuer quand même ?

[Files]
Source: "{#Build}\gragarda-voice.dll"; DestDir: "{app}\obs-plugins\64bit"; Flags: ignoreversion
Source: "..\data\locale\*"; DestDir: "{app}\data\obs-plugins\gragarda-voice\locale"; Flags: ignoreversion
Source: "..\licenses\*"; DestDir: "{app}\data\obs-plugins\gragarda-voice\licenses"; Flags: ignoreversion
Source: "..\LICENSE"; DestDir: "{app}\data\obs-plugins\gragarda-voice\licenses"; DestName: "gragarda-voice-GPL-2.0.txt"; Flags: ignoreversion
Source: "{#Model}"; DestDir: "{app}\data\obs-plugins\gragarda-voice\models"; Flags: ignoreversion

[Code]
function ObsDir(Param: String): String;
begin
  if not RegQueryStringValue(HKLM, 'SOFTWARE\OBS Studio', '', Result) then
    Result := ExpandConstant('{commonpf64}\obs-studio');
end;

// OBS ouvert = DLL verrouillee : on previent avant de copier
function PrepareToInstall(var NeedsRestart: Boolean): String;
var Code: Integer;
begin
  Result := '';
  if Exec('cmd.exe', '/c tasklist /FI "IMAGENAME eq obs64.exe" | find /I "obs64.exe"', '', SW_HIDE,
          ewWaitUntilTerminated, Code) and (Code = 0) then
    Result := CustomMessage('ObsRunning');
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if (CurPageID = wpSelectDir) and not FileExists(AddBackslash(WizardDirValue) + 'bin\64bit\obs64.exe') then
    Result := MsgBox(CustomMessage('ObsMissing'), mbConfirmation, MB_YESNO) = IDYES;
end;
