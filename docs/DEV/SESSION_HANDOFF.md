# 交接记录（SESSION_HANDOFF）

> 给「下一次开始工作的人（或 AI）」看的：现在到哪了、接下来做什么。

## 当前状态

- **阶段**：项目初始化完成（第 0 步），已具备接手开发的条件。
- **版本**：`0.1.0`（基线，Git 标签 `v0.1.0`）。
- **仓库**：`D:\CTLIB\FontHook\CTFontHook`（本地 Git，分支 `master`，基线提交 `6f73d6c`）。
- **可用能力**：环境自检、一键编译测试、一键打包、一键备份、一键回退（均已实测通过）。
- **外层隔离**：`D:\CTLIB\.gitignore` 已加入 `/FontHook/`，本仓库不会被 Chrono Tide 主仓嵌套跟踪。

## 已经做完（含验证结果）

1. 复制上游源码并改名为 CTFontHook（sln / 工程 / `src/` 目录），命名空间与引用修正。
2. `scripts\1-一键测试.bat` 实测编译出 32/64 位 `winmm.dll` 且导出校验通过 → **改名未破坏构建**。
3. `scripts\2-一键打包.bat` 实测产出可分发 zip（含双架构 DLL 与说明）。
4. 开发管理文档（DEV_LOG / STATUS_MAP / DECISIONS / ROADMAP / BACKUP）与一键脚本就位。
5. Git 基线提交 + `v0.1.0` 标签完成；构建产物已被 `.gitignore` 排除。

## 下一步（按优先级）

- **P0**：通读上游 `docs/architecture.md` 与 `src/ui/README.md`，建立对「用户使用全流程」的完整认知（这是重构的起点）。
- **P1**：在 `docs/DEV/ROADMAP.md` 中拆解「用户实际使用全流程体验重构」的具体子目标。
- **P1**：在 `docs/DEV/features/` 下为第一个体验优化点建立功能档案。
- **P2**：`docs/` 下继承自上游的技术文档仍写着 `SimpleFontHook/` 旧路径，按需校正。

## 注意事项

- 最终产物 DLL 必须仍叫 `winmm.dll`，不要改。
- **`scripts\*.bat` 必须保持 GBK + CRLF 编码**（cmd 在 65001 下读多行中文批处理会错位）。
  改脚本请以 UTF-8 编辑后运行 `scripts\tools\to_gbk.ps1` 转回 GBK。
- 源码内的 `SimpleFontHook.*` 字面量（窗口消息、注入标记等）是运行期协议，**不要**改名（见 ADR-0004）。
- 一个任务一个提交，提交前用 `scripts\3-一键备份.bat`。
- 不要直接改 `docs/` 下继承自上游的技术文档（除非确认需要）。
