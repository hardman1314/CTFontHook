# 开发日志（DEV_LOG）

> 规则：每次改动完成后，在最上方追加一条。倒序排列（最新在上）。

---

## 2026-10-11　界面重构 批次1「窗口骨架与配色」（设计稿复刻）

- **任务**：按 Figma 设计稿（`设计图/`）把字体选择窗口整体重绘为「深海蓝」暗色面板，
  **外观严格复刻、功能一个不丢**。本次为三批次中的**批次 1**：窗口骨架与配色。
- **三项用户决策**：①复刻严格度 = 设计外观 + 保留全部功能；
  ②窗口默认 410×724、仍可缩放（最小 380×660）；③列表背景图由主人从 Figma 导出（批次 3 接入）。
  另：UI 整体缩放由「标题栏 A−/A+」改为 **Ctrl+滚轮**；预览字号 A−/A+ **仅本次会话有效**（默认 22px）。
- **改动文件**（15 改 + 1 新建，全部在计划内）：
  - `state/font_picker_state.cppinc`：布局常量（410×724，TITLE 32 / SEARCH 27 / CHARSET 38 /
    HINT 24 / SPOOF 40 / TEXTSUB 34 / METRICS 150 / PREVIEW 136 / BOTTOM_BAR 29 / 圆角 11）；
    调色板整套换为设计色（保留 `COL_*` 名，仅改值）；新增预览字号状态与西文名缓存。
  - `state/font_picker_layout_geometry.cppinc`：区块自下而上推导新 `Get*Top()`；
    新增标题药丸/各区块/预览框/字号控件矩形；新增 `GradientH` / `GradientVStops` / `GradientHStops`；
    移除旧 `PickerPageTab*` API。
  - `paint/font_picker_paint_status.cppinc`（**新建**）：`PaintListHint` + `PaintBottomBar`。
  - `paint/font_picker_paint_background.cppinc`：底色渐变 + 新增 `PaintCompatBackdrop`（设置区底板）。
  - `paint/font_picker_paint_title/search/charset/font_list/preview/border.cppinc`：按设计重绘。
  - `paint/font_picker_paint_spoof/text_substitution/metrics.cppinc`：去各自背景/分隔线，
    改用设计配色与 `Microsoft YaHei`。
  - `font_picker_paint.cppinc`：挂接 `PaintCompatBackdrop` / `PaintListHint` / `PaintBottomBar`。
  - `input/font_picker_wndproc.cppinc`：改用 `GetCharsetTop()`/`GetSpoofTop()`；标题栏去 A−/A+，
    改单页签 `HeaderPillHitTest`，新增 **Ctrl+滚轮缩放** 与 **预览字号 A−/A+** 点击/悬停。
  - `font_picker_lifecycle.cppinc`：搜索框定位/字体对齐新布局。
- **做法**：调色板「保名改值」，使 696 行的指南绘制与旧工具条**自动换色**，避免逐文件重写；
  区块自下而上定位，保证底栏贴底；多段渐变用「逐段两色标填充」规避 `GradientFill` 多顶点限制。
- **验证**：`scripts\1-一键测试.bat` → **4/4 通过**（x64 Release / Win32 Release / 安装器 / 导出校验）。
- **遗留**：批次 2（兼容设置区合并重排）、批次 3（海报背景图 RCDATA+WIC）待做；
  圆角为 1px 描边贴合方角窗口（无原生边框）；**观感需主人真机验证**（本机无法可视化验证 GDI 窗口）。
  详见 [`features/界面重构-设计稿复刻.md`](features/界面重构-设计稿复刻.md)。

## 2026-10-11　预览样例文字可编辑并持久化（P2-2）

- **任务**：P2-2「预览看不到游戏自身文字」——让预览区第 1 行样例文字可由玩家自行输入，
  以检验候选字体对游戏实际文本（人名 / 对话 / 生僻字）的渲染效果，并持久化到 `FontHook.ini`。
- **路线选型（审计后二选一）**：采用**路线 A（用户可编辑样例）**；
  **路线 B（Hook 游戏 TextOut/ExtTextOut/DrawText 自动抓取游戏原文）**因
  需在渲染热路径截获字符串、涉及编解码与线程安全、出错会**直接影响游戏画面**而收益相近，
  本次**不做**，降级 P3 评估。
