# 开发日志（DEV_LOG）

> 规则：每次改动完成后，在最上方追加一条。倒序排列（最新在上）。

---

## 2026-10-11　真机反馈修复：海报背景跟随窗口缩放

- **任务**：主人反馈「拖拽拉缩窗口，背景（海报）没跟随整体拉缩」——附两张大/小窗口截图。
- **现象**：拖大窗口后，列表区右侧/下方露出无海报的空白（海报仍是固定宽度，未铺满列表区）。
- **根因（已看代码确认）**：`src/ui/internal/paint/font_picker_paint_list_bg.cppinc` 的
  `DrawListPoster` 把缩放帧尺寸**写死为设计稿常量** `frameW = 470; frameH = 410;`：
  海报资源本身是 343×412，按 470×410 做 CROP 后恒得约 470×565 的固定输出尺寸，
  **与窗口/列表区实时尺寸无关** → 窗口放大后海报不跟着放大、露底。
- **改法（3 行，最小变更）**：同一函数内把帧尺寸改为**列表区实时尺寸**：
  ```cpp
  const int frameW = listRc.right - listRc.left;
  const int frameH = listRc.bottom - listRc.top;
  if (frameW <= 0 || frameH <= 0) return;
  ```
  CROP 覆盖算法（`s = max(sx, sy)`）与锚点（列表区左上）保持不变，故仅为「帧尺寸取实时值」。
- **改动文件**（1 个，在批准的计划内）：`src/ui/internal/paint/font_picker_paint_list_bg.cppinc`
  （`DrawListPoster`，5 增 3 删）。
- **明确不改**：`PaintFontList` 的裁剪区（`font_picker_paint_font_list.cppinc:34`，仍裁到列表带、
  传实时 `listRc`）；CROP 覆盖算法与左上锚点；海报资源 `src/res/list_bg.png`；行卡片/圆角等其余绘制。
- **验证**：`scripts\1-一键测试.bat` → **4/4 通过**（x64 Release / Win32 Release / 安装器 / 导出校验）；
  Win32 DLL `938496 → 590848`。
- **偏差 / 限制**：⚠️ **未做真机验证** —— 需主人拖动窗口缩放，确认海报跟随放大缩小且右侧不再露底。
  缩放语义为 **CROP 覆盖**（始终铺满列表区、可能裁掉海报边缘），锚点为**列表区左上**；
  若希望角色居中或改「完整显示不裁剪（CONTAIN）」，可只调锚点/缩放策略（`DrawListPoster` 内可调）。

---

## 2026-10-11　单文件安装包：安装器内嵌 DLL + 一键打包产出安装包

