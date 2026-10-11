# 开发日志（DEV_LOG）

> 规则：每次改动完成后，在最上方追加一条。倒序排列（最新在上）。

---

## 2026-10-11　部署自动化（三）：一键脚本接入安装器 + 忽略产物

- **任务**：把独立安装器接入两个一键脚本，并让构建产物不进版本库。
- **改动文件**：
  - `scripts/1-一键测试.bat`：新增 `[3/4] 编译安装器 Release`；`[4/4]` 增加对
    `installer\bin\Win32\Release\CTFontHook-Setup.exe` 的存在校验；总步骤名 3/3 → 4/4。
  - `scripts/2-一键打包.bat`：新增 `[3/5] 编译安装器`；把 `CTFontHook-Setup.exe` 复制进包根目录；
    重写生成的《使用说明.txt》（首推「双击安装器」，手工步骤降级为附录）；总步骤名 4/4 → 5/5。
  - `.gitignore`：新增 `/installer/bin/`、`/installer/obj/`。
- **做法**：脚本以 UTF-8 编辑 → 运行 `scripts\tools\to_gbk.ps1` 转回 GBK + CRLF。
- **验证**（实测）：
  - `scripts\1-一键测试.bat`：`[1/4]`~`[4/4]` 全通过，控制台显示三份产物均「通过」。
  - `scripts\2-一键打包.bat`：产出 zip，内含 `CTFontHook-Setup.exe`（116736 B）、`32bit\winmm.dll`、
    `64bit\winmm.dll`、`版本.txt`、`使用说明.txt`；`使用说明.txt` 以 GBK 解码正确无乱码。
  - `git check-ignore` 确认 `installer/bin`、`installer/obj` 已被忽略，仅源码三个文件入库。
- **遗留**：安装器与脚本均未做真机安装闭环验证。

---

## 2026-10-11　部署自动化（二）：独立 GUI 安装器工程

- **任务**：把「判断架构 / 备份 / 复制 / 还原」四步手工流程，做成一个双击即用的安装器。
- **改动文件**（新增，纯新模块，未触碰既有源码）：
  - `installer/CTFontHookInstaller.vcxproj`（应用工程，v143 / Unicode / `/utf-8`；产物 `CTFontHook-Setup.exe`；UAC `requireAdministrator`）。
  - `installer/main.cpp`（Win32 GUI：`ReadPeMachine` 判架构、`DoInstall` / `DoUninstall`、`.CTFontHook` 备份与 `manifest.ini`）。
  - `installer/resource.h`（控件 ID）。
