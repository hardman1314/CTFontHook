# 用户实际使用全流程 · 现状图（USER_FLOW）

> 阶段 1 的产出物。目标是**只看用户看得见的部分**，把「一个普通玩家从装 DLL 到字体生效」
> 的完整链路画清楚，并列出卡点。所有结论都带源码位置，便于后续核对与复刻。
>
> **基线**：提交 `14c5795`（`v0.1.0` + 文档更新后）。
> **来源**：`src/ui/**`、`src/utils.cpp`、`docs/architecture.md`、`src/ui/README.md`。

---

## 1. 用户视角的全流程（现状）

```
① 准备   把对应位数的 winmm.dll 放进游戏目录（与 .exe 同级）
         （可选）把 .ttf/.otf/.ttc/.fon 也放进去，程序会自动加载为「本地字体」
   │
② 启动游戏   选择器线程启动，读取 FontHook.ini
   ├─ 首次（无 ini）        → 窗口自动弹出，停在「字体」页
   └─ 已有 ini              → 恢复上次的字体与排版；并「静默把字体重新应用到游戏」
   │
③ 呼出/收起   按 Insert（占用则 Pause，再占用则 Scroll Lock）；键无修饰符，按一下切换显隐
   │
④ 选择编码   点字符集药丸：默认 / 中文 / 日文 / 繁体 / 韩文
             ——必须按「游戏文本的编码」选，不能按界面语言猜
   │
⑤ 找字体     搜索框过滤 → 列表里点一下「选中」（仅预览，不生效）
   │
⑥ 应用       双击该行，或按 Enter   ← 这一步才真正生效
   │
⑦ 生效       写 FontHook.ini；递增 ConfigVersion；通知钩子 + 广播 WM_FONTCHANGE/重绘
   │
⑧ 微调排版   （可选）字宽/字高/字距/行距/粗细 等步进器，每次点击即时重绘并保存
   │
⑨ 回到游戏   字体即时变化；个别引擎若缓存了旧字体，需重进界面或重启游戏
```

## 2. 窗口里有什么（自上而下）

单一自绘顶层窗口，两个页面用 `Tab` 或点标题页签切换。

| 区块 | 用户能做什么 | 位置 |
| --- | --- | --- |
| 标题栏 | 切页（字体/指南）、A-/A+ 缩放界面、最小化、关闭（**只隐藏不退出**）、拖动窗口 | `paint/font_picker_paint_title.cppinc:1` |
| 搜索框 | 输入关键字过滤字体列表 | `lifecycle.cppinc:57` |
| 字符集药丸 | 默认/中文/日文/繁体/韩文；既过滤列表又决定强制字符集 | `state/font_picker_state.cppinc:66` |
| 伪装条 | 开/关 + 目标语言药丸（日→中→繁→默→韩） | `state/font_picker_state.cppinc:84` |
| 映射+转码条 | 映射开关/模式（日繁/繁>简/简>繁）、转码开关/预设（日文→UTF-8 等） | `state/font_picker_state.cppinc:106` |
| 字体列表 | 滚轮/上下键/PgUp/PgDn 浏览；**单击=选中**；**双击=应用**；带 `✓已应用` `☆最近` 徽标 | `paint/font_picker_paint_font_list.cppinc:41` |
| 排版条 | 字宽/字高/字距/上行/下行/行距/粗细 步进器；设参照/按参照计算/清空调整；点数值可直接输入 | `paint/font_picker_paint_metrics.cppinc:70` |
| 预览区 | 两行固定样例（中文/日文/数字）+ 目标/选择/序号信息 + `Enter=应用 Esc=隐藏` | `paint/font_picker_paint_preview.cppinc:61` |
| 指南页 | 四个章节：快速开始 / 功能说明 / 快捷键 / 问题处理 | `paint/font_picker_paint_guide.cppinc:192` |

## 3. 应用与持久化（机制）

- **应用入口**：`ApplySelectedFont()`（`ui/internal/font_picker_font_list.cppinc:711`）。
- **写配置**：`<winmm.dll 所在目录>\FontHook.ini`，UTF-8 **带 BOM**，`[FontHook]` 节，`key=value`（`src/utils.cpp:830`、`:924`）。
- **通知游戏**：`ConfigVersion++` → `FontHooks::NotifyConfigChanged(version)`（工作线程）+ 广播 `WM_FONTCHANGE` / `WM_SETTINGCHANGE "Font"` / `RedrawWindow`（`ui/internal/font_picker_apply_config.cppinc:11`、`:49`、`:83`）。
- **每次点击都保存**：排版/伪装/映射/转码的每次改动都会重绘 + 立即写盘（`apply_config.cppinc:133`、`:427`、`:476`、`:498`）。
- **`ConfigVersion` 不落盘**，是运行期版本号（`utils.cpp:92`）。
- **删除 `FontHook.ini`** 即恢复默认。