- **任务**：主人希望「发布的不是压缩包，而是安装包」，并把安装包制作并入一键打包。
- **背景（审计结论）**：`2-一键打包.bat` 早已编译安装器并放进 zip，但 `CTFontHook-Setup.exe`
  必须与同级 `32bit\` / `64bit\` 目录一起运行（旧 `main.cpp:528-531` 的
  「请在完整解压后的发布包内运行」），因此**无法作为单个文件发布**。
- **改动文件**（6 个，全部在批准的计划内）：
  - `installer/resource.h`：新增 `IDR_PAYLOAD_DLL32 3001` / `IDR_PAYLOAD_DLL64 3002`（>2048，避开控件 ID 区间）。
  - **新增** `installer/payload.rc`（UTF-8 BOM）：把 `..\Release\winmm.dll`、`..\x64\Release\winmm.dll`
    以 `RCDATA` 内嵌进安装器。
  - `installer/CTFontHookInstaller.vcxproj`：新增 `<ResourceCompile Include="payload.rc" />`。
  - `installer/main.cpp`：新增 `EmbeddedDllResource()` / `ExtractEmbeddedDll()`；`DoInstall()` 改为
    「同级目录优先 → 缺失则释放内嵌 DLL 到 `%TEMP%\CTFontHook_<pid>\`」，复制完成后删除临时文件与目录；
    顶部注释同步。
  - `scripts/2-一键打包.bat`（GBK+CRLF）：安装器编译改 `/t:Rebuild`（MSBuild 不追踪 .rc 对 DLL 的依赖）；
    第 5 步新增把单文件安装器复制为 `dist\CTFontHook-Setup-<VER>.exe`；结论与自我描述同步。
  - 文档：`docs/build-and-release.md`（新增「安装器与单文件安装包」+ 检查清单 2 条）、
    `docs/DEV/README.md`、`docs/DEV/SESSION_HANDOFF.md`、**新增** `docs/DEV/RELEASE_GUIDE.md`
    与 `docs/DEV/RELEASE_NOTES_TEMPLATE.md`。
  - 版本：`VERSION` → `0.1.1`、`CHANGELOG.md` 追加 `[0.1.1]`、
    `installer/version.rc` 版本号同步为 `0,1,1,0` / `0.1.1.0`。
- **明确不改**：`src\`（DLL 本体）、产物名 `winmm.dll`、安装/卸载/清单逻辑、
  `一键测试/备份/回退` 脚本、DLL 的 UPX 压缩方式。
- **验证**：
  - 单独 `msbuild installer\CTFontHookInstaller.vcxproj -t:Rebuild` → 退出码 0。
  - `scripts\1-一键测试.bat` → **4/4 通过**（含双架构导出表校验）。
  - `scripts\2-一键打包.bat` → 退出码 0，产出 `dist\CTFontHook-Setup-0.1.0.exe`（1,455,616 字节）。
  - `pefile` 枚举安装器资源：`RT_RCDATA id=3001 size=590848`、`id=3002 size=620032`（均以 `MZ` 开头），
    与 `Release/winmm.dll`、`x64/Release/winmm.dll` **逐字节相同**（True/True）；图标 `RT_GROUP_ICON id=2001` 仍在。
- **偏差 / 限制**：⚠️ **未做真机验证**——未实际运行单文件安装器完成「选游戏 → 安装 → 卸载还原」闭环，
  也未在**只有单个 exe、无 32bit/64bit 目录**的干净目录下实测释放路径；建议主人双击
  `dist\CTFontHook-Setup-0.1.0.exe` 验证一次。zip 体积因内含单文件安装器由约 1.35MB 增至约 2.54MB
  （同时保留 `32bit\`/`64bit\` 手动安装回退）。

---

## 2026-10-11　开源前准备：版权署名 / 第三方声明 / 换行锁定 / 忽略设计图

- **任务**：为新建 GitHub 公开仓库（`hardman1314/CTFontHook`，Public）做发布前清理。
- **改动文件**（6 个，全部在批准的计划内，均为仓库门面文件）：
  - `LICENSE.txt:3`：版权行 `Copyright (c) 2026 CTFontHook` → `Copyright (c) 2026 hardman1314`（MIT 需真实著作权人）。
  - **新增 `.gitattributes`**：仅声明 `*.bat`/`*.cmd` 为 `text eol=crlf`、二进制资源为 `binary`；
    **未使用 `* text=auto`**（本机 `core.autocrlf=true`、blob 已按 LF 存储，`text=auto` 会重写全部被跟踪文件行尾）。
  - **新增 `THIRD-PARTY-NOTICES.md`**：登记上游 SimpleFontHook（MIT）、Detours 4.0.1（MIT）、
    UPX 5.1.1（GPLv2+ 特殊例外）。
  - `.gitignore`：追加 `/设计图/`（Figma 导出，仅忽略不删除，本地内容保留）。
  - `README.md`：末尾新增「许可证与第三方」小节。
  - 本文件：追加本条记录。
- **明确不改**：所有源码 / 脚本 / 工程文件 / 构建配置；`设计图/` 本地内容；
  `tools/upx/` 与 `detours*.lib`（构建必需，保留在仓库，仅补许可声明）。
- **验证**：`git status --short` 确认改动面仅上述文件；`git diff --stat` 复核行数。
- **偏差 / 限制**：⚠️ 未做真机验证（本次仅文档/配置改动，不涉及编译产物）。

---

## 2026-10-11　修复安装器窗口图标显示为「系统默认图标」（rc.exe 编码陷阱）

- **任务**：主人反馈「程序的图标完全没更新为 `设计图/ico`」。实测现象：安装器**窗口标题栏 / 任务栏**
  图标是系统默认图标；而 EXE 的**桌面 / 资源管理器文件图标**其实是正确的设计图。
- **根因（已证实）**：`installer/resource.h:22` 的中文注释 `// 应用图标（…共用）。` 结尾 `。`
  的 UTF-8 字节为 `E3 80 82`；`rc.exe` **按 ANSI/GBK 读源文件**，其中 `0x82` 被当作 GBK 前导字节、
  把紧随的 `LF` 当后尾字节吃掉 → 该注释**吞掉下一行的 `#define IDI_APPICON 2001`** → 宏未定义 →
  `installer/version.rc:9` 的 `IDI_APPICON ICON "app.ico"` 被 RC 当成**资源名字符串**，
  图标组名成了字符串 `"IDI_APPICON"` 而非整数 `2001` → `installer/main.cpp:1260` 的
  `LoadImageW(hInstance, MAKEINTRESOURCEW(2001), ...)` 找不到资源、返回 NULL →
  `wc.hIcon/hIconSm = NULL`（`main.cpp:1272-1273`）→ Windows 画**默认图标**。
  - 为何编译期没暴露：工程只给 **cl.exe** 传了 `/utf-8`（`installer/CTFontHookInstaller.vcxproj:92`），
    **`ResourceCompile` 没有对应选项** —— 同一份中文注释在 C++ 里正常、在 RC 里被吞。
  - 证据：`pefile` dump 安装器 EXE → `RT_GROUP_ICON` 名为字符串 `4944495f41505049434f4e`（ASCII `IDI_APPICON`）；
    对照 `Release/winmm.dll` 为 `id=2001`。用 GBK 逐字节模拟 `resource.h`，第 1–2 行与第 22–23 行确实粘连。
  - 对照实验（真机 `rc.exe` 10.0.26100.0，同一份头文件）：无 BOM → 字符串名（复现缺陷）；
    **UTF-8 BOM → 整数 2001**；`/c65001` → 整数 2001。
