# 开发管理目录（docs/DEV）

本目录是 **CTFontHook 项目自身的开发管理中枢**，用来让每一次改动都留下痕迹、可追溯、可回退。

> 说明：项目根目录下的 `docs/*.md`（architecture / configuration / reproduction / build-and-release / diagnostics / adr）
> 是从上游 SimpleFontHook 继承的技术文档，描述引擎与钩子设计，**保持原样**。
> 本目录 `docs/DEV/` 只记录「我们的开发过程与决策」。

## 文件一览

| 文件 | 作用 | 什么时候更新 |
| --- | --- | --- |
| `DEV_LOG.md` | 开发日志：每次改动追加一条（日期/任务/改了哪些文件/结论/遗留） | **每次改动后必更新** |
| `SESSION_HANDOFF.md` | 交接记录：当前进度、正在做什么、下一步 | 每次阶段性完成后更新 |
| `STATUS_MAP.md` | 功能模块地图：各模块职责、入口文件、当前状态 | 模块结构调整时更新 |
| `DECISIONS.md` | 架构决策记录（ADR）：为什么这样设计 | 产生新决策时追加 |
| `ROADMAP.md` | 路线图：目标与优先级 | 目标变化时更新 |
| `BACKUP.md` | 备份/回退使用说明（面向非技术使用者） | 脚本流程变化时更新 |
| `features/` | 单个功能的设计档案（一个功能一个文件） | 开始/推进某个功能时 |

## 一键脚本（scripts/）

| 脚本 | 作用 |
| --- | --- |
| `0-环境自检.bat` | 检查编译工具链、依赖、工程文件、Git 是否齐备（只读） |
| `1-一键测试.bat` | 编译 32/64 位 Release 与安装器 + 校验产物与导出表 |
| `2-一键打包.bat` | 编译（含安装器）+ 加壳 + 生成 `dist\` 下可分发 zip（含 `CTFontHook-Setup.exe`） |
| `3-一键备份.bat` | 提交改动并打 `backup-<时间>` 标签 |
| `4-一键回退.bat` | 从存档点中选一个回退（回退前自动存 `safety-<时间>`） |
| `tools\to_gbk.ps1` | 把 `scripts\*.bat` 由 UTF-8 转回 GBK + CRLF（见下） |

> **编码约定（重要）**：`scripts\` 下所有 `.bat` 必须是 **GBK(codepage 936) + CRLF**。
> cmd.exe 在 `chcp 65001` 下读取多行中文批处理会读取错位。改脚本流程：
> 以 UTF-8 编辑 → 运行 `powershell -NoProfile -ExecutionPolicy Bypass -File scripts\tools\to_gbk.ps1`。

## 工作约定（每个任务一套流程）

1. 动手前：先读 `SESSION_HANDOFF.md` 和 `ROADMAP.md`，确认当前该做什么。
2. 改动中：一次任务只解决一个问题，不顺手重构无关代码。
3. 改动后：
   - 更新 `DEV_LOG.md`（追加一条）
   - 更新相关 `features/*.md`（若有）
   - 更新 `SESSION_HANDOFF.md`（进度与下一步）
   - 双击 `scripts\3-一键备份.bat` 提交并打标签
4. 需要回退：双击 `scripts\4-一键回退.bat`。

## 版本号规则

`VERSION` 文件保存当前版本号，形如 `0.1.0`。约定：

- **主版本号**：不兼容的大调整（如使用方式大改）。
- **次版本号**：新增功能。
- **修订号**：修复与小优化。

每次发布节奏与 `CHANGELOG.md` 对应。
