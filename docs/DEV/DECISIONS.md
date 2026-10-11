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
