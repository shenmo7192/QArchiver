; QArchiver (小Q解压) Windows installer (Inno Setup 6.3+).
; 小Q解压 Windows 安装器（Inno Setup 6.3+）。
;
; Build from the staged portable package:
; 从已部署好的便携包目录构建：
;
;   ISCC.exe QArchiver.iss /DAppVersion=2.2.0 ^
;       /DSourceDir=C:\path\to\staged ^
;       /DOutputDir=C:\path\to\dist /DArch=x64
;
; SourceDir must contain QArchiver.exe, 7zip\ and the deployed Qt runtime.
; SourceDir 中必须包含 QArchiver.exe、7zip\ 与部署后的 Qt 运行库。
;
; The Explorer menu written here mirrors install-shell-menu.ps1.
; Keep both definitions in sync when adding a format.
; 此处写入的资源管理器菜单与 install-shell-menu.ps1 一致；
; 新增格式时请同步修改两处。

#ifndef AppVersion
  #define AppVersion "2.2.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\..\build-windows\package"
#endif
#ifndef OutputDir
  #define OutputDir "..\..\dist"
#endif
#ifndef Arch
  #define Arch "x64"
#endif
#ifndef ArchAllowed
  #define ArchAllowed "x64compatible"
#endif

#define AppExeName "QArchiver.exe"
#define AppDisplayName "QArchiver (小Q解压)"
#define AppPublisher "QArchiver"
#define AppUrl "https://github.com/shenmo7192/QArchiver"

