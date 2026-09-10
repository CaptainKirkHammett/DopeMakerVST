; Dope Maker VST3 Installer — Inno Setup Script
; Requires Inno Setup 6+ (https://jrsoftware.org/isinfo.php)
;
; Build the installer:
;   1. Install Inno Setup 6
;   2. Open this file in Inno Setup Compiler
;   3. Press Ctrl+F9 to compile
;
; The installer copies:
;   - Dope Maker.vst3 bundle  -> C:\Program Files\Common Files\VST3\
;   - Factory sample WAVs      -> %APPDATA%\DopeMakerVST\Presets\Fvcktory Bank\Samples\

#define MyAppName      "Dope Maker"
#define MyAppVersion   "0.2780.0.2"
#define MyAppPublisher "Dope Science"
#define MyAppURL       ""

; Paths relative to this .iss file
#define ProjectRoot    ".."
#define VST3Source     ProjectRoot + "\output\Dope Maker.vst3"
#define SamplesSource  ProjectRoot + "\samplesounds"

[Setup]
AppId={{E7A3F2D1-4B8C-4D2E-9F1A-6C5B3D7E8F90}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={commonpf}\VST3
DirExistsWarning=no
DisableProgramGroupPage=yes
OutputDir={#ProjectRoot}\installer\output
OutputBaseFilename=DopeMaker_v{#MyAppVersion}_Setup
Compression=lzma2/ultra64
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayName={#MyAppName}
PrivilegesRequired=admin
SetupIconFile=
WizardStyle=modern
DefaultGroupName={#MyAppName}
DisableDirPage=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Types]
Name: "full";    Description: "Full installation (plugin + factory samples)"
Name: "compact"; Description: "Plugin only (no factory samples)"
Name: "custom";  Description: "Custom installation"; Flags: iscustom

[Components]
Name: "plugin";  Description: "Dope Maker VST3 Plugin";  Types: full compact custom; Flags: fixed
Name: "samples"; Description: "Factory Sample Sounds (~42 MB)"; Types: full

[Files]
; VST3 bundle — preserves the folder structure
Source: "{#VST3Source}\Contents\Resources\moduleinfo.json"; DestDir: "{commonpf}\VST3\Dope Maker.vst3\Contents\Resources"; Components: plugin; Flags: ignoreversion
Source: "{#VST3Source}\Contents\x86_64-win\Dope Maker.vst3"; DestDir: "{commonpf}\VST3\Dope Maker.vst3\Contents\x86_64-win"; Components: plugin; Flags: ignoreversion

; Factory samples — into the default preset bank's Samples folder
Source: "{#SamplesSource}\*.wav"; DestDir: "{userappdata}\DopeMakerVST\Presets\Fvcktory Bank\Samples"; Components: samples; Flags: ignoreversion onlyifdoesntexist

[Dirs]
; Ensure the preset structure exists even if no samples are installed
Name: "{userappdata}\DopeMakerVST\Presets\Fvcktory Bank\Samples"

[Icons]
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"

[UninstallDelete]
; Clean up the VST3 bundle folder on uninstall
Type: filesandordirs; Name: "{commonpf}\VST3\Dope Maker.vst3"

[Messages]
WelcomeLabel2=This will install [name/ver] on your computer.%n%nThe VST3 plugin will be placed in:%n  C:\Program Files\Common Files\VST3\Dope Maker.vst3\%n%nFactory samples will be placed in:%n  %APPDATA%\DopeMakerVST\Presets\Fvcktory Bank\Samples\

[InstallDelete]
; Remove the old VST3 bundle before installing to prevent stale leftover files
Type: filesandordirs; Name: "{commonpf}\VST3\Dope Maker.vst3"

[Code]
function InitializeSetup(): Boolean;
var
  UninstallKey: String;
  UninstallCmd: String;
  ResultCode: Integer;
begin
  Result := True;
  UninstallKey := 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{#SetupSetting("AppId")}_is1';

  // Check if a previous version is installed
  if RegQueryStringValue(HKLM, UninstallKey, 'UninstallString', UninstallCmd) then
  begin
    if MsgBox(
      'A previous version of Dope Maker was detected.' + #13#10 + #13#10 +
      'It will be uninstalled before continuing. Proceed?',
      mbConfirmation, MB_YESNO) = IDYES then
    begin
      // Run the old uninstaller silently
      Exec(RemoveQuotes(UninstallCmd), '/SILENT /NORESTART', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
    end
    else
    begin
      Result := False;
    end;
  end;
end;

// Show a post-install message with installed paths
procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssDone then
  begin
    MsgBox(
      'Dope Maker has been installed successfully!' + #13#10 + #13#10 +
      'VST3 plugin location:' + #13#10 +
      '  ' + ExpandConstant('{commonpf}\VST3\Dope Maker.vst3') + #13#10 + #13#10 +
      'Rescan your VST3 plugins in your DAW to load Dope Maker.',
      mbInformation, MB_OK);
  end;
end;
