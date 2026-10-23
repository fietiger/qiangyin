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

  ; 先尝试反注册已存在的旧版本（避免旧 DLL 被占用导致无法覆盖）
  ${DisableX64FSRedirection}
  ExecWait '"$SYSDIR\regsvr32.exe" /s /u "$INSTDIR\qiangyin_ime.dll"'
  ${EnableX64FSRedirection}

  File "..\build-installer\staging\qiangyin_ime.dll"
  IfErrors 0 qy_ime_ok
    ; DLL 被运行中的进程占用：重命名旧文件再写入，旧文件重启后删除
    DetailPrint "qiangyin_ime.dll 被占用，尝试重命名替换..."
    Delete "$INSTDIR\qiangyin_ime.dll.old"
    Rename "$INSTDIR\qiangyin_ime.dll" "$INSTDIR\qiangyin_ime.dll.old"
    File "..\build-installer\staging\qiangyin_ime.dll"
    IfErrors 0 qy_ime_ok
      MessageBox MB_ICONSTOP "无法替换 qiangyin_ime.dll，请关闭所有程序后重试。"
      Abort
    Delete /REBOOTOK "$INSTDIR\qiangyin_ime.dll.old"
  qy_ime_ok:
  File "..\build-installer\staging\qiangyin.dll"
  IfErrors 0 qy_eng_ok
    DetailPrint "qiangyin.dll 被占用，尝试重命名替换..."
    Delete "$INSTDIR\qiangyin.dll.old"
    Rename "$INSTDIR\qiangyin.dll" "$INSTDIR\qiangyin.dll.old"
    File "..\build-installer\staging\qiangyin.dll"
    IfErrors 0 qy_eng_ok
      MessageBox MB_ICONSTOP "无法替换 qiangyin.dll，请关闭所有程序后重试。"
      Abort
    Delete /REBOOTOK "$INSTDIR\qiangyin.dll.old"
  qy_eng_ok:
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
  ; DLL 可能被运行中的程序占用：/REBOOTOK 能删就删，删不掉则排到重启后删除，不报错
  Delete /REBOOTOK "$INSTDIR\qiangyin_ime.dll"
  Delete /REBOOTOK "$INSTDIR\qiangyin.dll"
  Delete /REBOOTOK "$INSTDIR\qiangyin_ime.dll.old"
  Delete /REBOOTOK "$INSTDIR\qiangyin.dll.old"
  Delete "$INSTDIR\mb.txt"
  Delete "$INSTDIR\usercode.txt"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir /REBOOTOK "$INSTDIR"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\QiangYin"
SectionEnd