[Setup]
AppId={{7D2C4A63-8B1E-4C9F-9A57-2E6B0D4F1C38}
AppName={#AppDisplayName}
AppVersion={#AppVersion}
AppVerName={#AppDisplayName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppUrl}
AppSupportURL={#AppUrl}
AppUpdatesURL={#AppUrl}
VersionInfoVersion={#AppVersion}
VersionInfoDescription={#AppDisplayName} installer
DefaultDirName={autopf}\QArchiver
DefaultGroupName=QArchiver
DisableProgramGroupPage=yes
PrivilegesRequired=admin
OutputDir={#OutputDir}
OutputBaseFilename=QArchiver-{#AppVersion}-windows-{#Arch}-setup
SetupIconFile=..\..\resources\app-icons\qarchiver.ico
UninstallDisplayIcon={app}\{#AppExeName}
UninstallDisplayName={#AppDisplayName}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
RestartApplications=no
ArchitecturesAllowed={#ArchAllowed}
ArchitecturesInstallIn64BitMode={#ArchAllowed}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "shellmenu"; Description: "添加资源管理器右键菜单（打开 / 解压 / 添加到 ZIP / 压缩到…）"; GroupDescription: "集成选项："

[Files]
; The whole portable payload: exe, 7zip\, Qt runtime and plugins, helper scripts.
; 完整的便携包内容：exe、7zip\、Qt 运行库与插件、辅助脚本。
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\QArchiver"; Filename: "{app}\{#AppExeName}"
Name: "{group}\卸载 QArchiver"; Filename: "{uninstallexe}"
Name: "{autodesktop}\QArchiver"; Filename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExeName}"; Description: "{cm:LaunchProgram,QArchiver}"; Flags: nowait postinstall skipifsilent

[Code]
function ArchiveExtensions: TArrayOfString;
begin
  SetArrayLength(Result, 13);
  Result[0] := '.zip';
  Result[1] := '.7z';
  Result[2] := '.rar';
  Result[3] := '.tar';
  Result[4] := '.gz';
  Result[5] := '.bz2';
  Result[6] := '.xz';
  Result[7] := '.tgz';
  Result[8] := '.txz';
  Result[9] := '.tbz2';
  Result[10] := '.tar.gz';
  Result[11] := '.tar.xz';
  Result[12] := '.tar.bz2';
end;

procedure RegisterExtractVerb(BaseKey, Verb, Caption, Command: String);
begin
  RegWriteStringValue(HKLM, BaseKey + '\shell\' + Verb, '', Caption);
  RegWriteStringValue(HKLM, BaseKey + '\shell\' + Verb, 'MUIVerb', Caption);
  RegWriteStringValue(HKLM, BaseKey + '\shell\' + Verb, 'Icon',
                      ExpandConstant('{app}\{#AppExeName}') + ',0');
  RegWriteStringValue(HKLM, BaseKey + '\shell\' + Verb + '\command', '', Command);
end;

procedure RegisterCompressMenu(BaseKey, TargetToken: String);
var
  Ids, Labels, Tokens: TArrayOfString;
  I: Integer;
  SubKey, Command, Exe: String;
begin
  Exe := ExpandConstant('{app}\{#AppExeName}');

  // "Add to <name>.zip" / “添加到同名 ZIP 压缩包”
  SubKey := BaseKey + '\shell\QArchiver.AddToZip';
  RegWriteStringValue(HKLM, SubKey, '', '添加到同名 ZIP 压缩包');
  RegWriteStringValue(HKLM, SubKey, 'MUIVerb', '添加到同名 ZIP 压缩包');
  RegWriteStringValue(HKLM, SubKey, 'Icon', Exe + ',0');
  Command := '"' + Exe + '" --compress zip "' + TargetToken + '"';
  RegWriteStringValue(HKLM, SubKey + '\command', '', Command);

  // "Compress to..." submenu / “压缩到…”二级菜单
  SubKey := BaseKey + '\shell\QArchiver.CompressTo';
  RegWriteStringValue(HKLM, SubKey, 'MUIVerb', '压缩到...');
  RegWriteStringValue(HKLM, SubKey, 'SubCommands', '');
  RegWriteStringValue(HKLM, SubKey, 'Icon', Exe + ',0');

  SetArrayLength(Ids, 6);
  SetArrayLength(Labels, 6);
  SetArrayLength(Tokens, 6);
  Ids[0] := '01zip';    Labels[0] := 'ZIP 压缩包 (.zip)';   Tokens[0] := 'zip';
  Ids[1] := '02sevenz'; Labels[1] := '7z 压缩包 (.7z)';      Tokens[1] := '7z';
  Ids[2] := '03targz';  Labels[2] := 'Gzip tar (.tar.gz)';   Tokens[2] := 'tar.gz';
  Ids[3] := '04tarxz';  Labels[3] := 'Xz tar (.tar.xz)';     Tokens[3] := 'tar.xz';
  Ids[4] := '05tarbz2'; Labels[4] := 'Bzip2 tar (.tar.bz2)'; Tokens[4] := 'tar.bz2';
  Ids[5] := '06tar';    Labels[5] := 'Tar (.tar)';           Tokens[5] := 'tar';

  for I := 0 to GetArrayLength(Ids) - 1 do begin
    RegWriteStringValue(HKLM, SubKey + '\shell\' + Ids[I], 'MUIVerb', Labels[I]);
    Command := '"' + Exe + '" --compress ' + Tokens[I] + ' "' + TargetToken + '"';
    RegWriteStringValue(HKLM, SubKey + '\shell\' + Ids[I] + '\command', '', Command);
  end;

  SubKey := BaseKey + '\shell\QArchiver.CompressTo\shell\99custom';
  RegWriteStringValue(HKLM, SubKey, 'MUIVerb', '自定义压缩选项...');
  Command := '"' + Exe + '" --compress-dialog "' + TargetToken + '"';
  RegWriteStringValue(HKLM, SubKey + '\command', '', Command);
end;

procedure RegisterShellMenu;
var
  Exts: TArrayOfString;
  I: Integer;
  BaseKey, Exe: String;
begin
  Exe := ExpandConstant('{app}\{#AppExeName}');

  // Archive files: open + one-click extract / 压缩包：打开 + 一键解压
  Exts := ArchiveExtensions;
  for I := 0 to GetArrayLength(Exts) - 1 do begin
    BaseKey := 'Software\Classes\SystemFileAssociations\' + Exts[I];
    RegisterExtractVerb(BaseKey, 'QArchiver.Open', '用 小Q解压 打开',
                        '"' + Exe + '" "%1"');
    RegisterExtractVerb(BaseKey, 'QArchiver.OneClick', '一键解压',
                        '"' + Exe + '" --oneclick "%1"');
  end;

  // Files, folders and the folder background.
  // 文件、文件夹与文件夹空白处。
  RegisterCompressMenu('Software\Classes\*', '%1');
  RegisterCompressMenu('Software\Classes\Directory', '%1');
  RegisterCompressMenu('Software\Classes\Directory\Background', '%V');
end;

procedure UnregisterShellMenu;
var
  Exts: TArrayOfString;
  I: Integer;
  BaseKey: String;
begin
  Exts := ArchiveExtensions;
  for I := 0 to GetArrayLength(Exts) - 1 do begin
    BaseKey := 'Software\Classes\SystemFileAssociations\' + Exts[I] + '\shell';
    RegDeleteKeyIncludingSubkeys(HKLM, BaseKey + '\QArchiver.Open');
    RegDeleteKeyIncludingSubkeys(HKLM, BaseKey + '\QArchiver.OneClick');
  end;

  RegDeleteKeyIncludingSubkeys(HKLM,
    'Software\Classes\*\shell\QArchiver.AddToZip');
  RegDeleteKeyIncludingSubkeys(HKLM,
    'Software\Classes\*\shell\QArchiver.CompressTo');
  RegDeleteKeyIncludingSubkeys(HKLM,
    'Software\Classes\Directory\shell\QArchiver.AddToZip');
  RegDeleteKeyIncludingSubkeys(HKLM,
    'Software\Classes\Directory\shell\QArchiver.CompressTo');
  RegDeleteKeyIncludingSubkeys(HKLM,
    'Software\Classes\Directory\Background\shell\QArchiver.AddToZip');
  RegDeleteKeyIncludingSubkeys(HKLM,
    'Software\Classes\Directory\Background\shell\QArchiver.CompressTo');
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then begin
    if WizardIsTaskSelected('shellmenu') then
      RegisterShellMenu;
  end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usUninstall then
    UnregisterShellMenu;
end;