- **改动文件**（3 个，**仅文件头插入 UTF-8 BOM，内容一字不改**，全部在批准的计划内）：
  - `installer/resource.h`（根因所在）
  - `installer/version.rc`
  - `src/CTFontHook.rc`（**同源隐患预防**：该文件当前用字面量 `2001`/`101` 未触发，
    但其中文注释同样有吞行风险，一旦将来改动就会静默弄坏选择器图标）
- **明确不改**：`installer/app.ico` 与 `src/res/app.ico`（逐帧校验 = `设计图/ico` 的 7 帧拼接，
  美术本来就是对的）；`installer/main.cpp` 的 `LoadImageW` 调用（代码正确）；`version.rc` 的资源 ID
  与其余内容；**所有 `.vcxproj`**（备选 `/c65001` 需改构建配置，未采用）。
- **验证**：
  - `scripts\1-一键测试.bat` → **4/4 通过**；Win32 DLL `938496 → 590848`。
  - `pefile` dump `installer/bin/Win32/Release/CTFontHook-Setup.exe` → **`RT_GROUP_ICON id=2001`**
    （不再是字符串名）；`Release/winmm.dll`、`x64/Release/winmm.dll` 仍为 `id=2001` + `RT_RCDATA id=101`。
  - 复跑安装器实际调用路径：`LoadLibraryExW` + `LoadImage(module, MAKEINTRESOURCE(2001), IMAGE_ICON, sz, sz)`
    在 16 / 32 / 48 三个尺寸下**均返回非 0 句柄**（修复前为 0、`err=1813`）。
- **偏差 / 限制**：⚠️ **未做真机验证** —— 需主人运行新打包的 `CTFontHook-Setup.exe` 确认
  标题栏 / 任务栏图标为设计图（若仍显示旧图标，多半是 Windows 图标缓存，换个目录运行即可排除）。
  **回归风险提示**：RC 参与的文件（`.rc` 及其 `include` 的头文件）**必须保留 UTF-8 BOM**，
  否则中文注释会再次静默吞行。

---

## 2026-10-11　P2-6：保存配置时保留 FontHook.ini 的手写注释

- **任务**：修 P2-6 —— 玩家在 `FontHook.ini` 里手写的注释（`;`/`#`）、空行、未知键，
  每次「应用字体 / 切换开关 / 保存」时都会被清掉。
- **根因**：`Utils::SaveConfig` 每次从 `Config::*` 重新拼接全文，再用 `CREATE_ALWAYS` 覆盖写
  （旧 `utils.cpp:928`），原文件里的非受管内容随之丢失。全项目只有 `SaveConfig` 一处写该文件。
