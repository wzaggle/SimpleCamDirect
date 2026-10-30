#define PluginName "SimpleCamPTZ OBS Plugin"
#define PluginVersion "0.1.8"
#define PluginId "simplecamptz"
#define PreviousPluginId "simplecamdirect"

[Setup]
AppId={{6F8B8E7D-73EC-4B4D-8CF9-95F1C3A267A4}
AppName={#PluginName}
AppVersion={#PluginVersion}
AppPublisher=SimpleCamPTZ Project
DefaultDirName={commonappdata}\obs-studio\plugins\{#PluginId}
DisableDirPage=yes
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
CloseApplications=yes
RestartApplications=no
OutputDir=dist
OutputBaseFilename=SimpleCamPTZ-Setup-{#PluginVersion}-x64
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName={#PluginName}
UninstallFilesDir={app}\uninstall

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[InstallDelete]
; Remove the prior package to avoid loading both modules, which register the same OBS source type.
Type: filesandordirs; Name: "{commonappdata}\obs-studio\plugins\{#PreviousPluginId}"
Type: filesandordirs; Name: "{userappdata}\obs-studio\plugins\{#PreviousPluginId}"

[Files]
Source: "stage\{#PluginId}\*"; DestDir: "{app}"; Excludes: "*.pdb"; Flags: ignoreversion recursesubdirs createallsubdirs
