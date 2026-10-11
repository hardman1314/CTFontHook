# GitHub Release 发布文档（时汐字体器 · CTFontHook）

> 面向**不熟悉命令行**的使用者/维护者：如何把 `一键打包` 产出的**单文件安装包**发布到
> GitHub Releases，供玩家直接下载。
>
> 仓库地址：<https://github.com/hardman1314/CTFontHook>

---

## 0. 一句话结论

**先双击 `scripts\2-一键打包.bat` 得到 `dist\CTFontHook-Setup-<版本>.exe`，
再到 <https://github.com/hardman1314/CTFontHook/releases/new> 填写并上传这个文件即可。**

---

## 1. 前置：先打包

1. 双击 `scripts\2-一键打包.bat`（会自动编译 32/64 位 DLL + 安装器，并加壳压缩）。
2. 结束后 `dist\` 目录里会有：

| 产物 | 说明 | 用途 |
| --- | --- | --- |
| `CTFontHook-Setup-<版本>.exe` | **单文件安装包**（已内嵌 32/64 位 winmm.dll） | **发布这个**，玩家双击即装 |
| `CTFontHook-<版本>-<时间>.zip` | 完整发布包（安装器 + `32bit\`/`64bit\` DLL + 说明） | 备用 / 手动安装 |
| `版本.txt`、`使用说明.txt` | 已打包进 zip | 供玩家查看 |

> 单文件安装包不需要附带任何其它文件，可以直接单独上传。

---

## 2. 先搞懂两个词：tag 与 Release

- **tag（标签）**：Git 里的一个「时间点标记」，例如 `v0.1.0`。它标记代码的某个快照。
- **Release（发布）**：GitHub 上以某个 tag 为基准的「发布页」，可以**附带下载文件**（附件）
  并写发布说明。玩家看到的就是 Release 页。

关系：**一个 Release 挂在一个 tag 上**；创建 Release 时可以新建 tag，也可以选择已存在的 tag。
GitHub 还会给每个 Release **自动附带**源码 `zip`/`tar.gz`（不用手动传）。

---

## 3. 方式 A：网页发布（推荐，无需命令行）

1. 打开 <https://github.com/hardman1314/CTFontHook/releases/new>
   （或仓库首页右侧 **Releases → Create a new release**）。
2. **Choose a tag**：
   - 若已有合适的 tag（如 `v0.1.0`），在下拉里选它；
   - 若要为新版本打tag，直接在输入框输入例如 `v0.1.1`，然后点 **Create new tag on publish**
     （分支选 `master`）。
3. **Release title**（标题）：填给玩家看的名字，例如
   `时汐字体器 v0.1.1`（纯英文可写 `CTFontHook v0.1.1`）。
4. **Describe this release**（说明）：粘贴第 6 节的模板，按需改写。
5. **Attach binaries**（附件）：把 `dist\CTFontHook-Setup-<版本>.exe` **拖进**页面上的
   “Attach binaries by dropping them here” 区域。
   - 如需一并提供完整包，也可把 `dist\CTFontHook-<版本>-<时间>.zip` 一起拖进去。
   - 单个附件上限 **2 GB**；本项目安装包约 1.4 MB，无需担心。
6. 若是首次公开、希望它显示为「最新版本」，保持勾选 **Set as the latest release**。
7. 点 **Publish release**。

完成后，玩家在仓库首页右侧就能看到并下载安装包。

---

## 4. 方式 B：命令行 `gh`（可选）

本机当前**未安装** `gh`。若你以后想用命令行发布：

```bat
winget install --id GitHub.cli
gh auth login
```

然后在仓库根目录执行（示例：tag `v0.1.1`，附件为单文件安装包）：

```bat
gh release create v0.1.1 ^
  "dist\CTFontHook-Setup-0.1.1.exe" ^
  --title "时汐字体器 v0.1.1" ^
  --notes-file docs\DEV\RELEASE_NOTES_TEMPLATE.md