- **改动文件**（仅 1 个，全部在批准的计划内）：
  - `src/utils.cpp`：
    - 新增 `ReadIniTextFile(iniPath)`（`utils.cpp:832`）：读回现有 ini 原文（去 UTF-8 BOM）；不存在/读失败返回空串。
    - 新增 `IniLineKey(line)`（`utils.cpp:858`）：取 `Key=Value` 的键名；注释/空行/节头返回空串（与 `ParseIni` 同规则）。
    - 改造 `SaveConfig` 写入段（`utils.cpp:958` 起）：把受管键解析成 `(key, 完整行)` 列表后与旧文件**合并**——
      命中受管键就地替换（仅首次），注释/空行/未知键/节头**原样保留**，旧文件缺少的受管键补到末尾；
      重复出现的受管键行丢弃（保证写回的新值生效）。旧文件不存在时输出与既有实现**逐字节一致**。
- **明确不改**：`LoadConfig` / `ParseIni` / `FindIniValue` / `AppendIni*` / `GetIniPath` 及读取语义；
  ini 键名、值格式、UTF-8 BOM、CRLF 换行；其余任何文件。
- **验证**：`scripts\1-一键测试.bat` → **4/4 通过**（x64/Win32 DLL + 安装器 + 32/64 导出校验）。
  逻辑核对：无注释文件走 `merged = out` 分支，与旧实现逐字节一致；带注释样例按「行序保留 + 受管键替换」推演正确。
- **偏差 / 限制**：⚠️ **未做真机验证** —— 需进游戏触发一次保存，再打开该游戏的 `FontHook.ini` 确认注释仍在。
  已知边界：受管键若写成「行内注释」（`Key=Value ; note`），整体会被替换、行内注释不保留
  （`ParseIni` 本就不支持行内注释，不影响读取语义）。

---

## 2026-10-11　按游戏「打开游戏时默认隐藏窗口」勾选框 + 顶部热键提示

- **任务**：在字体选择器窗口放一个勾选框——勾选后**只对该游戏**下次启动默认隐藏窗口（其他游戏照常弹窗），
  可热键呼出，并在顶部栏给出「呼出窗口」的快捷键提示。
- **关键结论（无需新增存储）**：`Config::PickerShowOnStartup`（默认 true）早已存在，启动时若为 false
  即隐藏（`font_picker_lifecycle.cppinc:239`）；而 `FontHook.ini` 放在 **DLL 同目录、每游戏一份**
  （`utils.cpp:738`），故该开关**天然只对该游戏生效**。热键呼出亦已具备（Insert/Pause/Scroll Lock）。
  本功能增量 = 一个勾选框 UI + 一处顶部栏提示。
- **改动文件**（6 改 + 1 新建，全部在计划内）：
  - `src/ui/internal/state/font_picker_state.cppinc`：新增 `g_hideOnStartupHovered` 与文案常量 `HIDE_ON_STARTUP_LABEL`。
  - `src/ui/internal/state/font_picker_layout_geometry.cppinc`：新增 `GetBottomBarHideToggleRect()`（右对齐、按文案实测宽度）。
  - `src/ui/internal/paint/font_picker_paint_status.cppinc`：底栏右侧改绘勾选框（方框+✓，悬停变亮）；
    「当前方案」右边界收缩到勾选框左侧以免重叠；原「界面预览」文案移除。
  - `src/ui/internal/paint/font_picker_paint_title.cppinc`：药丸右侧绘制「呼出窗口：<热键>」；
    无热键时显示警示色「无热键·关闭=最小化」（原底栏 P0-2 提示上移）。
  - `src/ui/internal/input/font_picker_wndproc.cppinc`：新增 `HandleHideOnStartupToggleClick()`（切换并 `SavePickerConfig()`）、
    hover 追踪、`WM_MOUSELEAVE` 复位、`WM_LBUTTONDOWN` 派发。
  - `docs/DEV/features/按游戏默认隐藏窗口.md`（**新建**）：本功能档案。
- **明确不改**：`FontHook.ini` 键名/格式（沿用既有 `PickerShowOnStartup`）、热键注册/协议、注入标记、
  `winmm.dll` 名与导出表、列表单击只选中 / 预览 A± 不落盘等既有交互。
