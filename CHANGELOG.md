# 更新日志（CHANGELOG）

本项目遵循「一个任务一个提交」的原则，每次可交付的改动在此登记。
更细的开发过程记录见 `docs/DEV/DEV_LOG.md`。

## [0.1.1] - 2026-10-11

### 新增
- 安装器改为**单文件安装包**：把 32/64 位 `winmm.dll` 以 `RCDATA` 内嵌进安装器
  （`installer/payload.rc` + `main.cpp` 的 `ExtractEmbeddedDll()`），安装时若缺少同级
  `32bit\` / `64bit\` 目录则释放内嵌 DLL。`一键打包` 额外产出
  `dist\CTFontHook-Setup-<版本>.exe`，可单独作为 Release 附件发布。
- 新增 `docs/DEV/RELEASE_GUIDE.md`（GitHub Release 发布指南）与
  `docs/DEV/RELEASE_NOTES_TEMPLATE.md`（发布说明模板）。

### 变更
- 开源发布：仓库公开于 <https://github.com/hardman1314/CTFontHook>；
  `LICENSE.txt` 版权行填真实权利人；新增 `.gitattributes`、`THIRD-PARTY-NOTICES.md`；
  `.gitignore` 忽略 `/设计图/`。

## [0.1.0] - 2026-10-11

### 新增
- 从 SimpleFontHook 2.6 源码分支，建立独立项目 **CTFontHook**。
- 工程文件改名：解决方案 `CTFontHook.sln`、工程 `CTFontHook.vcxproj`、源码目录 `src/`（最终产物 DLL 仍为 `winmm.dll`）。
- 建立开发管理文档：`docs/DEV/`（开发日志、交接、模块地图、决策、路线图、备份说明）。
- 建立一键脚本：`scripts/`（环境自检、一键测试、一键打包、一键备份、一键回退）。
- 建立 Git 版本管理与备份/回退机制。

### 说明
- 产品名称为 CTFontHook，但部署文件名必须保持 `winmm.dll`（DLL 劫持机制要求）。
- 上游项目：SimpleFontHook（https://github.com/SuQiandYing/SimpleFontHook），MIT 许可。