```

`gh release create` 会自动为不存在的 tag 创建 tag 并推送到 GitHub。

---

## 5. 方式 C：用 API + Token（可让 AI 代做）

需要在 GitHub 生成一个 **Personal Access Token**（Settings → Developer settings →
Personal access tokens → Fine-grained tokens，权限勾 **Contents: Read and write**），
再把 token 交给 AI，由 AI 用 API 上传附件。**注意**：

- Token 等同于密码，**不要**写进仓库文件或提交。
- 用完可随时在 GitHub 上撤销该 token。
- 未获得你的明确同意前，AI 不会索取或使用 token。

---

## 6. Release 说明模板（可直接复制修改）

```markdown
## 时汐字体器 v0.1.1

给 Windows 游戏替换字体的工具：双击安装 → 选择游戏主程序 → 自动识别 32/64 位。

### 下载
- **安装包**：`CTFontHook-Setup-0.1.1.exe`（单文件，已内嵌 32/64 位 DLL，双击即可安装）

### 本次更新
- 修复：……
- 新增：……
- 优化：……

### 使用方法
1. 双击 `CTFontHook-Setup-0.1.1.exe`，若弹出「用户账户控制」请点「是」。
2. 点「浏览」选中游戏主程序（.exe），点「安装」。
3. 启动游戏，按 `Insert` 呼出字体选择器（无反应时依次试 `Pause`、`Scroll Lock`）。

### 卸载 / 还原
再次运行安装器，点「卸载 / 还原」，会自动恢复游戏原有的 `winmm.dll`。

### 说明
- 产物文件名固定为 `winmm.dll`（代理劫持机制要求）。
- 安装器会备份游戏目录原有 `winmm.dll` 到游戏目录下的 `.CTFontHook\backup\`。
- 本软件以 MIT 许可发布；第三方组件见仓库 `THIRD-PARTY-NOTICES.md`。
- 请在使用前阅读并遵守 `README.md` 中的免责声明。
```

> 上面的模板已另存为 [`RELEASE_NOTES_TEMPLATE.md`](RELEASE_NOTES_TEMPLATE.md)，
> 供 `gh release create` 的 `--notes-file` 直接引用。

---

## 7. 版本号与命名约定

- 版本号写在仓库根的 `VERSION` 文件（当前 `0.1.0`），与 `CHANGELOG.md` 对应。
- 版本号规则（见 `docs/DEV/README.md`）：主版本=不兼容大改 / 次版本=新增功能 / 修订号=修复与小优化。
- tag 统一用 `v` 前缀：`v0.1.0`、`v0.1.1`……
- 安装包附件名由打包脚本自动生成为 `CTFontHook-Setup-<VERSION>.exe`。

> ⚠️ **注意**：当前 `master` 上的代码已经**领先**已推送的 tag `v0.1.0`（开源准备 + 单文件安装包改动
> 均在 `v0.1.0` 之后）。若要让「Release 的 tag」与「安装包对应的代码」一致，建议：
> 先把 `VERSION` 改为 `0.1.1` 并在 `CHANGELOG.md` 记一条，重新打包，再以 `v0.1.1` 发布。

---

## 8. 常见问题

| 问题 | 说明 |
| --- | --- |
| tag 已存在，报错 | 在网页创建 Release 时**下拉选择**该 tag，而不是新建；或换一个版本号 |
| 想改发布说明 | 打开该 Release 页 → 铅笔图标编辑 → 保存 |
| 要删除某次发布 | Release 页 → Delete；**删除 Release 默认不会删除 tag**，tag 需单独删除 |
| 只想更新附件 | 编辑 Release → 在附件区删除旧的、拖入新的 |
| 玩家下载后打不开 | 确认上传的是 `CTFontHook-Setup-<版本>.exe`；提醒对方关闭杀毒软件误报（加壳 DLL 可能被误报） |
| Release 与 tag 不一致 | 见第 7 节的注意事项，必要时补打 tag 并重新发布 |

---

## 9. 发布后自检清单

1. Release 页能正常打开，且被标记为 Latest。
2. 附件里有 `CTFontHook-Setup-<版本>.exe`。
3. 从 Release 页**下载**该安装包，在干净目录双击，能完成「选游戏 → 安装」。
4. README 顶部/说明里指向的下载链接可用。
5. `CHANGELOG.md` 已记录本次版本。