- **验证**：`scripts\1-一键测试.bat` → **4/4 通过**；Win32 `935936 → 588800`。
  逻辑核对：`AppendIniText` 空值也会写键（`utils.cpp:796`）→ 「只勾选、未应用字体」的游戏也能持久化生效。
  坐标估算（默认缩放 1.25 / 窗口 512）：勾选框右对齐止于 496、起约 351；「当前方案」右界 341，不重叠。
- **偏差 / 限制**：
  - **真机验证未做**（本机无 GUI）：勾选框观感/点击、顶部热键提示、以及「勾选→重启游戏默认隐藏→热键呼出」
    的端到端行为需主人真机确认。
  - 勾选仅在「字体」页显示（「指南」页有独立页脚，不绘底栏）。
  - **附带发现（只报告）**：原底栏「界面预览」在 Figma 导出里其实是**文本图层名**（`设计图/CT字体器.json`
    `textContent[78]`），语义可疑；本次按批准以勾选框替换该位置。

---

## 2026-10-11　选择器窗口优化（图标 / 标题 / 面积 / 页脚 / 缩放控件 / 整体缩放 / 定位）

- **任务**：主人真机使用字体选择器后提出 7 条体验问题，一次性修复：
  ① 窗口/任务栏图标沿用旧图标；② 顶部「指南」药丸与标题重合；③ 列表区偏高、预览区偏矮；
  ④ 预览框底部「游戏/选择/序号」信息与样例文字重叠；⑤ 预览标题右侧 A−/数值/A+ 控件重叠、文字被裁；
  ⑥ 拖拽缩放只拉大外框、内容不缩放；⑦ 底栏「当前方案：<字体>」不可点击定位。
- **改动文件**（7 改 + 2 新建，全部在计划内）：
  - `src/res/app.ico`（**新建**）：复制自 `installer/app.ico`（7 尺寸多帧 ICO）。
  - `src/CTFontHook.rc`：追加 `2001 ICON "res\\app.ico"`。
  - `src/ui/internal/state/font_picker_state.cppinc`：新增 `APP_ICON_RESOURCE_ID=2001`、
    `g_bottomAppliedHovered`、设计基准 `DESIGN_BASE_WIDTH/HEIGHT=410/724`、最小 `340×600`；
    `PREVIEW_HEIGHT 136→164`。
  - `src/ui/internal/state/font_picker_layout_geometry.cppinc`：`GetHeaderPillRect` 改 SF 化并右移；
    新增 `GetBottomBarAppliedRect`；A−/Value/A+ 三矩形 SF 化重排。
  - `src/ui/internal/font_picker_lifecycle.cppinc`：`wc.hIcon/hIconSm` 载入 + 创建后补 `WM_SETICON`；
    初始尺寸改为「按 410×724 等比夹取到可用屏」。
  - `src/ui/internal/input/font_picker_wndproc.cppinc`：新增 `HandleBottomAppliedClick`、
    `SizeWindowForUiScale`；新增 `WM_SIZING` 锁定 410:724；`WM_SIZE` 反推 `g_uiScale`；
    Ctrl+滚轮改缩放后同步窗口大小；底栏悬停态与点击派发接线。
  - `src/ui/internal/paint/font_picker_paint_preview.cppinc`：预览框内信息块删除，
    迁至页脚行右侧「动态剩余宽度」绘制，并预留右侧「当前字体」提示宽度。
  - `src/ui/internal/paint/font_picker_paint_status.cppinc`：底栏「当前方案」悬停变强调色。
  - `docs/DEV/features/选择器窗口优化-图标与布局.md`（**新建**）：本批档案。
- **明确不改**：列表单击仍只选中（P0-1 交互）、预览 A± 仍不落盘、各配置键/注入/热键协议/导出表零改动。
- **验证**：`scripts\1-一键测试.bat` → **4/4 通过**（x64 / Win32 DLL + 安装器 + 双架构导出校验）；
  Win32 `933376 → 589824`（+≈99KB 系内嵌 ICO）。坐标核算：药丸 `135..188` vs 标题止于 `125`；
  A−/Value/A+ = `408..433`/`435..470`/`473..500`；默认缩放 1.25 下列表可见 5 行。
