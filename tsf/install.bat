@echo off
rem 强音输入法安装脚本 —— 请右键“以管理员身份运行”
setlocal
cd /d "%~dp0"

if not exist "qiangyin_ime.dll" (
    echo [错误] 找不到 qiangyin_ime.dll，请把本脚本和以下文件放在同一目录：
    echo   qiangyin_ime.dll  qiangyin.dll  mb.txt  usercode.txt
    pause
    exit /b 1
)

echo 正在注册强音输入法 ...
regsvr32 /s "%~dp0qiangyin_ime.dll"
if errorlevel 1 (
    echo [错误] 注册失败。请确认以管理员身份运行。
    pause
    exit /b 1
)

echo.
echo 注册成功！请到“设置 - 时间和语言 - 语言 - 中文 - 选项 - 键盘”中
echo 添加“强音”，或按 Win+空格切换到强音。
echo.
echo 卸载：regsvr32 /u "%~dp0qiangyin_ime.dll"（同样需要管理员权限）
pause