- **改动文件**（8 改 + 1 新建，全部在计划内）：
  - `framework.h:98` / `utils.cpp:98,925,989`：新增配置 `Config::PreviewTextW[256]`，
    `SaveConfig` 追加 `PreviewTextW` 键、`LoadConfig` 读取（空 = 内置默认）。
  - `state/font_picker_state.cppinc:273-275,286`：新增行内编辑句柄/子类过程/关闭标志与样例区悬停标志。
  - `state/font_picker_layout_geometry.cppinc:81`：`GetPreviewSampleRect()`（对齐绘制处 `contentRc`）。
  - `input/font_picker_preview_edit.cppinc`（**新建**）：`StartPreviewTextEdit` /
    `FinishPreviewTextEdit` / `PreviewTextEditSubclassProc`。
  - `input/font_picker_wndproc.cppinc:166,329,585-641,664,680`：点击进入编辑、`EN_KILLFOCUS` 提交、
    样例区 hover / 离开复位、点击链挂接。
  - `paint/font_picker_paint_preview.cppinc:61-67,78-86`：第 1 行取 `Config::PreviewTextW`；
    样例区右上角绘制「点击此处可编辑样例文字」提示（悬停高亮）。
  - `font_picker_input.cppinc`：在 `wndproc` 之前 include 新 `.cppinc`（保证函数可见）。
  - `CTFontHook.vcxproj`：登记新 `.cppinc` 为 `<None Include>`（与兄弟项一致，不参与编译）。
- **做法**：完全复用既有「行内编辑框」模式（`input/font_picker_metric_edit.cppinc`）——
  `CreateWindowExW(L"EDIT")` + 子类化 + `Enter` 提交 / `Esc` 取消 / `Tab` 透传 / 失焦提交。
  空输入 = 回退内置默认样例，老用户无感知。
- **明确不改**：预览第 2 行（假名 + 数字）保持固定（字形/字宽稳定基准）；不引入路线 B。
- **验证**（实测）：`scripts\1-一键测试.bat` `[1/4]`~`[4/4]` 全通过；`git diff --stat` 8 文件 64 增 3 删 + 1 新建。
- **遗留**：未做真机验证（输入框弹出位置/字体、失焦提交、`Tab` 透传、中日文存取显示）；
  路线 B（游戏自身文字）仍待 P3 评估。

---

## 2026-10-11　排版数值体验修整（P2-3）

- **任务**：P2-3「排版数值单位不一致 / 下行填正数被静默取负」。
- **改动文件**：
  - `paint/font_picker_paint_metrics.cppinc:69`、`:74`：字宽/字高值显示由 `1.00` → `1.00×`，主界面即可看出是「倍率」。
  - `paint/font_picker_paint_guide.cppinc:106`：指南「上行 / 下行」条目补符号说明（上行正值、下行负值，填正数自动按负值处理）。
- **审计结论（为何很小）**：
  - 「单位」半边：指南 `paint_guide.cppinc:100-108` **早已解释**各数值含义（字宽/字高「倍率 1.00」、粗细「400 为常规字重」等），缺的只是主界面提示，故仅给倍率项补 `×`。
  - 「下行取负」半边：经核实是**必要且正确的语义**——下行范围 `-2000..-1`，若去掉 `if (value > 0) value = -value;`（`metric_edit.cppinc:102`）会被钳到 `-1`，比现状更糟；故只补文案说明、**不改代码**。
- **明确不改**：其余整数项（字距/上行/行距/粗细）不加 `‰`/`px` 等单位（存在不准确风险）；编辑框预填仍为纯数字。
- **验证**（实测）：`scripts\1-一键测试.bat` `[1/4]`~`[4/4]` 全通过；`git diff --stat` 仅 2 文件 4 增 3 删。
- **遗留**：未做真机验证（`1.00×` 观感）。

---

## 2026-10-11　界面汉化：窗口标题 + 搜索框占位符（P2-1）

- **任务**：P2-1「界面中英混杂」——审计后真实残留的英文界面串只有 2 处，本次清理。
- **改动文件**：`font_picker_lifecycle.cppinc`
  - `:22` 窗口标题 `Font Selection Assistant` → `字体选择器`（任务栏可见）。
  - `:61` 搜索框占位符 `Search fonts...` → `搜索字体…`（保留装饰 `✧` 与前导空格）。
- **审计结论（为何只改 2 行）**：选择器主界面（药丸/伪装条/映射转码条/排版条/预览/指南）本就全中文；
  其余英文命中项均为日志、字体名、物理键名或技术术语。详见《变更计划》第四节「明确不改」：
  键盘键名（Insert/Pause/…）保留；`L" Custom"` 是字体识别标识不动；
  `metric_layout` 的 `W/H/Gap` 是未命名形参（不显示）；安装器 `32bit/64bit` 是发布包子目录名（路径）不动。
- **验证**（实测）：`scripts\1-一键测试.bat` `[1/4]`~`[4/4]` 全通过；`git diff --stat` 仅 1 文件 2 行。
- **遗留**：未做真机验证（任务栏标题、搜索框提示实际观感）。