- **偏差 / 限制**：
  - 缩放采用 **reduced model**：`SF()` 内容随缩放变化，各定高区仍为设计稿原始像素，
    列表区吸收剩余高度。**未采用「全量等比」**——那会让列表从 5 行掉到约 3 行，与主人「稍微减小列表」相悖。
  - **真机验证未做**：本机为无 GUI 构建环境，7 项观感/手感均需主人真机确认。
  - 可调常量集中在 `font_picker_state.cppinc` 顶部（尺寸/比例）与 `layout_geometry` 的矩形定义。

---

## 2026-10-11　安装器窗口「浅色主题复刻 + 中文名 + 图标」

- **任务**：优化 CTFontHook-Setup 安装器窗口——① 提升易用性；② 按字体选择器的设计语言改为**浅色主题**
  （与选择器深色主题相反）；③ 应用中文名「时汐字体器」并接入 `设计图/ico` 的图标作为窗口 / 桌面图标。
- **改动文件**（3 改 + 1 新建，全部在计划内）：
  - `installer/app.ico`（**新建**）：把 `设计图/ico/时汐字体器_{16,24,32,48,64,128,256}.ico` 合并为单文件多尺寸
    ICO（原样保留各尺寸 PNG 位图，未重采样）。因 `设计图/` 未纳入版本控制，必须复制进仓库以保证构建可复现。
  - `installer/resource.h`：新增 `IDI_APPICON 2001`。
  - `installer/version.rc`：新增 `IDI_APPICON ICON "app.ico"`（EXE 文件图标 + 窗口图标）；仍保持全 ASCII。
  - `installer/main.cpp`（主要改动）：浅色调色板 + 圆角卡片 / 强调色；按钮改 `BS_OWNERDRAW` 自绘
    （悬停 / 按下 / 焦点态，主操作「安装到游戏」走主色、卸载 / 还原走警示色）；`WM_PAINT` 自绘窗口底 /
    顶部强调条 / 应用图标 / 副标题 / 列表与日志卡片；`WM_CTLCOLORSTATIC` 统一静态与只读输入框配色；
    `NM_CUSTOMDRAW` 给列表行与表头着色（已安装=绿）；新增列表双击=打开目录、行 tooltip、
    `WS_TABSTOP` + `IsDialogMessage` 键盘导航；标题改「时汐字体器 · 安装器」。
- **明确不改（功能一个不丢）**：`DoInstall` / `DoUninstallForDir` / 清单读写 / PE 架构检测 / 全部按钮回调
  **逻辑一行未动**；`games.ini` 与 `manifest.ini` 数据格式不变；`src/**`（DLL 与选择器）零改动；
  EXE 文件名仍为 `CTFontHook-Setup.exe`。
- **验证**：`scripts\1-一键测试.bat` → **4/4 通过**（x64 Release / Win32 Release / 安装器 / 导出校验）；
  PowerShell 校验 `CTFontHook-Setup.exe` 图标嵌入成功（`ExtractAssociatedIcon` = 32×32）。
- **偏差 / 限制**：
  - 安装器为**原生 GDI 窗口**，本机**无法可视化验证**观感 → 浅色主题观感与自绘细节需主人**真机确认**。
  - 配色与版式为可调常量（`main.cpp` 顶部 `COL_*` / `LY_*`），真机观感后可微调。
  - `version.rc` 的 `ProductName` 仍为 ASCII 的 `CTFontHook`（rc.exe 编码约束，塞中文有乱码风险）；
    界面可见中文名由**窗口标题**提供。

## 2026-10-11　界面优化「整体放大 + 兼容设置下拉菜单」（真机反馈）

- **任务**：主人真机反馈两点——① 窗口太小、内容看不清；②「兼容设置」里开关右侧的 ▾ 药丸
  应是**点击弹出的下拉菜单**（原为点一下循环切换），且菜单**不影响其他内容组件**。
