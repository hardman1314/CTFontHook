# 模块地图（STATUS_MAP）

> 记录各模块职责、入口文件与当前状态。结构变化时更新。
> 上游继承的技术细节见项目根 `docs/architecture.md`。

## 顶层结构

| 路径 | 职责 | 状态 |
| --- | --- | --- |
| `CTFontHook.sln` | 解决方案（Debug/Release × Win32/x64） | 正常 |
| `src/` | 全部源码（原 `SimpleFontHook/` 改名） | 正常 |
| `src/CTFontHook.vcxproj` | 工程文件，产物名固定 `winmm.dll` | 正常 |
| `detours.h` / `detours.lib` / `detours_x64.lib` | 内置 Detours 4.0.1（免下载依赖） | 正常 |
| `tools/upx/upx.exe` | 构建后自动加壳工具 | 正常 |
| `docs/` | 上游技术文档（架构/配置/复刻/构建/诊断） | 继承原样 |
| `docs/DEV/` | **本项目开发管理**（日志/交接/决策/路线图） | 使用中 |
| `scripts/` | 一键脚本 | 使用中 |
| `installer/` | 独立 GUI 安装器工程（部署自动化，产物 `CTFontHook-Setup.exe`） | 使用中 |

## 源码模块（src/）

| 模块 | 入口 | 职责 |
| --- | --- | --- |
| 进程入口 | `src/dllmain.cpp` | 只发布安装入口与退出信号 |
| 钩子聚合 | `src/hooks/font_hooks.cpp` | 按顺序包含 `internal/**/*.cppinc`，形成单一翻译单元 |
| 通用钩子实现 | `src/hooks/internal/font_hooks_*.cppinc` | 安装、运行时、绘制、代码页、文字映射 |
| 字体替换模型 | `src/hooks/internal/model/` | 替换缓存、逻辑字体视图、度量、字形虚拟化 |
| 查询适配 | `src/hooks/internal/queries/` | 身份、枚举、度量、字体数据查询 |
| 引擎适配器 | `src/hooks/internal/engines/<engine>/` | 17 个引擎的检测/资源/缓存/运行时桥接 |
| 字体表修改 | `src/font/font_patcher.cpp` | SFNT/TTC 解析与修改、校验和 |
| 字体选择器 UI | `src/ui/font_picker.cpp` + `src/ui/internal/**` | 枚举、编辑、保存配置、发布版本 |
| 配置与工具 | `src/utils.cpp` | 配置持久化、自定义字体加载、诊断 |

## 引擎适配器索引（17 个）

`common`、`artemis`、`artemis_legacy`、`bgi`、`catsystem2`、`dxlib`、`entis`、`escude`、
`krkr`、`majiro`、`mirai`、`renpy`、`softpal`、`tinkerbell`、`tyrano`、`unity`、`yuris`。

每个目录都有独立 `README.md`，统一包含：职责、检测条件、文件入口、功能、配置、运行约束、证据、复刻、验证。

## 当前状态总览

- 代码：在上游 2.6 基础上已做首批体验优化（P0-1 应用字体按钮、P0-2 热键兜底）。
- 新增模块：`installer/`（独立 GUI 安装器，负责部署自动化）。
- 编译：Win32 与 x64 Release（含安装器）均可编译（见 DEV_LOG 验证记录）。
- 待办：真机验证与后续 P1 优化（见 ROADMAP）。
