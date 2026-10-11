# 第三方组件声明

本仓库包含以下第三方组件。各组件的版权归其各自作者所有，按其自身许可证分发。

## SimpleFontHook（上游项目）

- **来源**：<https://github.com/SuQiandYing/SimpleFontHook>
- **许可证**：MIT License
- **版权**：Copyright (c) SuQiandYing
- **说明**：CTFontHook 由 SimpleFontHook 分支而来，本仓库为独立演进的二次开发项目。

## Detours 4.0.1

- **来源**：<https://github.com/microsoft/Detours>（Microsoft Detours，v4.0.1）
- **许可证**：MIT License
- **版权**：Copyright (c) Microsoft Corporation
- **仓库内文件**：`detours.h`、`detours.lib`（Win32）、`detours_x64.lib`（x64）
- **说明**：本仓库仅附带预编译的库文件与头文件，用于函数钩取；未附带 Detours 源码。

## UPX 5.1.1

- **来源**：<https://github.com/upx/upx/releases/tag/v5.1.1>
- **许可证**：GNU General Public License v2 或更高版本（见 `tools/upx/COPYING`），
  并附带 UPX 特别例外条款（见 `tools/upx/LICENSE`）——该例外允许用 UPX 压缩任意程序
  而不影响被压缩程序自身的许可证。
- **仓库内文件**：`tools/upx/`（`upx.exe` 可执行文件、`COPYING`、`LICENSE`、`README.md`）
- **说明**：仅作为构建期压缩工具使用，不参与最终产品的链接。
