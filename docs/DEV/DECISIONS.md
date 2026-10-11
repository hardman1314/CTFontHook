# 架构决策记录（DECISIONS / ADR）

> 记录会长期影响多个模块的设计取舍。已接受的决策不直接改写结论；如需变更，新增一条并注明取代关系。
> 模板：背景 / 决策 / 后果 / 备选方案 / 验证。

---

## ADR-0001：以独立仓库分支 SimpleFontHook，产品名 CTFontHook

- 状态：已接受
- 日期：2026-10-11
- 取代：无

### 背景
上游 SimpleFontHook 作者处于半隐退状态，项目需要持续演进（优先从用户实际使用全流程的体验开始重构）。
需要一个独立的代码与版本管理载体。

### 决策
- 在 `D:\CTLIB\FontHook\CTFontHook` 建立独立项目与独立 Git 仓库。
- 产品名称为 CTFontHook；解决方案/工程/源码目录同步改名。
- **最终产物 DLL 仍固定为 `winmm.dll`**，不得改名。

### 后果
- 收益：完全掌控演进节奏；Git 可留痕、可回退。
- 成本：需自行维护；上游若更新需手工合并。
- 风险：改名若遗漏引用会导致构建失败——已通过编译验证规避。

### 备选方案
- 直接在上游仓库改：会与上游历史纠缠，且外层的 Chrono Tide 仓库存在嵌套风险。未采用。

### 验证
- `scripts\1-一键测试.bat` 能编译出 `Release\winmm.dll` 与 `x64\Release\winmm.dll`，且代理导出完整。

---

## ADR-0002：产物命名策略

- 状态：已接受
- 日期：2026-10-11
- 取代：无

### 背景
DLL 通过「同名代理劫持」随目标游戏加载：游戏加载同目录下的 `winmm.dll`。

### 决策
- 工程 `TargetName` 保持 `winmm`；无论项目如何改名，产物名恒为 `winmm.dll`。
- `winmm_proxy.h` 中的导出转发保持原样。

### 后果
- 收益：劫持机制不受项目改名影响。
- 成本：产物名与项目名不一致，属预期行为，需在文档中反复说明。

### 验证
- `dumpbin /exports` 输出的导出名与系统 `winmm` 一致。

---

## ADR-0003：沿用聚合翻译单元（.cppinc）与 Detours org* 模式

- 状态：已接受（沿用上游）
- 日期：2026-10-11

### 背景
上游将钩子实现拆为 `.cppinc`，由聚合 `.cpp` 按固定顺序包含，使原始 API 指针与引擎状态保持内部链接。

### 决策
- 为降低风险与保持一致性，本次分支**不改变**该组织方式与 `org*` 调用模式。
- 任何新增/修改遵循 `CONTRIBUTING.md` 与上游 `AGENTS.md`。

### 后果
- 收益：改动面小，沿用既有工程约束。
- 成本：`.cppinc` 不能单独编译，需按聚合顺序维护。

---

## ADR-0004：源码内的运行时标识串保持 `SimpleFontHook`（不随产品改名）

- 状态：已接受
- 日期：2026-10-11
- 取代：无

### 背景
产品改名为 CTFontHook 后，源码里仍存在若干字面量 `SimpleFontHook`，例如：
`src/hooks/internal/engines/unity/unity_mono_window.cppinc:345` 的
`RegisterWindowMessageW(L"SimpleFontHook.UnityMono.ApplyFont.v1")`、
`src/hooks/internal/engines/tyrano/tyrano_bridge.cppinc:4` 的桥接标记、
`src/hooks/internal/engines/renpy/renpy_python_bridge.cppinc:102` 注入 Python 脚本的类名、
`src/hooks/internal/engines/catsystem2/catsystem2_font.cppinc:209` 探测的字体族候选名。

### 决策
- 这些字面量属于**运行时协议/行为标识**（窗口消息名、注入脚本标记、字体族候选名），
  **一律不改名**，保持与上游一致。
- 仅重命名「工程/项目/目录/产物封装」层面的名称，不触碰运行期协议。

### 后果
- 收益：避免改动引擎注入脚本与消息约定带来的行为回归，符合最小变更原则。
- 成本：源码中会同时出现 CTFontHook（工程名）与 SimpleFontHook（运行期标识），属预期，
  已在本文档说明以免后续误改。

### 备选方案
- 全局替换为 CTFontHook：会导致注入脚本、窗口消息约定变化，风险高且无收益，未采用。

### 验证
- `git grep -n "SimpleFontHook" -- src/` 的结果应全部落在上述运行期标识处。

---

## ADR-0005：部署自动化采用「独立 GUI 安装器」+ 备份/还原契约

- 状态：已接受
- 日期：2026-10-11
- 取代：无

### 背景
「让字体替换生效」原本完全依赖用户手工：判断游戏 EXE 架构（32/64 位）→ 选对应 `winmm.dll`
→ 备份游戏目录里已有的 `winmm.dll` / `FontHook.ini` → 复制 DLL → 回退时手工还原。
小白极易卡在「判断架构」这一步，且回退无保障。

### 决策
- 形态：做**独立 GUI 安装器 EXE**（C++ Win32，`installer/`），随发布包分发；**不引入**第三方安装框架
  （NSIS / Inno 等），不引入运行时依赖。
- 范围：**仅安装 / 卸载**（最小可用）；不做更新检查、批量管理。
- 安装器自身为 **32 位** EXE，产物名 `CTFontHook-Setup.exe`；**不加入 `CTFontHook.sln`**，
  由一键脚本按 `installer\CTFontHookInstaller.vcxproj` 路径直接构建。
- 架构判定以目标 EXE 的 **PE 头 `IMAGE_FILE_HEADER.Machine`** 为准（`0x014C`=32 位，`0x8664`=64 位）。
- **备份/还原契约**：所有中间产物集中在 `<游戏目录>\.CTFontHook\`：
  `backup\` 存原 `winmm.dll` / `FontHook.ini`，`manifest.ini` 记录 `HadWinmm` / `HadIni` / `DllArch`。
  卸载**只依据** `manifest.ini` 决定是否还原；用户删除 `.CTFontHook` 即视为放弃自动还原。

### 后果
- 收益：装 / 卸变成「双击 → 选游戏 EXE → 点安装」，无需用户懂架构；回退有据可依。
- 成本：发布包需保持「EXE 与 `32bit\`/`64bit\` 同目录」的结构，否则安装器找不到 DLL。
- 风险：需管理员权限（游戏常在 `Program Files`），已在工程开启 UAC `requireAdministrator`。

### 备选方案
- 用 `.bat` + PowerShell 脚本自动化：仍需用户手动选路径、且杀软/权限体验差，未采用。
- 第三方安装框架：引入体积与学习成本，超出「仅装 / 卸」的最小范围，未采用。

### 验证
- `scripts\1-一键测试.bat` 产出 `installer\bin\Win32\Release\CTFontHook-Setup.exe`（32 位、含 `requireAdministrator`）。
- `scripts\2-一键打包.bat` 产出的 zip 含安装器与双架构 DLL。
- 架构判定算法对 `System32` / `SysWOW64` `notepad.exe` 及本项目两种 `winmm.dll` 均判定正确。
- 详见 [`features/部署自动化.md`](features/部署自动化.md)。