---

## 2026-10-11　体验重构：关闭语义文案 + 保存失败提示（P1-3 / P1-4）

- **任务**：P1-3 澄清主界面「× 只是隐藏窗口、不退出游戏」；P1-4 把「配置写盘失败」从静默日志改为用户可见提示。
- **改动文件**（8 个，全部在计划内）：
  - `paint/font_picker_paint_title.cppinc`：标题栏副文案在字体页/指南页追加「· ×=隐藏」（无热键分支保持「关闭=最小化」）。
  - `framework.h` / `utils.cpp`：`Utils::SaveConfig` 由 `void` 改为 `bool`（写盘失败 `return false`，成功 `return true`）。
  - `font_picker_apply_config.cppinc`：`SavePickerConfig()` 改为 `bool`，透传 `SaveConfig` 结果。
  - `state/font_picker_state.cppinc`：新增 `g_applyToastError` 与警示色 `COL_WARN = RGB(240,176,96)`。
  - `font_picker_font_list.cppinc`：`ApplySelectedFont()` 捕获保存结果 —— 成功走原提示，失败改用告警文案。
  - `paint/font_picker_paint_preview.cppinc`：提示条按 `g_applyToastError` 取警示色/强调色。
  - `input/font_picker_wndproc.cppinc`：`WM_TIMER` 清提示时一并复位 `g_applyToastError`。
- **做法**：P1-3 复用标题栏既有副文案区（零新增状态）；P1-4 复用任务 C 的 toast 通道（仅加一个错误标志）。
  **有意只覆盖「应用字体」主路径**，高级页（缩放/伪代码页等）保存失败仍静默。
- **验证**（实测）：`scripts\1-一键测试.bat` `[1/4]`~`[4/4]` 全通过；`git status`/`git diff --stat` 仅计划内 8 文件。
- **遗留**：未做真机验证（无写权限目录下的失败提示、标题栏文案实际观感）。

---

## 2026-10-11　体验重构：字符集智能预选 + 应用后重启提示（任务 C：P1-1 / P1-2）

- **任务**：P1-1 按游戏检测到的编码自动预选字符集；P1-2 应用字体后提示「可能需要重启游戏」。
- **改动文件**：
  - `state/font_picker_state.cppinc`：新增 `g_charsetSelectionSet`；新增提示定时器常量与 `g_applyToastText`。
  - `input/font_picker_wndproc.cppinc`：新增 `PreselectCharsetFromDetection()`（显示选择器时预选）；
    `HandleCharsetClick` 置位「用户已选」；`WM_TIMER` 处理提示定时器。
  - `font_picker_font_list.cppinc`：`ApplySelectedFont()` 末尾设置提示文案 + 启动一次性定时器 + 重绘预览。
  - `paint/font_picker_paint_preview.cppinc`：预览区顶部绘制提示条（最上层）。
- **做法**：P1-1 只在「用户未指定字符集 + 检测已就绪」时自动预选一次，之后完全尊重用户；
  P1-2 用 4 秒一次性定时器（`WM_TIMER`）自动清除提示。
- **验证**（实测）：`scripts\1-一键测试.bat` `[1/4]`~`[4/4]` 全通过；`git status` 仅计划内文件。
- **遗留**：未做真机验证（预选是否命中真实编码、提示条观感）。

---

## 2026-10-11　安装器：扩展为多游戏字体管理器（任务 B2）

- **任务**：用户有「多个游戏都要改字体」的需求，把安装器从单游戏工具升级为多游戏集中管理。
- **改动文件**：
  - `installer/main.cpp`：新增中央清单 `%APPDATA%\CTFontHook\games.ini`（UTF-16LE+BOM）读写与按目录去重的
    `UpsertGame` / `RemoveGameRecord`；`ReadGameFont` 读取各游戏 `FontHook.ini` 的 `FontNameW`；
    `DoUninstall` 重构为 `DoUninstallForDir`；新增 `SysListView32` 列表（游戏/目录/架构/当前字体/状态）与
    按钮「卸载/还原」「打开目录」「移除记录」「刷新」「手动登记游戏…」；界面客户端 760×626。
  - `installer/resource.h`：新增列表与按钮 ID（1012–1018）。
  - `installer/CTFontHookInstaller.vcxproj`：`<Link><AdditionalManifestDependencies>` 引入 Common-Controls v6。
