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
#define MyAppVersion   "0.2780.0.3"
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
DefaultDirName={commoncf}\VST3
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
Source: "{#VST3Source}\Contents\Resources\moduleinfo.json"; DestDir: "{commoncf}\VST3\Dope Maker.vst3\Contents\Resources"; Components: plugin; Flags: ignoreversion
Source: "{#VST3Source}\Contents\x86_64-win\Dope Maker.vst3"; DestDir: "{commoncf}\VST3\Dope Maker.vst3\Contents\x86_64-win"; Components: plugin; Flags: ignoreversion

; Factory samples — into the default preset bank's Samples folder
; onlyifdoesntexist is toggled off at runtime when user chooses to overwrite
Source: "{#SamplesSource}\*.wav"; DestDir: "{userappdata}\DopeMakerVST\Presets\Fvcktory Bank\Samples"; Components: samples; Flags: ignoreversion; Check: ShouldInstallSample

[Dirs]
; Ensure the preset structure exists even if no samples are installed
Name: "{userappdata}\DopeMakerVST\Presets\Fvcktory Bank\Samples"

[Icons]
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"

[UninstallDelete]
; Clean up the VST3 bundle folder on uninstall
Type: filesandordirs; Name: "{commoncf}\VST3\Dope Maker.vst3"

[Messages]
WelcomeLabel2=This will install [name/ver] on your computer.%n%nThe VST3 plugin will be placed in:%n  C:\Program Files\Common Files\VST3\Dope Maker.vst3\%n%nFactory samples will be placed in:%n  %APPDATA%\DopeMakerVST\Presets\Fvcktory Bank\Samples\

[InstallDelete]
; Remove the old VST3 bundle before installing to prevent stale leftover files
Type: filesandordirs; Name: "{commoncf}\VST3\Dope Maker.vst3"

[Code]
var
  OverwriteSamples: Boolean;

const
  GENERIC_WRITE = $40000000;
  OPEN_EXISTING = 3;
  FILE_ATTR_NORMAL = $80;
  INVALID_HANDLE_VALUE_ = $FFFFFFFF;

function CreateFileA(
  lpFileName: AnsiString;
  dwDesiredAccess, dwShareMode: Cardinal;
  lpSecurityAttributes: Cardinal;
  dwCreationDisposition, dwFlagsAndAttributes: Cardinal;
  hTemplateFile: Cardinal): Cardinal;
  external 'CreateFileA@kernel32.dll stdcall';

function CloseHandle(hObject: Cardinal): Boolean;
  external 'CloseHandle@kernel32.dll stdcall';

function IsPluginFileLocked(): Boolean;
var
  PluginPath: String;
  FileHandle: Cardinal;
begin
  Result := False;
  PluginPath := ExpandConstant('{commoncf}\VST3\Dope Maker.vst3\Contents\x86_64-win\Dope Maker.vst3');
  if FileExists(PluginPath) then
  begin
    FileHandle := CreateFileA(PluginPath, GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTR_NORMAL, 0);
    if FileHandle = INVALID_HANDLE_VALUE_ then
      Result := True
    else
      CloseHandle(FileHandle);
  end;
end;

function ShouldInstallSample(): Boolean;
var
  DestPath: String;
begin
  if OverwriteSamples then
    Result := True
  else
  begin
    DestPath := ExpandConstant('{userappdata}\DopeMakerVST\Presets\Fvcktory Bank\Samples\') + ExtractFileName(CurrentFileName);
    Result := not FileExists(DestPath);
  end;
end;

function InitializeSetup(): Boolean;
var
  UninstallKey: String;
  UninstallCmd: String;
  InstalledVersion: String;
  PluginPath: String;
  ResultCode: Integer;
  FoundInRegistry: Boolean;
begin
  Result := True;
  UninstallKey := 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{E7A3F2D1-4B8C-4D2E-9F1A-6C5B3D7E8F90}_is1';
  PluginPath := ExpandConstant('{commoncf}\VST3\Dope Maker.vst3\Contents\x86_64-win\Dope Maker.vst3');

  // Check if the plugin is currently loaded in a DAW
  if IsPluginFileLocked() then
  begin
    MsgBox(
      'Dope Maker is currently in use by another application (likely a DAW).' + #13#10 + #13#10 +
      'Please close your DAW or remove the plugin from your session before installing.',
      mbError, MB_OK);
    Result := False;
    Exit;
  end;

  // Check registry for a previous Inno Setup install
  FoundInRegistry := RegQueryStringValue(HKLM, UninstallKey, 'UninstallString', UninstallCmd);

  if FoundInRegistry then
  begin
    // Check if the installed version is the same
    if RegQueryStringValue(HKLM, UninstallKey, 'DisplayVersion', InstalledVersion) then
    begin
      if InstalledVersion = '{#MyAppVersion}' then
      begin
        if MsgBox(
          'Dope Maker v{#MyAppVersion} is already installed.' + #13#10 + #13#10 +
          'Do you want to reinstall the same version?',
          mbConfirmation, MB_YESNO) = IDNO then
        begin
          Result := False;
          Exit;
        end;
      end;
    end;

    if MsgBox(
      'A previous version of Dope Maker (v' + InstalledVersion + ') was detected.' + #13#10 + #13#10 +
      'It will be uninstalled before continuing. Proceed?',
      mbConfirmation, MB_YESNO) = IDYES then
    begin
      Exec(RemoveQuotes(UninstallCmd), '/SILENT /NORESTART', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
    end
    else
    begin
      Result := False;
    end;
  end
  else if FileExists(PluginPath) then
  begin
    // Plugin exists on disk but was not installed via this installer (manual copy)
    if MsgBox(
      'An existing Dope Maker plugin was found at:' + #13#10 +
      '  ' + PluginPath + #13#10 + #13#10 +
      'It will be replaced with v{#MyAppVersion}. Proceed?',
      mbConfirmation, MB_YESNO) = IDNO then
    begin
      Result := False;
      Exit;
    end;
  end;

  // Check if existing presets or samples are present
  OverwriteSamples := False;
  if DirExists(ExpandConstant('{userappdata}\DopeMakerVST\Presets\Fvcktory Bank')) then
  begin
    if MsgBox(
      'Existing presets and samples were found in the Fvcktory Bank.' + #13#10 + #13#10 +
      'Do you want to overwrite them with the factory defaults?' + #13#10 + #13#10 +
      'Yes = overwrite existing files' + #13#10 +
      'No = keep your current files, only add missing ones',
      mbConfirmation, MB_YESNO) = IDYES then
    begin
      OverwriteSamples := True;
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
      '  ' + ExpandConstant('{commoncf}\VST3\Dope Maker.vst3') + #13#10 + #13#10 +
      'Rescan your VST3 plugins in your DAW to load Dope Maker.',
      mbInformation, MB_OK);
  end;
end;
