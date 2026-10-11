# 开发日志（DEV_LOG）

> 规则：每次改动完成后，在最上方追加一条。倒序排列（最新在上）。

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