- **做法（按批准方案，1.25×）**：
  - **整体放大**：默认 `g_uiScale` 1.0 → **1.25**；默认窗口 410×724 → **512×905**；
    最小窗口 380×660 → **475×825**；建窗时把初始尺寸夹到不超过屏幕可视区（小屏保护）。
  - **下拉菜单**：新增 `PickerPopupKind` 状态 + 弹出面板绘制分片；药丸矩形抽成
    「单一来源」helper（`GetSpoofCharsetPillRect` / `GetTextSubModePillRect` / `GetCodepagePresetPillRect`），
    绘制、命中、菜单锚点**共用**；菜单在 `PaintWindow` **最后叠加绘制**（只覆盖、不改动其他组件）；
    点击条目=选择、点其他=关闭并吞掉该次点击（Windows 菜单惯例）；Esc / 鼠标离开 / 切页 / 隐藏 均关闭。
- **改动文件**（10 改 + 1 新建，全部在计划内）：
  - `state/font_picker_state.cppinc`：默认缩放 / 窗口 / 最小尺寸；新增弹出菜单状态。
  - `font_picker_lifecycle.cppinc`：建窗尺寸夹取屏幕。
  - `state/font_picker_layout_geometry.cppinc`：药丸矩形 helper + 重构 `SpoofBarHitTest` /
    `TextSubstitutionHitTest` 复用；新增菜单面板/条目/命中 helper。
  - `paint/font_picker_paint_popup.cppinc`（**新建**）：`PaintPopupMenu`（面板 + 逐项 + 选中/悬停态）。
  - `paint/font_picker_paint_spoof.cppinc`、`paint/font_picker_paint_text_substitution.cppinc`：药丸改用 helper 矩形。
  - `font_picker_paint.cppinc`：挂接弹出分片并在 `PaintWindow` 末尾叠加绘制。
  - `input/font_picker_wndproc.cppinc`：弹出菜单交互（打开/关闭/选择/悬停/Esc）。
  - `input/font_picker_guide_input.cppinc`、`state/font_picker_window_animation.cppinc`：切页/隐藏时收起菜单。
  - `src/CTFontHook.vcxproj`：登记新分片。
- **验证**：`scripts\1-一键测试.bat` → **4/4 通过**（x64 Release / Win32 Release / 安装器 / 导出校验），
  `x64\Release\winmm.dll` 518656 字节、`Release\winmm.dll` 488960 字节。
- **遗留**：**观感需主人真机验证**（原生 GDI 窗口，本机无法可视化验证）——放大倍率可用 **Ctrl+滚轮**微调；
  菜单面板叠加会**视觉覆盖**下方组件（下拉菜单固有），但不改动任何组件。
  详见 [`features/界面重构-设计稿复刻.md`](features/界面重构-设计稿复刻.md)。

## 2026-10-11　界面重构 批次3「海报背景图接入」（设计稿复刻）

- **任务**：把设计稿里的角色海报（`设计图/Text.png`，343×412 RGBA，自身最大不透明度仅 ~27%）
  作为**列表区背景图**接入；行卡片改为**半透明底板**，让海报透出（贴合设计稿观感）；
  资源缺失 / 解码失败时回退批次 1 的纯渐变。
- **改动文件**（6 改 + 3 新建，全部在计划内）：
  - `src/res/list_bg.png`（**新建**）：海报资源（由 `设计图/Text.png` 复制，随 DLL 内嵌）。
  - `src/CTFontHook.rc`（**新建**）：`101 RCDATA "res\\list_bg.png"`。
  - `paint/font_picker_paint_list_bg.cppinc`（**新建**）：WIC 解码内嵌 PNG（预乘 32bpp DIB）+
    行卡片半透明底板生成（圆角 + 横向渐变 + 预乘 alpha）+ `AlphaBlend` 合成 + 释放。
  - `state/font_picker_state.cppinc`：新增海报 / 行卡片位图全局状态。
  - `paint/font_picker_paint_font_list.cppinc`：绘制海报；行底板改半透明混合（无位图时回退渐变）。
  - `font_picker.cpp`：`#include <wincodec.h>` + `#pragma comment(lib, "windowscodecs.lib")`。
  - `font_picker_paint.cppinc`：挂接新分片（在字体列表之前）。
  - `input/font_picker_wndproc.cppinc`：`WM_DESTROY` 释放海报 / 行卡片缓存。
  - `src/CTFontHook.vcxproj`：新增 `<ResourceCompile>` 与两个新分片登记。
- **做法**：WIC 把内嵌 PNG 解码为预乘 32bpp 顶朝下 DIB → `AlphaBlend` 缩放合成；
  3 张缓存行底板（常态 / 悬停 / 选中，按卡片尺寸重建）逐行 `AlphaBlend`，让海报从行底下透出；
  圆角用 1px 软边遮罩。
