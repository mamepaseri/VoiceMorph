#ifndef AppVersion
  #define AppVersion "0.8.0"
#endif

#define AppName "VoiceMorph"

[Setup]
AppId={{A2EE319A-9DF1-49F0-9EE1-5E3A8C770801}}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=VoiceMorph
DefaultDirName={code:GetVst3Dir}
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog commandline
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
UninstallDisplayName=VoiceMorph VST3
OutputDir=..\dist
OutputBaseFilename=VoiceMorph-Setup-x64-{#AppVersion}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
RestartApplications=no

[Files]
Source: "payload\VoiceMorph.vst3\*"; DestDir: "{code:GetVst3Dir}\VoiceMorph.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs

[Code]
function GetVst3Dir(Param: String): String;
begin
  if IsAdminInstallMode then
    Result := ExpandConstant('{commoncf}\VST3')
  else
    Result := ExpandConstant('{localappdata}\Programs\Common\VST3');
end;