## 4. 痛点清单（按严重度排序）

> 判定标准：一个「完全不懂技术」的玩家，在上面第 ①–⑨ 步里会不会卡住、卡住了能不能自己出来。

### P0

| # | 痛点 | 证据 | 影响 |
| --- | --- | --- | --- |
| P0-1 | **单击只选中、不应用**，必须双击或按 Enter 才生效；指南自己也把它列为「问题处理」第一条 | 单击仅 `SetSelectedIndex`（`input/font_picker_wndproc.cppinc:582`）；双击才 `ApplySelectedFont()`（`:603`）；指南 `paint_guide.cppinc:162` | 玩家点一下发现「没反应」，会以为工具坏了 |
| P0-2 | **三个热键全被占用时，窗口一旦隐藏就再也打不开**：只有 `WM_HOTKEY` 能切换显隐，且此时显示名是「未注册」 | 注册循环首个成功即止（`lifecycle.cppinc:84`）；失败兜底「未注册」（`:83`）；切换仅热键路径（`wndproc.cppinc:624`） | 直接「用不了」，且无自救入口 |

### P1

| # | 痛点 | 证据 | 影响 |
| --- | --- | --- | --- |
| P1-1 | **必须自己判断游戏文本编码**（日文/中文/繁体/韩文），选错就乱码或不生效；说明只在「指南」里 | 药丸 `state.cppinc:66`；说明文本 `paint_guide.cppinc:39` | 小白最可能选错的地方，且没有智能推荐 |
| P1-2 | **应用后不提示「可能需要重启游戏」**；只有指南「问题处理」里提了一句 | 指南 `paint_guide.cppinc:188`；应用路径无任何重启提示 | 部分引擎缓存旧字体，玩家以为失败 |
| P1-3 | **关闭按钮是「隐藏」不是「退出」**，玩家会疑惑「关了怎么还在」 | `wndproc.cppinc:98`；指南 `paint_guide.cppinc:131` | 认知负担 |
| P1-4 | **保存失败对用户完全静默**（如装在 `Program Files` 无写权限），设置当下看着生效、重启就没了 | 仅 `Log()`（`utils.cpp:926`） | 「设置存不住」的幽灵问题 |

### P2

| # | 痛点 | 证据 |
| --- | --- | --- |
| P2-1 | 界面中英混杂：窗口标题 `Font Selection Assistant`、搜索框 `Search fonts...`、热键显示名英文、`Enter=应用 Esc=隐藏` | `lifecycle.cppinc:22`、`:61`、`:78`；`paint_preview.cppinc:99` |
| P2-2 | 预览只有两行固定样例，看不到**游戏自己的文字**效果 | `paint_preview.cppinc:61` |
| P2-3 | 排版数值单位不一致：字宽/字高是倍数（`1.00`），其余是整数；下行填正数会被静默取负 | `paint_metrics.cppinc:70`、`:88`；`metric_edit.cppinc:101` |
| P2-4 | 字符集药丸放不下时需**水平滚轮**横向滚动，无可见提示 | `wndproc.cppinc:332` |
| P2-5 | 应用成功没有明确反馈（无提示条），只有列表里一个 `✓已应用` 徽标 | `paint_font_list.cppinc:41` |
| P2-6 | `FontHook.ini` 每次全量重写，玩家手写的注释会被清掉 | `utils.cpp:925` |

### P3

| # | 痛点 | 证据 |
| --- | --- | --- |
| P3-1 | `PickerShowOnStartup`（是否开机自启窗口）只能在 ini 里改，界面没有开关 | 仅 `lifecycle.cppinc:221` 读取 |
| P3-2 | 独占全屏/DXGI 全屏游戏下选择器可能被遮挡，代码里无相关处理 | `src/ui/` 无 DXGI/独占相关逻辑 |
| P3-3 | 全部 UI 文案硬编码散落在 6+ 个 `.cppinc` 与 `utils.cpp`，改文案成本高（也是本地化障碍） | 见各 `paint_*` 分片与 `state.cppinc` |

## 5. 建议的第一刀

从「用户实际使用全流程」出发，**P0-1（单击即应用）** 是最短路径、最高收益的一刀：
它是玩家第一次上手就会撞上的墙，且改动集中在输入层与文案，风险可控。

**P0-2（热键兜底）** 次之：影响面更小，但属于「有它才敢用」的兜底，建议紧随其后。

> 具体方案需另立 `docs/DEV/features/` 功能档案，并走《两阶段流程》征得同意后再动代码。