- **做法**：解析目标 EXE 的 PE 头 `IMAGE_FILE_HEADER.Machine`（`0x014C`/`0x8664`）自动选 DLL；
  备份与安装记录集中在 `<游戏目录>\.CTFontHook\`，卸载据 `manifest.ini` 还原。
- **验证**（实测）：
  - 构建出 `installer\bin\Win32\Release\CTFontHook-Setup.exe`（32 位、含 `requireAdministrator`）。
  - 用临时探针校验架构判定：`System32\notepad.exe`→64 位、`SysWOW64\notepad.exe`→32 位、
    本项目两种 `winmm.dll` 各自匹配，全部正确。
- **决策**：形态（独立 GUI EXE）与范围（仅装 / 卸）经用户确认，记入 ADR-0005。

---

## 2026-10-11　体验优化：P0-1「应用此字体」按钮 + P0-2 热键兜底

- **任务**：解决 `USER_FLOW.md` 两条 P0 痛点（用户已批准方案：P0-1=方案 B，P0-2=方案 A）。
- **改动文件**：
  - P0-1（方案 B）：`state/font_picker_layout_geometry.cppinc:60`（新增按钮矩形）、
    `state/font_picker_state.cppinc:280`（悬停状态）、
    `paint/font_picker_paint_preview.cppinc:97`（用按钮替换原键盘提示）、
    `input/font_picker_wndproc.cppinc:123`、`:513`、`:545`、`:600` 等（点击 / 悬停 / 离开）。
  - P0-2（方案 A）：`state/font_picker_window_animation.cppinc:106`、`:110`（无热键时隐藏→最小化）、
    `paint/font_picker_paint_title.cppinc:61`（标题提示「无热键·关闭=最小化」+ 警示色）。
- **做法**：P0-1 保留「单击=选中」，新增显式按钮；P0-2 在 `HidePickerWindowAnimated` 入口加守卫，
  无热键则改走既有 `MinimizePickerWindowNative`（窗口有 `WS_EX_APPWINDOW`，任务栏可重开）。
- **验证**：`scripts\1-一键测试.bat` 32/64 位编译通过、导出表校验通过。
- **遗留**：未做真机交互验证（按钮手感 / 「三热键全占用」场景）。详见 `docs/DEV/features/` 两份档案。

---

## 2026-10-11　阶段 1 梳理：用户全流程现状图 + 痛点清单

- **任务**：只读梳理「普通玩家从装 DLL 到字体生效」的完整链路，产出痛点清单，为体验重构定优先级。
- **改动文件**：
  - 新增 `docs/DEV/USER_FLOW.md`（全流程图 / 界面结构 / 应用与持久化机制 / P0–P3 痛点清单）。
  - 更新 `docs/DEV/ROADMAP.md`（阶段 1 的 P0、P1 标记完成）。
- **做法**：并行派发一次 UI 源码探索 + 主线程亲读 `src/ui/README.md`、`docs/architecture.md`；
  对将要据以决策的两条结论（单击仅选中、热键全占用无自救）**亲自回读源码确认**
  （`input/font_picker_wndproc.cppinc:582`、`:603`、`:624`；`font_picker_lifecycle.cppinc:83`、`:84`）。
- **结论**：共整理 15 条痛点。最高优先级两条：
  - **P0-1 单击只选中、不应用**（必须双击/Enter 才生效）——玩家第一次上手就会撞上。
  - **P0-2 三个热键全被占用时窗口隐藏后无法再打开**（只有热键能切换显隐）。
- **验证**：本轮为纯只读 + 新增文档，未改任何源码，无需编译验证。
- **遗留**：等待用户确认第一个优化目标；确认后为其建立 `docs/DEV/features/` 档案并出《变更计划》。

---

## 2026-10-11　建立 CTFontHook 项目基础

- **任务**：从 SimpleFontHook 2.6 分支出独立项目 CTFontHook，搭建开发环境与开发管理基础设施。
- **改动文件**：
  - 新增整个项目目录 `D:\CTLIB\FontHook\CTFontHook\`（复制自 SimpleFontHook-main 源码，排除构建产物）。
  - 改名：`SimpleFontHook.sln → CTFontHook.sln`、`SimpleFontHook/ → src/`、`SimpleFontHook.vcxproj → src/CTFontHook.vcxproj`；同步修正 `.sln` / `.vcxproj` / `build_x*.bat` / `pack_release.bat` 内的引用。
  - 新增：`VERSION`、`CHANGELOG.md`、`.gitignore`、`docs/DEV/*`、`scripts/*`。
  - 重写：`LICENSE.txt`（MIT + 上游署名）、`README.md`（改为 CTFontHook 叙述）。
- **验证**（均为实测，非推测）：
  - `scripts\0-环境自检.bat`：MSBuild / vcvars64 / dumpbin / Detours×3 / UPX / sln+vcxproj / Git 全部「通过」。
  - `scripts\1-一键测试.bat`：编译出 `Release\winmm.dll`(590848→255488B) 与 `x64\Release\winmm.dll`(678400→281600B)，且 32/64 位导出校验均「通过」。
  - `scripts\2-一键打包.bat`：产出 `dist\CTFontHook-0.1.0-<时间戳>.zip`，内含 `32bit\winmm.dll`、`64bit\winmm.dll`、`版本.txt`、`使用说明.txt`。
  - `scripts\3-一键备份.bat` / `scripts\4-一键回退.bat`：工作区干净时的「无改动 / 取消」分支行为正确。
  - Git：`git init -b master`，基线提交 `6f73d6c`，标签 `v0.1.0`；192 个文件入库，构建产物全部被忽略。
- **关键坑（已解决，务必保留）**：`scripts\*.bat` 必须为 **GBK(codepage 936) + CRLF**。
  cmd.exe 在 `chcp 65001` 下读取多行中文批处理会发生读取错位（中文行被截断当作命令执行）；
  实测 UTF-8 + chcp 65001 会间歇报错，GBK + 默认 936 控制台则完全稳定。
  修改脚本的正确流程：以 UTF-8 编辑 → 运行 `scripts\tools\to_gbk.ps1` 转回 GBK。
- **结论**：环境可用、改名有效、构建与打包链路端到端跑通，可正式接手开发。
- **遗留**：`docs/` 下的上游技术文档仍沿用 `SimpleFontHook/` 目录名与旧路径（P2，不影响使用，后续按需校正）；源码内运行期标识串仍为 `SimpleFontHook`（ADR-0004 已说明，属有意保留）。

