# 强音输入法 · TSF 外壳（v0.1）

Windows 文本服务（Text Input Processor），把 Rust 查词引擎接进
Windows 输入法框架。不用 ATL，手写 COM。

## 目录

```
tsf/
├── CMakeLists.txt
├── install.bat # 管理员安装脚本（regsvr32）
├── README.md
└── src/
├── Guids.h/cpp # CLSID / Profile GUID（固定，勿改）
├── Utf.h # UTF-8/UTF-16 转换
├── Engine.h/cpp # qiangyin.dll 动态加载 + 码表定位
├── KeyHandler.h/cpp # 按键状态机（纯逻辑，含退格修复）
├── TextService.h/cpp# TSF 主类：COM、组字、edit session、显示属性
├── CandidateWindow.h/cpp # 候选窗口（Win32 popup）
├── dllmain.cpp # DLL 入口、类厂、注册/反注册
└── qiangyin_ime.def # 导出表
```

## 构建

GitHub Actions 每次 push 自动构建（windows-latest + MSVC），产物在
Actions → Artifacts → `qiangyin-ime-windows-x64`（`qiangyin_ime.dll`）。

本地构建（VS 2022 + CMake）：

```powershell
cmake -S tsf -B build-tsf -A x64
cmake --build build-tsf --config Release
```

## 安装（Windows 本机）

把以下文件放同一目录，右键 `install.bat` → 以管理员身份运行：

- `qiangyin_ime.dll`（本外壳，Actions 下载）
- `qiangyin.dll`（Rust 引擎，Actions 下载）
- `mb.txt`、`usercode.txt`（仓库 `data/` 目录）

然后到 设置 → 时间和语言 → 语言 → 中文 → 选项 → 键盘 添加"强音"，
Win+空格切换。卸载：`regsvr32 /u qiangyin_ime.dll`（管理员）。

注意：64 位 Windows 上，64 位程序用 x64 的 DLL；32 位程序
（如部分老软件）需要再编译一份 Win32 的——v0.1 只提供 x64。

## 按键行为（v0.1）

- 中文模式：`a-z` 组字；`Backspace` 回退组字内字符（**组字中永远被
输入法吃掉，不下发给程序——这就是要修的那个 bug**）；
`Space`/`Enter` 上屏首选；`Esc` 取消组字；`1-9` 选重；
`Shift` 点按切换中/英文（切换前先把组字上屏，不丢字）；
`，。；：？！、` 等自动转全角标点
- 英文模式：只响应 `Shift` 切换，其余放行
- `Ctrl`/`Alt` 组合键一律放行，不抢快捷键

行为开关集中在 `KeyHandler.cpp`，对照原版微调只改这一处。

## 调试

- `OutputDebugStringW` 输出 `[qiangyin]` 前缀日志，用 DebugView 查看
- 引擎加载失败时打字无候选：检查 `qiangyin.dll` / `mb.txt` 是否和
`qiangyin_ime.dll` 在同一目录
- 候选窗口位置：取组字区屏幕坐标，失败时回退到光标位置

## 已知限制（v0.1）

- 无翻页（候选固定 9 个）、无语言栏按钮、无用户词频自学习
- 标点映射为固定表（引号不配对）
- 只做了 x64；IME 在某些 UWP/应用商店程序里行为可能受限（TSF 通病）