- **关键坑**：`FindResource` 必须传选择器自己的 `g_hModule`；传 `NULL` 会指向宿主 EXE 而非本 DLL。
- **验证**：`scripts\1-一键测试.bat` → **4/4 通过**（x64 Release / Win32 Release / 安装器 / 导出校验），
  `x64\Release\winmm.dll` 517120 字节、`Release\winmm.dll` 487424 字节；
  `src\*\Release\CTFontHook.res` = 225324 字节，确认资源已编入。
- **遗留**：海报缩放/锚点取自 `Text` 帧（470×410，CROP）+ 列表左上锚点，为近似值；
  **观感需主人真机验证**后微调（`frameW/frameH` 与行底板透明度为可调常量）。
  详见 [`features/界面重构-设计稿复刻.md`](features/界面重构-设计稿复刻.md)。

## 2026-10-11　界面重构 批次2「兼容设置区合并重排」（设计稿复刻）

- **任务**：把 伪装 + 映射/转码 + 字形微调 合并为**一个「兼容设置」区**，按设计稿几何重排
  （区高 225，左右内边距 13/13，行高 23，行间距 5）。**外观按设计、功能一个不丢**。
- **设计行几何**（自区顶向下）：标题行 +8；伪装行 +30；映射/转码行 +58；字形微调子标题行 +88；
  字号网格 +118 / +146；粗细行 +176；目标/参照行 +205。
- **改动文件**（8 改 + 1 新建，全部在计划内）：
  - `state/font_picker_layout_geometry.cppinc`：新增 `COMPAT_ROW_H/MARGIN_L/MARGIN_R/COL_GAP`
    与 `GetCompat*RowTop()` 系列；`GetSpoofAreaRect`/`GetTextSubAreaRect`/`GetMetricsAreaRect`
    改指向新行；`SpoofBarHitTest`（行 1）与 `TextSubstitutionHitTest`（行 2，映射/转码两组）
    按新行重写（9px、药丸预留 `▾` 宽度）。
  - `state/font_picker_metric_layout.cppinc`：`GetMetricsRects` 重写为三列×两行网格；
    粗细行改为标签 + 数值框（无 −/+，贴合设计稿）+ 设为参照 + 按参照计算；命中纵界改兼容设置区。
  - `paint/font_picker_paint_compat.cppinc`（**新建**）：`DrawSlidersIcon` + `PaintCompatHeader`
    （标题行「兼容设置 / COMPATIBILITY」+ 字形微调子标题行「A / 字形微调 / ↺ 清空调整」）。
  - `paint/font_picker_paint_spoof.cppinc`：行 1 重绘（伪装开关 + 目标字符集下拉 + 右侧状态提示）。
  - `paint/font_picker_paint_text_substitution.cppinc`：行 2 重绘（映射组 + 转码组）。
  - `paint/font_picker_paint_metrics.cppinc`：按新几何绘制网格与粗细行；**移除重复的「清空调整」**
    （改由 `PaintCompatHeader` 绘制）；目标/参照行改为整行纯文本（左「目标」右「参照」，无边框）。
  - `font_picker_paint.cppinc`：挂接 `PaintCompatHeader`。
  - `input/font_picker_wndproc.cppinc`：`HandleMetricClick` 纵界改兼容设置区；
    伪装/文本替换悬停门限改新行；滚轮纵界改兼容设置区。
  - `src/CTFontHook.vcxproj`：登记 `font_picker_paint_compat.cppinc`。
- **验证**：`scripts\1-一键测试.bat` → **4/4 通过**（x64 Release / Win32 Release / 安装器 / 导出校验），
  `x64\Release\winmm.dll` 286720 字节、`Release\winmm.dll` 259584 字节。
- **偏差说明（保留功能优先）**：设计稿「粗细」为数值框（无 −/+），此处按设计外观实现，
  数值仍可点击行内编辑（权重步进按钮取消，功能以输入方式保留）。
- **遗留**：批次 3（海报背景图 RCDATA+WIC）待做；**观感需主人真机验证**。
  详见 [`features/界面重构-设计稿复刻.md`](features/界面重构-设计稿复刻.md)。

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