- **做法**：主/清单分栏；列表面板与操作日志随窗口自适应（`WM_SIZE`）。
- **验证**（实测）：
  - `scripts\1-一键测试.bat` `[1/4]`~`[4/4]` 全通过。
  - `mt.exe` 提取 EXE 嵌入清单：同时含 `requestedExecutionLevel=requireAdministrator` 与
    `Microsoft.Windows.Common-Controls 6.0.0.0`。
  - `VersionInfo`（B1+B3 已验）保持正确。
- **坑（务必保留）**：`AdditionalManifestDependencies` 必须放在 `<Link>` **节点下**（映射 `/MANIFESTDEPENDENCY:`），
  放在 `<Manifest>` 工具节下不会生效（不会与 UAC 清单合并）。
- **遗留**：未做真机多游戏闭环验证；`<shellapi.h>` 需显式包含（`ShellExecuteW`）。

---

## 2026-10-11　安装器：修窗口显示（高 DPI 裁剪）+ 补版本信息（任务 B1+B3）

- **任务**：修复安装器窗口「提示内容显示不完整」，并为 EXE 补上开发者版本信息。
- **根因**：`SetProcessDPIAware()` + `DEFAULT_GUI_FONT`（随系统 DPI 放大）+ 固定像素布局（580×390）
  → 高分屏下文字变大但控件框不变，提示行被裁剪；且提示原为单行、长度超宽。
- **改动文件**：
  - `installer/main.cpp`：新增 DPI 感知的 `g_dpi` / `S(int)` 缩放；控件坐标与字体全部经 `S()` 计算；
    改用显式 `CreateFontW(-S(13), …, "Microsoft YaHei UI")`；提示改为两行多行 STATIC；
    窗口客户端 620×450，可调整大小（`WS_THICKFRAME`/`WS_MAXIMIZEBOX` + `WM_SIZE` 时日志区与提示区自适应）；
    `WM_GETMINMAXINFO` 设最小尺寸；`WM_DESTROY` 释放自建字体。
  - `installer/version.rc`（新增）：版本资源，`CompanyName=HardMan_Official`、`ProductName=CTFontHook`、
    `FileVersion=0.1.0.0`（与仓库根 `VERSION` 同步）。
  - `installer/CTFontHookInstaller.vcxproj`：`<ResourceCompile Include="version.rc" />`。
- **验证**（实测）：
  - `scripts\1-一键测试.bat` `[1/4]`~`[4/4]` 全通过。
  - PowerShell 读取产物 `VersionInfo`：`CompanyName=HardMan_Official`、`ProductName=CTFontHook`、
    `FileVersion=0.1.0.0`，均已写入 EXE。
- **遗留**：未在真实高分屏环境下肉眼确认排版；`version.rc` 有意只用 ASCII（规避 rc.exe 源编码问题）。

---

## 2026-10-11　体验重构：应用按钮移入选中行 + 当前字体提示（任务 A）

- **任务**：把「应用」入口挪到选中行右侧，并把原预览区按钮位置改为「当前字体」提示（可点击跳回该字体）。
- **改动文件**（7 个，全部在 `src/ui/internal/`，无越界）：
  - `state/font_picker_layout_geometry.cppinc`：删 `GetPreviewApplyButtonRect()`；新增
    `GetListRowActionButtonRect(filteredIdx)`、`GetCurrentFontHintRect()`。
  - `state/font_picker_state.cppinc`：悬停状态改为 `g_rowApplyHovered` / `g_currentFontHintHovered`。
  - `input/font_picker_selection.cppinc`：新增 `FocusAppliedFontInList()`。
  - `font_picker_lifecycle.cppinc`：首次显示时改调 `UpdateFilter()` + `FocusAppliedFontInList()`。
  - `paint/font_picker_paint_font_list.cppinc`：选中且非当前字体时在行右侧绘制「应用」按钮（最后绘制压于名称之上）。
  - `paint/font_picker_paint_preview.cppinc`：删按钮块；预览区右下角绘制「当前字体：<字体名>」提示（未应用显示「（未应用）」）。
  - `input/font_picker_wndproc.cppinc`：`HandlePreviewApplyClick` → `HandleRowApplyClick`；新增
    `HandleCurrentFontHintClick`；接入单击分发链；`WM_MOUSEMOVE` / `WM_MOUSELEAVE` 拆分两个新悬停状态。
- **不变式**：单击列表仍只「选中」，双击 / Enter 仍可应用；已应用字体所在行不画「应用」按钮（显示「✓ 已应用」徽标）。
- **验证**（实测）：`scripts\1-一键测试.bat` `[1/4]`~`[4/4]` 全通过（32/64 位 Release 编译 + 导出校验 + 安装器构建）；
  `git status` 确认仅 7 个计划内文件被改。
- **遗留**：未做真机交互验证（按钮手感、点击提示跳转是否顺滑），待实机确认。

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

