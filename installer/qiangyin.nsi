; 强音输入法 NSIS 安装脚本
; 由 GitHub Actions 在 windows-latest 上打包，产物 QiangYin-Setup.exe
; 需要文件（由 workflow 准备到 build-installer/staging/）：
;   qiangyin_ime.dll  qiangyin.dll  mb.txt  usercode.txt

!include "MUI2.nsh"
!include "x64.nsh"

Name "强音输入法"
OutFile "..\QiangYin-Setup.exe"
InstallDir "$PROGRAMFILES64\QiangYin"
RequestExecutionLevel admin
Unicode true
SetCompressor /SOLID lzma

!define MUI_ABORTWARNING
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "SimpChinese"

Section "强音输入法" SecMain
  SetOutPath "$INSTDIR"
  File "..\build-installer\staging\qiangyin_ime.dll"
  File "..\build-installer\staging\qiangyin.dll"
  File "..\build-installer\staging\mb.txt"
  File "..\build-installer\staging\usercode.txt"

  ; 注册 TSF 输入法（写 HKCR\CLSID + 注册 TSF profile）
  ; 注意：NSIS 安装包是 32 位进程，必须关掉文件系统重定向，
  ; 否则 regsvr32 会被重定向到 SysWOW64 的 32 位版本，加载不了 64 位 DLL
  ${DisableX64FSRedirection}
  ExecWait '"$SYSDIR\regsvr32.exe" /s "$INSTDIR\qiangyin_ime.dll"' $0
  ${EnableX64FSRedirection}
  ${If} $0 != 0
    MessageBox MB_ICONSTOP "输入法注册失败（regsvr32 返回 $0）。请确认以管理员身份运行安装包。"
    Abort
  ${EndIf}

  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\QiangYin" \
    "DisplayName" "强音输入法"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\QiangYin" \
    "UninstallString" "$INSTDIR\Uninstall.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\QiangYin" \
    "InstallLocation" "$INSTDIR"
SectionEnd

Section "Uninstall"
  ; 反注册输入法（同上，用 64 位 regsvr32）
  ${DisableX64FSRedirection}
  ExecWait '"$SYSDIR\regsvr32.exe" /s /u "$INSTDIR\qiangyin_ime.dll"'
  ${EnableX64FSRedirection}
  Delete "$INSTDIR\qiangyin_ime.dll"
  Delete "$INSTDIR\qiangyin.dll"
  Delete "$INSTDIR\mb.txt"
  Delete "$INSTDIR\usercode.txt"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\QiangYin"
SectionEnd
